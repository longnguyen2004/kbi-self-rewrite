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
  import { scaleLinear } from 'd3-scale';
  import { decimate } from './decimate';
  import { drawLineChart } from './drawLineChart';
  import { ChartController } from './chartController';
  import ChartTooltip from './ChartTooltip.svelte';
  import { QuadtreeSnap } from './quadtreeSnap';

  let { title, data, yMax }: Props = $props();

  let containerRef: HTMLDivElement | undefined = $state();
  let svgRef: SVGSVGElement | undefined = $state();
  let canvasRef: HTMLCanvasElement | undefined = $state();

  const xDomain: [number, number] = [0, 1000];
  const xScale = scaleLinear().domain(xDomain);
  const yScale = scaleLinear().domain([0, 1]);

  const snap = new QuadtreeSnap<{ x: number; y: number }>(
    (d) => xScale(d.x),
    (d) => yScale(d.y),
  );

  let cachedYMax = 0;
  let cachedDataLen = -1;

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

  const controller = new ChartController(
    xScale,
    yScale,
    {
      scaleExtent: [1, 1000],
      getXDomain: () => xDomain,
      ticks: (domain) => quantizedTicks(domain[0], domain[1]),
      formatTick,
      yTicks: 5,
      yTickFormat: (v) => `${v}`,
      draw: (ctx, dims) => {
        const points = decimate(
          data,
          xDomain,
          xScale.domain() as [number, number],
          dims.innerWidth,
        );
        drawLineChart(ctx, dims, points, xScale, yScale, {
          lineColor: 'rgb(65, 140, 240)',
          lineWidth: 1,
        });
        snap.set(points);
      },
    },
    zoomSync,
  );

  onMount(() => {
    controller.setup(containerRef!, svgRef!, canvasRef!);
    return () => controller.cleanup();
  });

  $effect(() => {
    if (!data || data.length === 0) return;
    untrack(() => {
      const len = data.length;
      if (len !== cachedDataLen) {
        cachedDataLen = len;
        // Reset zoom to show the first 25ms by default.
        controller.resetZoom(1000 / 25);
      }
      let newMax = yMax;
      if (newMax === undefined) {
        newMax = data[0];
        for (let i = 1; i < len; i++) if (data[i] > newMax) newMax = data[i];
      }
      if (newMax !== cachedYMax) {
        cachedYMax = newMax;
        yScale.domain([0, newMax]);
      }
      controller.queueRender();
    });
  });

  $effect(() => {
    controller.applyTheme();
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
    margin={controller.margin}
    {snap}
    toAnchor={(d, ox, oy) => ({ x: ox + xScale(d.x), y: oy + yScale(d.y) })}
  >
    {#snippet children(d)}
      {#if d}
        <div class="flex flex-col gap-0.5 text-xs">
          <div><b>x:</b> {d.x.toFixed(3)}ms</div>
          <div><b>y:</b> {d.y} events</div>
        </div>
      {/if}
    {/snippet}
  </ChartTooltip>
{/if}
