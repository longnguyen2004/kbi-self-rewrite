import { resultSchema } from "$lib/validator/validator";
import * as v from "valibot";

export async function parseKbiResult(file: File)
{
    if (file.type === "application/json")
    {
        return v.parse(resultSchema, JSON.parse(await file.text()));
    }
    return undefined;
}