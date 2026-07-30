import { select } from 'd3-selection';

export type ChartDimensions = {
  width: number;
  height: number;
  innerWidth: number;
  innerHeight: number;
  margin: { top: number; right: number; bottom: number; left: number };
};

export const defaultMargin = {
  top: 10,
  right: 0,
  bottom: 30,
  left: 40,
};

/**
 * Sets up an SVG chart container with a clip path and resize observation.
 * Returns helpers to access the root <g> group and current dimensions.
 */
export function setupSvgChart(
  svg: SVGSVGElement,
  margin: typeof defaultMargin = defaultMargin,
  onResize: (dims: ChartDimensions) => void,
): { cleanup: () => void; clipId: string } {
  const selection = select(svg);
  selection.attr('width', '100%').attr('height', '100%').style('display', 'block');

  // Clip path so data doesn't overflow the plot area.
  const clipId = `clip-${Math.random().toString(36).slice(2)}`;
  const defs = selection.append('defs');
  defs.append('clipPath')
    .attr('id', clipId)
    .append('rect')
    .attr('x', 0)
    .attr('y', 0);

  // Root group is translated by the margin. d3-zoom is attached to this
  // group (not the <svg>), so its coordinate space matches the scales'
  // range [0, innerWidth] / [0, innerHeight]. This keeps rescaleX aligned
  // with the rendered axes and avoids panning/zooming offsets.
  const root = selection.append('g').attr('transform', `translate(${margin.left},${margin.top})`);

  // Transparent background rect so the <g> captures pointer/wheel events
  // for d3-zoom even over empty areas of the plot.
  const bg = root.append('rect').attr('fill', 'none').attr('pointer-events', 'all');

  const compute = () => {
    const rect = svg.getBoundingClientRect();
    const width = Math.max(0, rect.width);
    const height = Math.max(0, rect.height);
    const innerWidth = Math.max(0, width - margin.left - margin.right);
    const innerHeight = Math.max(0, height - margin.top - margin.bottom);
    selection
      .select(`#${clipId} rect`)
      .attr('width', innerWidth)
      .attr('height', innerHeight);
    bg.attr('width', innerWidth).attr('height', innerHeight);
    onResize({ width, height, innerWidth, innerHeight, margin });
  };

  const ro = new ResizeObserver(compute);
  ro.observe(svg);
  compute();

  return {
    cleanup: () => {
      ro.disconnect();
      selection.selectAll('*').remove();
    },
    clipId
  }
}

export { select };
