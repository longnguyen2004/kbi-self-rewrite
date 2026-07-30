<script lang="ts" module>
  import type { Device, Input } from '$lib/validator/validator';
  export type Props = {
    devices?: Record<string, Device>;
    inputs?: Record<string, Input[]>;
  };
</script>

<script lang="ts">
  import { onMount, untrack } from 'svelte';
  import { select, type Selection } from 'd3-selection';
  import { scaleLinear, scaleBand } from 'd3-scale';
  import { axisBottom, axisLeft } from 'd3-axis';
  import { zoom, zoomIdentity, type ZoomBehavior, type ZoomTransform } from 'd3-zoom';
  import { setupSvgChart, type ChartDimensions } from './svgChart';
  import { useChartTheme } from './chartTheme.svelte';
  import { Keycode } from '$lib/keycode/keycode';

  type Keypress = { start: number; end: number; key: string; deviceId: string };

  const { devices = {}, inputs = {} }: Props = $props();

  const deviceIds = $derived.by(() => Object.keys(inputs));
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

  const processed = $derived.by(() => {
    if (!inputs) return { presses: [] as Keypress[], keys: [] as string[], max: 0 };
    const keys = new Set<string>();
    const presses: Keypress[] = [];
    let max = 0;
    for (const [id, events] of Object.entries(inputs)) {
      const keyDownMap = new Map<number, number>();
      for (const event of events) {
        if (event.pressed) {
          keyDownMap.set(event.code, event.timestamp);
        } else {
          const keyName = Keycode[event.code];
          const keydown = keyDownMap.get(event.code);
          if (keydown === undefined) continue;
          const start = keydown / 1000000;
          const end = event.timestamp / 1000000;
          presses.push({ start, end, key: keyName, deviceId: id });
          keys.add(keyName);
          max = Math.max(max, end);
        }
      }
    }
    return { presses, keys: [...keys], max };
  });

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
  let xDomain: [number, number] = $derived([0, processed.max]);
  let zoomBehavior: ZoomBehavior<SVGGElement, unknown> | undefined;
  let rootSel: Selection<SVGGElement, unknown, null, undefined> | undefined;
  let plotG: SVGGElement | undefined;
  let xAxisG: SVGGElement | undefined;
  let yAxisG: SVGGElement | undefined;
  let barsG: SVGGElement | undefined;
  let cleanup: (() => void) | undefined;

  function render() {
    if (!plotG || !xAxisG || !yAxisG || !barsG) return;
    xScale.range([0, dims.innerWidth]);
    yScale.range([0, dims.innerHeight]);

    const xAxis = axisBottom(xScale).ticks(Math.max(1, Math.floor(dims.innerWidth / 60))).tickFormat((d) => `${Number(d)}s`);
    select(xAxisG).call(xAxis).attr('transform', `translate(0,${dims.innerHeight})`);

    const yAxis = axisLeft(yScale);
    select(yAxisG).call(yAxis);

    // Grid lines (vertical)
    select(plotG).selectAll('.grid-x').remove();
    select(plotG)
      .selectAll('.grid-x')
      .data(xScale.ticks(Math.max(1, Math.floor(dims.innerWidth / 60))))
      .enter()
      .append('line')
      .attr('class', 'grid-x')
      .attr('x1', (d) => xScale(d))
      .attr('x2', (d) => xScale(d))
      .attr('y1', 0)
      .attr('y2', dims.innerHeight)
      .attr('stroke-width', 1);

    const { presses } = processed;
    const bandH = yScale.bandwidth();
    const sel = select(barsG).selectAll<SVGRectElement, Keypress>('rect').data(presses);
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
      render();
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
          const [d0, d1] = rescaled.domain();
          xScale.domain([Math.max(0, d0), Math.min(xDomain[1], d1)]);
        }
        render();
      })
    zoomBehavior = zb;
    rootSel.call(zb);

    return () => {
      cleanup?.();
    };
  });

  $effect(() => {
    if (zoomBehavior)
      zoomBehavior.translateExtent([[0, 0], [dims.innerWidth, dims.innerHeight]])
  });

  $effect(() => {
    const { keys } = processed;
    if (keys.length === 0) return;
    untrack(() => {
      xScale.domain(xDomain);
      yScale.domain(keys);
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

<div class="relative h-full min-h-0 w-full min-w-0">
  <svg bind:this={svgRef}></svg>
</div>
