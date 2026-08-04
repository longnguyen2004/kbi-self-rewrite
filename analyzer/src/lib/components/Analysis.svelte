<script lang="ts">
  import { TriangleAlert } from '@lucide/svelte';
  import { getCurrentMode } from '@/currentModeContext';
  import DiffChart from '$lib/components/charts/DiffChart.svelte';
  import FreqChart from '$lib/components/charts/FreqChart.svelte';
  import InputTimeline from '$lib/components/charts/InputTimeline.svelte';
  import { postprocessWithMax, type PostprocessOptions } from '$lib/analyzer/postprocess';
  import { Checkbox } from '$lib/components/ui/checkbox';
  import { Label } from '$lib/components/ui/label';
  import * as Select from '$lib/components/ui/select';
  import * as Tooltip from '$lib/components/ui/tooltip';
  import type { Analyzer } from '$lib/analyzer/analyzer.svelte';
  import type { TimelineProvider } from '$lib/analyzer/input_timeline.svelte';
  import type { Device } from '$lib/validator/validator';

  type Props = {
    analyzer: Analyzer;
    timeline: TimelineProvider;
    devices: Record<string, Device>;
  };
  const { analyzer, timeline, devices }: Props = $props();
  let postprocessOpts: PostprocessOptions = $state({
    lowCut: true,
  });

  const consecutiveDiffFreq = $derived(
    postprocessWithMax(analyzer.consecutiveDiffFreq, postprocessOpts),
  );
  const allDiffFreq = $derived(postprocessWithMax(analyzer.allDiffFreq, postprocessOpts));
  const wrappedTimestampFreq = $derived(
    postprocessWithMax(analyzer.wrappedTimestampFreq, postprocessOpts),
  );
</script>

<div class="analysis-options mb-2 flex flex-row items-center gap-2">
  <div class="flex flex-row gap-2">
    <Label for="binning-rate">Binning rate</Label>
    <Select.Root
      type="single"
      bind:value={() => analyzer.binRate.toString(), (v) => (analyzer.binRate = parseInt(v))}
    >
      <Select.Trigger id="binning-rate">
        {analyzer.binRate}Hz
      </Select.Trigger>
      <Select.Content>
        {#each [1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000] as binRate}
          <Select.Item value={binRate.toString()} label="{binRate}Hz" />
        {/each}
      </Select.Content>
    </Select.Root>
    {#if getCurrentMode()() === "recording" && analyzer.binRate > 16000}
      <Tooltip.Provider delayDuration={0} disableCloseOnTriggerClick>
        <Tooltip.Root>
          <Tooltip.Trigger>
            <TriangleAlert class="text-amber-500"/>
          </Tooltip.Trigger>
          <Tooltip.Content>
            It is not recommended to go above 16000Hz bin rate during live analysis
          </Tooltip.Content>
        </Tooltip.Root>
      </Tooltip.Provider>
    {/if}
  </div>
  <div class="flex flex-row gap-2">
    <Label for="low-cut">Low Cut Filter</Label>
    <Checkbox id="low-cut" bind:checked={postprocessOpts.lowCut} />
  </div>
</div>
<div class="diff-chart flex min-h-0 flex-3 flex-row gap-10">
  <DiffChart
    title="Consecutive timestamp diffs"
    data={analyzer.consecutiveDiff}
    yMax={analyzer.consecutiveDiffMax}
  />
  <DiffChart title="All timestamp diffs" data={analyzer.allDiff} yMax={analyzer.allDiffMax} />
  <DiffChart
    title="Timestamp fractional parts"
    data={analyzer.wrappedTimestamp}
    yMax={analyzer.wrappedTimestampMax}
  />
</div>
<div class="freq-chart flex min-h-0 flex-3 flex-row gap-10">
  <FreqChart
    title="Consecutive timestamp diffs (frequency domain)"
    data={consecutiveDiffFreq.data}
    yMax={consecutiveDiffFreq.max}
  />
  <FreqChart
    title="All timestamp diffs (frequency domain)"
    data={allDiffFreq.data}
    yMax={allDiffFreq.max}
  />
  <FreqChart
    title="Timestamp fractional parts (frequency domain)"
    data={wrappedTimestampFreq.data}
    yMax={wrappedTimestampFreq.max}
  />
</div>
<div class="input-timeline min-h-0 flex-4">
  <InputTimeline {timeline} deviceIds={Object.keys(devices)} />
</div>
