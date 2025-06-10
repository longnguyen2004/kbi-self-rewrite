import FftWorker from "./analyzer_fft_thread?worker";
import { OrderedMap } from "js-sdsl";
import type { Request, Response } from "./analyzer_fft_thread";

class FftCalculator {
    private _worker = new FftWorker();
    private _tasks = new Map<number, (res: Float32Array) => unknown>();
    constructor() {
        this._worker.onmessage = (msg: MessageEvent<Response>) => {
            const { key, result } = msg.data;
            this._tasks.get(key)?.(result);
            this._tasks.delete(key);
        }
    }
    submit(data: number[]): Promise<Float32Array> {
        let key;
        do {
            key = Math.random();
        }
        while (this._tasks.has(key));
        return new Promise((resolve) => {
            this._tasks.set(key, resolve);
            const req: Request = { key, data };
            this._worker.postMessage(req);
        })
    }
}

export class Analyzer {
    private _binRate: number = $state(4000);
    private _timestampRaw: number[] = [];
    private _timestampBinned = new OrderedMap<number, number>();

    private _consecutiveDiffCount: number[] = $state.raw([]);
    private _allDiffCount: number[] = $state.raw([]);

    private _consecutiveDiffFreq?: number[] = $state.raw();
    private _allDiffFreq?: number[] = $state.raw();

    private _calculating = false;
    private _calculationQueued = false;
    private _consecutiveDiffFftCalculator = new FftCalculator();
    private _allDiffFftCalculator = new FftCalculator();

    constructor() {
        this._recreateArray();
    }
    private _recreateArray() {
        this._timestampBinned.clear();
        this._consecutiveDiffCount = Array(this._binRate + 1).fill(0);
        this._allDiffCount = Array(this._binRate + 1).fill(0);
        this._consecutiveDiffFreq = [];
        this._allDiffFreq = [];
    }
    add(timestamps: number[]) {
        if (!timestamps.length)
            return;
        for (const timestamp of timestamps) {
            // timestamp needs to be strictly monotonic
            if (timestamp < (this._timestampRaw.at(-1) ?? 0))
                continue;
            this._timestampRaw.push(timestamp);
            const interval = 1000000 / this._binRate;
            const binned = Math.round(timestamp / interval) * interval;

            const last = this._timestampBinned.back();
            if (last) {
                const consecutiveDiff = binned - last[0];
                if (consecutiveDiff <= 1000000)
                    this._consecutiveDiffCount[consecutiveDiff / interval] += last[1];
                const start = this._timestampBinned.lowerBound(binned - 1000000);
                for (let i = start; !i.equals(this._timestampBinned.end()); i = i.next()) {
                    const longDiff = binned - i.pointer[0];
                    this._allDiffCount[longDiff / interval] += i.pointer[1];
                }
                this._timestampBinned.setElement(binned, consecutiveDiff === 0 ? last[1] + 1 : 1);
            }
            else {
                this._timestampBinned.setElement(binned, 1);
            }
        }
        const end = performance.now();
        this._consecutiveDiffCount = this._consecutiveDiffCount;
        this._allDiffCount = this._allDiffCount;
        this._queueRecalcFourier();
    }
    private _queueRecalcFourier() {
        this._calculationQueued = true;
        if (this._calculating)
            return
        (async () => {
            while (this._calculationQueued) {
                this._calculationQueued = false;
                const calc1 = this._consecutiveDiffFftCalculator.submit(
                    this._consecutiveDiffCount
                );
                const calc2 = this._allDiffFftCalculator.submit(
                    this._allDiffCount
                );
                const [freq1, freq2] = await Promise.all([calc1, calc2]);
                this._consecutiveDiffFreq = new Array(freq1.length / 2);
                this._allDiffFreq = new Array(freq2.length / 2);
                for (let i = 0; i < freq1.length; i += 2)
                {
                    this._consecutiveDiffFreq[i / 2] = (Math.hypot(freq1[i], freq1[i + 1]));
                    this._allDiffFreq[i / 2] = (Math.hypot(freq2[i], freq2[i + 1]));
                }
            }
            this._calculating = false;
        })();
    }
    reset() {
        this._timestampRaw = [];
        this._recreateArray();
    }
    get binRate() {
        return this._binRate;
    }
    set binRate(newBin: number) {
        if (newBin <= 0)
            return;
        if (newBin % 125 != 0)
            return;
        let v = newBin / 125;
        if ((v & (v - 1)) != 0)
            return;
        this._binRate = newBin;
        const oldTimestamps = this._timestampRaw;
        this.reset();
        this.add(oldTimestamps);
    }
    get consecutiveDiff() {
        return this._consecutiveDiffCount;
    }
    get allDiff() {
        return this._allDiffCount;
    }
    get consecutiveDiffFreq() {
        return this._consecutiveDiffFreq;
    }
    get allDiffFreq() {
        return this._allDiffFreq;
    }
    get calculating() {
        return this._calculating;
    }
}