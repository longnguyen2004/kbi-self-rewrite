<script lang="ts" module>
    export type Props = {
        data: number[];
    };
</script>

<script lang="ts">
    import { LineChart } from "layerchart";
    import * as Chart from "$lib/components/ui/chart";

    let { data }: Props = $props();
    
    const chartConfig = {
        count: { label: "Count", color: "#FFFFFF" },
    } satisfies Chart.ChartConfig;
</script>

<Chart.Container config={chartConfig}>
    <LineChart
        data={data.flat().map((val, i) => ({
            time: i / (data.length - 1),
            count: val,
        }))}
        x="time"
        series={[
            {
                key: "count",
                ...chartConfig.count
            }
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
