
export type ResultType = 'ok' | 'error' | 'crashed' | 'timeout' | 'transpile_failed';

export interface TestResult {
    file: string;
    output: string;
    code: number | null;
    signal: string | null;
    resultType: ResultType;
    nodeFailed: boolean;
}