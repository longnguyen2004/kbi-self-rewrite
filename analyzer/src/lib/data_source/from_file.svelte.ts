import { parseKbiResult } from "$lib/parser/parser";
import type { Result } from "$lib/validator/validator";
import type { DataSource } from ".";

export class FromFileDataSource implements DataSource {
    private _result: Result | undefined = $state.raw();

    constructor() {}

    async parse(file: File) {
        this._result = await parseKbiResult(file);
        return this._result;
    }

    get result() {
        return this._result;
    }
}
