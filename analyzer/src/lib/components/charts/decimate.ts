import { clamp } from '$lib/helper/math';

/**
 * Downsamples `data` over the visible `domain` (in chart x-units) into one
 * point per pixel column of `width`, using bucket-max selection.
 *
 * `xExtent` is the full x-range covered by `data` (e.g. `[0, 1000]` for a
 * 1000-unit ms chart, or `[0, binRate/2]` for an FFT chart). It is used to
 * map between chart x-units and array indices, and to emit output x-values
 * in the chart's own coordinate system so the line generator + scale work
 * unchanged.
 *
 * Zoom-stability: buckets are anchored to the **pixel grid** (one bucket
 * per pixel column), not to a fixed point count derived from the visible
 * range. For a given zoom level each pixel column always covers the same
 * data-index span, so the bucket-max sample selected per column is
 * deterministic and the line shape does not reshuffle as you pan within
 * that zoom. Changing the zoom level re-buckets, but the shape stays
 * consistent for that level rather than morphing continuously.
 */
export function decimate(
  data: number[],
  extent: [number, number],
  domain: [number, number],
  width: number,
) {
  const n = data.length;
  if (n === 0 || width <= 0) return [];

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

  // Map the visible domain to pixel columns, then to data-index ranges.
  // One bucket per pixel column keeps the output count stable at `width`
  // and anchors bucket boundaries to the pixel grid (zoom-stable shape).
  const domainSpan = xMax - xMin || 1;
  const pxToX = (px: number) => xMin + (px / width) * domainSpan;

  const points: { x: number; y: number }[] = [];

  // Track the last emitted x to skip duplicate pixel columns (when many
  // pixels map to the same single data index, e.g. extreme zoom-in).
  let lastIndex = -1;
  for (let px = 0; px < width; px++) {
    // Sample at the pixel's left edge; use the next pixel's left edge as
    // the bucket's right boundary so columns are contiguous and non-
    // overlapping.
    const xLeft = pxToX(px);
    const xRight = pxToX(px + 1);
    const iStart = toIndex(xLeft);
    const iEnd = clamp(toIndex(xRight), iStart, n - 1);

    // Skip pixel columns that map to an index we already emitted (extreme
    // zoom-in where one data point spans many pixels). The line generator
    // will draw the horizontal run between the duplicated x's anyway.
    if (iStart === lastIndex && iEnd === lastIndex) continue;
    lastIndex = iEnd;

    let bucketMax = -Infinity;
    let bucketMaxIndex = iStart;
    for (let i = iStart; i <= iEnd; i++) {
      const value = data[i];
      if (value > bucketMax) {
        bucketMax = value;
        bucketMaxIndex = i;
      }
    }

    const x = extentMin + (bucketMaxIndex / indexDenom) * denom;
    const y = bucketMax;
    points.push({ x, y });
  }

  return points;
}
