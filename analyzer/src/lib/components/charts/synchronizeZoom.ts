import type { Chart, ChartType, PluginOptionsByType } from "chart.js"

type ZoomPluginOptions = PluginOptionsByType<ChartType>["zoom"]
type ZoomCallback = NonNullable<ZoomPluginOptions["zoom"]>["onZoom"];
type PanCallback = NonNullable<ZoomPluginOptions["pan"]>["onPan"];

export type SyncOptions = {
    axis: ("x" | "y" | (string & {}))[]
}

export class ZoomSynchronizer {
    private _charts = new Map<Chart, SyncOptions>();
    private _doSync(srcChart: Chart) {
        for (const [dstChart, syncOpts] of this._charts.entries()) {
            if (dstChart === srcChart)
                continue;
            for (const srcAxis of Object.values(srcChart.scales)) {
                const { id, axis, min, max } = srcAxis;
                if (!syncOpts.axis.includes(id) && !syncOpts.axis.includes(axis))
                    continue;
                dstChart.zoomScale(id, { min, max });
            }
        }
    }
    public onZoom: ZoomCallback = ({ chart, trigger }) => {
        // do not trigger on api calls to prevent infinite loops
        if (trigger === "api")
            return;
        this._doSync(chart);
    }
    public onPan: PanCallback = ({ chart }) => {
        // No need to check for API trigger here because pan isn't called again
        this._doSync(chart);
    }
    add(chart: Chart, options: SyncOptions) {
        this._charts.set(chart, options);
    }
    remove(chart: Chart) {
        this._charts.delete(chart);
    }
    clear() {
        this._charts.clear();
    }
}
