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
  import { clamp } from '$lib/helper/math';

  let { title, data, yMax }: Props = $props();

  let containerRef: HTMLDivElement | undefined = $state();
  let svgRef: SVGSVGElement | undefined = $state();
  let canvasRef: HTMLCanvasElement | undefined = $state();

  const xDomain: [number, number] = $derived([0, data.length - 1]);
  const xScale = scaleLinear().domain([0, 1]);
  const yScale = scaleLinear().domain([0, 1]);

  const snap = new QuadtreeSnap<{ x: number; y: number }>(
    (d) => xScale(d.x),
    (d) => yScale(d.y),
  );

  let cachedYMax = 0;
  let cachedDataLen = -1;

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

  const controller = new ChartController(
    xScale,
    yScale,
    {
      scaleExtent: [1, 32],
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
          lineColor: 'rgb(240, 120, 0)',
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
        xScale.domain(xDomain);
        controller.resetZoom();
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
          <div><b>freq:</b> {Math.round(d.x)}Hz</div>
          <div><b>mag:</b> {d.y.toFixed(3)}</div>
        </div>
      {/if}
    {/snippet}
  </ChartTooltip>
{/if}
