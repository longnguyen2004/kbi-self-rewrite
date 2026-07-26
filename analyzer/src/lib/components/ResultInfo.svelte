<script lang="ts">
    import type { Result, UsbDevice } from "$lib/validator/validator";
    import { UsbDeviceSpeed } from "$lib/validator/validator";
    import type { Snippet } from "svelte";
    import * as Dialog from "$lib/components/ui/dialog";
    import * as Tabs from "$lib/components/ui/tabs";
    import { Separator } from "$lib/components/ui/separator";

    type Props = {
        result: Result;
        child: Snippet<[{ props: Record<string, unknown> }]>;
    };

    const { result, child }: Props = $props();

    const USB_SPEED_NAMES: Record<UsbDeviceSpeed, string> = {
        [UsbDeviceSpeed.UNKNOWN]: "Unknown",
        [UsbDeviceSpeed.LOW_SPEED]: "Low Speed",
        [UsbDeviceSpeed.FULL_SPEED]: "Full Speed",
        [UsbDeviceSpeed.HIGH_SPEED]: "High Speed",
        [UsbDeviceSpeed.SUPERSPEED]: "Super Speed",
    };

    const USB_DESCRIPTOR_TYPES: Record<number, string> = {
        0x01: "DEVICE",
        0x02: "CONFIGURATION",
        0x03: "STRING",
        0x04: "INTERFACE",
        0x05: "ENDPOINT",
        0x06: "DEVICE_QUALIFIER",
        0x07: "OTHER_SPEED_CONFIGURATION",
        0x0b: "INTERFACE_ASSOCIATION",
        0x0c: "HID",
        0x0d: "REPORT",
        0x0e: "PHYSICAL",
        0x21: "HID",
        0x22: "REPORT",
        0x23: "PHYSICAL",
        0x24: "HUB",
        0x29: "HID_BUTTON",
    };

    type ParsedDescriptor = {
        index: number;
        type: number;
        typeName: string;
        length: number;
        raw: Uint8Array;
    };

    function decodeBase64(base64: string): Uint8Array {
        const binary = atob(base64);
        const bytes = new Uint8Array(binary.length);
        for (let i = 0; i < binary.length; i++)
            bytes[i] = binary.charCodeAt(i);
        return bytes;
    }

    function parseDescriptors(raw: Uint8Array): ParsedDescriptor[] {
        const descriptors: ParsedDescriptor[] = [];
        let offset = 0;
        let index = 0;
        while (offset < raw.length) {
            const length = raw[offset];
            if (!length || offset + length > raw.length)
                break;
            const type = raw[offset + 1];
            const slice = raw.slice(offset, offset + length);
            descriptors.push({
                index,
                type,
                typeName:
                    USB_DESCRIPTOR_TYPES[type] ??
                    `0x${type.toString(16).padStart(2, "0").toUpperCase()}`,
                length,
                raw: slice,
            });
            offset += length;
            index++;
        }
        return descriptors;
    }

    function toHex(bytes: Uint8Array): string {
        return Array.from(bytes)
            .map((b) => b.toString(16).padStart(2, "0").toUpperCase())
            .join(" ");
    }

    function parseUsbDescriptors(usbDevice: UsbDevice): ParsedDescriptor[] {
        if (!usbDevice.descriptors) return [];
        try {
            return parseDescriptors(decodeBase64(usbDevice.descriptors));
        } catch {
            return [];
        }
    }

    function formatVidPid(usbDevice: UsbDevice): string {
        return `VID:${usbDevice.vid
            .toString(16)
            .padStart(4, "0")
            .toUpperCase()} PID:${usbDevice.pid
            .toString(16)
            .padStart(4, "0")
            .toUpperCase()}`;
    }
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
                {@const { os_name, os_ver, arch, backend, safe_mode } =
                    result.info}
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
                        <details class="border rounded-md px-2 py-1">
                            <summary class="cursor-pointer">
                                Device {i + 1}: <code class="text-xs">{id}</code>
                            </summary>
                            <ul class="list-disc ml-6 mt-1">
                                <li>Name: {device.name}</li>
                                <li>VID: <code>{device.vid}</code></li>
                                <li>PID: <code>{device.pid}</code></li>
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
                    <div class="flex flex-col gap-2 max-h-[60vh] overflow-y-auto">
                        {#each Object.entries(result.usb_devices) as [id, usbDevice], i}
                            {@const descriptors = parseUsbDescriptors(usbDevice)}
                            <details class="border rounded-md px-2 py-1">
                                <summary class="cursor-pointer">
                                    USB Device {i + 1}: <code class="text-xs">{id}</code>
                                </summary>
                                <div class="ml-4 mt-1 flex flex-col gap-1">
                                    <div>VID: <code>{usbDevice.vid.toString(16).padStart(4, '0')}</code></div>
                                    <div>PID: <code>{usbDevice.pid.toString(16).padStart(4, '0')}</code></div>
                                    <div>Speed: {USB_SPEED_NAMES[usbDevice.speed] ?? "Unknown"}</div>
                                    <Separator class="my-1" />
                                    <div class="font-medium">Descriptors:</div>
                                    {#if descriptors.length === 0}
                                        <div class="text-muted-foreground">
                                            No descriptors available
                                        </div>
                                    {:else}
                                        <div class="flex flex-col gap-1">
                                            {#each descriptors as desc}
                                                <details class="border rounded px-2 py-1">
                                                    <summary class="cursor-pointer">
                                                        Descriptor #{desc.index} - {desc.typeName} ({desc.length} bytes)
                                                    </summary>
                                                    <div class="ml-4 mt-1">
                                                        <div
                                                            class="text-muted-foreground mb-1"
                                                        >
                                                            Type: 0x{desc.type
                                                                .toString(16)
                                                                .padStart(2, "0")
                                                                .toUpperCase()} ({desc.typeName}),
                                                            Length: {desc.length}
                                                        </div>
                                                        <code
                                                            class="text-xs break-all block bg-muted p-1 rounded"
                                                        >
                                                            {toHex(desc.raw)}
                                                        </code>
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