<script lang="ts">
  import { FiniteStateMachine } from 'runed';
  import { ModeWatcher, toggleMode } from 'mode-watcher';
  import { untrack, onDestroy } from 'svelte';
  import { SunIcon, MoonIcon } from '@lucide/svelte';

  import { Button } from '$lib/components/ui/button';
  import { Input } from '$lib/components/ui/input';
  import { Label } from '$lib/components/ui/label';
  import * as Tabs from '$lib/components/ui/tabs';

  import Analysis from './pages/Analysis.svelte';
  import ResultInfo from '$lib/components/ResultInfo.svelte';

  import { Analyzer } from '$lib/analyzer/analyzer.svelte.js';
  import { FromFileDataSource } from '$lib/data_source/from_file.svelte';
  import { FromRecorderDataSource } from '$lib/data_source/from_recorder.svelte';
  import type { DataSource } from '$lib/data_source';

  let analyzer = new Analyzer();

  type States = 'from-file' | 'from-recorder' | 'recording';
  type Events = 'changeSource' | 'toggleRecording';

  let dataSource: DataSource | undefined = $state();
  let result = $derived(dataSource?.result);
  let pendingTimestamps: number[] = [];
  let submitLoop: number | undefined;

  let currentTab = $state('from-file');
  let f = new FiniteStateMachine<States, Events>('from-file', {
    'from-file': {
      _enter({ from, event }) {
        if (event === "changeSource" || from === null)
        {
          analyzer.reset();
          dataSource = new FromFileDataSource();
        }
        currentTab = 'from-file';
      },
      changeSource: 'from-recorder',
    },
    'from-recorder': {
      _enter({ event }) {
        if (event === "changeSource")
        {
          analyzer.reset();
          const recorderSource = dataSource = new FromRecorderDataSource();
          recorderSource.on("input", (e) => {
            analyzer.add([e.data[1].timestamp]);
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
          <Tabs.Trigger value="from-recorder">From recorder</Tabs.Trigger>
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
              await (dataSource as FromFileDataSource).parse(files[0]);
              if (!result) return;
              analyzer.reset();
              const timestamps = Object.values(result.inputs)
                .flat()
                .map((val) => val.timestamp);
              timestamps.sort((a, b) => a - b);
              analyzer.add(timestamps);
            }}
          />
        </Tabs.Content>
        <Tabs.Content class="flex flex-row gap-4" value="from-recorder">
          <Button onclick={() => f.send('toggleRecording')}>
            {f.current === 'recording' ? 'Stop Recording' : 'Start Recording'}
          </Button>
        </Tabs.Content>
      </Tabs.Root>

      {#if result}
        <ResultInfo result={result}>
          {#snippet child({ props })}
            <Button {...props} variant="outline">Info</Button>
          {/snippet}
        </ResultInfo>
      {/if}

      <Button class="ml-auto" onclick={toggleMode} variant="outline" size="icon">
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
  {#if result}
    <Analysis {analyzer} {...result} />
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
