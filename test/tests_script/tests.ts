require('source-map-support').install();
import * as fs from 'fs-extra';
import * as os from 'os';
import execa from 'execa';
import * as path from 'path';
import * as React from 'react';
import { renderToString } from 'react-dom/server';
import globby from 'globby';
import assert from 'assert';
import chalk from 'chalk';
import read from 'fs-readdir-recursive';
import * as yargs from 'yargs';
const babel = require("babel-core");
import { TestResult, ResultType } from './summary/result';

const tmpResults = path.resolve(os.tmpdir(), 'results');

/* todo

In no particular order:

* execute tests in parallel that can safely be executed in parallel (e.g. tests in compiled/node/parallel)
* log diagnostics/output into summary file if es5 transformation fails
* general optimization
    * node -> lowjs test flow seems clunky and is slow, we execute most test 2 times!
    * do more in parallel
* way to open test file from summary file directly, maybe even utilizing sourcemaps
* also support tests written in typescript
* should exit process on unhandled promise, is that the case?
* parallel/test-util-inspect.js transforms of node tests (remove last 5 rows)
* all console.error in red
* do not hardcode nodejs repo commit id

*/

process.on('unhandledRejection', (reason, p) => {
    console.error(chalk.white.bgRed('Unhandled Rejection at: Promise' + p, 'reason:', reason));
    console.error(reason);
});

const blacklist = new Set(['node/tick-processor/test-tick-processor-builtin.js', 'node/parallel/test-vm-sigint.js']); // somehow test runs timeout with these files, without being able to kill them

const argv = yargs
    .strict()
    .command('run [files..]', 'Run tests.', yargs => {
        return yargs.positional('files', {
            describe: 'The files to test.'
        })
    })
    .option('clear', {
        type: 'boolean',
        default: false
    })
    .demandCommand(1, 1)
    .argv;

interface Options2 {
    command: 'run';
    files?: string[];
    clear: boolean;
}
const cmd = argv._[0];
const options = {
    command: cmd,
    files: argv.files.length ? argv.files : undefined,
    clear: argv.clear
} as Options2;
const { files: optsFiles, command } = options;

console.log('Options', JSON.stringify(options, null, 4));

const PROJ_DIR = path.resolve(__dirname, '..', '..');
assert(fs.pathExistsSync(path.resolve(PROJ_DIR, 'Makefile')))
const NODEJS_REPO = path.resolve(PROJ_DIR, 'test', 'node');
const NODEJS_REPO_TESTS = path.resolve(NODEJS_REPO, 'test');
const TESTS_COMPILED = path.resolve(PROJ_DIR, 'test', 'compiled');
const NODETESTS_COMPILED_DIR = path.resolve(TESTS_COMPILED, 'node');

const OTHERTESTS = path.resolve(PROJ_DIR, 'test', 'tests');

async function createSummaryFile(results: TestResult[], file: string) {
    const json = JSON.stringify(results, null, 4);
    await fs.writeFile(path.resolve(__dirname, 'summary', 'data.json'), json);
    await execa('npm', ['run', 'build_summary'], { cwd: __dirname });
    await fs.move(path.resolve(__dirname, 'summary.html'), file);
}

async function fileOlderThan(file: string, otherFile: string) {
    if (!await fs.pathExists(file) || !await fs.pathExists(otherFile)) {
        return false;
    }
    const stat = await fs.stat(file);
    const otherStat = await fs.stat(otherFile);
    return stat.mtime < otherStat.mtime;
}

