import type { Chart, ChartType, PluginOptionsByType } from "chart.js"
import type {} from "chartjs-plugin-zoom"

type ZoomPluginOptions = PluginOptionsByType<ChartType>["zoom"]
type ZoomCallback = NonNullable<ZoomPluginOptions["zoom"]>["onZoom"];
type PanCallback = NonNullable<ZoomPluginOptions["pan"]>["onPan"];

export type SyncOptions = {
    axis: ("x" | "y" | (string & {}))[]
}

export class ZoomSynchronizer {
    private _charts = new Map<Chart, SyncOptions>();
    public onZoom: ZoomCallback = ({ chart: srcChart, trigger }) => {
        // do not trigger on api calls to prevent infinite loops
        if (trigger === "api")
            return;
        for (const [dstChart, syncOpts] of this._charts.entries())
        {
            if (dstChart === srcChart)
                continue;
            for (const srcAxis of Object.values(srcChart.scales))
            {
                const { id, axis, min, max } = srcAxis;
                if (!syncOpts.axis.includes(id) && !syncOpts.axis.includes(axis))
                    continue;
                dstChart.zoomScale(id, { min, max });
            }
        }
    }
    public onPan: PanCallback = ({ chart: srcChart }) => {
        // No need to check for API trigger here because pan isn't called again
        for (const [dstChart, syncOpts] of this._charts.entries())
        {
            if (dstChart === srcChart)
                continue;
            for (const srcAxis of Object.values(srcChart.scales))
            {
                const { id, axis, min, max } = srcAxis;
                if (!syncOpts.axis.includes(id) && !syncOpts.axis.includes(axis))
                    continue;
                dstChart.zoomScale(id, { min, max });
            }
        }
    }
    add(chart: Chart, options: SyncOptions)
    {
        this._charts.set(chart, options);
    }
    remove(chart: Chart)
    {
        this._charts.delete(chart);
    }
    clear()
    {
        this._charts.clear();
    }
}
