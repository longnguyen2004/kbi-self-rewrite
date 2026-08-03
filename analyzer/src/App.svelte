<script lang="ts">
  import { FiniteStateMachine } from 'runed';
  import { ModeWatcher, toggleMode } from 'mode-watcher';
  import { onDestroy } from 'svelte';
  import { SunIcon, MoonIcon } from '@lucide/svelte';
  import DiscordLogo from './assets/discord_white.svg';

  import { Button, buttonVariants } from '$lib/components/ui/button';
  import { Input } from '$lib/components/ui/input';
  import { Label } from '$lib/components/ui/label';
  import * as Tabs from '$lib/components/ui/tabs';

  import Analysis from '$lib/components/Analysis.svelte';
  import ResultInfo from '$lib/components/ResultInfo.svelte';
  import FpsCounter from '$lib/components/FpsCounter.svelte';

  import { Analyzer } from '$lib/analyzer/analyzer.svelte.js';
  import { KeyEventToInputTimeline } from '$lib/analyzer/input_timeline.svelte';
  import { FromFileDataSource } from '$lib/data_source/from_file.svelte';
  import { FromRecorderDataSource } from '$lib/data_source/from_recorder.svelte';

  import { isNeutralino } from '$lib/helper/is_neutralino';
  import type { DataSource } from '$lib/data_source';

  let analyzer = new Analyzer();
  let timeline = new KeyEventToInputTimeline();

  type States = 'from-file' | 'from-recorder' | 'recording';
  type Events = 'changeSource' | 'toggleRecording';

  let dataSource: DataSource | undefined = $state();
  let result = $derived(dataSource?.result);

  let currentTab = $state('from-file');
  let f = new FiniteStateMachine<States, Events>('from-file', {
    'from-file': {
      _enter({ from, event }) {
        if (event === 'changeSource' || from === null) {
          analyzer.reset();
          timeline.reset();
          dataSource = new FromFileDataSource();
        }
        currentTab = 'from-file';
      },
      changeSource: 'from-recorder',
    },
    'from-recorder': {
      _enter({ event }) {
        if (event === 'changeSource') {
          analyzer.reset();
          timeline.reset();
          const recorderSource = (dataSource = new FromRecorderDataSource());
          recorderSource.on('input', (e) => {
            analyzer.add([e.data[1].timestamp]);
            timeline.add([e.data]);
          });
        }
        currentTab = 'from-recorder';
      },
      changeSource: 'from-file',
      toggleRecording: 'recording',
    },
    recording: {
      _enter() {
        analyzer.reset();
        timeline.reset();
        const recorderSource = dataSource as FromRecorderDataSource;
        recorderSource.start();
      },
      _exit() {
        const recorderSource = dataSource as FromRecorderDataSource;
        recorderSource.stop();
      },
      toggleRecording: 'from-recorder',
    },
  });
  onDestroy(() => analyzer.terminate());
</script>

<ModeWatcher />
<main class="flex h-full w-full flex-col">
  <div class="controls mb-4">
    <div class="row">
      <Tabs.Root
        value={currentTab}
        onValueChange={(s) => {
          switch (s) {
            case 'from-file':
              f.send('changeSource');
              return;
            case 'from-recorder':
              f.send('changeSource');
              return;
          }
        }}
        class="flex flex-row! gap-4"
      >
        <Tabs.List>
          <Tabs.Trigger value="from-file">From file</Tabs.Trigger>
          {#if isNeutralino()}
            <Tabs.Trigger value="from-recorder">From recorder</Tabs.Trigger>
          {/if}
        </Tabs.List>
        <Tabs.Content class="flex flex-row gap-4" value="from-file">
          <Label for="kbi-file" class="whitespace-nowrap">KBI file</Label>
          <Input
            type="file"
            id="kbi-file"
            accept=".json,.kbi"
            onclick={(e) => (e.currentTarget.value = '')}
            oninput={async (e) => {
              const { files } = e.currentTarget;
              if (!files?.[0]) {
                return;
              }
              const fileSource = dataSource as FromFileDataSource;
              await fileSource.parse(files[0]);
              if (!fileSource.result) return;
              analyzer.reset();
              timeline.reset();
              const timestamps = Object.values(fileSource.result.inputs)
                .flat()
                .map((val) => val.timestamp);
              timestamps.sort((a, b) => a - b);
              analyzer.add(timestamps);
              timeline.add(
                Object.entries(fileSource.result.inputs).flatMap(([id, events]) =>
                  events.map((el) => [id, el] as [string, typeof el]),
                ),
              );
            }}
          />
        </Tabs.Content>
        {#if isNeutralino()}
          <Tabs.Content class="flex flex-row gap-4" value="from-recorder">
            <Button onclick={() => f.send('toggleRecording')}>
              {f.current === 'recording' ? 'Stop Recording' : 'Start Recording'}
            </Button>
          </Tabs.Content>
        {/if}
      </Tabs.Root>

      {#if result}
        <ResultInfo {result}>
          {#snippet child({ props })}
            <Button {...props} variant="outline">Info</Button>
          {/snippet}
        </ResultInfo>
      {/if}

      <div class="ml-auto flex flex-row gap-2 items-center">
        <FpsCounter decimals={1} />
        <a href="https://discord.gg/kX4cJQH5Zn" target="_blank" class={[buttonVariants(), "bg-[#5865F2]! text-white"]}>
          <img class="h-full py-2" alt="Discord logo" src={DiscordLogo}>
          Join our Discord server
        </a>
        <Button onclick={toggleMode} variant="outline" size="icon">
          <SunIcon
            class="h-[1.2rem] w-[1.2rem] scale-100 rotate-0 transition-all dark:scale-0 dark:-rotate-90"
          />
          <MoonIcon
            class="absolute h-[1.2rem] w-[1.2rem] scale-0 rotate-90 transition-all dark:scale-100 dark:rotate-0"
          />
          <span class="sr-only">Toggle theme</span>
        </Button>
      </div>
    </div>
  </div>
  {#if result}
    <Analysis {analyzer} {timeline} />
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
