// USB descriptor type constants
export const USB_DESCRIPTOR_TYPES: Record<number, string> = {
  0x01: 'DEVICE',
  0x02: 'CONFIGURATION',
  0x03: 'STRING',
  0x04: 'INTERFACE',
  0x05: 'ENDPOINT',
  0x06: 'DEVICE_QUALIFIER',
  0x07: 'OTHER_SPEED_CONFIGURATION',
  0x0b: 'INTERFACE_ASSOCIATION',
  0x0c: 'HID',
  0x0d: 'REPORT',
  0x0e: 'PHYSICAL',
  0x21: 'HID',
  0x22: 'REPORT',
  0x23: 'PHYSICAL',
  0x24: 'HUB',
  0x29: 'HID_BUTTON',
};

// USB class codes
const USB_CLASS_CODES: Record<number, string> = {
  0x00: 'Use class info from Interface Descriptors',
  0x01: 'Audio',
  0x02: 'Communications and CDC Control',
  0x03: 'HID (Human Interface Device)',
  0x05: 'Physical',
  0x06: 'Image',
  0x07: 'Printer',
  0x08: 'Mass Storage',
  0x09: 'Hub',
  0x0a: 'CDC Data',
  0x0b: 'Smart Card',
  0x0d: 'Content Security',
  0x0e: 'Video',
  0x0f: 'Personal Healthcare',
  0x10: 'Audio/Video Devices',
  0x11: 'Billboard Device Class',
  0x12: 'USB Type-C Bridge Class',
  0xdc: 'Diagnostic Device',
  0xe0: 'Wireless Controller',
  0xef: 'Miscellaneous',
  0xfe: 'Application Specific',
  0xff: 'Vendor Specific',
};

// Endpoint transfer types
const EP_TRANSFER_TYPES: Record<number, string> = {
  0: 'Control',
  1: 'Isochronous',
  2: 'Bulk',
  3: 'Interrupt',
};

// Endpoint synchronization types (for isochronous)
const EP_SYNC_TYPES: Record<number, string> = {
  0: 'No Synchronization',
  1: 'Asynchronous',
  2: 'Adaptive',
  3: 'Synchronous',
};

// Endpoint usage types (for isochronous)
const EP_USAGE_TYPES: Record<number, string> = {
  0: 'Data endpoint',
  1: 'Feedback endpoint',
  2: 'Implicit feedback Data endpoint',
  3: 'Reserved',
};

// HID country codes
const HID_COUNTRY_CODES: Record<number, string> = {
  0x00: 'Not supported',
  0x01: 'Arabic',
  0x02: 'Belgian',
  0x03: 'Canadian-Bilingual',
  0x04: 'Canadian-French',
  0x05: 'Czech Republic',
  0x06: 'Danish',
  0x07: 'Finnish',
  0x08: 'French',
  0x09: 'German',
  0x0a: 'Greek',
  0x0b: 'Hebrew',
  0x0c: 'Hungary',
  0x0d: 'International (ISO)',
  0x0e: 'Italian',
  0x0f: 'Japan (Katakana)',
  0x10: 'Korean',
  0x11: 'Latin American',
  0x12: 'Netherlands/Dutch',
  0x13: 'Norwegian',
  0x14: 'Persian (Farsi)',
  0x15: 'Poland',
  0x16: 'Portuguese',
  0x17: 'Russia',
  0x18: 'Slovakia',
  0x19: 'Spanish',
  0x1a: 'Swedish',
  0x1b: 'Swiss/French',
  0x1c: 'Swiss/German',
  0x1d: 'Switzerland',
  0x1e: 'Taiwan',
  0x1f: 'Turkish-Q',
  0x20: 'UK',
  0x21: 'US',
  0x22: 'Yugoslavia',
  0x23: 'Turkish-F',
};

// Hub characteristics
const HUB_CHAR_POWER_MODE: Record<number, string> = {
  0: 'Ganged power switching',
  1: 'Individual port power switching',
};

const HUB_CHAR_COMPOUND: Record<number, string> = {
  0: 'Not a compound device',
  1: 'Compound device',
};

const HUB_CHAR_OC_MODE: Record<number, string> = {
  0: 'Global over-current protection',
  1: 'Individual port over-current protection',
};

// ============================================================
// Types
// ============================================================

export type ParsedDescriptor = {
  index: number;
  type: number;
  typeName: string;
  length: number;
  raw: Uint8Array;
  fields: DescriptorField[];
};

export type DescriptorField = {
  name: string;
  value: string;
  children?: DescriptorField[];
};

// ============================================================
// Helpers
// ============================================================

