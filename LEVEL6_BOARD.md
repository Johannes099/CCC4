# LEVEL 6 Coordination Board (CCC-26, FAB-42)

Score = total instruction lines over 1-small … 4-enormous (lower is better).
All numbers below are re-validated by `tools/board.py` with the **full-mask** simulator
(`tools/validate.py`): every pattern `M` cell must be matter and every `O` cell must be empty at acceptance.

## Leaderboard (auto-generated)
<!-- AUTO:BEGIN -->
| Agent ID | Strategy / Approach | Example | 1-Small | 2-Medium | 3-Large | 4-Enormous | **Grand Total** | Commit |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| `ENSEMBLE_BEST` | Per-case minimum across all agents | - | 108 | 6232 | 6435 | 7233 | **20008** | `c364755` |
| `agent_sa_walk` | SA over emitter walks (chain / ping-pong / cycle) + exact single-config | - | 108 | 6232 | 6435 | 7233 | **20008** | `c364755` |

### Case-Level Bottleneck Tracker (ENSEMBLE_BEST, top 10 by lines)

| Dataset | Case | Lines | req |
| :--- | :---: | :---: | :---: |
| 4-enormous | 51 | 293 | 2 |
| 3-large | 146 | 276 | 2 |
| 4-enormous | 57 | 244 | 2 |
| 3-large | 143 | 244 | 6 |
| 4-enormous | 117 | 196 | 2 |
| 3-large | 141 | 183 | 6 |
| 4-enormous | 122 | 176 | 2 |
| 4-enormous | 83 | 168 | 1 |
| 4-enormous | 92 | 163 | 2 |
| 4-enormous | 116 | 153 | 1 |
<!-- AUTO:END -->

## Angle claims
| Agent | Angle |
| :--- | :--- |
| `agent_sa_walk` | Angle 2+3 hybrid: joint routing+cover via simulated annealing over the 6 emitter trajectories (step-by-step), exact single-config search |

## Lab Notes
### agent_sa_walk
- **Last-event formulation** (`agents/agent_sa_walk/solver.cpp`, `Solver::cellViol`): starting from an empty board, a cell's
  final content depends only on its last step with count ≥ 2 (`==2` → matter, `>=3` → empty). A walk is valid iff every
  cell's last event matches the target, so SA can score a whole walk incrementally (only cells on changed rays are rescanned).
  Played backwards the *first* event decides → ping-pong walks need both.
- **Three copy-repetition modes** (`main`): `chain` (each copy a fresh walk from the previous end state, good for req ≤ 3),
  `ping` (walk W, then reverse(W), …: k lines per copy), `cycle` (closed walk, forward only, best for k = 2).
  Setup cost for ping/cycle = max over emitters of |p| (−1 if inactive).
- **Exact single-config search** (`exactSingle`): if one static configuration's count-2 set equals the pattern, the cost is
  `S + req` lines (each extra copy is a single `- - - - - -` line). Optimal for those cases.
- **Emitter geometry**: emitter t at relative perimeter position p ∈ [−(R−1), R−1] covers exactly one interior line of
  axis t mod 3; |p| = R is an edge ray (= off). Emitters t and t+3 share an axis; putting both on one line paints a whole line.
- **Infra**: `.github/workflows/level6-solve.yml` runs the solver in a 19-job matrix on GitHub runners whenever
  `agents/agent_sa_walk/run.cfg` changes, then merges via `tools/keep_best_cases.py` + `tools/merge.py` and refreshes this board.
