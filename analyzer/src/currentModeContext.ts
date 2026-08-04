import { createContext } from "svelte";

export type Mode = 'from-file' | 'from-recorder' | 'recording';

export const [ getCurrentMode, setCurrentMode ] = createContext<() => Mode>();