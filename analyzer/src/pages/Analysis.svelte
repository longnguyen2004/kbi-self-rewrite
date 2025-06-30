<script lang="ts">
    import DiffChart from "$lib/components/charts/DiffChart.svelte";
    import FreqChart from "$lib/components/charts/FreqChart.svelte";
    import InputTimeline from "$lib/components/charts/InputTimeline.svelte";
    import {
        postprocess,
        type PostprocessOptions,
    } from "$lib/analyzer/postprocess";
    import { Checkbox } from "$lib/components/ui/checkbox";
    import { Label } from "$lib/components/ui/label";
    import * as Select from "$lib/components/ui/select";
    import type { Device, Input } from "$lib/validator/validator";
    import type { Analyzer } from "$lib/analyzer/analyzer.svelte";

    type Props = {
        analyzer: Analyzer;
        devices: Record<string, Device>;
        inputs: Record<string, Input[]>;
    };
    const { analyzer, devices, inputs }: Props = $props();
    let postprocessOpts: PostprocessOptions = $state({
        lowCut: true,
    });
</script>

<div class="analysis-options flex flex-row gap-2 items-center">
    <div class="flex flex-row gap-2">
        <Label for="binning-rate">Binning rate</Label>
        <Select.Root
            type="single"
            bind:value={
                () => analyzer.binRate.toString(),
                (v) => (analyzer.binRate = parseInt(v))
            }
        >
            <Select.Trigger id="binning-rate">
                {analyzer.binRate}Hz
            </Select.Trigger>
            <Select.Content>
                {#each [1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000] as binRate}
                    <Select.Item
                        value={binRate.toString()}
                        label="{binRate}Hz"
                    />
                {/each}
            </Select.Content>
        </Select.Root>
    </div>
    <div class="flex flex-row gap-2">
        <Label for="low-cut">Low Cut Filter</Label>
        <Checkbox id="low-cut" bind:checked={postprocessOpts.lowCut} />
    </div>
</div>
<div class="diff-chart min-h-0 flex-3 flex flex-row gap-10">
    <DiffChart
        title="Consecutive timestamp diffs"
        data={analyzer.consecutiveDiff}
    />
    <DiffChart title="All timestamp diffs" data={analyzer.allDiff} />
    <DiffChart
        title="Timestamp fractional parts"
        data={analyzer.wrappedTimestamp}
    />
</div>
<div class="freq-chart min-h-0 flex-3 flex flex-row gap-10">
    <FreqChart
        title="Consecutive timestamp diffs (frequency domain)"
        data={postprocess(analyzer.consecutiveDiffFreq, postprocessOpts)}
    />
    <FreqChart
        title="All timestamp diffs (frequency domain)"
        data={postprocess(analyzer.allDiffFreq, postprocessOpts)}
    />
    <FreqChart
        title="Timestamp fractional parts (frequency domain)"
        data={postprocess(analyzer.wrappedTimestampFreq, postprocessOpts)}
    />
</div>
<div class="input-timeline min-h-0 flex-4">
    <InputTimeline {devices} {inputs} />
</div>
