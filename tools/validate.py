#!/usr/bin/env python3
"""Reference simulator / validator for CCC-26 level 6.

Mirrors sandbox-level-6.html (rebuildPerimeter / applyEmitterStep / testPattern)
with one deliberate difference: by default the pattern check uses the FULL
board mask (every 'O' cell must be empty), which is stricter than the sandbox's
skirting-only rule. Pass --skirting to use the sandbox rule.

usage: validate.py INPUT OUTPUT [--skirting] [--allow-noop] [-v]
By default a program containing an all-noop line ('- - - - - -') is INVALID.
"""
import sys

DIR_TO = [(2, 0), (1, 1), (-1, 1), (-2, 0), (-1, -1), (1, -1)]  # (dc, dr) E SE SW W NW NE
PERIM_DIRS = [5, 0, 1, 2, 3, 4]  # NE, E, SE, SW, W, NW


def parse_input(path):
    lines = open(path).read().split('\n')
    n = int(lines[0])
    i = 1
    cases = []
    for _ in range(n):
        while not lines[i].strip():
            i += 1
        R, rows, cols, req = map(int, lines[i].split())
        grid = [list(l) for l in lines[i + 1:i + 1 + rows]]
        i += 1 + rows
        cases.append((R, rows, cols, req, grid))
    return cases


def parse_output(path, n):
    progs = [[]]
    for line in open(path).read().split('\n'):
        if not line.strip():
            if progs[-1]:
                progs.append([])
            continue
        toks = line.split()
        progs[-1].append(toks)
    if not progs[-1]:
        progs.pop()
    return progs


class Board:
    def __init__(self, R):
        rad = R + 1
        self.rad = rad
        self.rows = 2 * rad - 1
        self.cols = 4 * rad - 3
        per = []
        cur = (0, rad - 1)  # (c, r)
        for d in PERIM_DIRS:
            dc, dr = DIR_TO[d]
            for _ in range(rad - 1):
                per.append(cur)
                cur = (cur[0] + dc, cur[1] + dr)
        self.perimeter = per
        cols = self.cols

        def in_grid(c, r):
            if r < 0 or r >= 2 * rad - 1:
                return False
            if (r + c + rad + 1) % 2:
                return False
            if r < rad:
                return c + r >= rad - 1 and c - r <= cols - rad + 1
            return c + (2 * rad - 1 - r) >= rad - 1 and c + r - 2 * rad + 1 < cols - rad + 1

        self.ray = []
        for (c0, r0) in per:
            rr = []
            for (dc, dr) in DIR_TO:
                cells = []
                c, r = c0 + dc, r0 + dr
                while in_grid(c, r):
                    cells.append((r, c))
                    c += dc
                    r += dr
                rr.append(cells)
            self.ray.append(rr)
        # base grid
        base = []
        for r in range(self.rows):
            row = []
            rRail = r == 0 or r == self.rows - 1
            for c in range(cols):
                if (r + c + rad + 1) % 2:
                    row.append('-')
                    continue
                if r < rad:
                    cRail = c == rad - 1 - r or c == cols - 1 - rad + 1 + r
                    if c + r >= rad - 1 and c - r <= cols - rad + 1:
                        row.append('#' if rRail or cRail else 'O')
                    else:
                        row.append(':')
                else:
                    cRail = c == rad - 1 + r - 2 * rad + 2 or c == cols - 1 + rad - 1 - r
                    if c + 2 * rad - 1 - r >= rad - 1 and c + r - 2 * rad + 1 < cols - rad + 1:
                        row.append('#' if rRail or cRail else 'O')
                    else:
                        row.append(':')
            base.append(row)
        self.base = base


def simulate(case, prog, full_mask=True, stop_at_req=False, allow_noop=False):
    if not allow_noop and any(all(t == '-' for t in l) for l in prog):
        return 0, len(prog)  # all-noop lines are rejected (strict grader assumption)
    R, rows, cols, req, pat = case
    b = Board(R)
    assert b.rows == rows and b.cols == cols
    P = len(b.perimeter)
    idx = [t * R for t in range(6)]
    act = [False] * 6
    grid = [row[:] for row in b.base]
    must_m, must_o = [], []
    for r in range(rows):
        for c in range(cols):
            ch = pat[r][c]
            if ch == 'M':
                must_m.append((r, c))
            elif ch == 'O':
                skirting = any(0 <= r + dr < rows and 0 <= c + dc < cols and pat[r + dr][c + dc] == 'M'
                               for dc, dr in DIR_TO)
                if full_mask or skirting:
                    must_o.append((r, c))
    accepted = 0
    for step, line in enumerate(prog):
        if len(line) != 6 or any(t not in '-TFB' for t in line):
            raise ValueError(f'bad line {step}: {line}')
        for r in range(rows):
            for c in range(cols):
                v = grid[r][c]
                if v in 'xd':
                    grid[r][c] = 'O'
                elif v == 'm':
                    grid[r][c] = 'M'
        for t in range(6):
            ins = line[t]
            if ins == 'F':
                idx[t] = (idx[t] + 1) % P
            elif ins == 'B':
                idx[t] = (idx[t] + P - 1) % P
            elif ins == 'T':
                act[t] = not act[t]
        hit = {}
        for t in range(6):
            if act[t]:
                for rc in b.ray[idx[t]][t]:
                    hit[rc] = hit.get(rc, 0) + 1
        for (r, c), n in hit.items():
            v = grid[r][c]
            if v not in 'OM':
                continue
            if n == 2:
                grid[r][c] = 'm' if v == 'O' else 'M'
            elif n >= 3:
                grid[r][c] = 'd'
        ok = all(grid[r][c] in 'Mm' for r, c in must_m) and not any(grid[r][c] in 'Mm' for r, c in must_o)
        if ok:
            for r, c in must_m:
                grid[r][c] = 'x'
            accepted += 1
            if stop_at_req and accepted >= req:
                return accepted, step + 1
    return accepted, len(prog)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    full = '--skirting' not in sys.argv
    verbose = '-v' in sys.argv
    cases = parse_input(args[0])
    progs = parse_output(args[1], len(cases))
    if len(progs) != len(cases):
        print(f'program count {len(progs)} != case count {len(cases)}')
        sys.exit(1)
    total = 0
    bad = 0
    for i, (case, prog) in enumerate(zip(cases, progs)):
        acc, _ = simulate(case, prog, full, allow_noop='--allow-noop' in sys.argv)
        total += len(prog)
        if acc < case[3]:
            bad += 1
            print(f'case {i}: accepted {acc} < req {case[3]}')
        elif verbose:
            print(f'case {i}: lines {len(prog)} req {case[3]} ok')
    print(f'{"VALID" if not bad else "INVALID"} lines={total} bad={bad} mask={"full" if full else "skirting"}')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
