#!/usr/bin/env python3
"""Reference implementation of the web analyzer, used to validate the native port.

Mirrors analyzer/src/lib/analyzer/analyzer.svelte.ts exactly.
"""
import json
import sys

import numpy as np


class Analyzer:
    def __init__(self, bin_rate=16000):
        self.bin_rate = bin_rate
        self.reset()

    def reset(self):
        self.timestamp_raw = []
        self.timestamp_binned = []
        self.consecutive = np.zeros(self.bin_rate + 1)
        self.all_diff = np.zeros(self.bin_rate + 1)
        self.wrapped = np.zeros(self.bin_rate + 1)

    def add(self, timestamps):
        interval = 1000000 / self.bin_rate
        for timestamp in timestamps:
            if self.timestamp_raw and timestamp < self.timestamp_raw[-1]:
                continue
            self.timestamp_raw.append(timestamp)
            binned = round(timestamp / interval) * interval

            if self.timestamp_binned:
                last = self.timestamp_binned[-1]
                if last:
                    consecutive = binned - last
                    if consecutive <= 1000000:
                        index = int(round(consecutive / interval))
                        self.consecutive[index] += 1

                    binned_list = np.array(self.timestamp_binned)
                    start = np.searchsorted(binned_list, binned - 1000000, side='left')
                    for i in range(start, len(binned_list)):
                        long_diff = binned - binned_list[i]
                        index = int(round(long_diff / interval))
                        self.all_diff[index] += 1

                    wrapped = binned % 1000000
                    index = int(round(wrapped / interval))
                    self.wrapped[index] += 1

            self.timestamp_binned.append(binned)

    def freqs(self):
        """The magnitude spectrum of the histograms.

        The transform size is the next power of two, the histograms are zero
        padded to it (the same as the native analyzer does).
        """
        n = len(self.consecutive)
        size = max(1 << (n - 1).bit_length(), 32)
        results = []
        for counts in (self.consecutive, self.all_diff, self.wrapped):
            padded = np.pad(counts, (0, size - n))
            spectrum = np.fft.rfft(padded)
            norm = np.full(len(spectrum), 2.0 / n)
            norm[0] = 1.0 / n
            norm[-1] = 1.0 / n
            results.append(np.abs(spectrum) * norm)
        return results


def main():
    recording, dump_path = sys.argv[1], sys.argv[2]

    with open(recording) as f:
        result = json.load(f)

    timestamps = []
    for events in result['inputs'].values():
        for event in events:
            timestamps.append(event['timestamp'])
    timestamps.sort()

    analyzer = Analyzer()
    analyzer.add(timestamps)
    freqs = analyzer.freqs()

    dumped = {}
    with open(dump_path) as f:
        for line in f:
            parts = line.split()
            dumped[parts[0]] = np.array([float(x) for x in parts[2:]])

    ok = True

    def compare(name, expected, actual):
        global ok
        if len(expected) != len(actual):
            print(f"  FAIL {name}: length {len(expected)} != {len(actual)}")
            ok = False
            return
        error = np.max(np.abs(expected - actual)) if len(expected) else 0.0
        scale = max(1.0, float(np.max(np.abs(expected))) if len(expected) else 1.0)
        relative = error / scale
        # The native analyzer uses a single precision FFT, so allow for a
        # small relative difference.
        status = "ok  " if relative < 1e-5 else "FAIL"
        if status == "FAIL":
            ok = False
        print(f"  {status} {name}: max error {error:.3e} (relative {relative:.3e})")

    compare("consecutive_diff", analyzer.consecutive, dumped["consecutive_diff"])
    compare("all_diff", analyzer.all_diff, dumped["all_diff"])
    compare("wrapped_timestamp", analyzer.wrapped, dumped["wrapped_timestamp"])
    compare("consecutive_diff_freq", freqs[0], dumped["consecutive_diff_freq"])
    compare("all_diff_freq", freqs[1], dumped["all_diff_freq"])
    compare("wrapped_timestamp_freq", freqs[2], dumped["wrapped_timestamp_freq"])

    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
