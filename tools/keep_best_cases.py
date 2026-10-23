#!/usr/bin/env python3
"""Maintain an agent's per-case archive and its merged output file.

usage: keep_best_cases.py DATASET AGENT_OUTPUT_DIR NEW_CASE_DIR [...]
Archive: AGENT_OUTPUT_DIR/cases/DATASET/case_XXX.txt (shortest valid seen).
Writes AGENT_OUTPUT_DIR/out_level-6_DATASET.txt when every case is present.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from validate import parse_input, simulate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_prog(path):
    return [l.split() for l in open(path).read().split('\n') if l.strip()]


def main():
    ds, outdir, news = sys.argv[1], sys.argv[2], sys.argv[3:]
    cases = parse_input(os.path.join(ROOT, 'data', f'in_level-6_{ds}.txt'))
    arch = os.path.join(outdir, 'cases', ds)
    os.makedirs(arch, exist_ok=True)
    progs = []
    better = 0
    for i, case in enumerate(cases):
        name = f'case_{i:03d}.txt'
        cur = read_prog(os.path.join(arch, name)) if os.path.exists(os.path.join(arch, name)) else None
        if cur is not None and simulate(case, cur, True, stop_at_req=True)[0] < case[3]:
            os.remove(os.path.join(arch, name))
            cur = None
        for d in news:
            p = os.path.join(d, name)
            if not os.path.exists(p):
                continue
            prog = read_prog(p)
            if cur is not None and len(prog) >= len(cur):
                continue
            acc, used = simulate(case, prog, True, stop_at_req=True)
            if acc >= case[3]:
                cur = prog[:used]
                better += 1
                with open(os.path.join(arch, name), 'w') as f:
                    f.write('\n'.join(' '.join(l) for l in cur) + '\n')
        progs.append(cur)
    print(f'{ds}: {better} improved cases in archive')
    if all(p is not None for p in progs):
        with open(os.path.join(outdir, f'out_level-6_{ds}.txt'), 'w') as f:
            f.write('\n\n'.join('\n'.join(' '.join(l) for l in p) for p in progs) + '\n')
        print(f'{ds}: agent total {sum(len(p) for p in progs)}')


if __name__ == '__main__':
    main()
