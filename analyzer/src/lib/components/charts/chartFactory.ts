import { Chart, LineController, LineElement, PointElement, LinearScale, Tooltip } from "chart.js";
import Zoom from "chartjs-plugin-zoom";

Chart.register(LineController, LineElement, PointElement, LinearScale, Tooltip, Zoom);

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