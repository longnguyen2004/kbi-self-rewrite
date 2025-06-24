import { LinearScale } from "chart.js";
import type { Tick, LinearScaleOptions, ScaleTypeRegistry } from "chart.js";

declare module "chart.js" {
    interface CartesianScaleTypeRegistry {
        [QuantizedTickLinearScale.id]: {
            options: LinearScaleOptions
        }
    }
}

export class QuantizedTickLinearScale extends LinearScale
{
    static id = "quantizedTickLinear" as const
    
    public buildTicks(): Tick[] {
        const { stepSize } = this.options.ticks;
        if (!stepSize)
            throw new Error("Step size must be defined for quantized tick axis to work");
        const { min, max } = this;
        const quantizedMin = Math.floor(min / stepSize + 1) * stepSize;
        const quantizedMax = Math.ceil(max / stepSize - 1) * stepSize;
        let ticks: Tick[] = [{ value: min }];
        for (let i = quantizedMin; i <= quantizedMax; i += stepSize)
            ticks.push({ value: i });
        ticks.push({ value: max });
        return ticks;
    }
}
