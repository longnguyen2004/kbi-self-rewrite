import { Chart, Colors, BarController, BarElement, LineController, LineElement, PointElement, CategoryScale, LinearScale, Tooltip, Decimation, type ChartType, type ChartOptions, type ChartConfiguration } from "chart.js";
import { QuantizedTickLinearScale } from "./quantizedTickLinearScale";
import { ThemeChanger } from "./themeChanger.svelte";
import Zoom from "chartjs-plugin-zoom";

Chart.register(Colors, BarController, BarElement, LineController, LineElement, PointElement, CategoryScale, LinearScale, Tooltip, Decimation, Zoom);
Chart.register(QuantizedTickLinearScale);
Chart.register(ThemeChanger);

export function createChart<TType extends ChartType, TData>(canvas: HTMLCanvasElement, type: TType) {
    return new Chart<TType, TData>(canvas, {
        type,
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
            ...(type === "line" ? { spanGaps: true } : {}),
            maintainAspectRatio: false,
        } as ChartOptions<TType>
    });
}