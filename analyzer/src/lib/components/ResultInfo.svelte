<script lang="ts">
  import type { Result } from '$lib/validator/validator';
  import { UsbDeviceSpeed } from '$lib/validator/validator';
  import type { Snippet } from 'svelte';
  import * as Dialog from '$lib/components/ui/dialog';
  import * as Tabs from '$lib/components/ui/tabs';
  import { Separator } from '$lib/components/ui/separator';
  import { toHex } from '$lib/helper/hex';
  import {
    parseUsbDescriptors,
    type DescriptorField,
  } from '$lib/parser/parser_usb_descriptors';

  type Props = {
    result: Result;
    child: Snippet<[{ props: Record<string, unknown> }]>;
  };

  const { result, child }: Props = $props();

  const USB_SPEED_NAMES: Record<UsbDeviceSpeed, string> = {
    [UsbDeviceSpeed.UNKNOWN]: 'Unknown',
    [UsbDeviceSpeed.LOW_SPEED]: 'Low Speed',
    [UsbDeviceSpeed.FULL_SPEED]: 'Full Speed',
    [UsbDeviceSpeed.HIGH_SPEED]: 'High Speed',
    [UsbDeviceSpeed.SUPERSPEED]: 'Super Speed',
  };
</script>

<Dialog.Root>
  <Dialog.Trigger>
    {@render child({ props: {} })}
  </Dialog.Trigger>
  <Dialog.Content class="max-w-2xl!">
    <Dialog.Header>
      <Dialog.Title>Result information</Dialog.Title>
    </Dialog.Header>
    <Tabs.Root value="summary">
      <Tabs.List>
        <Tabs.Trigger value="summary">Summary</Tabs.Trigger>
        <Tabs.Trigger value="devices">Devices</Tabs.Trigger>
        {#if result.usb_devices}
          <Tabs.Trigger value="usb_devices">USB Devices</Tabs.Trigger>
        {/if}
      </Tabs.List>
      <Tabs.Content value="summary">
        {@const { os_name, os_ver, arch, backend, safe_mode } = result.info}
        <div class="flex flex-col gap-1">
          <div>OS: {os_name}</div>
          <div>OS version: {os_ver}</div>
          <div>Architecture: {arch}</div>
          <div>Recording backend: {backend}</div>
          {#if safe_mode !== undefined}
            <div>Is safe mode?: {safe_mode}</div>
          {/if}
        </div>
      </Tabs.Content>
      <Tabs.Content value="devices">
        <div class="flex flex-col gap-1">
          {#each Object.entries(result.devices) as [id, device], i}
            <details class="rounded-md border px-2 py-1">
              <summary class="cursor-pointer">
                Device {i + 1}: <code class="text-xs">{id}</code>
              </summary>
              <ul class="mt-1 ml-6 list-disc">
                <li>Name: {device.name}</li>
                <li>VID: <code>{device.vid.toString(16).padStart(4, '0')}</code></li>
                <li>PID: <code>{device.pid.toString(16).padStart(4, '0')}</code></li>
                <li>Number of inputs: {result.inputs[id]?.length ?? 0}</li>
                {#if device.usb_device}
                  <li>Associated USB device: <code class="text-xs">{device.usb_device}</code></li>
                {/if}
              </ul>
            </details>
          {/each}
        </div>
      </Tabs.Content>
      {#if result.usb_devices}
        <Tabs.Content value="usb_devices">
          <div class="flex max-h-[60vh] flex-col gap-2 overflow-y-auto">
            {#each Object.entries(result.usb_devices) as [id, usbDevice], i}
              {@const descriptors = parseUsbDescriptors(usbDevice.descriptors ?? '')}
              <details class="rounded-md border px-2 py-1">
                <summary class="cursor-pointer">
                  USB Device {i + 1}: <code>{id}</code>
                </summary>
                <div class="mt-1 ml-4 flex flex-col gap-1">
                  <div>VID: <code>{usbDevice.vid.toString(16).padStart(4, '0')}</code></div>
                  <div>PID: <code>{usbDevice.pid.toString(16).padStart(4, '0')}</code></div>
                  <div>Speed: {USB_SPEED_NAMES[usbDevice.speed] ?? 'Unknown'}</div>
                  <Separator class="my-1" />
                  <div class="font-medium">Descriptors:</div>
                  {#if descriptors.length === 0}
                    <div class="text-muted-foreground">No descriptors available</div>
                  {:else}
                    <div class="flex flex-col gap-1">
                      {#each descriptors as desc}
                        <details class="rounded border px-2 py-1">
                          <summary class="cursor-pointer">
                            Descriptor #{desc.index} - {desc.typeName} ({desc.length} bytes)
                          </summary>
                          <div class="mt-1 ml-4 flex flex-col gap-1">
                            <div class="text-xs font-medium">Fields:</div>
                            <div class="flex flex-col gap-0.5">
                              {#snippet renderField(field: DescriptorField)}
                                {const container = field.children?.length ? 'details' : 'div'}
                                {const summaryTextContainer = field.children?.length
                                  ? 'summary'
                                  : 'div'}
                                <svelte:element this={container}>
                                  <svelte:element
                                    this={summaryTextContainer}
                                    class="gap-2 align-baseline text-xs"
                                  >
                                    <span class="shrink-0 text-muted-foreground">{field.name}:</span
                                    >
                                    <code class="break-all">{field.value}</code>
                                  </svelte:element>
                                  <div class="ml-4">
                                    {#each field.children ?? [] as childField}
                                      {@render renderField(childField)}
                                    {/each}
                                  </div>
                                </svelte:element>
                              {/snippet}
                              {#each desc.fields as field}
                                {@render renderField(field)}
                              {/each}
                            </div>
                            <details class="mt-1">
                              <summary class="cursor-pointer text-xs text-muted-foreground">
                                Raw bytes
                              </summary>
                              <code class="mt-1 block rounded bg-muted p-1 text-xs break-all">
                                {toHex(desc.raw)}
                              </code>
                            </details>
                          </div>
                        </details>
                      {/each}
                    </div>
                  {/if}
                </div>
              </details>
            {/each}
          </div>
        </Tabs.Content>
      {/if}
    </Tabs.Root>
  </Dialog.Content>
</Dialog.Root>

<style>
  ul {
    list-style: disc;
  }
</style>
