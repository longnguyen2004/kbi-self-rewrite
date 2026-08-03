import type { ChartDimensions } from './canvasChart';

/**
 * Imperative canvas draw for a line chart data layer.
 *
 * Draws in plot-local CSS pixels (the caller's 2D context is already
 * DPR-scaled by `setupCanvasChart`, so we work in CSS px here).
 *
 * Grid lines are NOT drawn here — they live in the SVG overlay (`gridG`)
 * so they are themed via `applyTheme()` (which runs on theme change) and
 * stay pixel-aligned with the d3-axis ticks. This helper only clears the
 * backing store and strokes the line path through the decimated points.
 *
 * `points` are in chart x-units; `xScale`/`yScale` map them to plot-local
 * pixels. The line color and stroke width are passed in so the helper
 * stays theme-agnostic.
 */
export function drawLineChart(
  ctx: CanvasRenderingContext2D,
  dims: ChartDimensions,
  points: { x: number; y: number }[],
  xScale: (x: number) => number,
  yScale: (y: number) => number,
  opts: {
    lineColor: string;
    lineWidth?: number;
  },
): void {
  const { innerWidth: w, innerHeight: h } = dims;
  ctx.clearRect(0, 0, w, h);

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
