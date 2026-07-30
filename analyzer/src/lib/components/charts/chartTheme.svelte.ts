import { mode } from 'mode-watcher';

export type ChartTheme = {
  grid: string;
  axis: string;
  text: string;
  background: string;
};

export function useChartTheme(): () => ChartTheme {
  return () => {
    const isDark = mode.current === 'dark';
    return {
      grid: isDark ? 'rgba(255, 255, 255, 0.25)' : 'rgba(0, 0, 0, 0.1)',
      axis: isDark ? 'rgba(255, 255, 255, 0.4)' : 'rgba(0, 0, 0, 0.3)',
      text: isDark ? 'rgba(255, 255, 255, 0.85)' : 'rgba(0, 0, 0, 0.85)',
      background: isDark ? 'oklch(0.205 0 0)' : 'oklch(1 0 0)',
    };
  };
}
