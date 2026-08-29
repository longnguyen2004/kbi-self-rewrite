import { select, type Selection } from 'd3-selection';
import { type ScaleLinear, type NumberValue } from 'd3-scale';
import { axisBottom, axisLeft, type Axis, type AxisScale, type AxisDomain } from 'd3-axis';
import { zoom, zoomIdentity, type ZoomBehavior, type ZoomTransform } from 'd3-zoom';
import { getExtent, setupChart, defaultMargin, type ChartDimensions } from './chartCommon';
import { useChartTheme } from './chartTheme.svelte';
import { D3ZoomSynchronizer } from './d3ZoomSync';

/**
 * The y-scale contract the controller needs: the d3-axis `AxisScale` minimal
 * interface (callable + `domain()`/`range()` getters + optional `bandwidth`),
 * plus the `range()` *setter* that `AxisScale` omits but every concrete scale
 * provides. Generic over the y-domain type `Y` (`number` for line charts,
 * `string` for the band chart).
 */
export type YScale<Y extends AxisDomain> = AxisScale<Y> & {
  range(range: Iterable<number>): unknown;
};

export type ChartControllerOptions = {
  margin?: typeof defaultMargin;
  scaleExtent: [number, number];
  /** Full x-extent of the data, used to rescale the domain on zoom. */
  getXDomain: () => [number, number];
  /** Tick values for the x-axis, given the current visible domain + dims. */
  ticks: (domain: [number, number], dims: ChartDimensions) => number[];
  formatTick: (v: number) => string;
  /** When set, configures the y-axis with `.ticks(n)`. */
  yTicks?: number;
  /** When set, configures the y-axis with `.tickFormat(fn)`. */
  yTickFormat?: (v: number) => string;
  /** When set, draws horizontal grid lines (band charts). */
  horizontalGridKeys?: () => string[];
  /** Draws the data layer (line/bars) and stages tooltip snap points. */
  draw: (ctx: CanvasRenderingContext2D, dims: ChartDimensions) => void;
};

/**
 * Shared imperative d3/canvas rendering for the line and bar charts.
 *
 * Owns everything the three charts previously duplicated: the hybrid
 * canvas+SVG setup, d3-zoom + cross-chart sync, axis/grid joins with
 * tick caching, theme application, and rAF coalescing. Each chart supplies
 * its scales, tick generation/formatting, and a `draw` callback for its
 * data layer, keeping the components small and chart-specific.
 */
export class ChartController<Y extends AxisDomain> {
  readonly xScale: ScaleLinear<number, number>;
  readonly yScale: YScale<Y>;
  readonly margin: typeof defaultMargin;
  dims: ChartDimensions;

  private readonly opts: ChartControllerOptions;
  private readonly getTheme = useChartTheme();
  private readonly zoomSync?: D3ZoomSynchronizer;
  private readonly xAxisGen: Axis<NumberValue>;
  private readonly yAxisGen: Axis<Y>;

  private zoomBehavior?: ZoomBehavior<SVGGElement, unknown>;
  private rootSel?: Selection<SVGGElement, unknown, null, undefined>;
  private xAxisG?: SVGGElement;
  private yAxisG?: SVGGElement;
  private gridG?: SVGGElement;
  private ctx?: CanvasRenderingContext2D;
  private cleanupFn?: () => void;

  private lastAxisXTicks?: number[];
  private lastAxisYDomain?: Y[];
  private lastGridTicks?: number[];
  private lastGridKeys?: string[];
  private rafId = 0;
  private renderQueued = false;

  constructor(
    xScale: ScaleLinear<number, number>,
    yScale: YScale<Y>,
    opts: ChartControllerOptions,
    zoomSync?: D3ZoomSynchronizer,
  ) {
    this.xScale = xScale;
    this.yScale = yScale;
    this.opts = opts;
    this.zoomSync = zoomSync;
    this.margin = opts.margin ?? defaultMargin;
    this.dims = { width: 0, height: 0, innerWidth: 0, innerHeight: 0, margin: this.margin };
    this.xAxisGen = axisBottom(xScale);
    this.yAxisGen = axisLeft(yScale);
  }

  setup(container: HTMLDivElement, svg: SVGSVGElement, canvas: HTMLCanvasElement) {
    const { cleanup, ctx, rootSel, gridG, xAxisG, yAxisG } = setupChart(
      container,
      svg,
      canvas,
      this.margin,
      (newDims) => {
        this.dims = newDims;
        // Dimensions changed: axis tick + grid positions are now stale.
        this.lastAxisXTicks = undefined;
        this.lastAxisYDomain = undefined;
        this.lastGridTicks = undefined;
        this.lastGridKeys = undefined;
        this.updateExtent();
        this.queueRender();
      },
    );
    this.ctx = ctx;
    this.rootSel = rootSel;
    this.gridG = gridG;
    this.xAxisG = xAxisG;
    this.yAxisG = yAxisG;

    const zb = zoom<SVGGElement, unknown>()
      .scaleExtent(this.opts.scaleExtent)
      .on('zoom', (event) => {
        const t = event.transform as ZoomTransform;
        const rescaled = t.rescaleX(this.xScale.copy().domain(this.opts.getXDomain()));
        this.xScale.domain(rescaled.domain() as [number, number]);
        this.queueRender();
      });
    this.zoomBehavior = zb;
    rootSel.call(zb);
    this.updateExtent();

    if (this.zoomSync) {
      const syncable = { zoom: zb, applyTransform: (t: ZoomTransform) => this.applyTransform(t) };
      this.zoomSync.add(syncable, { axis: 'x' });
      this.cleanupFn = () => {
        cleanup();
        this.zoomSync!.remove(syncable);
      };
    } else {
      this.cleanupFn = cleanup;
    }
  }

