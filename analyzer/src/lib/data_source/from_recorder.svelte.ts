import * as Neutralino from "@neutralinojs/lib";
import Emittery from "emittery";
import { isNeutralino } from "$lib/helper/is_neutralino";
import type { DataSource } from ".";
import type { Result, Device, UsbDevice, Input } from "$lib/validator/validator";

const extension = "recorder";

export type Events = {
    input: [string, Input]
}

export class FromRecorderDataSource extends Emittery<Events> implements DataSource {
    private _result: Result | undefined = $state();

    constructor() {
        super();
        if (!isNeutralino())
            throw new Error("This source can only be used in a Neutralino environment");
    }

    get result() {
        return this._result;
    }

    private _onSystemInfo = (ev: CustomEvent) => {
        this._result!.info = ev.detail;
    }

    private _onUsbDevice = (ev: CustomEvent) => {
        const [id, data] = Object.entries(ev.detail as Record<string, UsbDevice>)[0];
        this._result!.usb_devices![id] = data;
    }

    private _onDevice = (ev: CustomEvent) => {
        const [id, data] = Object.entries(ev.detail as Record<string, Device>)[0];
        this._result!.devices![id] = data;
    }

    private _onInput = (ev: CustomEvent) => {
        const [id, data] = Object.entries(ev.detail as Record<string, Input>)[0];
        this._result!.inputs![id] ??= [];
        this._result!.inputs![id].push(data);
        this.emit("input", [id, data]);
    }

    async start() {
        this._result = {
            info: {
                os_name: "",
                os_ver: "",
                arch: "",
                backend: ""
            },
            time: new Date().toISOString(),
            usb_devices: {},
            devices: {},
            inputs: {}
        };
        Neutralino.events.on("systemInfo", this._onSystemInfo);
        Neutralino.events.on("usbDevice", this._onUsbDevice);
        Neutralino.events.on("device", this._onDevice);
        Neutralino.events.on("input", this._onInput);
        await Neutralino.extensions.dispatch(extension, "start");
    }

    async stop() {
        await Neutralino.extensions.dispatch(extension, "stop");
        Neutralino.events.off("systemInfo", this._onSystemInfo);
        Neutralino.events.off("usbDevice", this._onUsbDevice);
        Neutralino.events.off("device", this._onDevice);
        Neutralino.events.off("input", this._onInput);
    }
}