function toHex(bytes: Uint8Array): string {
  return Array.from(bytes)
    .map((b) => b.toString(16).padStart(2, '0').toUpperCase())
    .join(' ');
}

function toHexByte(b: number): string {
  return '0x' + b.toString(16).padStart(2, '0').toUpperCase();
}

function decodeBase64(base64: string): Uint8Array {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

/**
 * Create a DataView over a Uint8Array for convenient multi-byte reads.
 */
function dv(buf: Uint8Array): DataView {
  return new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
}

const utf16leDecoder = new TextDecoder('utf-16le');

// ============================================================
// Descriptor field decoders
// ============================================================

function parseDeviceDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  return [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'bcdUSB', value: v.getUint16(2, true).toString(16).padStart(4, '0') },
    { name: 'bDeviceClass', value: USB_CLASS_CODES[v.getUint8(4)] ?? toHexByte(v.getUint8(4)) },
    { name: 'bDeviceSubClass', value: toHexByte(v.getUint8(5)) },
    { name: 'bDeviceProtocol', value: toHexByte(v.getUint8(6)) },
    { name: 'bMaxPacketSize0', value: v.getUint8(7).toString() },
    { name: 'idVendor', value: toHexByte(v.getUint16(8, true)) },
    { name: 'idProduct', value: toHexByte(v.getUint16(10, true)) },
    { name: 'bcdDevice', value: v.getUint16(12, true).toString(16).padStart(4, '0') },
    { name: 'iManufacturer', value: v.getUint8(14).toString() },
    { name: 'iProduct', value: v.getUint8(15).toString() },
    { name: 'iSerialNumber', value: v.getUint8(16).toString() },
    { name: 'bNumConfigurations', value: v.getUint8(17).toString() },
  ];
}

function parseConfigurationDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  return [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'wTotalLength', value: v.getUint16(2, true).toString() },
    { name: 'bNumInterfaces', value: v.getUint8(4).toString() },
    { name: 'bConfigurationValue', value: v.getUint8(5).toString() },
    { name: 'iConfiguration', value: v.getUint8(6).toString() },
    { name: 'bmAttributes', value: toHexByte(v.getUint8(7)) },
    { name: 'bMaxPower', value: `${v.getUint8(8) * 2} mA` },
  ];
}

function parseInterfaceDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  return [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'bInterfaceNumber', value: v.getUint8(2).toString() },
    { name: 'bAlternateSetting', value: v.getUint8(3).toString() },
    { name: 'bNumEndpoints', value: v.getUint8(4).toString() },
    { name: 'bInterfaceClass', value: USB_CLASS_CODES[v.getUint8(5)] ?? toHexByte(v.getUint8(5)) },
    { name: 'bInterfaceSubClass', value: toHexByte(v.getUint8(6)) },
    { name: 'bInterfaceProtocol', value: toHexByte(v.getUint8(7)) },
    { name: 'iInterface', value: v.getUint8(8).toString() },
  ];
}

function parseEndpointDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  const epAddr = v.getUint8(2);
  const epNum = epAddr & 0x0f;
  const epDir = epAddr & 0x80 ? 'IN' : 'OUT';
  const attr = v.getUint8(3);
  const transferType = attr & 0x03;
  const syncType = (attr >> 2) & 0x03;
  const usageType = (attr >> 4) & 0x03;
  return [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'bEndpointAddress', value: `${toHexByte(epAddr)} (Endpoint ${epNum}, ${epDir})` },
    {
      name: 'bmAttributes',
      value: toHexByte(attr),
      children: [
        {
          name: 'Transfer Type',
          value: EP_TRANSFER_TYPES[transferType] ?? `Unknown (${transferType})`,
        },
        { name: 'Sync Type', value: EP_SYNC_TYPES[syncType] ?? `Unknown (${syncType})` },
        { name: 'Usage Type', value: EP_USAGE_TYPES[usageType] ?? `Unknown (${usageType})` },
      ],
    },
    { name: 'wMaxPacketSize', value: v.getUint16(4, true).toString() },
    { name: 'bInterval', value: v.getUint8(6).toString() },
  ];
}

function parseHidDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  const fields: DescriptorField[] = [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'bcdHID', value: v.getUint16(2, true).toString(16).padStart(4, '0') },
    { name: 'bCountryCode', value: HID_COUNTRY_CODES[v.getUint8(4)] ?? toHexByte(v.getUint8(4)) },
    { name: 'bNumDescriptors', value: v.getUint8(5).toString() },
  ];
  // Parse subordinate descriptors
  let offset = 6;
  for (let i = 0; i < v.getUint8(5) && offset + 3 <= raw.length; i++) {
    const subType = v.getUint8(offset);
    const subLen = v.getUint16(offset + 1, true);
    fields.push({
      name: `  Sub-descriptor ${i + 1}`,
      value: `${USB_DESCRIPTOR_TYPES[subType] ?? toHexByte(subType)} (${subLen} bytes)`,
    });
    offset += 3;
  }
  return fields;
}

function parseHubDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  const chars = v.getUint16(3, true);
  const powerMode = chars & 0x03;
  const compound = (chars >> 2) & 0x01;
  const ocMode = (chars >> 3) & 0x01;
  return [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'bNbrPorts', value: v.getUint8(2).toString() },
    {
      name: 'wHubCharacteristics',
      value: toHexByte(chars),
      children: [
        {
          name: 'Power Switching',
          value: HUB_CHAR_POWER_MODE[powerMode] ?? `Unknown (${powerMode})`,
        },
        { name: 'Compound Device', value: HUB_CHAR_COMPOUND[compound] ?? `Unknown (${compound})` },
        {
          name: 'Over-current Protection',
          value: HUB_CHAR_OC_MODE[ocMode] ?? `Unknown (${ocMode})`,
        },
      ],
    },
    { name: 'bPwrOn2PwrGood', value: `${v.getUint8(5) * 2} ms` },
    { name: 'bHubContrCurrent', value: `${v.getUint8(6)} mA` },
  ];
}

function parseInterfaceAssociationDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  return [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
    { name: 'bFirstInterface', value: v.getUint8(2).toString() },
    { name: 'bInterfaceCount', value: v.getUint8(3).toString() },
    { name: 'bFunctionClass', value: USB_CLASS_CODES[v.getUint8(4)] ?? toHexByte(v.getUint8(4)) },
    { name: 'bFunctionSubClass', value: toHexByte(v.getUint8(5)) },
    { name: 'bFunctionProtocol', value: toHexByte(v.getUint8(6)) },
    { name: 'iFunction', value: v.getUint8(7).toString() },
  ];
}

function parseStringDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  const fields: DescriptorField[] = [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
  ];
  // String data is UTF-16LE starting at offset 2
  if (raw.length > 2) {
    const str = utf16leDecoder.decode(raw.slice(2));
    fields.push({ name: 'bString', value: str });
  }
  return fields;
}

function parseGenericDescriptor(raw: Uint8Array): DescriptorField[] {
  const v = dv(raw);
  const fields: DescriptorField[] = [
    { name: 'bLength', value: v.getUint8(0).toString() },
    { name: 'bDescriptorType', value: toHexByte(v.getUint8(1)) },
  ];
  if (raw.length > 2) {
    fields.push({ name: 'Data', value: toHex(raw.slice(2)) });
  }
  return fields;
}

// ============================================================
// Main parser
// ============================================================

function parseDescriptorFields(raw: Uint8Array): DescriptorField[] {
  const type = raw[1];
  switch (type) {
    case 0x01:
      return parseDeviceDescriptor(raw);
    case 0x02:
      return parseConfigurationDescriptor(raw);
    case 0x03:
      return parseStringDescriptor(raw);
    case 0x04:
      return parseInterfaceDescriptor(raw);
    case 0x05:
      return parseEndpointDescriptor(raw);
    case 0x0b:
      return parseInterfaceAssociationDescriptor(raw);
    case 0x21:
      return parseHidDescriptor(raw);
    case 0x24:
      return parseHubDescriptor(raw);
    default:
      return parseGenericDescriptor(raw);
  }
}

export function parseDescriptors(raw: Uint8Array): ParsedDescriptor[] {
  const v = dv(raw);
  const descriptors: ParsedDescriptor[] = [];
  let offset = 0;
  let index = 0;
  while (offset < raw.length) {
    const length = v.getUint8(offset);
    if (!length || offset + length > raw.length) break;
    const type = v.getUint8(offset + 1);
    const slice = raw.slice(offset, offset + length);
    descriptors.push({
      index,
      type,
      typeName:
        USB_DESCRIPTOR_TYPES[type] ?? `0x${type.toString(16).padStart(2, '0').toUpperCase()}`,
      length,
      raw: slice,
      fields: parseDescriptorFields(slice),
    });
    offset += length;
    index++;
  }
  return descriptors;
}

export function parseUsbDescriptors(base64: string): ParsedDescriptor[] {
  try {
    return parseDescriptors(decodeBase64(base64));
  } catch {
    return [];
  }
}

export { toHex, toHexByte, decodeBase64 };
