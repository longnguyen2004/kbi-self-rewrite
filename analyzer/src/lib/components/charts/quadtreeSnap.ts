import { quadtree, type Quadtree } from 'd3-quadtree';

/**
 * Shared nearest-neighbour helper for chart tooltips. Each chart maintains
 * its own `QuadtreeSnap` over the data points it wants to snap to (line
 * samples, bar centres, …) and calls `find()` on every pointer move.
 *
 * This deduplicates the quadtree construction + lookup that was previously
 * inlined in both the line charts (DiffChart / FreqChart) and the bar chart
 * (InputTimeline).
 *
 * Lazy rebuild: `set(data)` only stashes the data and marks the tree dirty
 * — it does NOT construct the quadtree. The tree is built on the first
 * `find()` after a `set()` (i.e. on the first pointer move over the chart),
 * and reused for subsequent queries until the next `set()`. This avoids
 * rebuilding the tree on every render/zoom/resize when the pointer is not
 * over the chart (the common case during real-time recording).
 */
export class QuadtreeSnap<T> {
  private tree: Quadtree<T> | undefined;
  private data: T[] | undefined;
  private dirty = false;
  private readonly xOf: (d: T) => number;
  private readonly yOf: (d: T) => number;

  constructor(xOf: (d: T) => number, yOf: (d: T) => number) {
    this.xOf = xOf;
    this.yOf = yOf;
  }

  /** Stage new data for the tree. Cheap: just stores the data and marks the
   *  tree dirty so the next `find()` rebuilds it. The actual quadtree is
   *  constructed lazily on the first query. */
  set(data: Iterable<T>): void {
    this.data = Array.from(data) as T[];
    this.dirty = true;
  }

  /** Build the tree from the staged data if it is dirty. Called lazily by
   *  `find()`; safe to call when not dirty (no-op). */
  private ensureTree(): void {
    if (!this.dirty) return;
    this.dirty = false;
    if (this.data === undefined || this.data.length === 0) {
      this.tree = undefined;
      return;
    }
    this.tree = quadtree<T>()
      .x(this.xOf)
      .y(this.yOf)
      .addAll(this.data);
  }

  /** Returns the nearest datum to `(x, y)`, or `undefined` if the tree is
   *  empty. An optional `radius` limits the search to points within that
   *  pixel distance (pass `Infinity` for an unbounded nearest). Triggers a
   *  lazy rebuild if the staged data has changed since the last build. */
  find(x: number, y: number, radius: number = Infinity): T | undefined {
    this.ensureTree();
    if (!this.tree || this.tree.size() === 0) return undefined;
    // d3-quadtree's `find(x, y, radius)` already returns the nearest point
    // within `radius`; it falls back to the global nearest when radius is
    // omitted/Infinity.
    return this.tree.find(x, y, radius) as T | undefined;
  }

  /** Drop the staged data and the tree. Call on chart teardown. */
  clear(): void {
    this.data = undefined;
    this.tree = undefined;
    this.dirty = false;
  }
}
