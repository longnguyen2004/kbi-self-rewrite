import { fft_real_f32 } from "@lng2004/rustfft-shim";

declare var self: DedicatedWorkerGlobalScope;

export type Request = {
    key: number,
    data: number[]
}
export type Response = {
    key: number,
    result: Float32Array
}

self.onmessage = (msg: MessageEvent<Request>) => {
    const { data } = msg;
    const data_typed = new Float32Array(data.data);
    const fft = fft_real_f32(data_typed);
    self.postMessage({
        key: data.key,
        result: fft
    } as Response, [fft.buffer]);
}
