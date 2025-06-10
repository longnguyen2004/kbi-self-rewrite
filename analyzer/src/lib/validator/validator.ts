import * as v from "valibot";

export const deviceSchema = v.object({
    name: v.string(),
    vid: v.pipe(
        v.number(),
        v.minValue(0),
        v.maxValue(0xFFFF)
    ),
    pid: v.pipe(
        v.number(),
        v.minValue(0),
        v.maxValue(0xFFFF)
    )
});
export type Device = v.InferOutput<typeof deviceSchema>;

export const inputSchema = v.object({
    timestamp: v.pipe(
        v.number(),
        v.minValue(0)
    ),
    pressed: v.boolean(),
    code: v.number()
});
export type Input = v.InferOutput<typeof inputSchema>

export const resultSchema = v.object({
    info: v.object({
        os_name: v.string(),
        os_ver: v.string(),
        arch: v.string(),
        safe_mode: v.optional(v.boolean()),
        backend: v.string()
    }),
    time: v.string(),
    devices: v.record(v.string(), deviceSchema),
    inputs: v.record(v.string(), v.array(inputSchema))
});
export type Result = v.InferOutput<typeof resultSchema>;
