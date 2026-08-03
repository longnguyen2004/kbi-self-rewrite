import type { ChartDimensions } from './canvasChart';

/**
 * Imperative canvas draw for a bar chart plot layer (InputTimeline).
 *
 * Draws in plot-local CSS pixels (the caller's 2D context is already
 * DPR-scaled by `setupCanvasChart`).
 *
 * Steps:
 *  1. Clear the backing store for the plot area.
 *  2. Draw vertical grid lines at each x tick (half-pixel-snapped).
 *  3. Draw horizontal grid lines at the center of each key's band.
 *  4. Fill each bar (rounded rect) with its device color.
 *
 * `bars` carry `start`/`end` (chart x-units) and `key` (band name); the
 * `xScale`, `yScale(key)`, and `bandwidth` accessors map them to pixels.
 * `colorOf` returns the fill color for a bar.
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
  gridXTicks: number[],
  gridYKeys: string[],
  xScale: (x: number) => number,
  yScaleOf: (key: string) => number | undefined,
  bandwidth: number,
  opts: {
    gridColor: string;
    radius?: number;
  },
): void {
  const { innerWidth: w, innerHeight: h } = dims;
  ctx.clearRect(0, 0, w, h);

  // Vertical grid lines.
  if (gridXTicks.length > 0) {
    ctx.beginPath();
    ctx.strokeStyle = opts.gridColor;
    ctx.lineWidth = 1;
    for (const tick of gridXTicks) {
      const x = Math.round(xScale(tick)) + 0.5;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
    }
    ctx.stroke();
  }

  // Horizontal grid lines at band centers.
  if (gridYKeys.length > 0) {
    ctx.beginPath();
    ctx.strokeStyle = opts.gridColor;
    ctx.lineWidth = 1;
    for (const key of gridYKeys) {
      const y0 = yScaleOf(key);
      if (y0 === undefined) continue;
      const y = Math.round(y0 + bandwidth / 2) + 0.5;
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
    }
    ctx.stroke();
  }

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
