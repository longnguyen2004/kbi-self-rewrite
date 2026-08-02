import { quadtree, type Quadtree } from 'd3-quadtree';

/**
 * Shared nearest-neighbour helper for chart tooltips. Each chart maintains
 * its own `QuadtreeSnap` over the data points it wants to snap to (line
 * samples, bar centres, …) and calls `find()` on every pointer move.
 *
 * This deduplicates the quadtree construction + lookup that was previously
 * inlined in both the line charts (DiffChart / FreqChart) and the bar chart
 * (InputTimeline).
 */
export class QuadtreeSnap<T> {
  private tree: Quadtree<T> | undefined;
  private readonly xOf: (d: T) => number;
  private readonly yOf: (d: T) => number;

  constructor(xOf: (d: T) => number, yOf: (d: T) => number) {
    this.xOf = xOf;
    this.yOf = yOf;
  }

  /** Rebuild the tree from scratch. Call whenever the underlying data or
   *  the pixel mapping (scales) change. */
  set(data: Iterable<T>): void {
    this.tree = quadtree<T>()
      .x(this.xOf)
      .y(this.yOf)
      .addAll(Array.from(data) as T[]);
  }

  /** Returns the nearest datum to `(x, y)`, or `undefined` if the tree is
   *  empty. An optional `radius` limits the search to points within that
   *  pixel distance (pass `Infinity` for an unbounded nearest). */
  find(x: number, y: number, radius: number = Infinity): T | undefined {
    if (!this.tree || this.tree.size() === 0) return undefined;
    // d3-quadtree's `find(x, y, radius)` already returns the nearest point
    // within `radius`; it falls back to the global nearest when radius is
    // omitted/Infinity.
    return this.tree.find(x, y, radius) as T | undefined;
  }

  clear(): void {
    this.tree = undefined;
  }
}
