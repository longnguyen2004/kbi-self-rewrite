import tippy, { type Instance as TippyInstance, type Props as TippyProps } from 'tippy.js';

/**
 * A thin wrapper around a single tippy.js instance attached to a chart's
 * SVG element. The wrapper only owns the tippy lifecycle and content —
 * each chart is responsible for its own hit-testing (e.g. a d3-quadtree)
 * and any SVG overlay elements (crosshair, dot, etc.).
 *
 * The tooltip follows the pointer: callers pass a screen-space anchor
 * point on every `show()` so tippy can position itself relative to the
 * cursor rather than the (large) SVG bounding box.
 */
export function createChartTooltip(target: Element, initialContent: string = '') {
  let instance: TippyInstance | undefined;
  let currentContent: string | null = null;

  function ensureInstance() {
    if (instance) return instance;
    instance = tippy(target, {
      content: initialContent,
      trigger: 'manual',
      // Position relative to the virtual reference point we feed in
      // show(), not the target's bounding box.
      getReferenceClientRect: null,
      placement: 'top',
      offset: [0, 12],
      duration: 0,
      hideOnClick: false,
      interactive: false,
      allowHTML: true,
      appendTo: () => document.body,
    });
    return instance;
  }

  /**
   * Show the tooltip with the given HTML content, anchored at the given
   * screen-space (clientX/clientY) point.
   */
  function show(clientX: number, clientY: number, content: string) {
    const inst = ensureInstance();
    if (currentContent !== content) {
      inst.setContent(content);
      currentContent = content;
    }
    // Provide a zero-size virtual reference rect at the cursor so tippy
    // positions the tooltip above the pointer.
    inst.setProps({
      getReferenceClientRect: () => ({
        width: 0,
        height: 0,
        top: clientY,
        bottom: clientY,
        left: clientX,
        right: clientX,
        x: clientX,
        y: clientY,
        toJSON: () => ({}),
      }) as DOMRect,
    });
    inst.show();
  }

  function hide() {
    instance?.hide();
  }

  function destroy() {
    instance?.destroy();
    instance = undefined;
  }

  return { show, hide, destroy };
}
