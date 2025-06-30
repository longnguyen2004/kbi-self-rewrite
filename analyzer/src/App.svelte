<script lang="ts">
    import { FiniteStateMachine } from "runed";
    import { ModeWatcher, toggleMode } from "mode-watcher";
    import { untrack, onDestroy } from "svelte";
    import { SunIcon, MoonIcon } from "@lucide/svelte";

    import { Button } from "$lib/components/ui/button";
    import { Input } from "$lib/components/ui/input";
    import { Label } from "$lib/components/ui/label";
    import * as Tabs from "$lib/components/ui/tabs";

    import Analysis from "./pages/Analysis.svelte";

    import { Analyzer } from "$lib/analyzer/analyzer.svelte.js";
    import { parseKbiResult } from "$lib/parser/parser";
    import type { Result } from "$lib/validator/validator";

    let analyzer = new Analyzer();
    let data: Result | undefined = $state.raw();

    type States = "from-file" | "from-recorder" | "recording";
    type Events = "changeSource" | "startRecording" | "stopRecording";
    let currentTab = $state("from-file");
    let f = new FiniteStateMachine<States, Events>("from-file", {
        "from-file": {
            _enter() {
                currentTab = "from-file";
            },
            _exit() {
                analyzer.reset();
                data = undefined;
            },
            changeSource: "from-recorder",
        },
        "from-recorder": {
            _enter() {
                currentTab = "from-recorder";
            },
            _exit() {
                analyzer.reset();
                data = undefined;
            },
            changeSource: "from-file",
            startRecording: "recording"
        },
        "recording": {
            stopRecording: "from-recorder"
        }
    });
    onDestroy(() => analyzer.terminate());
</script>

<ModeWatcher />
<main class="w-full h-full flex flex-col">
    <div class="controls mb-4">
        <div class="row">
            <Tabs.Root value={currentTab} onValueChange={(s) => {
                switch (s)
                {
                    case "from-file":
                        f.send("changeSource");
                        return;
                    case "from-recorder":
                        f.send("changeSource");
                        return;
                }
            }} class="flex flex-row gap-4">
                <Tabs.List>
                    <Tabs.Trigger value="from-file">
                        From file
                    </Tabs.Trigger>
                    <Tabs.Trigger value="from-recorder">
                        From recorder
                    </Tabs.Trigger>
                </Tabs.List>
                <Tabs.Content class="flex flex-row gap-4" value="from-file">
                    <Label for="kbi-file" class="whitespace-nowrap">KBI file</Label>
                    <Input type="file" id="kbi-file" accept=".json,.kbi"
                        onclick={(e) => e.currentTarget.value = ""}
                        oninput={async (e) => {
                            const { files } = e.currentTarget;
                            if (!files?.[0])
                            {
                                data = undefined;
                                return;
                            }
                            data = await parseKbiResult(files[0]);
                            if (!data)
                                return;
                            analyzer.reset();
                            const timestamps = Object.values(data.inputs)
                                .flat().map(val => val.timestamp);
                            timestamps.sort((a, b) => a - b);
                            analyzer.add(timestamps);
                        }}
                    />
                </Tabs.Content>
                <Tabs.Content class="flex flex-row gap-4" value="from-recorder">
                    <Button>{f.current === "recording" ? "Stop Recording" : "Start Recording"}</Button>
                </Tabs.Content>
            </Tabs.Root>

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
    </div>
    {#if data}
    <Analysis {analyzer} {...data}/>
    {/if}
</main>
<style>
    .controls {
        display: flex;
        flex-direction: column;
        gap: 1rem;

        .row {
            display: flex;
            flex-direction: row;
            align-items: center;
            gap: 1rem;
        }
    }
</style>