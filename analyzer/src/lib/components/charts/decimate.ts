import { clamp } from '$lib/helper/math';

/**
 * Downsamples `data` over the visible `domain` (in chart x-units) into at
 * most `width * 2` points using bucket-max selection.
 *
 * `xExtent` is the full x-range covered by `data` (e.g. `[0, 1000]` for a
 * 1000-unit ms chart, or `[0, binRate/2]` for an FFT chart). It is used to
 * map between chart x-units and array indices, and to emit output x-values
 * in the chart's own coordinate system so the line generator + scale work
 * unchanged.
 */
export function decimate(
  data: number[],
  extent: [number, number],
  domain: [number, number],
  width: number,
) {
  const n = data.length;
  if (n === 0) return [];

  const [extentMin, extentMax] = extent;
  const extentSpan = extentMax - extentMin;
  // Degenerate extent: fall back to identity mapping so we still emit
  // something rather than NaN-poisoning the path.
  const denom = extentSpan !== 0 ? extentSpan : 1;
  const indexDenom = n > 1 ? n - 1 : 1;

  const [domainMin, domainMax] = domain;
  const xMin = Math.min(domainMin, domainMax);
  const xMax = Math.max(domainMin, domainMax);

  const toIndex = (x: number) =>
    clamp(Math.round(((x - extentMin) / denom) * indexDenom), 0, n - 1);

  const indexMin = toIndex(xMin);
  const indexMax = clamp(toIndex(xMax), indexMin, n - 1);
  const visibleCount = indexMax - indexMin + 1;
  const maxPoints = Math.max(256, Math.floor(width * 2));
  const bucketSize = Math.max(1, Math.ceil(visibleCount / maxPoints));
  const sampleCount = Math.ceil(visibleCount / bucketSize);

  const points = new Array(sampleCount);

  let pointIndex = 0;
  for (let bucketStart = indexMin; bucketStart <= indexMax; bucketStart += bucketSize) {
    const bucketEnd = Math.min(indexMax + 1, bucketStart + bucketSize);
    let bucketMax = -Infinity;
    let bucketMaxIndex = bucketStart;
    for (let i = bucketStart; i < bucketEnd; i++) {
      const value = data[i];
      if (value > bucketMax) {
        bucketMax = value;
        bucketMaxIndex = i;
      }
    }

    const x = extentMin + (bucketMaxIndex / indexDenom) * denom;
    const y = bucketMax;
    if (!points[pointIndex]) points[pointIndex] = { x, y };
    else {
      points[pointIndex].x = x;
      points[pointIndex].y = y;
    }
    pointIndex++;
  }

  return points;
}
