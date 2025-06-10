<script lang="ts" module>
    export type Props = {
        data: number[] | undefined;
    };
</script>

<script lang="ts">
    import { LineChart } from "layerchart";
    import * as Chart from "$lib/components/ui/chart";

    let { data }: Props = $props();

    const chartConfig = {
        mag: { label: "Magnitude", color: "#FFFFFF" },
    } satisfies Chart.ChartConfig;
</script>

<Chart.Container config={chartConfig}>
    <LineChart
        data={(data ?? []).map((val, i) => ({
            freq: i,
            mag: val,
        }))}
        x="freq"
        series={[
            {
                key: "mag",
                ...chartConfig.mag,
            },
        ]}
        props={{
            tooltip: { context: { mode: "quadtree" }}
        }}
    >
        {#snippet tooltip()}
            <Chart.Tooltip />
        {/snippet}
    </LineChart>
</Chart.Container>
