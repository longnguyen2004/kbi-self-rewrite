import { SvelteSet } from "svelte/reactivity";
import { Keycode } from "$lib/keycode/keycode";
import type { Input } from "$lib/validator/validator";

export type Keypress = {
    start: number; end: number; key: string; deviceId: string
};

export interface TimelineProvider {
    keypresses: Keypress[],
    keys: Set<string>,
    endTimestamp: number
}

export class KeyEventToInputTimeline implements TimelineProvider {
    private _keypresses: Keypress[] = $state([]);
    private _keys = new SvelteSet<string>();
    private _endTimestamp = $state(0);
    private _keyDownMap = new Map<string, number>();

    constructor() {}

    get keypresses() {
        return this._keypresses;
    }

    get keys() {
        return this._keys
    }

    get endTimestamp() {
        return this._endTimestamp;
    }

    reset() {
        this._keypresses = [];
        this._keys.clear();
        this._keyDownMap.clear();
        this._endTimestamp = 0;
    }

    add(eventsById: [string, Input][]) {
        for (const [id, event] of eventsById) {
            const { code, timestamp, pressed } = event;
            const mapKey = `${id}-${code}`;
            if (pressed) {
                this._keyDownMap.set(mapKey, timestamp);
            }
            else {
                const keyName = Keycode[code];
                const keydownTime = this._keyDownMap.get(mapKey);
                if (keydownTime === undefined)
                    continue;
                const start = keydownTime / 1000000;
                const end = timestamp / 1000000;
                this._keypresses.push({ start, end, key: keyName, deviceId: id });
                this._keys.add(keyName);
                this._endTimestamp = Math.max(this._endTimestamp, end);
                this._keyDownMap.delete(mapKey);
            }
        }
    }
}
