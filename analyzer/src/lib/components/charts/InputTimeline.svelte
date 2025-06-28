<script lang="ts" module>
    import type { Device, Input } from "$lib/validator/validator";
    export type Props = {
        devices?: Record<string, Device>,
        inputs?: Record<string, Input[]>
    };
</script>

<script lang="ts">
    import type { Chart, ChartDataset, ChartData } from "chart.js";
    import { createChart } from "./chartFactory";
    import { onMount, untrack } from "svelte";
    import { Keycode } from "$lib/keycode/keycode";

    type ProcessedChartData = {x: [number, number], y: string}[];
    const { devices = {}, inputs = {} }: Props = $props();

    let ref: HTMLCanvasElement;
    let chart: Chart<"bar", ProcessedChartData> | undefined = $state.raw();

    let chartData = $derived.by(() => {
        type Dataset = ChartDataset<"bar", ProcessedChartData>;
        type DataPoint = ProcessedChartData[number];
        if (!inputs)
            return;
        let max = 0;
        const keys = new Set<DataPoint["y"]>();
        const datasets: Dataset[] = [];
        for (const [id, events] of Object.entries(inputs))
        {
            const keyDownMap = new Map<number, number>();
            const keypresses: DataPoint[] = [];
            for (const event of events)
            {
                if (event.pressed)
                {
                    keyDownMap.set(event.code, event.timestamp);
                }
                else
                {
                    const keyName = Keycode[event.code];
                    const keydown = keyDownMap.get(event.code);
                    if (keydown === undefined)
                        continue;
                    keypresses.push({ x: [keydown / 1000000, event.timestamp / 1000000], y: keyName });
                    keys.add(keyName);
                    max = Math.max(max, event.timestamp / 1000000);
                }
            }
            datasets.push({
                data: keypresses,
                xAxisID: "time",
                yAxisID: "keys",
                label: id
            });
        }
        return [{
            labels: [...keys.values()],
            datasets
        }, max] satisfies [ChartData<"bar", ProcessedChartData>, number];
    })
    onMount(() => {
        const newChart = createChart<"bar", ProcessedChartData>(ref, "bar");
        chart = newChart;
        newChart.options.indexAxis = "y";
        newChart.options.scales = {
            time: {
                title: {
                    display: true,
                    text: "Time (s)"
                },
                type: "linear",
                axis: "x",
                ticks: {
                    precision: 3
                }
            },
            keys: {
                type: "category",
                axis: "y",
                stacked: true,
                min: 0,
                max: 1,
                ticks: {
                    autoSkip: false,
                }
            }
        }
        newChart.options.interaction!.intersect = true;
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
                time: colorOptions,
                keys: colorOptions
            }
        }
        newChart.update();
        return () => {
            newChart.destroy();
        }
    });
    $effect.pre(() => {
        if (!chart || !chartData)
            return;
        const nonNullChart = chart;
        untrack(() => {
            const [data, max] = chartData;
            nonNullChart.data = data;
            nonNullChart.options.scales!.keys!.max = data.labels.length;
            nonNullChart.options.scales!.time!.min = 0;
            nonNullChart.options.scales!.time!.max = max;
            nonNullChart.update();
            nonNullChart.resetZoom();
        })
    })
</script>

<div class="overflow-hidden w-full h-full relative">
    <canvas bind:this={ref}> </canvas>
</div>
