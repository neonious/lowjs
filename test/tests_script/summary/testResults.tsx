import * as React from 'react';
import { TestResultComp } from './testResult';
import * as s from './styles.scss';
import { TestResult } from './result';
import { chain } from 'lodash';

interface ResultsProps {
    results: TestResult[];
}

interface ResultsState {
    open: {
        [file: string]: boolean;
    }
}

export class TestResultsComp extends React.PureComponent<ResultsProps, ResultsState> {

    constructor(props: ResultsProps) {
        super(props);
        this.state = { open: {} };
    }

    render() {
        const { results } = this.props;
        return <div className={s.resultsContainer}>
            <h1>Test results</h1>
            <div className={s.buttons}>
                <a className={s.button} onClick={() => {
                    const open = chain(results).keyBy(r => r.file).mapValues(r => true).value();
                    this.setState({ open })
                }}>Expand all</a>
                <a className={s.button} onClick={() => {
                    this.setState({ open: {} })
                }}>Collapse all</a>
            </div>
            <div className={s.results}>
                {
                    results.map((r, i) => {
                        return <TestResultComp toggleOpen={() => {
                            this.setState(s => ({ open: { ...s.open, [r.file]: !s.open[r.file] } }))
                        }} open={!!this.state.open[r.file]} result={r} key={i} />
                    })
                }
            </div>
        </div>
    }
}