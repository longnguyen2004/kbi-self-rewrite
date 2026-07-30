<script lang="ts" module>
  import { D3ZoomSynchronizer } from './d3ZoomSync';
  export type Props = {
    title?: string;
    data: number[];
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
  import { setupSvgChart, defaultMargin, type ChartDimensions } from './svgChart';
  import { useChartTheme } from './chartTheme.svelte';
  import { minmax } from './yeOldeMinMax';

  let { title, data }: Props = $props();

  let svgRef: SVGSVGElement;
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
  let plotG: SVGGElement | undefined;
  let xAxisG: SVGGElement | undefined;
  let yAxisG: SVGGElement | undefined;
  let pathEl: SVGPathElement | undefined;
  let cleanup: (() => void) | undefined;

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

  function clamp(x: number, min: number, max: number): number {
    return Math.min(max, Math.max(min, x));
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
    if (!xAxisG || !yAxisG || !plotG)
      return;
    const theme = getTheme();
    select(xAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(xAxisG).selectAll('text').attr('fill', theme.text);

    select(yAxisG).selectAll('line, path').attr('stroke', theme.axis);
    select(yAxisG).selectAll('text').attr('fill', theme.text);

    select(plotG).attr('stroke', theme.grid)
  }

  function render() {
    if (!plotG || !xAxisG || !yAxisG || !pathEl) return;
    xScale.range([0, dims.innerWidth]);
    yScale.range([dims.innerHeight, 0]);

    const xAxis = axisBottom(xScale)
      .tickValues(quantizedTicks(xScale.domain()[0], xScale.domain()[1]))
      .tickFormat((d) => formatTick(Number(d)));
    select(xAxisG).call(xAxis).attr('transform', `translate(0,${dims.innerHeight})`);

    const yAxis = axisLeft(yScale).ticks(5).tickFormat((d) => `${Number(d)}`);
    select(yAxisG).call(yAxis);

    // Grid lines
    select(plotG).selectAll('.grid-x').remove();
    select(plotG)
      .selectAll('.grid-x')
      .data(xScale.ticks(8))
      .enter()
      .append('line')
      .attr('class', 'grid-x')
      .attr('x1', (d) => xScale(d))
      .attr('x2', (d) => xScale(d))
      .attr('y1', 0)
      .attr('y2', dims.innerHeight)
      .attr('stroke-width', 1);

    const lineGen = line<{ x: number; y: number }>()
      .x((d) => xScale(d.x))
      .y((d) => yScale(d.y));

    const points = data.map((val, i) => ({ x: i, y: val }));
    select(pathEl)
      .datum(points)
      .attr('d', lineGen)
      .attr('fill', 'none')
      .attr('stroke', 'rgb(240, 120, 0)')
      .attr('stroke-width', 1);

    applyTheme();
  }

  function applyTransform(transform: ZoomTransform) {
    if (!zoomBehavior || !rootSel) return;
    rootSel.call(zoomBehavior.transform, transform);
  }

  onMount(() => {
    const svg = svgRef;
    const { cleanup: chartCleanup, clipId } = setupSvgChart(svg, defaultMargin, (newDims) => {
      dims = newDims;
      render();
    });
    cleanup = chartCleanup;
    const svgSel = select(svg);
    rootSel = svgSel.select<SVGGElement>('g');
    plotG = rootSel.append('g').node()!;
    plotG.setAttribute('clip-path', `url(#${clipId})`);
    xAxisG = rootSel.append('g').node()!;
    yAxisG = rootSel.append('g').node()!;
    pathEl = plotG.appendChild(
      document.createElementNS('http://www.w3.org/2000/svg', 'path'),
    );

    const zb = zoom<SVGGElement, unknown>()
      .scaleExtent([1, 32])
      .on('zoom', (event) => {
        const t = event.transform;
        // When fully zoomed out (scale = 1), don't allow panning — the full domain should always be visible
        if (t.k <= 1.0001) {
          xScale.domain(xDomain);
          render();
          return;
        }
        const newX = xScale.copy().domain(xDomain);
        const rescaled = t.rescaleX(newX);
        const [d0, d1] = rescaled.domain();
        xScale.domain([Math.max(0, d0), Math.min(xDomain[1], d1)]);
        render();
      });
    zoomBehavior = zb;
    rootSel.call(zb);

    const syncable = {
      zoom: zb,
      applyTransform,
    };
    zoomSync.add(syncable, { axis: 'x' });

    return () => {
      cleanup?.();
      zoomSync.remove(syncable);
    };
  });

  $effect(() => {
    if (zoomBehavior)
      zoomBehavior.translateExtent([[0, 0], [dims.innerWidth, dims.innerHeight]])
  });

  $effect(() => {
    if (!data || data.length === 0) return;
    untrack(() => {
      const [, max] = minmax(data);
      xScale.domain(xDomain);
      yScale.domain([0, max]);
      // Reset zoom to show full range
      if (zoomBehavior && rootSel) {
        rootSel.call(zoomBehavior.transform, zoomIdentity);
      }
      render();
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
