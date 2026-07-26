export function toHex(bytes: Uint8Array): string {
  return Array.from(bytes)
    .map((b) => b.toString(16).padStart(2, '0').toUpperCase())
    .join(' ');
}

export function toHexByte(b: number, minLength = 2): string {
  return '0x' + b.toString(16).padStart(minLength, '0').toUpperCase();
}