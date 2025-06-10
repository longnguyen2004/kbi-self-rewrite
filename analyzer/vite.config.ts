import tailwindcss from '@tailwindcss/vite';
import wasm from 'vite-plugin-wasm';
import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import * as path from "node:path";

export default defineConfig({
    plugins: [tailwindcss(), svelte(), wasm()],
    worker: {
        plugins: () => [wasm()],
    },
    resolve: {
        alias: {
            $lib: path.resolve("./src/lib")
        }
    }
});
