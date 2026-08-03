<script lang="ts" module>
  import type { TimelineProvider } from '$lib/analyzer/input_timeline.svelte';
  export type Props = {
    timeline: TimelineProvider;
  };
</script>

<script lang="ts">
  import { onMount } from 'svelte';
  import { select, type Selection } from 'd3-selection';
  import { scaleLinear, scaleBand } from 'd3-scale';
  import { axisBottom, axisLeft } from 'd3-axis';
  import { zoom, zoomIdentity, type ZoomBehavior, type ZoomTransform } from 'd3-zoom';
  import { getTranslateExtent, setupCanvasChart, type ChartDimensions } from './canvasChart';
  import { drawBarChart, type BarDatum } from './drawBarChart';
  import { useChartTheme } from './chartTheme.svelte';
  import ChartTooltip from './ChartTooltip.svelte';
  import { QuadtreeSnap } from './quadtreeSnap';
  import type { Keypress } from '$lib/analyzer/input_timeline.svelte';

  const { timeline }: Props = $props();
  const { keypresses, keys, endTimestamp } = $derived(timeline);

  const deviceColors = new Map<string, string>();
  const palette = [
    'rgb(65, 140, 240)',
    'rgb(240, 120, 0)',
    'rgb(80, 200, 120)',
    'rgb(220, 80, 80)',
    'rgb(180, 100, 220)',
    'rgb(240, 200, 60)',
  ];
  function colorFor(id: string): string {
    let c = deviceColors.get(id);
    if (!c) {
      c = palette[deviceColors.size % palette.length];
      deviceColors.set(id, c);
    }
    return c;
  }

  let containerRef: HTMLDivElement | undefined = $state();
  let svgRef: SVGSVGElement | undefined = $state();
  let canvasRef: HTMLCanvasElement | undefined = $state();
  let dims: ChartDimensions = $state({
    width: 0,
    height: 0,
    innerWidth: 0,
    innerHeight: 0,
    margin: { top: 10, right: 10, bottom: 30, left: 60 },
  });
  const getTheme = useChartTheme();

  let xScale = scaleLinear().domain([0, 1]);
  let yScale = scaleBand<string>().domain([]).padding(0.2);
  let xDomain: [number, number] = $derived([0, endTimestamp]);
  let zoomBehavior: ZoomBehavior<SVGGElement, unknown> | undefined;
  let rootSel: Selection<SVGGElement, unknown, null, undefined> | undefined;
  let xAxisG: SVGGElement | undefined;
  let yAxisG: SVGGElement | undefined;
  let ctx: CanvasRenderingContext2D | undefined;
  let cleanup: (() => void) | undefined;
  // Quadtree over the pixel centers of each bar, rebuilt on every render
  // so we can snap the tooltip to the nearest keypress under the pointer.
  // Shared helper deduplicates the nearest-neighbour logic across charts.
  const snap = new QuadtreeSnap<Keypress>(
    (d) => (xScale(d.start) + xScale(d.end)) / 2,
    (d) => (yScale(d.key) ?? 0) + yScale.bandwidth() / 2,
  );

  // --- Real-time insertion optimization caches ---
  // Last tick values used for the x-axis join — skip the remove/append
  // cycle when unchanged (the common case during pan/zoom where the tick
  // count is stable for a given width).
  let lastAxisXTicks: number[] | undefined;
  // Last y-domain (key set) used for the y-axis join — only rebuild when
  // the key set actually changes.
  let lastAxisYDomain: string[] | undefined;
  // Last key set (as a joined string) used to detect when the y-domain
  // actually changes — only then do we reset zoom; otherwise we preserve the
  // user's zoom/pan during real-time recording.
  let lastKeysSig = '';
  // Cached axis component instances (scales are stable refs, only their
  // domain/range mutate).
  const xAxisGen = axisBottom(xScale);
  const yAxisGen = axisLeft(yScale);
  // rAF coalescing: collapse multiple input events within the same animation
  // frame into a single render.
  let rafId = 0;
  let renderQueued = false;

  function render() {
    if (!xAxisG || !yAxisG || !ctx) return;
    xScale.range([0, dims.innerWidth]);
    yScale.range([0, dims.innerHeight]);

    const tickCount = Math.max(1, Math.floor(dims.innerWidth / 60));
    const gridTicks = xScale.ticks(tickCount);

    // X-axis: only run the d3-axis join (full tick remove/re-append) when
    // the tick SET changes; otherwise reposition existing g.tick groups in
    // place via a pure attribute write (no structural change → no layout
    // reflow).
    const sameXTicks =
      lastAxisXTicks !== undefined &&
      lastAxisXTicks.length === gridTicks.length &&
      lastAxisXTicks.every((t, i) => t === gridTicks[i]);
    xAxisGen.tickValues(gridTicks).tickFormat((d) => `${Number(d)}s`);
    if (!sameXTicks) {
      select(xAxisG).call(xAxisGen).attr('transform', `translate(0,${dims.innerHeight})`);
      lastAxisXTicks = gridTicks;
    } else {
      select(xAxisG)
        .selectAll<SVGGElement, number>('g.tick')
        .attr('transform', (tv) => `translate(${xScale(tv)},0)`);
      select(xAxisG).attr('transform', `translate(0,${dims.innerHeight})`);
    }

    // Y-axis: only run the join when the y-domain (key set) changes.
    const yDom = yScale.domain();
    const sameYDomain =
      lastAxisYDomain !== undefined &&
      lastAxisYDomain.length === yDom.length &&
      lastAxisYDomain.every((k, i) => k === yDom[i]);
    if (!sameYDomain) {
      select(yAxisG).call(yAxisGen);
      lastAxisYDomain = [...yDom];
    }

    const bandH = yScale.bandwidth();
    const keysArr = [...keys];

    // Build the bar draw list. We map each keypress to a BarDatum carrying
    // its device color so the canvas helper stays decoupled from the
    // device-color map.
    const bars: BarDatum[] = new Array(keypresses.length);
    for (let i = 0; i < keypresses.length; i++) {
      const k = keypresses[i];
      bars[i] = {
        start: k.start,
        end: k.end,
        key: k.key,
        color: colorFor(k.deviceId),
      };
    }

    const theme = getTheme();
    drawBarChart(ctx, dims, bars, gridTicks, keysArr, xScale, (key) => yScale(key), bandH, {
      gridColor: theme.grid,
      radius: 2,
    });

    // Stage the bar centers for the snap quadtree. The tree is built lazily
    // on the first pointer move over the chart (see QuadtreeSnap), so this
    // is cheap when the pointer is off-chart. Using bar centers (not
    // corners) keeps the tooltip snapping to the visually nearest keypress
    // even when bars are densely packed.
    snap.set(keypresses);

    applyTheme();
  }

  // Coalesce multiple input events within a single animation frame into one
  // render. During real-time recording, keypresses may arrive faster than the
  // display refreshes; this avoids redundant synchronous re-renders.
  function queueRender() {
    if (renderQueued) return;
    renderQueued = true;
    rafId = requestAnimationFrame(() => {
      renderQueued = false;
      render();
    });
  }

  function applyTheme() {
    if (!xAxisG || !yAxisG) return;
    const theme = getTheme();
    select(xAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(xAxisG).selectAll('text').attr('fill', theme.text);

    select(yAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(yAxisG).selectAll('text').attr('fill', theme.text);
  }

  function applyTransform(transform: ZoomTransform) {
    if (!zoomBehavior || !rootSel) return;
    rootSel.call(zoomBehavior.transform, transform);
  }

  onMount(() => {
    const container = containerRef;
    const svg = svgRef;
    const canvas = canvasRef;
    if (!container || !svg || !canvas) return;
    const { cleanup: chartCleanup, ctx: context, rootSel: root, xAxisG: xag, yAxisG: yag } =
      setupCanvasChart(container, svg, canvas, dims.margin, (newDims) => {
        dims = newDims;
        // Dimensions changed: axis tick positions are now stale, force
        // rebuild of both axis joins.
        lastAxisXTicks = undefined;
        lastAxisYDomain = undefined;
        queueRender();
      });
    cleanup = chartCleanup;
    ctx = context;
    rootSel = root;
    xAxisG = xag;
    yAxisG = yag;

    // Tooltip pointer handling (pointermove/leave + coord conversion +
    // quadtree hit-test) is owned by the <ChartTooltip> component, which
    // receives `snap` and a `toAnchor` mapping below in markup.

    const zb = zoom<SVGGElement, unknown>()
      .scaleExtent([1, 64])
      .on('zoom', (event) => {
        const t = event.transform as ZoomTransform;
        const newX = xScale.copy().domain(xDomain);
        const rescaled = t.rescaleX(newX);
        xScale.domain(rescaled.domain() as [number, number]);
        queueRender();
      });
    zoomBehavior = zb;
    rootSel.call(zb);

    return () => {
      if (rafId) cancelAnimationFrame(rafId);
      snap.clear();
      cleanup?.();
    };
  });

  $effect(() => {
    if (zoomBehavior)
      zoomBehavior.extent(getTranslateExtent(dims)).translateExtent(getTranslateExtent(dims));
  });

  $effect(() => {
    // Only reset the y-domain (and zoom) when the set of keys actually
    // changes. During real-time recording new keypresses for already-known
    // keys arrive frequently. We detect a key-set change via a cheap joined
    // signature.
    const keysSig = [...keys.entries()].join('\u0000');
    const keysChanged = keysSig !== lastKeysSig;
    lastKeysSig = keysSig;

    if (keysChanged) {
      yScale.domain(keys);
      // Invalidate the y-axis join cache so the next render rebuilds it.
      lastAxisYDomain = undefined;
      // Keep the user's zoom/pan; just refresh the bars via a coalesced
      // render so multiple events per frame collapse into one.
      queueRender();
    }
  });

  $effect(() => {
    xScale.domain(xDomain);
    // Reset zoom to show full range
    if (zoomBehavior && rootSel) {
      rootSel.call(zoomBehavior.transform, zoomIdentity);
    }
    queueRender();
  });

  $effect(() => {
    applyTheme();
  });
</script>

<div bind:this={containerRef} class="relative h-full min-h-0 w-full min-w-0">
  <canvas bind:this={canvasRef}></canvas>
  <svg bind:this={svgRef} class="absolute inset-0"></svg>
</div>

{#if svgRef}
  <ChartTooltip
    target={svgRef}
    margin={dims.margin}
    {snap}
    toAnchor={(d, ox, oy) => {
      const bandH = yScale.bandwidth();
      const barCx = (xScale(d.start) + xScale(d.end)) / 2;
      const barCy = (yScale(d.key) ?? 0) + bandH / 2;
      return { x: ox + barCx, y: oy + barCy };
    }}
  >
    {#snippet children(d)}
      {#if d}
        {@const duration = Math.max(0, d.end - d.start)}
        <div class="flex flex-col gap-0.5 text-xs">
          <div><b>device:</b> <code class="break-all">{d.deviceId}</code></div>
          <div><b>key:</b> {d.key}</div>
          <div><b>start:</b> {d.start.toFixed(3)}s</div>
          <div><b>end:</b> {d.end.toFixed(3)}s</div>
          <div><b>duration:</b> {duration.toFixed(3)}s</div>
        </div>
      {/if}
    {/snippet}
  </ChartTooltip>
{/if}
