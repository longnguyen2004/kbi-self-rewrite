import { select } from 'd3-selection';

export type ChartDimensions = {
  width: number;
  height: number;
  innerWidth: number;
  innerHeight: number;
  margin: { top: number; right: number; bottom: number; left: number };
};

export function getTranslateExtent(dims: ChartDimensions): [[number, number], [number, number]] {
  return [
    [0, 0],
    [dims.innerWidth, dims.innerHeight],
  ];
}

export const defaultMargin = {
  top: 10,
  right: 30,
  bottom: 30,
  left: 40,
};

/**
 * Sets up a hybrid canvas+SVG chart container.
 *
 * Layout:
 *  - A `<canvas>` sized to the inner plot area, positioned absolutely at
 *    `(margin.left, margin.top)`, with a DPR-scaled backing store. It has
 *    `pointer-events: none` so it never intercepts gestures — it is purely
 *    a render target for the data layer (line/bars).
 *  - A transparent `<svg>` overlay on top that still owns the d3-axis
 *    groups, a clipped `gridG` for SVG grid lines (themed via
 *    `applyTheme()`, so they update on theme change without a canvas
 *    redraw), a transparent `bg` rect (so d3-zoom captures pointer/wheel
 *    events over empty plot area), and acts as the `ChartTooltip` pointer
 *    surface.
 *
 * The SVG root `<g>` is translated by the margin and d3-zoom is attached
 * to it, so its zoom space is the root-local chart area
 * `[0, innerWidth] / [0, innerHeight]`, which means the chart components'
 * zoom/pan, `getTranslateExtent`, and `D3ZoomSynchronizer` logic is
 * unchanged from the previous SVG-only setup.
 *
 * The canvas backing store is resized to `innerWidth * dpr` ×
 * `innerHeight * dpr` and the 2D context is scaled by `dpr` once on each
 * resize, so callers draw in CSS pixels and get crisp output on HiDPI.
 *
 * Returns the canvas element, the 2D context, the SVG root `<g>` selection,
 * the axis group nodes, the clip id, and a cleanup function.
 */
export function setupCanvasChart(
  container: HTMLDivElement,
  svg: SVGSVGElement,
  canvas: HTMLCanvasElement,
  margin: typeof defaultMargin = defaultMargin,
  onResize: (dims: ChartDimensions) => void,
): {
  cleanup: () => void;
  clipId: string;
  ctx: CanvasRenderingContext2D;
  rootSel: import('d3-selection').Selection<SVGGElement, unknown, null, undefined>;
  gridG: SVGGElement;
  plotG: SVGGElement;
  xAxisG: SVGGElement;
  yAxisG: SVGGElement;
} {
  const selection = select(svg);
  selection.attr('width', '100%').attr('height', '100%').style('display', 'block');

  // Clip path so SVG axis content outside the plot area is hidden (kept
  // for parity with the old SVG-only setup; the canvas is clipped by its
  // own element bounds).
  const clipId = `clip-${Math.random().toString(36).slice(2)}`;
  const defs = selection.append('defs');
  defs.append('clipPath').attr('id', clipId).append('rect').attr('x', 0).attr('y', 0);

  // Root group is translated by the margin. d3-zoom is attached to this
  // group (not the <svg>), so its zoom space is the root-local chart area
  // [0, innerWidth] / [0, innerHeight].
  const root = selection.append('g').attr('transform', `translate(${margin.left},${margin.top})`);

  // Transparent background rect so the <g> captures pointer/wheel events
  // for d3-zoom even over empty areas of the plot.
  const bg = root.append('rect').attr('fill', 'none').attr('pointer-events', 'all');

  // Grid group (clipped) — SVG grid lines live here so they are themed
  // via `applyTheme()` (which runs on theme change) and stay pixel-aligned
  // with the d3-axis ticks. The data layer (line/bars) draws on the canvas.
  const gridG = root.append('g').node()!;
  gridG.setAttribute('clip-path', `url(#${clipId})`);

  // Plot group (clipped) — kept for any SVG overlays a chart may want,
  // though the data layer now draws on the canvas.
  const plotG = root.append('g').node()!;
  plotG.setAttribute('clip-path', `url(#${clipId})`);

  const xAxisG = root.append('g').node()!;
  const yAxisG = root.append('g').node()!;

  const ctx = canvas.getContext('2d')!;
  canvas.style.position = 'absolute';
  canvas.style.left = `${margin.left}px`;
  canvas.style.top = `${margin.top}px`;
  canvas.style.pointerEvents = 'none';

  const compute = () => {
    const rect = container.getBoundingClientRect();
    const width = Math.max(0, rect.width);
    const height = Math.max(0, rect.height);
    const innerWidth = Math.max(0, width - margin.left - margin.right);
    const innerHeight = Math.max(0, height - margin.top - margin.bottom);

    // SVG overlay sizing.
    selection.select(`#${clipId} rect`).attr('width', innerWidth).attr('height', innerHeight);
    bg.attr('width', innerWidth).attr('height', innerHeight);

    // Canvas backing store: CSS size = inner plot area, backing store
    // scaled by devicePixelRatio for crisp rendering. Reset the transform
    // before scaling so we don't accumulate scales across resizes.
    const dpr = window.devicePixelRatio || 1;
    canvas.style.width = `${innerWidth}px`;
    canvas.style.height = `${innerHeight}px`;
    canvas.width = Math.max(0, Math.floor(innerWidth * dpr));
    canvas.height = Math.max(0, Math.floor(innerHeight * dpr));
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    onResize({ width, height, innerWidth, innerHeight, margin });
  };

  const ro = new ResizeObserver(compute);
  ro.observe(container);
  compute();

  return {
    cleanup: () => {
      ro.disconnect();
      selection.selectAll('*').remove();
    },
    clipId,
    ctx,
    rootSel: root as import('d3-selection').Selection<SVGGElement, unknown, null, undefined>,
    gridG,
    plotG,
    xAxisG,
    yAxisG,
  };
}

export { select };
