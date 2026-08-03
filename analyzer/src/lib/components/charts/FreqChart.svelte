<script lang="ts" module>
  import { D3ZoomSynchronizer } from './d3ZoomSync';
  export type Props = {
    title?: string;
    data: number[];
    yMax?: number;
  };

  const zoomSync = new D3ZoomSynchronizer();
</script>

<script lang="ts">
  import { onMount, untrack } from 'svelte';
  import { select, type Selection } from 'd3-selection';
  import { scaleLinear } from 'd3-scale';
  import { axisBottom, axisLeft } from 'd3-axis';
  import { zoom, zoomIdentity, type ZoomBehavior, type ZoomTransform } from 'd3-zoom';
  import {
    getExtent,
    setupChart,
    defaultMargin,
    type ChartDimensions,
  } from './chartCommon';
  import { decimate } from './decimate';
  import { drawLineChart } from './drawLineChart';
  import { useChartTheme } from './chartTheme.svelte';
  import ChartTooltip from './ChartTooltip.svelte';
  import { QuadtreeSnap } from './quadtreeSnap';
  import { clamp } from '$lib/helper/math';

  let { title, data, yMax }: Props = $props();

  let containerRef: HTMLDivElement | undefined = $state();
  let svgRef: SVGSVGElement | undefined = $state();
  let canvasRef: HTMLCanvasElement | undefined = $state();
  let dims: ChartDimensions = {
    width: 0,
    height: 0,
    innerWidth: 0,
    innerHeight: 0,
    margin: defaultMargin,
  };
  const getTheme = useChartTheme();

  let xScale = scaleLinear().domain([0, 1]);
  let yScale = scaleLinear().domain([0, 1]);
  let xDomain: [number, number] = $derived([0, data.length - 1]);
  let zoomBehavior: ZoomBehavior<SVGGElement, unknown> | undefined;
  let rootSel: Selection<SVGGElement, unknown, null, undefined> | undefined;
  let xAxisG: SVGGElement | undefined;
  let yAxisG: SVGGElement | undefined;
  let gridG: SVGGElement | undefined;
  let ctx: CanvasRenderingContext2D | undefined;
  // Decimated points currently drawn, in chart x-units. Reused as the
  // quadtree source so the tooltip snaps to a visible sample.
  let currentPoints: { x: number; y: number }[] = [];
  // Quadtree over the *pixel* positions of currentPoints, rebuilt each
  // render (after scales change). Shared helper deduplicates the
  // nearest-neighbour logic across charts.
  const snap = new QuadtreeSnap<{ x: number; y: number }>(
    (d) => xScale(d.x),
    (d) => yScale(d.y),
  );

  // Incremental y-domain max: updated as new data arrives instead of a full
  // O(n) minmax scan on every insertion.
  let cachedYMax = 0;
  let cachedDataLen = -1;
  // Last tick values used for the x-axis join — skip the remove/append
  // cycle when unchanged (the common case during pan within a quantized
  // step).
  let lastAxisXTicks: number[] | undefined;
  // Last y-domain used for the y-axis join — skip when unchanged (y-domain
  // doesn't change on pan).
  let lastAxisYDomain: [number, number] | undefined;
  // Last tick values used for the SVG grid lines — skip the remove/append
  // cycle when unchanged; positions are still refreshed every render so
  // pan/zoom stays aligned with the axis.
  let lastGridTicks: number[] | undefined;
  // Cached axis component instances (scales are stable refs, only their
  // domain/range mutate).
  const xAxisGen = axisBottom(xScale);
  const yAxisGen = axisLeft(yScale);
  // rAF coalescing: collapse multiple data updates in the same frame into a
  // single render.
  let rafId = 0;
  let renderQueued = false;

  function ceil2(x: number): number {
    x--;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    x++;
    return x;
  }

  function quantizedTicks(min: number, max: number): number[] {
    const stepSize = clamp(125 * ceil2(Math.floor((max - min) / 1000)), 125, 8000);
    const quantizedMin = Math.floor(min / stepSize + 1) * stepSize;
    const quantizedMax = Math.ceil(max / stepSize - 1) * stepSize;
    const ticks: number[] = [min];
    for (let i = quantizedMin; i <= quantizedMax; i += stepSize) ticks.push(i);
    ticks.push(max);
    return ticks;
  }

  function formatTick(v: number): string {
    const fractionalPart = v - Math.floor(v);
    if (fractionalPart !== 0) return '';
    return `${v}Hz`;
  }

  function applyTheme() {
    if (!xAxisG || !yAxisG || !gridG) return;
    const theme = getTheme();
    select(xAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(xAxisG).selectAll('text').attr('fill', theme.text);

    select(yAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(yAxisG).selectAll('text').attr('fill', theme.text);

    select(gridG).selectAll('line').attr('stroke', theme.grid);
  }

  function updateGridLines(ticks: number[]) {
    if (!gridG) return;

    const gridSel = select(gridG).selectAll<SVGLineElement, number>('.grid-x');
    const sameTicks =
      lastGridTicks !== undefined &&
      lastGridTicks.length === ticks.length &&
      lastGridTicks.every((tick, index) => tick === ticks[index]);

    if (!sameTicks) {
      gridSel.remove();
      select(gridG)
        .selectAll('.grid-x')
        .data(ticks)
        .enter()
        .append('line')
        .attr('class', 'grid-x')
        .attr('y1', 0)
        .attr('stroke-width', 1);
      lastGridTicks = ticks;
    }

    select(gridG)
      .selectAll<SVGLineElement, number>('.grid-x')
      .attr('x1', (tick) => xScale(tick))
      .attr('x2', (tick) => xScale(tick))
      .attr('y2', dims.innerHeight);
  }

  function render() {
    if (!xAxisG || !yAxisG || !ctx) return;
    xScale.range([0, dims.innerWidth]);
    yScale.range([dims.innerHeight, 0]);

    const ticks = quantizedTicks(xScale.domain()[0], xScale.domain()[1]);

    // X-axis: only run the d3-axis join (full tick remove/re-append) when
    // the tick SET changes; otherwise reposition existing g.tick groups in
    // place via a pure attribute write (no structural change → no layout
    // reflow).
    const sameXTicks =
      lastAxisXTicks !== undefined &&
      lastAxisXTicks.length === ticks.length &&
      lastAxisXTicks.every((t, i) => t === ticks[i]);
    xAxisGen.tickValues(ticks).tickFormat((d) => formatTick(Number(d)));
    if (!sameXTicks) {
      select(xAxisG).call(xAxisGen).attr('transform', `translate(0,${dims.innerHeight})`);
      lastAxisXTicks = ticks;
    } else {
      select(xAxisG)
        .selectAll<SVGGElement, number>('g.tick')
        .attr('transform', (tv) => `translate(${xScale(tv)},0)`);
      select(xAxisG).attr('transform', `translate(0,${dims.innerHeight})`);
    }

    // Y-axis: only run the join when the y-domain changes.
    const yDom = yScale.domain() as [number, number];
    const sameYDomain =
      lastAxisYDomain !== undefined &&
      lastAxisYDomain[0] === yDom[0] &&
      lastAxisYDomain[1] === yDom[1];
    yAxisGen.ticks(5).tickFormat((d) => `${Number(d)}`);
    if (!sameYDomain) {
      select(yAxisG).call(yAxisGen);
      lastAxisYDomain = yDom;
    }

    updateGridLines(ticks);

    currentPoints = decimate(data, xDomain, xScale.domain() as [number, number], dims.innerWidth);

    drawLineChart(ctx, dims, currentPoints, xScale, yScale, {
      lineColor: 'rgb(240, 120, 0)',
      lineWidth: 1,
    });

    // Stage the decimated points for the snap quadtree. The tree is built
    // lazily on the first pointer move over the chart (see QuadtreeSnap),
    // so this is cheap when the pointer is off-chart.
    snap.set(currentPoints);

    applyTheme();
  }

  // Coalesce multiple data updates within a single animation frame into one
  // render. During real-time insertion, add() may fire several times per
  // frame; this avoids redundant synchronous re-renders.
  function queueRender() {
    if (renderQueued) return;
    renderQueued = true;
    rafId = requestAnimationFrame(() => {
      renderQueued = false;
      render();
    });
  }

  function applyTransform(transform: ZoomTransform) {
    if (!zoomBehavior || !rootSel) return;
    rootSel.call(zoomBehavior.transform, transform);
  }

  onMount(() => {
    const container = containerRef!;
    const svg = svgRef!;
    const canvas = canvasRef!;
    let cleanup;
    ({
      cleanup,
      ctx,
      rootSel,
      gridG,
      xAxisG,
      yAxisG,
    } = setupChart(container, svg, canvas, defaultMargin, (newDims) => {
      dims = newDims;
      // Dimensions changed: axis tick + grid line positions are now stale,
      // force rebuild of both axis joins and the grid.
      lastAxisXTicks = undefined;
      lastAxisYDomain = undefined;
      lastGridTicks = undefined;
      queueRender();
    }));

    // Tooltip pointer handling (pointermove/leave + coord conversion +
    // quadtree hit-test) is owned by the <ChartTooltip> component, which
    // receives `snap` and a `toAnchor` mapping below in markup.

    const zb = zoom<SVGGElement, unknown>()
      .scaleExtent([1, 32])
      .on('zoom', (event) => {
        const t = event.transform as ZoomTransform;
        const newX = xScale.copy().domain(xDomain);
        const rescaled = t.rescaleX(newX);
        xScale.domain(rescaled.domain() as [number, number]);
        queueRender();
      });
    zoomBehavior = zb;
    rootSel.call(zb);

    const syncable = {
      zoom: zb,
      applyTransform,
    };
    zoomSync.add(syncable, { axis: 'x' });

    return () => {
      if (rafId) cancelAnimationFrame(rafId);
      snap.clear();
      cleanup();
      zoomSync.remove(syncable);
    };
  });

  $effect(() => {
    if (zoomBehavior)
      zoomBehavior.extent(getExtent(dims)).translateExtent(getExtent(dims));
  });

  $effect(() => {
    if (!data || data.length === 0) return;
    untrack(() => {
      const len = data.length;
      // When the array length changes (bin-rate change or reset), the x
      // mapping changes and we must recompute everything and reset zoom.
      // Otherwise (real-time insertion: same length, new values) we keep
      // the user's zoom/pan state and only update the y-domain + path.
      const lengthChanged = len !== cachedDataLen;
      if (lengthChanged) {
        cachedDataLen = len;
        xScale.domain(xDomain);
        // Reset zoom to show full range
        if (zoomBehavior && rootSel) {
          rootSel.call(zoomBehavior.transform, zoomIdentity);
        }
      }
      let newMax = yMax;
      if (newMax === undefined)
      {
        // Incremental y-max update: only scan if the previous max was
        // exceeded (cheap), otherwise the cached max is still valid.
        newMax = data[0]
        for (let i = 1; i < len; i++) {
          const v = data[i];
          if (v > newMax) newMax = v;
        }
      }
      if (newMax !== cachedYMax) {
        cachedYMax = newMax;
        yScale.domain([0, newMax]);
      }
      // Coalesce: avoid a synchronous full render per insertion.
      queueRender();
    });
  });

  $effect(() => {
    applyTheme();
  });
</script>

<div class="flex h-full w-full flex-col items-center gap-1">
  {#if title}
    <h1 class="select-none">{title}</h1>
  {/if}
  <div bind:this={containerRef} class="relative h-full w-full overflow-hidden">
    <canvas bind:this={canvasRef}></canvas>
    <svg bind:this={svgRef} class="absolute inset-0"></svg>
  </div>
</div>

{#if svgRef}
  <ChartTooltip
    target={svgRef}
    margin={defaultMargin}
    {snap}
    toAnchor={(d, ox, oy) => ({ x: ox + xScale(d.x), y: oy + yScale(d.y) })}
  >
    {#snippet children(d)}
      {#if d}
        <div class="flex flex-col gap-0.5 text-xs">
          <div><b>freq:</b> {Math.round(d.x)}Hz</div>
          <div><b>mag:</b> {d.y.toFixed(3)}</div>
        </div>
      {/if}
    {/snippet}
  </ChartTooltip>
{/if}
