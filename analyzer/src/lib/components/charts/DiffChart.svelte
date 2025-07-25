<script lang="ts" module>
    import { ZoomSynchronizer } from "./synchronizeZoom.js";
    export type Props = {
        title?: string;
        data: number[];
    };

    const zoomSync = new ZoomSynchronizer();
</script>

<script lang="ts">
    import { createChart } from "./chartFactory.js";
    import { minmax } from "./yeOldeMinMax.js";
    import { onMount, untrack } from "svelte";
    import type { Chart, LinearScale } from "chart.js";

    let ref: HTMLCanvasElement;
    let chart: Chart | undefined = $state.raw();
    let { title, data }: Props = $props();

    onMount(() => {
        const newChart = createChart<"line", {x: number, y: number}[]>(ref, "line");
        newChart.options.layout!.padding = {
            left: 10,
            right: 10,
        }
        newChart.options.scales = {
            delta: {
                axis: "x",
                type: "quantizedTickLinear",
                min: 0,
                max: 1000,
                ticks: {
                    callback(tickValue, index, ticks) {
                        if (typeof tickValue === "string")
                            return tickValue;
                        const fractionalPart = tickValue - Math.floor(tickValue);
                        if (Math.floor(fractionalPart * 8) - fractionalPart * 8 !== 0)
                            return '';
                        if (fractionalPart === 0)
                            return `${tickValue}ms`;
                        return `${fractionalPart.toFixed(3).slice(1)}`;
                    },
                    maxRotation: 45,
                    minRotation: 45,
                },
                beforeBuildTicks(axis: LinearScale) {
                    const { min, max } = axis;
                    axis.options.ticks.stepSize =
                        max - min <= 3 ? 0.125 :
                        max - min <= 5 ? 0.25  :
                        max - min <= 10 ? 0.5  : 1;
                },
            },
            count: {
                axis: "y",
                type: "linear",
                min: 0,
                ticks: {
                    precision: 0,
                    display: false
                },
            },
        };
        newChart.options.plugins!.tooltip = {
            callbacks: {
                title(tooltipItems) {
                    return tooltipItems.map(el => `${el.parsed.x}ms`);
                },
            }
        };
        newChart.options.plugins!.zoom!.zoom!.onZoom = zoomSync.onZoom;
        newChart.options.plugins!.zoom!.pan!.onPan = zoomSync.onPan;
        const gridColors = {
            light: "rgba(0, 0, 0, 0.1)",
            dark: "rgba(255, 255, 255, 0.25)"
        }
        const colorOptions = {
            grid: {
                color: gridColors
            },
            border: {
                color: gridColors
            }
        }
        newChart.options.plugins!.themeChanger = {
            scales: {
                delta: colorOptions,
                count: colorOptions
            }
        }
        zoomSync.add(newChart, { axis: ["x"] });
        chart = newChart;
        return () => {
            newChart.destroy();
            zoomSync.remove(newChart);
        }
    });
    $effect.pre(() => {
        if (!chart || !data) return;
        const nonNullChart = chart;
        untrack(() => {
            nonNullChart.data.datasets = [
                {
                    label: "Count",
                    data: data.map((val, i) => ({
                        x: i / (data.length - 1) * 1000,
                        y: val,
                    })),
                    xAxisID: "delta",
                    yAxisID: "count",
                    parsing: false,
                    normalized: true,
                    borderColor: "rgb(65, 140, 240)",
                    borderWidth: 1,
                },
            ];
            nonNullChart.options.scales!.count!.max = minmax(data)[1];
            nonNullChart.update();
            nonNullChart.zoomScale("delta", {min: 0, max: 50});
        });
    });
</script>

<div class="flex flex-col items-center gap-1 w-full h-full">
    {#if title}
    <h1 class="select-none">{title}</h1>
    {/if}
    <div class="overflow-hidden w-full h-full relative">
        <canvas bind:this={ref}> </canvas>
    </div>
</div>
