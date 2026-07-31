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
  import { line } from 'd3-shape';
  import { axisBottom, axisLeft } from 'd3-axis';
  import { zoom, zoomIdentity, type ZoomBehavior, type ZoomTransform } from 'd3-zoom';
  import { getTranslateExtent, setupSvgChart, defaultMargin, type ChartDimensions } from './svgChart';
  import { decimate } from './decimate';
  import { useChartTheme } from './chartTheme.svelte';

  let { title, data, yMax }: Props = $props();

  let svgRef: SVGSVGElement;
  let dims: ChartDimensions = {
    width: 0,
    height: 0,
    innerWidth: 0,
    innerHeight: 0,
    margin: defaultMargin,
  };
  const getTheme = useChartTheme();

  let xScale = scaleLinear().domain([0, 1000]);
  let yScale = scaleLinear().domain([0, 1]);
  let xDomain: [number, number] = [0, 1000];
  let zoomBehavior: ZoomBehavior<SVGGElement, unknown> | undefined;
  let rootSel: Selection<SVGGElement, unknown, null, undefined> | undefined;
  let plotG: SVGGElement | undefined;
  let xAxisG: SVGGElement | undefined;
  let yAxisG: SVGGElement | undefined;
  let pathEl: SVGPathElement | undefined;
  let cleanup: (() => void) | undefined;

  // Cached line generator (scales are stable references, only their
  // domain/range mutate).
  const lineGen = line<{ x: number; y: number }>()
    .x((d) => xScale(d.x))
    .y((d) => yScale(d.y));
  // Incremental y-domain max: updated as new data arrives instead of a full
  // O(n) minmax scan on every insertion.
  let cachedYMax = 0;
  let cachedDataLen = -1;
  // Last tick values used for grid lines — skip the remove/append cycle when
  // unchanged (the common case during real-time insertion since x-domain is
  // constant).
  let lastGridTicks: number[] | undefined;
  // rAF coalescing: collapse multiple data updates in the same frame into a
  // single render.
  let rafId = 0;
  let renderQueued = false;

  function quantizedTicks(min: number, max: number): number[] {
    const range = max - min;
    let stepSize: number;
    if (range <= 1.5) stepSize = 0.125;
    else if (range <= 3) stepSize = 0.25;
    else if (range <= 5) stepSize = 0.5;
    else if (range <= 10) stepSize = 1;
    else if (range <= 20) stepSize = 2;
    else if (range <= 50) stepSize = 5;
    else if (range <= 100) stepSize = 10;
    else if (range <= 200) stepSize = 20;
    else if (range <= 500) stepSize = 50;
    else if (range <= 1000) stepSize = 100;
    else stepSize = 200;
    const quantizedMin = Math.floor(min / stepSize + 1) * stepSize;
    const quantizedMax = Math.ceil(max / stepSize - 1) * stepSize;
    const ticks: number[] = [min];
    for (let i = quantizedMin; i <= quantizedMax; i += stepSize) ticks.push(i);
    ticks.push(max);
    return ticks;
  }

  function formatTick(v: number): string {
    const fractionalPart = v - Math.floor(v);
    if (Math.floor(fractionalPart * 8) - fractionalPart * 8 !== 0) return '';
    if (fractionalPart === 0) return `${v}ms`;
    return `${fractionalPart.toFixed(3).slice(1)}`;
  }

  function applyTheme() {
    if (!xAxisG || !yAxisG || !plotG)
      return;
    const theme = getTheme();
    select(xAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(xAxisG).selectAll('text').attr('fill', theme.text);

    select(yAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(yAxisG).selectAll('text').attr('fill', theme.text);

    select(plotG).attr('stroke', theme.grid)
  }

  function updateGridLines(ticks: number[]) {
    if (!plotG) return;

    const gridSel = select(plotG).selectAll<SVGLineElement, number>('.grid-x');
    const sameTicks =
      lastGridTicks !== undefined &&
      lastGridTicks.length === ticks.length &&
      lastGridTicks.every((tick, index) => tick === ticks[index]);

    if (!sameTicks) {
      gridSel.remove();
      select(plotG)
        .selectAll('.grid-x')
        .data(ticks)
        .enter()
        .append('line')
        .attr('class', 'grid-x')
        .attr('y1', 0)
        .attr('stroke-width', 1);
      lastGridTicks = ticks;
    }

    select(plotG)
      .selectAll<SVGLineElement, number>('.grid-x')
      .attr('x1', (tick) => xScale(tick))
      .attr('x2', (tick) => xScale(tick))
      .attr('y2', dims.innerHeight);
  }

  function render() {
    if (!plotG || !xAxisG || !yAxisG || !pathEl) return;
    xScale.range([0, dims.innerWidth]);
    yScale.range([dims.innerHeight, 0]);

    const ticks = quantizedTicks(xScale.domain()[0], xScale.domain()[1]);
    const xAxis = axisBottom(xScale)
      .tickValues(ticks)
      .tickFormat((d) => formatTick(Number(d)));
    select(xAxisG).call(xAxis).attr('transform', `translate(0,${dims.innerHeight})`);

    const yAxis = axisLeft(yScale).ticks(5).tickFormat((d) => `${Number(d)}`);
    select(yAxisG).call(yAxis);

    updateGridLines(ticks);

    select(pathEl)
      .datum(decimate(data, xDomain, xScale.domain() as [number, number], dims.innerWidth))
      .attr('d', lineGen)
      .attr('fill', 'none')
      .attr('stroke', 'rgb(65, 140, 240)')
      .attr('stroke-width', 1);

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
    const svg = svgRef;
    const { cleanup: chartCleanup, clipId } = setupSvgChart(svg, defaultMargin, (newDims) => {
      dims = newDims;
      // Dimensions changed: grid line positions are now stale, force rebuild.
      lastGridTicks = undefined;
      queueRender();
    });
    cleanup = chartCleanup;
    const svgSel = select(svg);
    // The root <g> is the first child appended by setupSvgChart.
    rootSel = svgSel.select<SVGGElement>('g');
    plotG = rootSel.append('g').node()!;
    plotG.setAttribute('clip-path', `url(#${clipId})`);
    xAxisG = rootSel.append('g').node()!;
    yAxisG = rootSel.append('g').node()!;
    pathEl = plotG.appendChild(
      document.createElementNS('http://www.w3.org/2000/svg', 'path'),
    );

    const zb = zoom<SVGGElement, unknown>()
      .scaleExtent([1, 1000])
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
      cleanup?.();
      zoomSync.remove(syncable);
    };
  });

  $effect(() => {
    if (zoomBehavior)
      zoomBehavior
        .extent(getTranslateExtent(dims))
        .translateExtent(getTranslateExtent(dims));
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
        // Reset zoom to show first 25ms by default
        if (zoomBehavior && rootSel) {
          const k = 1000 / 25;
          rootSel.call(zoomBehavior.transform, zoomIdentity.scale(k));
        }
      }
      if (yMax !== undefined) {
        cachedYMax = yMax;
        yScale.domain([0, yMax]);
      } else {
        // Incremental y-max update: only scan if the previous max was
        // exceeded (cheap), otherwise the cached max is still valid.
        let newMax = cachedYMax;
        for (let i = 0; i < len; i++) {
          const v = data[i];
          if (v > newMax) newMax = v;
        }
        if (newMax !== cachedYMax) {
          cachedYMax = newMax;
          yScale.domain([0, newMax]);
        }
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
  <div class="relative h-full w-full overflow-hidden">
    <svg bind:this={svgRef}></svg>
  </div>
</div>
