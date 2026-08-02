<script lang="ts" generics="T">
  /**
   * A reusable chart tooltip built on tippy.js.
   *
   * Unlike the old `createChartTooltip` factory, this is a real Svelte
   * component: callers declare it in markup and supply the tooltip body
   * through the `children` snippet, so content is authored in Svelte
   * (with reactivity, components, etc.) instead of raw HTML strings.
   *
   * The component also owns all pointer-event handling that drives the
   * tooltip: it attaches `pointermove` / `pointerleave` listeners to
   * `target`, converts the pointer position to plot-local coordinates
   * (using `margin`), runs the shared `QuadtreeSnap` hit-test, and — when a
   * datum is found — positions the tooltip at the screen-space anchor
   * returned by `toAnchor`. The currently hit datum is exposed to the
   * `children` snippet as its first parameter so the chart can render it.
   *
   * This keeps every chart's pointer logic identical and out of the chart
   * components themselves.
   */
  import tippy, { type Instance as TippyInstance } from 'tippy.js';
  import { onMount, onDestroy } from 'svelte';
  import { QuadtreeSnap } from './quadtreeSnap';

  type Anchor = { x: number; y: number } | null;

  type Props = {
    /** A stable element to listen for pointer events on and to attach the
     *  tippy instance to. Usually the chart's <svg> root. The tooltip is
     *  positioned via a virtual reference rect, so this element's box is
     *  not used for placement. */
    target: SVGSVGElement;
    /** Plot margin used to convert pointer client coords → plot-local
     *  coords (`mx = clientX - rect.left - margin.left`, etc.). */
    margin: { top: number; right: number; bottom: number; left: number };
    /** Shared nearest-neighbour hit-test over the chart's data. The chart
     *  owns the instance (its accessors close over the chart's scales) and
     *  rebuilds it on every render. */
    snap: QuadtreeSnap<T>;
    /** Maps the hit datum's plot-local pixel position to a screen-space
     *  anchor (clientX/clientY). Receives the datum plus the plot origin
     *  (`rect.left + margin.left`, `rect.top + margin.top`) so the chart
     *  only needs to add its scale-derived pixel offsets. Returning `null`
     *  hides the tooltip for that datum. */
    toAnchor: (datum: T, plotOriginX: number, plotOriginY: number) => Anchor;
    /** Optional placement override (defaults to 'top'). */
    placement?: 'top' | 'bottom' | 'left' | 'right';
    /** Optional pixel offset `[skid, distance]` (defaults to `[0, 12]`). */
    offset?: [number, number];
    /** Tooltip body, authored as a Svelte snippet. Receives the hit datum
     *  (or `null` when the pointer is over empty space / has left). */
    children: import('svelte').Snippet<[T | null]>;
  };

  let {
    target,
    margin,
    snap,
    toAnchor,
    placement = 'top',
    offset = [0, 12],
    children,
  }: Props = $props();

  let instance: TippyInstance | undefined;
  // The host <div> is rendered by Svelte (so the snippet's reactive effects
  // are owned by this component) and handed to tippy as its `content`.
  // tippy relocates the node into its popper on show(), but Svelte's snippet
  // bindings remain valid because they are tracked by the component, not the
  // DOM parent.
  let host: HTMLDivElement;

  // Tooltip state: anchor in screen (client) space + the hit datum. `null`
  // anchor hides the tooltip. Both are reactive so the `children` snippet
  // re-renders when the pointer moves to a different datum.
  let anchor: Anchor = $state(null);
  let datum: T | null = $state(null);

  onMount(() => {
    instance = tippy(target, {
      content: host,
      trigger: 'manual',
      getReferenceClientRect: null,
      placement,
      offset,
      duration: 0,
      hideOnClick: false,
      interactive: false,
      allowHTML: true,
      appendTo: () => document.body,
    });

    const onPointerMove = (event: PointerEvent) => {
      const rect = target.getBoundingClientRect();
      // Convert client coords to plot-local by subtracting the SVG rect
      // origin and the plot margin.
      const mx = event.clientX - rect.left - margin.left;
      const my = event.clientY - rect.top - margin.top;
      const nearest = snap.find(mx, my);
      if (nearest === undefined) {
        anchor = null;
        datum = null;
        return;
      }
      const plotOriginX = rect.left + margin.left;
      const plotOriginY = rect.top + margin.top;
      const a = toAnchor(nearest, plotOriginX, plotOriginY);
      anchor = a;
      datum = a === null ? null : nearest;
    };
    const onPointerLeave = () => {
      anchor = null;
      datum = null;
    };
    target.addEventListener('pointermove', onPointerMove);
    target.addEventListener('pointerleave', onPointerLeave);

    return () => {
      target.removeEventListener('pointermove', onPointerMove);
      target.removeEventListener('pointerleave', onPointerLeave);
    };
  });

  onDestroy(() => {
    instance?.destroy();
    instance = undefined;
  });

  // React to anchor changes: show/hide + reposition.
  $effect(() => {
    if (!instance) return;
    if (anchor === null) {
      instance.hide();
      return;
    }
    const { x, y } = anchor;
    instance.setProps({
      getReferenceClientRect: () =>
        ({
          width: 0,
          height: 0,
          top: y,
          bottom: y,
          left: x,
          right: x,
          x,
          y,
          toJSON: () => ({}),
        }) as DOMRect,
    });
    instance.show();
  });

  // Keep placement / offset in sync if they change.
  $effect(() => {
    instance?.setProps({ placement, offset });
  });
</script>

<!-- Render the snippet into a host <div>. tippy moves this node into its
     popper on creation (in onMount, before the browser paints, so it never
     flashes in the page). tippy's popper wrapper controls show/hide via its
     own `data-state`/CSS, so the host itself needs no visibility styling. -->
<div bind:this={host}>
  {@render children(datum)}
</div>
