import FftWorker from './fft_thread?worker';
import type { Request, Response } from './fft_thread';

class FftCalculator {
  private _worker = new FftWorker();
  private _tasks = new Map<number, (res: Float32Array) => unknown>();
  constructor() {
    this._worker.onmessage = (msg: MessageEvent<Response>) => {
      const { key, result } = msg.data;
      this._tasks.get(key)?.(result);
      this._tasks.delete(key);
    };
  }
  submit(data: number[]): Promise<Float32Array> {
    let key;
    do {
      key = Math.random();
    } while (this._tasks.has(key));
    return new Promise((resolve) => {
      this._tasks.set(key, resolve);
      const req: Request = { key, data };
      this._worker.postMessage(req);
    });
  }
  terminate() {
    this._worker.terminate();
  }
}

function lowerBound<T>(
  arr: T[],
  start: number,
  end: number,
  value: T,
  less_than: (a: T, b: T) => boolean,
) {
  let count = end - start;
  while (count > 0) {
    const step = Math.floor(count / 2);
    const it = start + step;
    if (less_than(arr[it], value)) {
      start = it + 1;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  return start;
}

export class Analyzer {
  private _binRate: number = $state(16000);
  private _timestampRaw: number[] = [];
  private _timestampBinned: number[] = [];

  private _consecutiveDiffCount: number[] = $state.raw([]);
  private _allDiffCount: number[] = $state.raw([]);
  private _wrappedTimestampCount: number[] = $state.raw([]);
  private _consecutiveDiffCountMax = 0;
  private _allDiffCountMax = 0;
  private _wrappedTimestampCountMax = 0;

  private _consecutiveDiffFreq: number[] = $state.raw([]);
  private _allDiffFreq: number[] = $state.raw([]);
  private _wrappedTimestampFreq: number[] = $state.raw([]);
  private _consecutiveDiffFreqMax = 0;
  private _allDiffFreqMax = 0;
  private _wrappedTimestampFreqMax = 0;

  private _eventCount = $state(0);

  private _calculating = false;
  private _calculationQueued = false;
  private _consecutiveDiffFftCalculator = new FftCalculator();
  private _allDiffFftCalculator = new FftCalculator();
  private _wrappedTimestampFftCalculator = new FftCalculator();

  constructor() {
    this.reset();
  }
  add(timestamps: number[]) {
    if (!timestamps.length) return;
    console.time('add timestamp');
    let added = 0;
    for (const timestamp of timestamps) {
      // timestamp needs to be strictly monotonic
      if (timestamp < (this._timestampRaw.at(-1) ?? 0)) continue;
      this._timestampRaw.push(timestamp);
      const interval = 1000000 / this._binRate;
      const binned = Math.round(timestamp / interval) * interval;

      const last = this._timestampBinned.at(-1);
      if (last) {
        // Consecutive diff calculation
        const consecutiveDiff = binned - last;
        if (consecutiveDiff <= 1000000) {
          const index = consecutiveDiff / interval;
          this._consecutiveDiffCount[index]++;
          if (this._consecutiveDiffCount[index] > this._consecutiveDiffCountMax) {
            this._consecutiveDiffCountMax = this._consecutiveDiffCount[index];
          }
        }

        // All diff calculation
        const start = lowerBound(
          this._timestampBinned,
          0,
          this._timestampBinned.length,
          binned - 1000000,
          (a, b) => a < b,
        );
        for (let i = start; i < this._timestampBinned.length; ++i) {
          const longDiff = binned - this._timestampBinned[i];
          const index = longDiff / interval;
          this._allDiffCount[index]++;
          if (this._allDiffCount[index] > this._allDiffCountMax) {
            this._allDiffCountMax = this._allDiffCount[index];
          }
        }

        // 1s modulo calculation
        const binnedWrapped = binned % 1000000;
        const wrappedIndex = binnedWrapped / interval;
        this._wrappedTimestampCount[wrappedIndex]++;
        if (this._wrappedTimestampCount[wrappedIndex] > this._wrappedTimestampCountMax) {
          this._wrappedTimestampCountMax = this._wrappedTimestampCount[wrappedIndex];
        }
      }
      this._timestampBinned.push(binned);
      added++;
    }
    this._consecutiveDiffCount = Array.from(this._consecutiveDiffCount);
    this._allDiffCount = Array.from(this._allDiffCount);
    this._wrappedTimestampCount = Array.from(this._wrappedTimestampCount);
    console.timeEnd('add timestamp');
    console.log(`added ${added} events`);
    this._eventCount += added;
    this._queueRecalcFourier();
  }
  private _queueRecalcFourier() {
    this._calculationQueued = true;
    if (this._calculating) return;
    this._calculating = true;
    (async () => {
      while (this._calculationQueued) {
        this._calculationQueued = false;
        const maxFreq = this._binRate / 2;
        const len = this._consecutiveDiffCount.length;
        const calc1 = this._consecutiveDiffFftCalculator.submit(this._consecutiveDiffCount);
        const calc2 = this._allDiffFftCalculator.submit(this._allDiffCount);
        const calc3 = this._wrappedTimestampFftCalculator.submit(this._wrappedTimestampCount);
        const [freq1, freq2, freq3] = await Promise.all([calc1, calc2, calc3]);
        this._consecutiveDiffFreq = new Array(maxFreq + 1);
        this._allDiffFreq = new Array(maxFreq + 1);
        this._wrappedTimestampFreq = new Array(maxFreq + 1);
        this._consecutiveDiffFreqMax = 0;
        this._allDiffFreqMax = 0;
        this._wrappedTimestampFreqMax = 0;
        for (let i = 0; i < freq1.length; i += 2) {
          const freq = i / 2;
          const normFactor = (freq == 0 || freq == maxFreq ? 1 : 2) / len;
          const consecutive = Math.hypot(freq1[i], freq1[i + 1]) * normFactor;
          const all = Math.hypot(freq2[i], freq2[i + 1]) * normFactor;
          const wrapped = Math.hypot(freq3[i], freq3[i + 1]) * normFactor;
          this._consecutiveDiffFreq[freq] = consecutive;
          this._allDiffFreq[freq] = all;
          this._wrappedTimestampFreq[freq] = wrapped;
          if (consecutive > this._consecutiveDiffFreqMax)
            this._consecutiveDiffFreqMax = consecutive;
          if (all > this._allDiffFreqMax) this._allDiffFreqMax = all;
          if (wrapped > this._wrappedTimestampFreqMax) this._wrappedTimestampFreqMax = wrapped;
        }
      }
      this._calculating = false;
    })();
  }
  reset() {
    this._timestampRaw = [];
    this._timestampBinned = [];
    this._consecutiveDiffCount = new Array(this._binRate + 1).fill(0);
    this._allDiffCount = new Array(this._binRate + 1).fill(0);
    this._wrappedTimestampCount = new Array(this._binRate + 1).fill(0);
    this._consecutiveDiffCountMax = 0;
    this._allDiffCountMax = 0;
    this._wrappedTimestampCountMax = 0;
    this._consecutiveDiffFreq = new Array(this._binRate / 2 + 1).fill(0);
    this._allDiffFreq = new Array(this._binRate / 2 + 1).fill(0);
    this._wrappedTimestampFreq = new Array(this._binRate / 2 + 1).fill(0);
    this._consecutiveDiffFreqMax = 0;
    this._allDiffFreqMax = 0;
    this._wrappedTimestampFreqMax = 0;
    this._eventCount = 0;
  }
  terminate() {
    this._consecutiveDiffFftCalculator.terminate();
    this._allDiffFftCalculator.terminate();
    this._wrappedTimestampFftCalculator.terminate();
  }
  get binRate() {
    return this._binRate;
  }
  set binRate(newBin: number) {
    if (newBin <= 0) return;
    if (newBin % 125 != 0) return;
    let v = newBin / 125;
    if ((v & (v - 1)) != 0) return;
    this._binRate = newBin;
    const oldTimestamps = this._timestampRaw;
    this.reset();
    this.add(oldTimestamps);
  }
  get consecutiveDiff() {
    return this._consecutiveDiffCount;
  }
  get consecutiveDiffMax() {
    return this._consecutiveDiffCountMax;
  }
  get allDiff() {
    return this._allDiffCount;
  }
  get allDiffMax() {
    return this._allDiffCountMax;
  }
  get wrappedTimestamp() {
    return this._wrappedTimestampCount;
  }
  get wrappedTimestampMax() {
    return this._wrappedTimestampCountMax;
  }
  get consecutiveDiffFreq() {
    return this._consecutiveDiffFreq;
  }
  get consecutiveDiffFreqMax() {
    return this._consecutiveDiffFreqMax;
  }
  get allDiffFreq() {
    return this._allDiffFreq;
  }
  get allDiffFreqMax() {
    return this._allDiffFreqMax;
  }
  get wrappedTimestampFreq() {
    return this._wrappedTimestampFreq;
  }
  get wrappedTimestampFreqMax() {
    return this._wrappedTimestampFreqMax;
  }
  get eventCount() {
    return this._eventCount;
  }
  get calculating() {
    return this._calculating;
  }
}
