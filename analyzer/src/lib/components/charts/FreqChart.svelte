<script lang="ts" module>
    import { ZoomSynchronizer } from "./synchronizeZoom.js";
    export type Props = {
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
    let { data }: Props = $props();

    onMount(() => {
        const newChart = createChart(ref);
        newChart.options.scales = {
            freq: {
                axis: "x",
                type: "quantizedTickLinear",
                min: 0,
                max: 1000,
                title: {
                    display: true,
                    text: "Frequency (Hz)"
                },
                ticks: {
                    callback(tickValue, index, ticks) {
                        if (typeof tickValue === "string")
                            return tickValue;
                        const fractionalPart = tickValue - Math.floor(tickValue);
                        if (fractionalPart !== 0)
                            return '';
                        return `${tickValue}Hz`;
                    },
                    maxRotation: 45,
                    minRotation: 45,
                },
                beforeBuildTicks(axis: LinearScale) {
                    const { min, max } = axis;
                    function ceil2(x: number) {
                        x--;
                        x = x | (x >> 1);
                        x = x | (x >> 2);
                        x = x | (x >> 4);
                        x = x | (x >> 8);
                        x = x | (x >> 16);
                        x++;
                        return x;
                    }
                    function clamp(x: number, min: number, max: number)
                    {
                        return Math.min(max, Math.max(min, x));
                    }
                    axis.options.ticks.stepSize = clamp(
                        125 * ceil2(Math.floor((max - min) / 1000)),
                        125,
                        8000
                    );
                },
            },
            mag: {
                axis: "y",
                type: "linear",
                min: 0,
                max: 1,
                ticks: {
                    display: false
                }
            },
        };
        newChart.options.plugins!.tooltip = {
            callbacks: {
                title(tooltipItems) {
                    return tooltipItems.map(el => `${el.parsed.x}Hz`);
                },
            }
        }
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
                freq: colorOptions,
                mag: colorOptions
            }
        }
        zoomSync.add(newChart, { axis: ["x"] });
        chart = newChart;
        return () => {
            newChart.destroy();
            zoomSync.remove(newChart);
        }
    });
    $effect(() => {
        if (!chart || !data) return;
        const nonNullChart = chart;
        untrack(() => {
            nonNullChart.data.datasets = [
                {
                    label: "Magnitude",
                    data: data.map((val, i) => ({
                        x: i, y: val,
                    })),
                    xAxisID: "freq",
                    yAxisID: "mag",
                    parsing: false,
                    normalized: true,
                    borderColor: "rgb(240, 120, 0)",
                    borderWidth: 1,
                },
            ];
            nonNullChart.options.scales!.freq!.max = data.length - 1,
            nonNullChart.options.scales!.mag!.max = minmax(data)[1],
            nonNullChart.update();
            nonNullChart.zoomScale("freq", {min: 0, max: data.length - 1});
        })
    });
</script>

<div class="w-full h-full relative">
    <canvas bind:this={ref}> </canvas>
</div>
