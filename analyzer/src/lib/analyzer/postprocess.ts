export type PostprocessOptions = {
  lowCut: boolean;
};

export function postprocessWithMax(arr: number[], opts: PostprocessOptions) {
  const result = opts.lowCut ? Array.from(arr) : arr;
  if (opts.lowCut) {
    for (let i = 0; i < Math.min(70, result.length); i++)
      result[i] /= 1 + Math.exp(-(i - 25) / 4.0);
  }
  let max = 0;
  for (let i = 0; i < result.length; i++) {
    if (result[i] > max) max = result[i];
  }
  return { data: result, max };
}
