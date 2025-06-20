<script lang="ts">
    import { ModeWatcher, toggleMode } from "mode-watcher";
    import { untrack } from "svelte";
    import { SunIcon, MoonIcon } from "@lucide/svelte";

    import { Button } from "$lib/components/ui/button";
    import { Input } from "$lib/components/ui/input";
    import { Label } from "$lib/components/ui/label";
    import * as Select from "$lib/components/ui/select";

    import DiffChart from "$lib/components/charts/DiffChart.svelte";
    import FreqChart from "$lib/components/charts/FreqChart.svelte";

    import { Analyzer } from "$lib/analyzer/analyzer.svelte.js";
    import { parseKbiResult } from "$lib/parser/parser";
    import type { Result } from "$lib/validator/validator";

    let analyzer = new Analyzer();
    let data: Result | undefined = $state.raw();

    $effect(() => {
        untrack(() => analyzer.reset());
        if (!data)
            return;
        const timestamps = Object.values(data.inputs).flat().map(val => val.timestamp);
        timestamps.sort((a, b) => a - b);
        untrack(() => analyzer.add(timestamps));
    });
</script>

<ModeWatcher />
<main class="w-full h-full flex flex-col gap-10">
    <div class="controls">
        <div class="row">
            <div class="flex flex-row gap-4">
                <Label for="kbi-file" class="whitespace-nowrap">KBI file</Label>
                <Input type="file" id="kbi-file" accept=".json,.kbi"
                    onchange={async (e) => {
                        const { files } = e.currentTarget;
                        if (!files?.[0])
                        {
                            data = undefined;
                            return;
                        }
                        data = await parseKbiResult(files[0]);
                        e.currentTarget.files = null;
                    }}
                />
            </div>

            <Button class="ml-auto" onclick={toggleMode} variant="outline" size="icon">
                <SunIcon
                    class="h-[1.2rem] w-[1.2rem] rotate-0 scale-100 transition-all dark:-rotate-90 dark:scale-0"
                />
                <MoonIcon
                    class="absolute h-[1.2rem] w-[1.2rem] rotate-90 scale-0 transition-all dark:rotate-0 dark:scale-100"
                />
                <span class="sr-only">Toggle theme</span>
            </Button>
        </div>
        <div class="row">
            <div>
                <Label for="binning-rate">Binning rate</Label>
                <Select.Root id="binning-rate" type="single" bind:value={
                    () => analyzer.binRate.toString(),
                    (v) => analyzer.binRate = parseInt(v)
                }>
                    <Select.Trigger>
                        {analyzer.binRate}Hz
                    </Select.Trigger>
                    <Select.Content>
                        {#each [1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000] as binRate}
                            <Select.Item value={binRate.toString()} label="{binRate}Hz" />
                        {/each}
                    </Select.Content>
                </Select.Root>
            </div>
        </div>
    </div>
    <div class="diff-chart flex-1 flex flex-row gap-15">
        <DiffChart data={analyzer.consecutiveDiff} />
        <DiffChart data={analyzer.allDiff} />
    </div>
    <div class="freq-chart flex-1 flex flex-row gap-15">
        <FreqChart data={analyzer.consecutiveDiffFreq} />
        <FreqChart data={analyzer.allDiffFreq} />
    </div>
</main>
<style>
    .controls {
        display: flex;
        flex-direction: column;

        .row {
            display: flex;
            flex-direction: row;
        }
    }
</style>