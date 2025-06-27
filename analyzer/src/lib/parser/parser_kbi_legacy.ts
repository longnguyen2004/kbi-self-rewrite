import type { Input, Result } from "$lib/validator/validator";
import { keyStrToVk, vkToKeycode } from "$lib/helper/vk_to_keycode";
import { Keycode } from "$lib/keycode/keycode";

type ReaderState = {
    buf: Uint8Array,
    offset: number
};

function read_uleb128(state: ReaderState)
{
    let { buf, offset } = state;
    let result = 0;
    let shift = 0;
    while (true)
    {
        if (offset + 1 > buf.length)
            return undefined;
        const byte = buf[offset++];
        result |= (byte & 0x7F) << shift;
        if (!(byte & 0x80))
        {
            state.offset = offset;
            return result;
        }
        shift += 7;
    }
}

function read_bool(state: ReaderState)
{
    let { buf, offset } = state;
    if (offset + 1 > buf.length)
        return undefined;
    return !!buf[state.offset++];
}

function read_int32(state: ReaderState, unsigned = false)
{
    let { buf, offset } = state;
    if (offset + 4 > buf.length)
        return undefined;
    const dataView = new DataView(buf.buffer);
    const result = unsigned ? dataView.getUint32(offset, true) : dataView.getInt32(offset, true);
    state.offset += 4;
    return result;
}

function read_int64(state: ReaderState, unsigned = false)
{
    let { buf, offset } = state;
    if (offset + 8 > buf.length)
        return undefined;
    const dataView = new DataView(buf.buffer);
    const result = unsigned ? dataView.getBigUint64(offset, true) : dataView.getBigInt64(offset, true);
    state.offset += 8;
    return result;
}

function read_double(state: ReaderState)
{
    let { buf, offset } = state;
    if (offset + 8 > buf.length)
        return undefined;
    const dataView = new DataView(buf.buffer);
    const result = dataView.getFloat64(offset, true);
    state.offset += 8;
    return result;
}

function read_string(state: ReaderState)
{
    let { offset: originalOffset } = state;
    const len = read_uleb128(state);
    if (len === undefined)
        return undefined;
    let { buf, offset } = state;
    if (offset + len > buf.length)
    {
        state.offset = originalOffset;
        return undefined;
    }
    const decoder = new TextDecoder();
    const str = decoder.decode(buf.subarray(offset, offset + len));
    state.offset += len;
    return str;
}

function read_time(state: ReaderState)
{
    const time = read_int64(state, true);
    if (time === undefined)
        return undefined;
    return new Date(Number(time * 100n / 1000000n) - Date.UTC(1, 0, 1, 0, 0, 0, 0));
}

function read_list<T>(state: ReaderState, read_elem: (state: ReaderState) => T | undefined)
{
    let { offset } = state;
    let length = read_int32(state);
    if (length === undefined)
        return undefined;
    const arr = Array<T>(length);
    for (let i = 0; i < length; ++i)
    {
        const elem = read_elem(state);
        if (elem === undefined)
        {
            state.offset = offset;
            return undefined;
        }
        arr[i] = elem;
    }
    return arr;
}

type LegacyKbiInput = {
    key: string,
    source: BigInt
}

type LegacyKbiEvent = {
    time: number,
    pressed: boolean,
    input: LegacyKbiInput
}

type LegacyKbiDevice = {
    id: BigInt,
    input_count: number,
    name: string,
    device_id: string
}

function read_kbiinput(state: ReaderState)
{
    const key = read_string(state);
    if (key === undefined)
        return undefined;
    const source = read_int64(state);
    if (source === undefined)
        return undefined;
    return { key, source } as LegacyKbiInput
}

function read_kbievent(state: ReaderState)
{
    const time = read_double(state);
    if (time === undefined)
        return undefined;
    const pressed = read_bool(state);
    if (pressed === undefined)
        return undefined;
    const input = read_kbiinput(state);
    if (input === undefined)
        return undefined
    return { time, pressed, input } as LegacyKbiEvent
}

function read_kbidevice(state: ReaderState)
{
    const id = read_int64(state);
    if (id === undefined)
        return undefined;
    const input_count = read_int32(state);
    if (input_count === undefined)
        return undefined;
    const name = read_string(state);
    if (name === undefined)
        return undefined;
    const device_id = read_string(state);
    if (device_id === undefined)
        return undefined;
    return { id, input_count, name, device_id } as LegacyKbiDevice
}

function parse_kbi_legacy_v3(state: ReaderState)
{
    const creator = read_string(state);
    if (creator === undefined)
        return undefined;
    const title = read_string(state);
    if (title === undefined)
        return undefined;
    const time = read_time(state);
    if (!time)
        return undefined;
    const duration = read_double(state);
    if (duration === undefined)
        return undefined;
    const inputs = read_list(state, read_kbievent);
    if (!inputs)
        return undefined;
    const sources = read_list(state, read_kbidevice);
    if (!sources)
        return undefined;
    const sourceMap = new Map<BigInt, string>();
    for (const source of sources)
    {
        sourceMap.set(source.id, source.device_id);
    }
    const inputMap = new Map<string, Input[]>();
    for (const input of inputs)
    {
        const source = sourceMap.get(input.input.source)!;
        let inputArr = inputMap.get(source);
        if (!inputArr)
        {
            inputArr = [] as Input[];
            inputMap.set(source, inputArr);
        }
        const vk = keyStrToVk(input.input.key);
        inputArr.push({
            timestamp: input.time * 1000000,
            pressed: input.pressed,
            code: vk ? vkToKeycode(vk) : Keycode.None
        })
    }
    return {
        info: {
            os_name: "",
            os_ver: "",
            arch: "",
            backend: ""
        },
        time: time.toString(),
        devices: Object.fromEntries(
            sources.map(el => [el.device_id, { name: el.name, vid: 0, pid: 0 }])
        ),
        inputs: Object.fromEntries(
            inputMap.entries()
        )
    } satisfies Result
}

export function parse_kbi_legacy(buffer: Uint8Array)
{
    const header = "KBI\0";
    for (let i = 0; i < 4; ++i)
        if (buffer[i] != header.charCodeAt(i))
            return undefined;
    const state = {
        buf: buffer,
        offset: 4
    };
    const version = read_int32(state, true);
    switch (version)
    {
        case 3:
            return parse_kbi_legacy_v3(state);
        default:
            return undefined;
    }
}
