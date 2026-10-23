#!/usr/bin/env python3
"""Rewrite all-noop lines ('- - - - - -') into equivalent non-noop lines.

A noop line is replaced by an F/B move of an emitter that contributes no ray
(inactive, or parked off-arc) before and after the move; the displacement is
undone later on a line where that emitter is idle and still harmless (unless
the emitter never fires again). Every rewritten program is re-validated with
the full-mask simulator; programs that cannot be rewritten are reported.

usage: denoop.py INPUT OUTPUT_IN OUTPUT_OUT
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from validate import Board, parse_input, parse_output, simulate  # noqa: E402

NOOP = ['-'] * 6


def states(R, P, prog):
    """per line: (idx[6], act[6]) after the line is applied"""
    idx = [t * R for t in range(6)]
    act = [False] * 6
    out = []
    for line in prog:
        for t, c in enumerate(line):
            if c == 'F':
                idx[t] = (idx[t] + 1) % P
            elif c == 'B':
                idx[t] = (idx[t] - 1) % P
            elif c == 'T':
                act[t] = not act[t]
        out.append((idx[:], act[:]))
    return out


def fix_program(case, prog):
    R = case[0]
    b = Board(R)
    P = len(b.perimeter)
    interior = {(r, c) for r in range(b.rows) for c in range(b.cols) if b.base[r][c] == 'O'}
    fires = {}

    def harmless(t, idx, act):
        if not act:
            return True
        key = (t, idx)
        if key not in fires:
            fires[key] = any(rc in interior for rc in b.ray[idx][t])
        return not fires[key]

    prog = [l[:] for l in prog]
    for i in range(len(prog)):
        if prog[i] != NOOP:
            continue
        st = states(R, P, prog)
        done = False
        for t in range(6):
            if done:
                break
            for d in (1, -1):
                idx0, act0 = st[i][0][t], st[i][1][t]
                if not harmless(t, idx0, act0) or not harmless(t, (idx0 + d) % P, act0):
                    continue
                # does t ever fire again?
                fires_later = any(not harmless(t, st[j][0][t], st[j][1][t]) for j in range(i + 1, len(prog)))
                cand = [l[:] for l in prog]
                cand[i][t] = 'F' if d == 1 else 'B'
                if not fires_later:
                    ok = True
                else:
                    ok = False
                    # compensation slot: first j>i with t idle; shifted states i..j-1 must stay harmless
                    for j in range(i + 1, len(prog)):
                        sidx, sact = (st[j - 1][0][t] + d) % P, st[j - 1][1][t]
                        if not harmless(t, sidx, sact):
                            break
                        if prog[j][t] == '-' and harmless(t, st[j][0][t], st[j][1][t]):
                            cand[j][t] = 'B' if d == 1 else 'F'
                            ok = True
                            break
                        if prog[j][t] != '-':
                            # its own instruction applies to the shifted state; check harmlessness after
                            pass
                if not ok:
                    continue
                acc, used = simulate(case, cand, True, stop_at_req=True)
                if acc >= case[3]:
                    prog = cand[:used]
                    done = True
                    break
        if not done:
            return None
    return prog


def main():
    inp, src, dst = sys.argv[1:4]
    cases = parse_input(inp)
    progs = parse_output(src, len(cases))
    out = []
    fails = []
    for k, (case, prog) in enumerate(zip(cases, progs)):
        if NOOP in prog:
            fixed = fix_program(case, prog)
            if fixed is None:
                fails.append(k)
                fixed = prog
            prog = fixed
        out.append(prog)
    with open(dst, 'w') as f:
        f.write('\n\n'.join('\n'.join(' '.join(l) for l in p) for p in out) + '\n')
    print(f'lines={sum(len(p) for p in out)} unfixable={fails}')


if __name__ == '__main__':
    main()
