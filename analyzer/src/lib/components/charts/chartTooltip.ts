import { select } from 'd3-selection';
import { quadtree, type Quadtree } from 'd3-quadtree';
import type { ScaleLinear } from 'd3-scale';
import type { ChartTheme } from './chartTheme.svelte';

export type TooltipDatum = {
  x: number;
  y: number;
};

/**
 * Creates a tooltip overlay for an SVG chart. Renders a vertical crosshair
 * line and a floating label showing the nearest data point's (x, y) values.
 *
 * Call `update()` on mousemove and `hide()` on mouseleave.
 */
export function createChartTooltip(
  plotG: SVGGElement,
  getTheme: () => ChartTheme,
  xScale: () => ScaleLinear<number, number>,
  yScale: () => ScaleLinear<number, number>,
  formatX: (v: number) => string,
  formatY: (v: number) => string,
) {
  // --- Build tooltip elements (once) ---
  const container = select(plotG).append('g').attr('class', 'chart-tooltip').style('display', 'none');

  // Vertical crosshair line
  container
    .append('line')
    .attr('class', 'tooltip-crosshair')
    .attr('stroke-width', 1)
    .attr('stroke-dasharray', '4,3');

  // Dot marker at the snapped data point
  container
    .append('circle')
    .attr('class', 'tooltip-dot')
    .attr('r', 3.5)
    .attr('stroke-width', 1.5);

  // Background rect for the label
  const labelBg = container
    .append('rect')
    .attr('class', 'tooltip-bg')
    .attr('rx', 3)
    .attr('ry', 3);

  // Label text
  const labelText = container
    .append('text')
    .attr('class', 'tooltip-text')
    .attr('text-anchor', 'middle')
    .attr('dominant-baseline', 'hanging')
    .attr('font-size', '11px')
    .attr('font-family', 'monospace');

  function applyTheme() {
    const theme = getTheme();
    container
      .select('.tooltip-crosshair')
      .attr('stroke', theme.text)
      .attr('opacity', 0.5);
    container
      .select('.tooltip-dot')
      .attr('fill', theme.background)
      .attr('stroke', theme.text);
    labelBg.attr('fill', theme.background).attr('stroke', theme.text);
    labelText.attr('fill', theme.text);
  }

  function update(data: TooltipDatum[], mouseX: number, mouseY: number, innerHeight: number) {
    if (!data || data.length === 0) {
      hide();
      return;
    }

    const xs = xScale();
    const ys = yScale();

    // Build a quadtree over the *pixel* coordinates of each data point so
    // find() can do an efficient 2D nearest-neighbor lookup against the
    // mouse position. We rebuild on every update since the underlying
    // scales (and therefore pixel positions) change during zoom/pan.
    const tree: Quadtree<TooltipDatum> = quadtree<TooltipDatum>()
      .x((d) => xs(d.x))
      .y((d) => ys(d.y))
      .addAll(data);

    const nearest = tree.find(mouseX, mouseY);
    console.log([mouseX, mouseY]);
    console.log(nearest);
    if (!nearest) {
      hide();
      return;
    }

    const cx = xs(nearest.x);
    const cy = ys(nearest.y);

    container.style('display', null);

    // Crosshair
    container
      .select('.tooltip-crosshair')
      .attr('x1', cx)
      .attr('x2', cx)
      .attr('y1', 0)
      .attr('y2', innerHeight);

    // Dot marker at the snapped data point
    container.select('.tooltip-dot').attr('cx', cx).attr('cy', cy);

    // Label text
    const label = `${formatX(nearest.x)}, ${formatY(nearest.y)}`;
    labelText.text(label);

    // Position label above the data point, centered horizontally
    const bbox = (labelText.node() as SVGTextElement | null)?.getBBox();
    const lw = bbox ? bbox.width + 10 : 60;
    const lh = bbox ? bbox.height + 6 : 18;
    let lx = cx - lw / 2;
    let ly = cy - lh - 6;

    // Keep label inside the chart area
    const maxX = xs.range()[1];
    if (lx < 0) lx = 0;
    if (lx + lw > maxX) lx = maxX - lw;
    if (ly < 0) {
      // Place below if above would overflow
      ly = cy + 6;
    }

    labelBg.attr('x', lx).attr('y', ly).attr('width', lw).attr('height', lh);
    labelText.attr('x', lx + lw / 2).attr('y', ly + 3);

    applyTheme();
  }

  function hide() {
    container.style('display', 'none');
  }

  function destroy() {
    container.remove();
  }

  return { update, hide, destroy, applyTheme };
}