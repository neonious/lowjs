# Running tests

## How to run

Prerequisite:

    npm install
    cd test/tests_script && npm install

It is recommended to run a single test file first, like:

    npm run test -- run node/parallel/test-fs-close.js

In order to run more tests:

    npm run test -- run node/parallel/test-fs-*.js

Glob patterns are interpreted relative to the test/compiled directory that is created/updated when running the test runner. See the section 'How it works' for more information.

Or to run all tests, just do. This can take a very long time:

    npm run test -- run

Results from test files are cached in the os's temp directory until the test runner successfully completes and writes all cached results into the created summary html file, in which case the cache is cleared.

This means that aborting a test run and running anew, will result in the runner fetching the results for completed tests from the cache.

To forcefully clear the cache before running the test runner, use the --clear option, e.g.:

    npm run test -- run --clear

## How it works

The test runner script does the following (pseudocode):

    // First, transform all test files so they can be run in lowjs:
    
    if test/node does not exist:
        clone official node repository to test/node

    nodeTestFiles = test/node/test
    ourCustomTestFiles = test/tests

    for each sourceFile in nodeTestFiles and ourCustomTestFiles, recursively:
        if sourceFile in nodeTestFiles
            destFile = test/compiled/node/<sourceFile>
        else
            destFile = test/compiled/<sourceFile>
        if destFile exists and sourceFile not newer than destFile:
            continue, nothing changed
        if file is javascript:
            if file contains '// Flags:' (tests node command-line switches):
                continue, irrelevant for lowjs for now
            else
                transform sourceFile to es5
                write output to destFile
        else
            copy sourceFile to destFile

    // Of course we need to compile lowjs via make. make will only compile the changed parts, so subsequent make calls should be fast enough:

    run make in project directory

    // Now we need to actually run the test files:

    If no glob patterns have been specified on the command line, all files named test-*.js and test.js in test/compiled are selected (recursively) for testing.

    for each test file:
        run file with node, so we have a reference result
        if node failed on test file:
            continue, no need to test with lowjs
        run test file with lowjs

All test results are written to a summary html file at the end of the run. The location is printed to stdout. Each test result (one per test file) is annotated in the html file. The following information is stored alongside each test result:
* the output and exit code of the test, if any
* information about es5 transformation failure, if applicable
* did node fail or did lowjs fail? If node failed, lowjs was not tested, and the cause should be determined why node failed. Maybe the node version is outdated. At the time of this writing node v10.8 is preferred. Maybe the test itsself is buggy or not suitable for specific node versions.
* timeout: the test timed out and had to be killed. The timeout interval is hardcoded in the test runner.
* crash: the lowjs/node process recieved a signal, indicating that the test crashed and did not end on its own accord.
* error: the test returned a non-zero result code, e.g. an error was thrown, due to a failed assertion or another error.
* ok: test process ended normally with exit code zero.