async function runTestsInternal() {
    let files: string[];
    if (!optsFiles) {
        let otherFiles = await globby(['**/test-*.js', '**/test.js', '!node'], { cwd: TESTS_COMPILED });
        otherFiles = otherFiles.map(f => path.resolve(TESTS_COMPILED, f))
        assert(otherFiles.length);
        let nodeTestFiles = await globby(['**/test-*.js', '**/test.js'], { cwd: NODETESTS_COMPILED_DIR });
        nodeTestFiles = nodeTestFiles.map(f => path.resolve(NODETESTS_COMPILED_DIR, f));
        assert(nodeTestFiles.length);
        files = nodeTestFiles.concat(otherFiles);
    } else {
        files = await globby(optsFiles, { cwd: TESTS_COMPILED });
        files = files.map(f => path.resolve(TESTS_COMPILED, f));
        if (!files.length) {
            console.error('No files found matching your criteria!');
            process.exit(1);
        }
    }

    assert(files.length);
    files.forEach(f => assert(path.isAbsolute(f)));

    const parallel = false;
    const finished: TestResult[] = [];
    async function runAbs(abs: string) {
        const relFile = path.relative(TESTS_COMPILED, abs);
        const resultFile = path.resolve(tmpResults, relFile);
        if (!await fs.pathExists(resultFile)) {
            if (blacklist.has(relFile)) {
                return null;
            }
            console.log('====================================================');
            console.log('Testing file', relFile, ` (${finished.length++}/${files.length})`);
            const opts = {
                cwd: path.dirname(abs),
                reject: false,
                timeout: 15000,
                killSignal: 'SIGKILL'
            };
            let output = '';
            let proc = execa('node', [abs], opts);
            proc.stdout.on('data', (data: string) => {
                output += data;
            })
            proc.stderr.on('data', (data: string) => {
                output += data;
            });
            let result = await proc;
            let nodeFailed = false;
            if (result.signal || result.timedOut || result.code !== 0) {
                nodeFailed = true;
                console.error(chalk.red('NODE FAILED'));
                process.stdout.write(output);
            } else {
                proc = execa(path.resolve(PROJ_DIR, 'bin/low'), [abs], opts);
                proc.stdout.pipe(process.stdout);
                proc.stderr.pipe(process.stderr);
                output = '';
                proc.stdout.on('data', (data: string) => {
                    output += data;
                })
                proc.stderr.on('data', (data: string) => {
                    output += data;
                });
                result = await proc;
            }
            const crash = !!result.signal && result.signal !== 'SIGKILL';
            const { signal, timedOut: to, code, stdout, stderr, killed, cmd } = result;
            const resultType = result.timedOut ? 'timeout' : result.signal ? 'crashed' : code !== 0 ? 'error' : 'ok';
            const col = resultType === 'ok' ? chalk.green : chalk.red;
            console.log('Test ', col(resultType.toUpperCase()), ': ', relFile);
            const testResult = {
                file: abs,
                code: result.timedOut || crash ? null : result.code,
                output,
                signal: crash ? result.signal : null,
                resultType,
                nodeFailed
            } as TestResult;
            await fs.mkdirp(path.dirname(resultFile));
            await fs.writeFile(resultFile, JSON.stringify(testResult));

            return testResult;
        } else {
            return JSON.parse((await fs.readFile(resultFile)).toString()) as TestResult;
        }
    }
    if (parallel) {
        const MAX = 5;
        let running = 0;
        const pending: string[] = [];
        async function runItem(abs: string) {
            running++;
            const testResult = await runAbs(abs);
            testResult && finished.push(testResult);
            running--;
            const next = pending.shift();
            next && await runItem(next);
        }
        const all = [];
        for (const abs of files) {
            if (running >= MAX) {
                pending.push(abs);
            } else {
                all.push(runItem(abs));
            }
        }
        await Promise.all(all);
    } else {
        for (const abs of files) {
            const testResult = await runAbs(abs);
            testResult && finished.push(testResult);
        }
    }
    return finished;
}

async function checkoutIfNotCheckouted() {
    if (!await fs.pathExists(NODEJS_REPO)) {
        console.log(`Cloning node repository into ${NODEJS_REPO}, this may take a while...`);
        await fs.mkdirp(NODEJS_REPO);
        await execa('git', ['clone', 'https://github.com/nodejs/node.git', NODEJS_REPO], { stdio: 'inherit' });
        await execa('git', ['branch', 'lowjs_test', '44d04a8c01dba1d7e4a9c9d9a4415eeacc580bf4'], { cwd: NODEJS_REPO });
        await execa('git', ['checkout', 'lowjs_test'], { cwd: NODEJS_REPO });
    }
}