  /** Resets zoom to show the full range (`scale` > 1 zooms in). */
  resetZoom(scale = 1) {
    if (this.zoomBehavior && this.rootSel) {
      this.rootSel.call(this.zoomBehavior.transform, zoomIdentity.scale(scale));
    }
  }

  applyTransform(t: ZoomTransform) {
    if (this.zoomBehavior && this.rootSel) {
      this.rootSel.call(this.zoomBehavior.transform, t);
    }
  }

  /** Coalesces multiple updates within one animation frame into one render. */
  queueRender() {
    if (this.renderQueued) return;
    this.renderQueued = true;
    this.rafId = requestAnimationFrame(() => {
      this.renderQueued = false;
      this.render();
    });
  }

  render() {
    if (!this.xAxisG || !this.yAxisG || !this.ctx) return;
    this.xScale.range([0, this.dims.innerWidth]);
    this.yScale.range([this.dims.innerHeight, 0]);

    const ticks = this.opts.ticks(this.xScale.domain() as [number, number], this.dims);
    this.updateXAxis(ticks);
    this.updateYAxis();
    this.updateGridLines(ticks);
    if (this.opts.horizontalGridKeys)
      this.updateHorizontalGridLines(this.opts.horizontalGridKeys());

    this.opts.draw(this.ctx, this.dims);
    this.applyTheme();
  }

  applyTheme() {
    if (!this.xAxisG || !this.yAxisG || !this.gridG) return;
    const theme = this.getTheme();
    select(this.xAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(this.xAxisG).selectAll('text').attr('fill', theme.text);
    select(this.yAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(this.yAxisG).selectAll('text').attr('fill', theme.text);
    select(this.gridG).selectAll('line').attr('stroke', theme.grid);
  }

  cleanup() {
    if (this.rafId) cancelAnimationFrame(this.rafId);
    this.cleanupFn?.();
  }

  private updateExtent() {
    if (this.zoomBehavior) {
      this.zoomBehavior.extent(getExtent(this.dims)).translateExtent(getExtent(this.dims));
    }
  }

  private updateXAxis(ticks: number[]) {
    if (!this.xAxisG) return;
    this.xAxisGen.tickValues(ticks).tickFormat((d) => this.opts.formatTick(Number(d)));
    if (this.sameArray(this.lastAxisXTicks, ticks)) {
      // Tick set unchanged: reposition existing groups in place (no reflow).
      select(this.xAxisG)
        .selectAll<SVGGElement, number>('g.tick')
        .attr('transform', (tv) => `translate(${this.xScale(tv)},0)`);
      select(this.xAxisG).attr('transform', `translate(0,${this.dims.innerHeight})`);
    } else {
      select(this.xAxisG)
        .call(this.xAxisGen)
        .attr('transform', `translate(0,${this.dims.innerHeight})`);
      this.lastAxisXTicks = ticks;
    }
  }

  private updateYAxis() {
    if (!this.yAxisG) return;
    const yDom = this.yScale.domain();
    if (this.opts.yTicks !== undefined) this.yAxisGen.ticks(this.opts.yTicks);
    if (this.opts.yTickFormat) this.yAxisGen.tickFormat((d) => this.opts.yTickFormat!(Number(d)));
    if (!this.sameArray(this.lastAxisYDomain, yDom)) {
      select(this.yAxisG).call(this.yAxisGen);
      this.lastAxisYDomain = yDom;
    }
  }

  private updateGridLines(ticks: number[]) {
    if (!this.gridG) return;
    if (!this.sameArray(this.lastGridTicks, ticks)) {
      select(this.gridG).selectAll('.grid-x').remove();
      select(this.gridG)
        .selectAll('.grid-x')
        .data(ticks)
        .enter()
        .append('line')
        .attr('class', 'grid-x')
        .attr('y1', 0)
        .attr('stroke-width', 1);
      this.lastGridTicks = ticks;
    }
    select(this.gridG)
      .selectAll<SVGLineElement, number>('.grid-x')
      .attr('x1', (t) => this.xScale(t))
      .attr('x2', (t) => this.xScale(t))
      .attr('y2', this.dims.innerHeight);
  }

  private updateHorizontalGridLines(keys: string[]) {
    if (!this.gridG) return;
    if (!this.sameArray(this.lastGridKeys, keys)) {
      select(this.gridG).selectAll('.grid-y').remove();
      select(this.gridG)
        .selectAll('.grid-y')
        .data(keys)
        .enter()
        .append('line')
        .attr('class', 'grid-y')
        .attr('x1', 0)
        .attr('stroke-width', 1);
      this.lastGridKeys = keys;
    }

    const y = this.yScale;
    const bw = y.bandwidth?.() ?? 0;
    select(this.gridG)
      .selectAll<SVGLineElement, Y>('.grid-y')
      .attr('x2', this.dims.innerWidth)
      .attr('y1', (k) => (y(k) ?? 0) + bw / 2)
      .attr('y2', (k) => (y(k) ?? 0) + bw / 2);
  }

  private sameArray<T>(a: T[] | undefined, b: T[]): boolean {
    return a !== undefined && a.length === b.length && a.every((v, i) => v === b[i]);
  }
}
