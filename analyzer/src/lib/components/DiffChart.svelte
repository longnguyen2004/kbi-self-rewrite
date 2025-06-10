<script lang="ts" module>
    export type Props = {
        data: number[];
    };
</script>

<script lang="ts">
    import { LineChart } from "layerchart";
    import { chartProps } from "./ChartProps";
    import * as Chart from "$lib/components/ui/chart";

    let { data }: Props = $props();

    const chartConfig = {
        count: { label: "Count", color: "rgb(65, 140, 240)" },
    } satisfies Chart.ChartConfig;

    let chartData = $derived(
        data.flat().map((val, i) => ({
            time: i / (data.length - 1),
            count: val
        })),
    );
</script>

<Chart.Container config={chartConfig}>
    <LineChart
        data={chartData}
        x="time"
        series={[
            {
                key: "count",
                ...chartConfig.count,
            },
        ]}
        props={chartProps}
        brush
    >
        {#snippet tooltip()}
            <Chart.Tooltip />
        {/snippet}
    </LineChart>
</Chart.Container>
