<script lang="ts" module>
  import type { TimelineProvider } from '$lib/analyzer/input_timeline.svelte';
  export type Props = {
    timeline: TimelineProvider
  };
</script>

<script lang="ts">
  import { onMount } from 'svelte';
  import { select, type Selection } from 'd3-selection';
  import { scaleLinear, scaleBand } from 'd3-scale';
  import { axisBottom, axisLeft } from 'd3-axis';
  import { zoom, zoomIdentity, type ZoomBehavior, type ZoomTransform } from 'd3-zoom';
  import { getTranslateExtent, setupSvgChart, type ChartDimensions } from './svgChart';
  import { useChartTheme } from './chartTheme.svelte';
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

  let svgRef: SVGSVGElement;
  let dims: ChartDimensions = {
    width: 0,
    height: 0,
    innerWidth: 0,
    innerHeight: 0,
    margin: { top: 10, right: 10, bottom: 30, left: 60 },
  };
  const getTheme = useChartTheme();

  let xScale = scaleLinear().domain([0, 1]);
  let yScale = scaleBand<string>().domain([]).padding(0.2);
  let xDomain: [number, number] = $derived([0, endTimestamp]);
  let zoomBehavior: ZoomBehavior<SVGGElement, unknown> | undefined;
  let rootSel: Selection<SVGGElement, unknown, null, undefined> | undefined;
  let plotG: SVGGElement | undefined;
  let xAxisG: SVGGElement | undefined;
  let yAxisG: SVGGElement | undefined;
  let barsG: SVGGElement | undefined;
  let cleanup: (() => void) | undefined;

  // --- Real-time insertion optimization caches ---
  // Last tick values used for grid lines — skip the remove/append cycle when
  // unchanged (the common case during pan/zoom where the tick count is
  // stable for a given width).
  let lastGridTicks: number[] | undefined;
  // Last key set (as a joined string) used to detect when the y-domain
  // actually changes — only then do we reset zoom; otherwise we preserve the
  // user's zoom/pan during real-time recording.
  let lastKeysSig = '';
  // rAF coalescing: collapse multiple input events within the same animation
  // frame into a single render.
  let rafId = 0;
  let renderQueued = false;

  function render() {
    if (!plotG || !xAxisG || !yAxisG || !barsG) return;
    xScale.range([0, dims.innerWidth]);
    yScale.range([0, dims.innerHeight]);

    const xAxis = axisBottom(xScale).ticks(Math.max(1, Math.floor(dims.innerWidth / 60))).tickFormat((d) => `${Number(d)}s`);
    select(xAxisG).call(xAxis).attr('transform', `translate(0,${dims.innerHeight})`);

    const yAxis = axisLeft(yScale);
    select(yAxisG).call(yAxis);

    // Grid lines (vertical): only rebuild when the tick values actually
    // changed. During pan/zoom with a stable width the tick count is
    // constant, so we skip the remove/append churn.
    const gridTicks = xScale.ticks(Math.max(1, Math.floor(dims.innerWidth / 60)));
    const gridSel = select(plotG).selectAll('.grid-x');
    const sameTicks =
      lastGridTicks !== undefined &&
      lastGridTicks.length === gridTicks.length &&
      lastGridTicks.every((t, i) => t === gridTicks[i]);
    if (!sameTicks) {
      gridSel.remove();
      select(plotG)
        .selectAll('.grid-x')
        .data(gridTicks)
        .enter()
        .append('line')
        .attr('class', 'grid-x')
        .attr('x1', (d) => xScale(d))
        .attr('x2', (d) => xScale(d))
        .attr('y1', 0)
        .attr('y2', dims.innerHeight)
        .attr('stroke-width', 1);
      lastGridTicks = gridTicks;
    }

    const bandH = yScale.bandwidth();
    const sel = select(barsG).selectAll<SVGRectElement, Keypress>('rect').data(keypresses);
    sel.exit().remove();
    sel.enter()
      .append('rect')
      .attr('rx', 2)
      .attr('ry', 2)
      .merge(sel)
      .attr('x', (d) => xScale(d.start))
      .attr('y', (d) => yScale(d.key) ?? 0)
      .attr('width', (d) => Math.max(1, xScale(d.end) - xScale(d.start)))
      .attr('height', bandH)
      .attr('fill', (d) => colorFor(d.deviceId));

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
    if (!xAxisG || !yAxisG || !plotG)
      return;
    const theme = getTheme();
    select(xAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(xAxisG).selectAll('text').attr('fill', theme.text);

    select(yAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(yAxisG).selectAll('text').attr('fill', theme.text);

    select(plotG).attr('stroke', theme.grid)
  }

  function applyTransform(transform: ZoomTransform) {
    if (!zoomBehavior || !rootSel) return;
    rootSel.call(zoomBehavior.transform, transform);
  }

  onMount(() => {
    const svg = svgRef;
    const { cleanup: chartCleanup, clipId } = setupSvgChart(svg, dims.margin, (newDims) => {
      dims = newDims;
      // Dimensions changed: grid line positions are now stale, force rebuild.
      lastGridTicks = undefined;
      queueRender();
    });
    cleanup = chartCleanup;
    const svgSel = select(svg);
    rootSel = svgSel.select<SVGGElement>('g');
    plotG = rootSel.append('g').node()!;
    plotG.setAttribute('clip-path', `url(#${clipId})`);
    xAxisG = rootSel.append('g').node()!;
    yAxisG = rootSel.append('g').node()!;
    barsG = plotG.appendChild(document.createElementNS('http://www.w3.org/2000/svg', 'g'));

    const zb = zoom<SVGGElement, unknown>()
      .scaleExtent([1, 64])
      .on('zoom', (event) => {
        const t = event.transform;        // When fully zoomed out (scale = 1), don't allow panning — the full domain should always be visible
        if (t.k <= 1.0001) {
          xScale.domain(xDomain);
        }
        else {
          const newX = xScale.copy().domain(xDomain);
          const rescaled = t.rescaleX(newX);
          xScale.domain(rescaled.domain() as [number, number]);
        }
        queueRender();
      })
    zoomBehavior = zb;
    rootSel.call(zb);

    return () => {
      if (rafId) cancelAnimationFrame(rafId);
      cleanup?.();
    };
  });

  $effect(() => {
    if (zoomBehavior)
      zoomBehavior
        .extent(getTranslateExtent(dims))
        .translateExtent(getTranslateExtent(dims))
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

<div class="relative h-full min-h-0 w-full min-w-0">
  <svg bind:this={svgRef}></svg>
</div>
