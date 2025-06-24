import type { Chart, ChartType, Plugin, Color, GridLineOptions, TickOptions, BorderOptions } from "chart.js";
import type { PartialDeep } from "type-fest";
import { mode } from "mode-watcher";

const stopWatcher = new WeakMap<Chart, () => void>();

type LightDarkColor<T> = Partial<Record<'light' | 'dark', T>>;
type ThemeChangerOptions = PartialDeep<{
    enabled: boolean,

    backgroundColor: LightDarkColor<Color>,
    borderColor: LightDarkColor<Color>,
    color: LightDarkColor<Color>,

    scales: {
        [key: string]: {
            border: {
                color: LightDarkColor<BorderOptions["color"]>
            }
            grid: {
                color: LightDarkColor<GridLineOptions["color"]>,
                tickColor: LightDarkColor<GridLineOptions["tickColor"]>
            },
            ticks: {
                color: LightDarkColor<TickOptions["color"]>
            }
        }
    }
}>

declare module 'chart.js' {
    interface PluginOptionsByType<TType extends ChartType> {
        [ThemeChanger.id]: ThemeChangerOptions
    }
}

export const ThemeChanger = {
    id: "themeChanger" as const,
    start(chart, args, options) {
        stopWatcher.set(chart, $effect.root(() => {
            $effect.pre(() => {
                mode.current;
                chart.update();
            })
        }));
    },
    stop(chart) {
        stopWatcher.get(chart)?.();
        stopWatcher.delete(chart);
    },
    beforeUpdate(chart, args, options: ThemeChangerOptions) {
        const theme = mode.current === "dark" ? "dark" : "light";
        if (options.backgroundColor)
            chart.options.backgroundColor = options.backgroundColor[theme];
        if (options.borderColor)
            chart.options.borderColor = options.borderColor[theme];
        if (options.color)
            chart.options.color = options.color?.[theme];

        if (options.scales && chart.options.scales)
        {
            for (const id in chart.options.scales)
            {
                if (!chart.options.scales[id] || !options.scales[id])
                    continue;
                if ("border" in chart.options.scales[id] && chart.options.scales[id].border)
                {
                    if (options.scales[id].border?.color)
                        chart.options.scales[id].border.color = options.scales[id].border.color[theme];
                }
                if (chart.options.scales[id].grid)
                {
                    if (options.scales[id].grid?.color)
                        chart.options.scales[id].grid.color = options.scales[id].grid.color[theme];
                    if (options.scales[id].grid?.tickColor)
                        chart.options.scales[id].grid.tickColor = options.scales[id].grid.tickColor[theme];
                }
                if (chart.options.scales[id].ticks)
                {
                    if (options.scales[id].ticks?.color)
                        chart.options.scales[id].ticks.color = options.scales[id].ticks.color[theme];
                }
            }
        }
    },
} satisfies Plugin;
