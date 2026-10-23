#!/bin/bash
# usage: run_local.sh WORKDIR SEED "ds secs" ["ds secs" ...]
# Runs each dataset in turn on all cores, then merges into the agent archive and best_outputs.
set -u
cd "$(dirname "$0")/../.."
W=$1; SEED=$2; shift 2
for spec in "$@"; do
  set -- $spec; ds=$1; secs=$2
  mkdir -p "$W/$ds-$SEED"
  printf "%s %s\nseed %s\n" "$ds" "$secs" "$SEED" > "$W/$ds-$SEED.cfg"
  python3 agents/agent_sa_walk/run_chunk.py 0 1 "$W/$ds-$SEED" "$W/$ds-$SEED.cfg" > "$W/$ds-$SEED.log" 2>&1
  python3 tools/keep_best_cases.py "$ds" agents/agent_sa_walk/outputs "$W/$ds-$SEED/$ds"
  python3 tools/merge.py "$ds" agents/agent_sa_walk/outputs/cases/$ds | head -1
done
echo ALLDONE
