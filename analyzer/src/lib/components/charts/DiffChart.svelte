<script lang="ts" module>
    export type Props = {
        data: number[];
    };
</script>

<script lang="ts">
    import { createChart } from "./chartFactory.js";
    import { minmax } from "./yeOldeMinMax.js";
    import type { Chart } from "chart.js";

    let ref: HTMLCanvasElement;
    let chart: Chart | undefined = $state.raw();
    let { data }: Props = $props();

    $effect(() => {
        chart = createChart(ref);
    });
    $effect(() => {
        if (!chart || !data) return;
        chart.data.datasets = [
            {
                label: "Count",
                data: data.map((val, i) => ({
                    x: i / (data.length - 1),
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
        chart.options.scales = {
            delta: {
                title: {
                    display: true,
                    text: "Delta (ms)"
                },
                axis: "x",
                type: "linear",
                min: 0,
                max: 1,
            },
            count: {
                axis: "y",
                type: "linear",
                min: 0,
                max: minmax(data)[1],
                ticks: {
                    precision: 0,
                    display: false
                },
            },
        };
        chart.update();
        chart.zoomScale("delta", {min: 0, max: 0.05});
    });
</script>

<div class="w-full h-full relative">
    <canvas bind:this={ref}> </canvas>
</div>
