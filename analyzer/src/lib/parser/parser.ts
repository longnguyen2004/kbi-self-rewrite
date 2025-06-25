import { resultSchema } from "$lib/validator/validator";
import * as v from "valibot";
import { parse_kbi_legacy } from "./parser_kbi_legacy";

export async function parseKbiResult(file: File)
{
    if (file.type === "application/json")
    {
        return v.parse(resultSchema, JSON.parse(await file.text()));
    }
    const bytes = new Uint8Array(await file.arrayBuffer());
    let result;
    if ((result = parse_kbi_legacy(bytes)))
        return result;
    return undefined;
}