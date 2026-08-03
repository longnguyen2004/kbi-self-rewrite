import type { ChartDimensions } from './canvasChart';

/**
 * Imperative canvas draw for a line chart plot layer.
 *
 * Draws in plot-local CSS pixels (the caller's 2D context is already
 * DPR-scaled by `setupCanvasChart`, so we work in CSS px here).
 *
 * Steps:
 *  1. Clear the backing store for the plot area.
 *  2. Draw vertical grid lines at each tick x, half-pixel-snapped for a
 *     crisp 1px stroke.
 *  3. Stroke the line path through the decimated points.
 *
 * `points` are in chart x-units; `xScale`/`yScale` map them to plot-local
 * pixels. The grid color, line color and stroke width are passed in so the
 * helper stays theme-agnostic.
 */
export function drawLineChart(
  ctx: CanvasRenderingContext2D,
  dims: ChartDimensions,
  points: { x: number; y: number }[],
  gridTicks: number[],
  xScale: (x: number) => number,
  yScale: (y: number) => number,
  opts: {
    gridColor: string;
    lineColor: string;
    lineWidth?: number;
  },
): void {
  const { innerWidth: w, innerHeight: h } = dims;
  ctx.clearRect(0, 0, w, h);

  // Grid verticals.
  if (gridTicks.length > 0) {
    ctx.beginPath();
    ctx.strokeStyle = opts.gridColor;
    ctx.lineWidth = 1;
    for (const tick of gridTicks) {
      // Half-pixel snap for a crisp 1px vertical line.
      const x = Math.round(xScale(tick)) + 0.5;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
    }
    ctx.stroke();
  }

  // Line path.
  if (points.length === 0) return;
  ctx.beginPath();
  ctx.strokeStyle = opts.lineColor;
  ctx.lineWidth = opts.lineWidth ?? 1;
  // Start at the first point, then line through the rest. Decimated points
  // are already in chart x-units.
  let started = false;
  for (let i = 0; i < points.length; i++) {
    const p = points[i];
    const px = xScale(p.x);
    const py = yScale(p.y);
    if (!started) {
      ctx.moveTo(px, py);
      started = true;
    } else {
      ctx.lineTo(px, py);
    }
  }
  ctx.stroke();
}