function babelTransform(text: string) {

    const ast = require('babylon').parse(text, { allowReturnOutsideFunction: true, sourceType: "module" });
    const babelResult = babel.transformFromAst(ast, text, {
        presets: [
            "es2015",
            "stage-3"
        ]
    });
    const { code: compiled } = babelResult;
    assert(compiled);
    return compiled;
}
const runtimesource = (async () => {
    const runtimesource = (await fs.readFile(path.resolve(PROJ_DIR, 'node_modules', 'regenerator-runtime', 'runtime.js'))).toString();
    return runtimesource;
})()
async function transformNodeSource(relFile: string, source: string): Promise<string | false> {
    if (source.indexOf('// Flags:') !== -1) {
        return false;// do not include these files because they require cmd line flags to be set
    }
    return source.replace(/\bcatch\s\{/g, 'catch(e){')
        .replace('exports.buildType = process.config.target_defaults.default_configuration;', '')
        .replace("assert.fail(`Unexpected global(s) found: ${leaked.join(', ')}`);", ''); // else babel global is leaked
}

async function runTests() {

    if (options.clear) {
        await fs.emptyDir(tmpResults);
    }

    await checkoutIfNotCheckouted();

    async function transformFiles(from: string, to: string, transform: (relFile: string, source: string) => Promise<string | false>) {
        const files = read(from);
        assert(files.length)
        files.forEach(f => assert(!path.isAbsolute(f)));
        const fileErrors = [];
        for (const relFile of files) {
            const file = path.resolve(from, relFile);
            const destFile = path.resolve(to, relFile);
            const doCompile = !await fileOlderThan(file, destFile);
            if (doCompile) {
                if (relFile.toLowerCase().endsWith('.js')) {
                    const source = (await fs.readFile(file)).toString();
                    let compiled: string | false;
                    try {
                        compiled = await transform(relFile, source);
                    } catch (e) {
                        const ex = e as SyntaxError;
                        console.error('============================================');
                        console.error(chalk.red('ERROR COMPILING FILE'));
                        console.error(`File: ${file}:${e.loc.line}`);
                        console.error('Error:');
                        console.error(e);
                        fileErrors.push({
                            file: destFile,
                            output: ex.stack || '',
                            code: null,
                            signal: null,
                            resultType: <ResultType>'transpile_failed',
                            nodeFailed: false
                        });
                        continue;
                    }
                    if (compiled !== false) {
                        if (compiled.indexOf('regeneratorRuntime') !== -1) {
                            compiled = `${await runtimesource}\n${compiled}`;
                        }
                        await fs.mkdirp(path.dirname(destFile));
                        await fs.writeFile(destFile, compiled);
                    }
                } else {
                    await fs.mkdirp(path.dirname(destFile));
                    await fs.copy(file, destFile, { overwrite: true })
                }
            }
        }
        return fileErrors;
    }

    console.log('Transforming and compiled node test files...');
    const transpileErrors = await transformFiles(NODEJS_REPO_TESTS, NODETESTS_COMPILED_DIR, async (relFile, source) => {
        const transformed = await transformNodeSource(relFile, source);
        if (transformed === false)
            return false;
        return babelTransform(transformed)
    });

    console.log('Compiling other test files...');
    await transformFiles(OTHERTESTS, TESTS_COMPILED, async (_, source) => babelTransform(source));

    console.log('Making lowjs...');
    await execa('make', { cwd: PROJ_DIR });

    console.log('Running tests...');
    const finished = await runTestsInternal();
    const results = finished.concat(transpileErrors);

    const ok = results.filter(f => f.resultType === 'ok');
    console.log(`${ok.length} / ${results.length} tests OK.`);
    console.log(`Generating summary file...`);
    await createSummaryFile(results, 'summary.html');
    console.log('The summary file was written to', path.resolve('summary.html'));
    await fs.emptyDir(tmpResults);
}

runTests();
