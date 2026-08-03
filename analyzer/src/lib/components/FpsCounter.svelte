<script lang="ts">
  import { AnimationFrames } from 'runed';

  /**
   * Smoothing factor for the exponential moving average.
   * 0 < alpha <= 1 — lower = smoother/slower to respond, higher = more responsive/noisier.
   */
  let {
    alpha = 0.1,
    decimals = 0,
    class: className = '',
  }: {
    alpha?: number;
    decimals?: number;
    class?: string;
  } = $props();

  let smoothed = $state(0);
  let displayed = $state(0);
  let lastUpdate = 0;

  const raf = new AnimationFrames(({ delta, timestamp }) => {
    if (delta <= 0) return;
    const instant = 1000 / delta;
    smoothed = smoothed === 0 ? instant : alpha * instant + (1 - alpha) * smoothed;
    if (timestamp - lastUpdate >= 500) {
      displayed = smoothed;
      lastUpdate = timestamp;
    }
  });

  let fps = $derived(Math.round(displayed * 10 ** decimals) / 10 ** decimals);
</script>

<span class={className}>{fps} FPS</span>
