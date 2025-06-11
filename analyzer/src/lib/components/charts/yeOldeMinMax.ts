export function minmax(arr: number[])
{
    let min = +Infinity;
    let max = -Infinity;
    for (const num of arr)
    {
        if (num < min)
            min = num;
        if (num > max)
            max = num;
    }
    return [min, max];
}