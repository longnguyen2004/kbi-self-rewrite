<script lang="ts" module>
  import type { TimelineProvider } from '$lib/analyzer/input_timeline.svelte';
  export type Props = {
    timeline: TimelineProvider;
    deviceIds: string[];
  };
</script>

<script lang="ts">
  import { onMount } from 'svelte';
  import { scaleLinear, scaleBand } from 'd3-scale';
  import { drawBarChart } from './drawBarChart';
  import { ChartController } from './chartController';
  import ChartTooltip from './ChartTooltip.svelte';
  import { QuadtreeSnap } from './quadtreeSnap';
  import type { Keypress } from '$lib/analyzer/input_timeline.svelte';

  const { timeline, deviceIds }: Props = $props();
  const { keypresses, keys, endTimestamp } = $derived(timeline);

  const palette = [
    'rgb(65, 140, 240)',
    'rgb(240, 120, 0)',
    'rgb(80, 200, 120)',
    'rgb(220, 80, 80)',
    'rgb(180, 100, 220)',
    'rgb(240, 200, 60)',
  ];
  const deviceColors = $derived(
    Object.fromEntries(deviceIds.map((el, i) => [el, palette[i % palette.length]])),
  );

  let containerRef: HTMLDivElement | undefined = $state();
  let svgRef: SVGSVGElement | undefined = $state();
  let canvasRef: HTMLCanvasElement | undefined = $state();

  const margin = { top: 10, right: 10, bottom: 30, left: 60 };
  const xDomain: [number, number] = $derived([0, endTimestamp]);
  const xScale = scaleLinear().domain([0, 1]);
  const yScale = scaleBand<string>().domain([]).padding(0.2);

  const snap = new QuadtreeSnap<Keypress>(
    (d) => (xScale(d.start) + xScale(d.end)) / 2,
    (d) => (yScale(d.key) ?? 0) + yScale.bandwidth() / 2,
  );

  let lastKeysSig = '';

  const controller = new ChartController(xScale, yScale, {
    margin,
    scaleExtent: [1, 64],
    getXDomain: () => xDomain,
    ticks: (domain, dims) => xScale.ticks(Math.max(1, Math.floor(dims.innerWidth / 60))),
    formatTick: (v) => `${v}s`,
    draw: (ctx, dims) => {
      const bandH = yScale.bandwidth();
      const bars = keypresses.map((k) => ({
        start: k.start,
        end: k.end,
        key: k.key,
        color: deviceColors[k.deviceId],
      }));
      drawBarChart(ctx, dims, bars, xScale, yScale, bandH, { radius: 2 });
      snap.set(keypresses);
    },
  });

  onMount(() => {
    controller.setup(containerRef!, svgRef!, canvasRef!);
    return () => controller.cleanup();
  });

  $effect(() => {
    const keysSig = [...keys.entries()].join('\u0000');
    const keysChanged = keysSig !== lastKeysSig;
    lastKeysSig = keysSig;
    if (keysChanged) {
      yScale.domain(keys);
      controller.queueRender();
    }
  });

  $effect(() => {
    xScale.domain(xDomain);
    controller.resetZoom();
    controller.queueRender();
  });

  $effect(() => {
    controller.applyTheme();
  });
</script>

<div bind:this={containerRef} class="relative h-full min-h-0 w-full min-w-0">
  <canvas bind:this={canvasRef}></canvas>
  <svg bind:this={svgRef} class="absolute inset-0"></svg>
</div>

{#if svgRef}
  <ChartTooltip
    target={svgRef}
    margin={controller.margin}
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
