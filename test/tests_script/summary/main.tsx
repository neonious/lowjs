import * as React from 'react';
import * as ReactDOM from 'react-dom';
import { TestResultsComp } from './testResults';
import { TestResult } from './result';
import data from './data.json';

let prio = {
    ok: 0,
    transpile_failed: 1,
    error: 2,
    timeout: 3,
    crashed: 4
}
let results = data as TestResult[];
results = results.sort((r1, r2) => prio[r1.resultType] > prio[r2.resultType] ? -1 : prio[r1.resultType] === prio[r2.resultType] ? r1.file < r2.file ? -1 : 1 : 1);

ReactDOM.render(<TestResultsComp results={results} />, document.getElementById('app'))