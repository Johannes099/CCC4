#!/usr/bin/env python3
"""Per-case ensemble merger.

usage: merge.py DATASET CASE_DIR [CASE_DIR ...]
Each CASE_DIR holds case_XXX.txt programs. Every candidate is validated with the
full-mask simulator; the shortest valid program per case (also considering the
current best_outputs/out_level-6_<DATASET>.txt) is written back to best_outputs/.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from validate import parse_input, parse_output, simulate  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_prog(path):
    return [l.split() for l in open(path).read().split('\n') if l.strip()]


def main():
    ds = sys.argv[1]
    dirs = sys.argv[2:]
    cases = parse_input(os.path.join(ROOT, 'data', f'in_level-6_{ds}.txt'))
    best_path = os.path.join(ROOT, 'best_outputs', f'out_level-6_{ds}.txt')
    best = [None] * len(cases)
    if os.path.exists(best_path):
        progs = parse_output(best_path, len(cases))
        if len(progs) == len(cases):
            best = progs
    improved = 0
    for i, case in enumerate(cases):
        if best[i] is not None:
            acc, used = simulate(case, best[i], True, stop_at_req=True)
            best[i] = best[i][:used] if acc >= case[3] else None
        for d in dirs:
            p = os.path.join(d, f'case_{i:03d}.txt')
            if not os.path.exists(p):
                continue
            prog = read_prog(p)
            if best[i] is not None and len(prog) >= len(best[i]):
                continue
            acc, used = simulate(case, prog, True, stop_at_req=True)
            if acc >= case[3]:
                best[i] = prog[:used]
                improved += 1
    missing = [i for i, b in enumerate(best) if b is None]
    if missing:
        print(f'{ds}: missing cases {missing}; not writing')
        sys.exit(1)
    os.makedirs(os.path.dirname(best_path), exist_ok=True)
    with open(best_path, 'w') as f:
        f.write('\n\n'.join('\n'.join(' '.join(l) for l in p) for p in best) + '\n')
    print(f'{ds}: total={sum(len(p) for p in best)} improved={improved}')
    print('per-case:', ' '.join(str(len(p)) for p in best))


if __name__ == '__main__':
    main()
