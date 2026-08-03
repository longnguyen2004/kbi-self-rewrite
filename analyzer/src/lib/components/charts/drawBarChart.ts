import type { ChartDimensions } from './canvasChart';

/**
 * Imperative canvas draw for a bar chart data layer (InputTimeline).
 *
 * Draws in plot-local CSS pixels (the caller's 2D context is already
 * DPR-scaled by `setupCanvasChart`).
 *
 * Grid lines are NOT drawn here — they live in the SVG overlay (`gridG`)
 * so they are themed via `applyTheme()` (which runs on theme change) and
 * stay pixel-aligned with the d3-axis ticks. This helper only clears the
 * backing store and fills each bar (rounded rect) with its device color.
 *
 * `bars` carry `start`/`end` (chart x-units) and `key` (band name); the
 * `xScale`, `yScale(key)`, and `bandwidth` accessors map them to pixels.
 */
export type BarDatum = {
  start: number;
  end: number;
  key: string;
  color: string;
};

export function drawBarChart(
  ctx: CanvasRenderingContext2D,
  dims: ChartDimensions,
  bars: BarDatum[],
  xScale: (x: number) => number,
  yScaleOf: (key: string) => number | undefined,
  bandwidth: number,
  opts: {
    radius?: number;
  },
): void {
  const { innerWidth: w, innerHeight: h } = dims;
  ctx.clearRect(0, 0, w, h);

  // Bars.
  const r = opts.radius ?? 2;
  for (let i = 0; i < bars.length; i++) {
    const b = bars[i];
    const x = xScale(b.start);
    const y = yScaleOf(b.key);
    if (y === undefined) continue;
    const width = Math.max(1, xScale(b.end) - xScale(b.start));
    const height = bandwidth;
    ctx.fillStyle = b.color;
    drawRoundedRect(ctx, x, y, width, height, r);
    ctx.fill();
  }
}

function drawRoundedRect(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  w: number,
  h: number,
  r: number,
): void {
  // Clamp the corner radius to half the smaller dimension so very thin/short
  // bars don't produce weird arcs.
  const rr = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + rr, y);
  ctx.arcTo(x + w, y, x + w, y + h, rr);
  ctx.arcTo(x + w, y + h, x, y + h, rr);
  ctx.arcTo(x, y + h, x, y, rr);
  ctx.arcTo(x, y, x + w, y, rr);
  ctx.closePath();
}
