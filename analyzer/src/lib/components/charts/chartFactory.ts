import { Chart, LineController, LineElement, PointElement, LinearScale, Tooltip, scales, type Tick } from "chart.js";
import { ThemeChanger } from "./themeChanger.svelte";
import Zoom from "chartjs-plugin-zoom";

class QuantizedTickLinearScale extends scales.LinearScale
{
    static id = "quantizedTickLinear"
    
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

Chart.register(LineController, LineElement, PointElement, LinearScale, Tooltip, Zoom);
Chart.register(QuantizedTickLinearScale);
Chart.register(ThemeChanger);

export function createChart(canvas: HTMLCanvasElement) {
    return new Chart(canvas, {
        type: "line",
        data: {
            datasets: [],
        },
        options: {
            elements: {
                point: {
                    radius: 0,
                },
            },
            plugins: {
                decimation: {
                    enabled: true
                },
                zoom: {
                    pan: {
                        enabled: true,
                        mode: "x",
                    },
                    zoom: {
                        wheel: {
                            enabled: true,
                        },
                        pinch: {
                            enabled: true,
                        },
                        mode: "x",
                        scaleMode: "x",
                    },
                    limits: {
                        x: {
                            min: "original",
                            max: "original",
                        },
                    },
                },
                themeChanger: {
                    enabled: true
                }
            },
            interaction: {
                intersect: false,
            },
            animation: false,
            spanGaps: true,
            maintainAspectRatio: false,
        },
    });
}