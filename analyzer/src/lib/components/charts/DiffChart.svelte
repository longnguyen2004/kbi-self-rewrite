<script lang="ts" module>
    export type Props = {
        data: number[];
    };
</script>

<script lang="ts">
    import { createChart } from "./chartFactory.js";
    import { minmax } from "./yeOldeMinMax.js";
    import type { Chart, LinearScaleOptions, LinearScale } from "chart.js";
    import type { PartialDeep } from "type-fest";

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
        chart.options.scales = {
            delta: {
                title: {
                    display: true,
                    text: "Delta (ms)"
                },
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
                        return `${tickValue}ms`;
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
            } as PartialDeep<LinearScaleOptions>,
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
        chart.options.plugins!.tooltip = {
            callbacks: {
                title(tooltipItems) {
                    return tooltipItems.map(el => `${el.parsed.x * 1000}ms`);
                },
            }
        }
        chart.update();
        chart.zoomScale("delta", {min: 0, max: 5});
    });
</script>

<div class="w-full h-full relative">
    <canvas bind:this={ref}> </canvas>
</div>
