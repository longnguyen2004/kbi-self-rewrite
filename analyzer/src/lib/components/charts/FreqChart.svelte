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
        chart.options.scales = {
            freq: {
                axis: "x",
                type: "linear",
                min: 0,
                max: data.length - 1,
                title: {
                    display: true,
                    text: "Frequency (Hz)"
                }
            },
            mag: {
                axis: "y",
                type: "linear",
                min: 0,
                max: minmax(data)[1],
                ticks: {
                    display: false
                }
            },
        };
        chart.update();
    });
</script>

<div class="w-full h-full relative">
    <canvas bind:this={ref}> </canvas>
</div>
