import type { ZoomBehavior, ZoomTransform } from 'd3-zoom';

export type SyncOptions = {
  axis: 'x' | 'y';
};

export type Syncable = {
  zoom: ZoomBehavior<SVGGElement, unknown>;
  applyTransform: (transform: ZoomTransform) => void;
};

/**
 * Synchronizes zoom/pan across multiple d3-based charts along a shared axis.
 * When one chart zooms/pans, the others receive the same ZoomTransform.
 */
export class D3ZoomSynchronizer {
  private _charts = new Map<Syncable, SyncOptions>();
  private _suppress = false;

  add(chart: Syncable, options: SyncOptions) {
    this._charts.set(chart, options);
    chart.zoom.on('zoom.sync', (event) => {
      if (this._suppress) return;
      this._suppress = true;
      try {
        for (const [dst, opts] of this._charts.entries()) {
          if (dst === chart) continue;
          if (opts.axis !== options.axis) continue;
          dst.applyTransform(event.transform);
        }
      } finally {
        this._suppress = false;
      }
    });
  }

  remove(chart: Syncable) {
    this._charts.delete(chart);
  }

  clear() {
    this._charts.clear();
  }
}
