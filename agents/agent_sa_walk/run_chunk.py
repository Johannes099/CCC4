#!/usr/bin/env python3
"""Run solver on every (dataset, case) assigned to chunk CHUNK of NCHUNKS.

usage: run_chunk.py CHUNK NCHUNKS OUTDIR   (expects ./solver built at repo root)
Cases are dealt round-robin after sorting by estimated cost, one solver
process per case, nproc processes in parallel.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
chunk, nchunks, outdir = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
cfg = {}
for line in open(os.path.join(HERE, 'run.cfg')):
    line = line.split('#')[0].split()
    if len(line) == 2:
        cfg[line[0]] = line[1]
seed = int(cfg.pop('seed', 1))
jobs = []
for ds, secs in cfg.items():
    inp = os.path.join(ROOT, 'data', f'in_level-6_{ds}.txt')
    n = int(open(inp).readline())
    for i in range(n):
        jobs.append((float(secs), ds, i))
jobs.sort(key=lambda j: (-j[0], j[1], j[2]))
mine = jobs[chunk::nchunks]


def run(job):
    secs, ds, i = job
    d = os.path.join(outdir, ds)
    os.makedirs(d, exist_ok=True)
    inp = os.path.join(ROOT, 'data', f'in_level-6_{ds}.txt')
    r = subprocess.run([os.path.join(ROOT, 'solver'), inp, str(i), str(i), str(secs), str(seed * 100003 + i * 7 + chunk), d],
                       capture_output=True, text=True)
    sys.stdout.write(f'{ds} {r.stdout.strip()} {r.stderr.strip()}\n')
    sys.stdout.flush()


with ThreadPoolExecutor(os.cpu_count() or 2) as ex:
    list(ex.map(run, mine))
