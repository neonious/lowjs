import * as React from 'react';
import classNames from 'classnames';
import * as s from './styles.scss';
import { TestResult } from './result';

export interface TestResultProps {
    result: TestResult;
    open: boolean;
    toggleOpen(): void;
}

export class TestResultComp extends React.PureComponent<TestResultProps> {
    constructor(props: TestResultProps) {
        super(props);
    }

    render() {
        const { toggleOpen, open, result: { file, output, code, signal, resultType, nodeFailed } } = this.props;
        const statusClass = s[resultType];

        return <div className={classNames(s.result, {
            [s.closed]: !open,
            [s.open]: open
        })}>
            <div className={classNames(s.summary, statusClass)} onClick={() => {
                toggleOpen();
            }}>
                <div className={s.file}>{file}</div>
                <div className={s.nodeFailed}>{nodeFailed ? 'NODE FAILED' : null}</div>
                <div className={s.statusString}>{resultType.toUpperCase()}</div>
            </div>
            {
                open ?
                    <div className={s.detail}>
                        <div className={s.row}>
                            <div className={s.header}>
                                Output
                            </div>
                            <div className={s.text}>
                                {output}
                            </div>
                        </div>
                        {typeof code === 'number' ? <div className={s.row}>
                            <div className={s.header}>
                                Exit code
</div>
                            <div className={s.text}>
                                {code}
                            </div>
                        </div> : null}
                        {signal ? <div className={s.row}>
                            <div className={s.header}>
                                Signal
</div>
                            <div className={s.text}>
                                {signal}
                            </div>
                        </div> : null}
                    </div>
                    : null
            }
        </div>

    }
}