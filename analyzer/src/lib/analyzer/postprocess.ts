export type PostprocessOptions = {
    lowCut: boolean
}

export function postprocess(arr: number[], { lowCut }: PostprocessOptions)
{
    let result = arr;
    if (lowCut)
    {
        result = Array.from(arr);
        for (let i = 0; i < Math.min(70, result.length); i++)
            result[i] /= 1 + Math.exp(-(i - 25) / 4.0);
    }
    return result;
}