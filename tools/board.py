#!/usr/bin/env python3
"""Regenerate the auto-generated scoreboard section of LEVEL6_BOARD.md.

Scores every out_level-6_<ds>.txt in best_outputs/ and agents/*/outputs/
(validated with the full-mask simulator) and rewrites the block between
<!-- AUTO:BEGIN --> and <!-- AUTO:END -->.
"""
import glob
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from validate import parse_input, parse_output, simulate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DS = ['0-example', '1-small', '2-medium', '3-large', '4-enormous']
BOARD = os.path.join(ROOT, 'LEVEL6_BOARD.md')


def score(path, cases):
    if not os.path.exists(path):
        return None, None
    progs = parse_output(path, len(cases))
    if len(progs) != len(cases):
        return None, None
    per = []
    for case, prog in zip(cases, progs):
        acc, used = simulate(case, prog, True, stop_at_req=True)
        if acc < case[3]:
            return None, None
        per.append(len(prog))
    return sum(per), per


def main():
    cases = {d: parse_input(os.path.join(ROOT, 'data', f'in_level-6_{d}.txt')) for d in DS}
    try:
        commit = subprocess.check_output(['git', '-C', ROOT, 'rev-parse', '--short', 'HEAD'], text=True).strip()
    except Exception:
        commit = '?'
    rows = [('ENSEMBLE_BEST', 'Per-case minimum across all agents', os.path.join(ROOT, 'best_outputs'))]
    for d in sorted(glob.glob(os.path.join(ROOT, 'agents', '*', 'outputs'))):
        agent = os.path.basename(os.path.dirname(d))
        desc_file = os.path.join(os.path.dirname(d), 'STRATEGY.txt')
        desc = open(desc_file).read().strip() if os.path.exists(desc_file) else ''
        rows.append((agent, desc, d))
    out = ['| Agent ID | Strategy / Approach | Example | 1-Small | 2-Medium | 3-Large | 4-Enormous | **Grand Total** | Commit |',
           '| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |']
    bottle = []
    for name, desc, d in rows:
        vals = []
        for ds in DS:
            tot, per = score(os.path.join(d, f'out_level-6_{ds}.txt'), cases[ds])
            vals.append(tot)
            if name == 'ENSEMBLE_BEST' and per and ds != '0-example':
                bottle += [(n, ds, i, cases[ds][i][3]) for i, n in enumerate(per)]
        grand = sum(v for v in vals[1:] if v is not None) if all(v is not None for v in vals[1:]) else None
        fmt = lambda v: '-' if v is None else str(v)  # noqa: E731
        out.append(f'| `{name}` | {desc} | ' + ' | '.join(fmt(v) for v in vals) + f' | **{fmt(grand)}** | `{commit}` |')
    out.append('')
    out.append('### Case-Level Bottleneck Tracker (ENSEMBLE_BEST, top 10 by lines)')
    out.append('')
    out.append('| Dataset | Case | Lines | req |')
    out.append('| :--- | :---: | :---: | :---: |')
    for n, ds, i, req in sorted(bottle, reverse=True)[:10]:
        out.append(f'| {ds} | {i} | {n} | {req} |')
    text = open(BOARD).read()
    a = text.index('<!-- AUTO:BEGIN -->') + len('<!-- AUTO:BEGIN -->')
    b = text.index('<!-- AUTO:END -->')
    text = text[:a] + '\n' + '\n'.join(out) + '\n' + text[b:]
    open(BOARD, 'w').write(text)
    print('\n'.join(out))


if __name__ == '__main__':
    main()
