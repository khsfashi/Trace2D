# Benchmark B0 results

Status: **completed scored cohort for #102**.

B0 is a harness-integrity milestone over one narrow matched task. These results demonstrate that Trace2D can run a preregistered, isolated, repeated comparison with preserved raw evidence and independent reverification. They do **not** support a broad claim that Trace2D is superior to Godot.

## Cohort identity

Accepted owner-local archive:

```text
codex-chatgpt-scored-20260811-180458-214dfeb0.zip
SHA-256 0625e084b6704258a537de7005a3f9a427d66147663abc7878d5880b2860ea52
```

Committed acceptance evidence:

[`qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json`](qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json)

Frozen common setup:

```text
Agent             openai-codex-cli@0.144.6
model             gpt-5.5
reasoning         high
profile hash      2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
wall budget       300 s
tool budget       80
input-token cap   100000
output-token cap  20000
human intervention 0
```

The task requires one semantic `player` entity named `Player` at exact position `(4, 1)`.

## Preregistered repetition

Three attempts per lane, nine total, no automatic retry, no replacement retry, no early stopping and no best-of-N selection.

```text
R1  godot.generic -> godot.agent   -> trace2d.agent
R2  godot.agent   -> trace2d.agent -> godot.generic
R3  trace2d.agent -> godot.generic -> godot.agent
```

All nine scheduled slots produced one preserved raw record. All nine preserved workspaces were independently reverified after the stochastic Agent turn.

## Authoritative benchmark status

Every trial exceeded the frozen `100000` input-token limit. Therefore the benchmark-level status is the same in all lanes:

| Lane | Samples | `budget_exceeded` | Benchmark success |
|---|---:|---:|---:|
| `godot.generic` | 3 | 3 | 0/3 |
| `godot.agent` | 3 | 3 | 0/3 |
| `trace2d.agent` | 3 | 3 | 0/3 |

The input-token budget was frozen before scored execution and was not raised after observing the unscored calibration or scored outcomes.

A completed provider turn beyond budget remains a visible implementation-domain `budget_exceeded` result rather than being reclassified as transport/infrastructure failure.

## Independent semantic verifier outcomes

Verifier outcomes are reported separately from benchmark status so that useful authored-result evidence is not hidden by the resource ceiling.

| Lane | Verifier pass | Verifier fail | Failure detail |
|---|---:|---:|---|
| `godot.generic` | 1/3 | 2/3 | two `player_missing` |
| `godot.agent` | 3/3 | 0/3 | — |
| `trace2d.agent` | 3/3 | 0/3 | — |

These verifier passes are **not** promoted to benchmark successes because all corresponding Agent runs exceeded the preregistered token budget.

## Raw resource distributions

### `godot.generic`

```text
input tokens   median 201304   range 137882..363990
tool calls     median 16       range 14..32
revisions      median 2        range 1..6
output tokens  median 3669     range 3214..7711
wall time      median 138061 ms range 127069..255325 ms
verifier time  median 181.8 ms range 179.8..195.4 ms
```

### `godot.agent`

```text
input tokens   median 420560   range 316070..505697
tool calls     median 27       range 21..32
revisions      median 3        range 1..4
output tokens  median 5747     range 2805..6322
wall time      median 212327 ms range 116342..232069 ms
verifier time  median 191.8 ms range 183.6..193.7 ms
```

### `trace2d.agent`

```text
input tokens   median 273128   range 245642..279966
tool calls     median 23       range 19..26
revisions      median 4        range 3..6
output tokens  median 4373     range 4331..4699
wall time      median 153199 ms range 150773..177833 ms
verifier time  median 29.6 ms  range 29.1..78.0 ms
```

With only three samples per lane and one narrow task, these distributions are descriptive evidence, not a general engine ranking.

## Isolation and evidence integrity

The accepted archive verifies:

- model preflight passed;
- real elevated-Windows ACL canary passed;
- the exact held-out canary read was attempted and denied;
- no canary secret leaked;
- sandbox SID differed from host SID;
- ACL apply/cleanup succeeded for the pre-cohort isolation turn and all nine scored turns (`10/10` records);
- all nine records share the frozen canonical Agent profile hash;
- human intervention is zero in all nine trials;
- the raw nine-record SHA chain independently recomputes;
- the nine-record replay SHA chain independently recomputes;
- `9/9` reverify processes returned zero;
- `9/9` preserved workspaces match their original workspace hashes;
- `9/9` reverified verdicts match the original verifier verdicts;
- the packaged ZIP contains no `auth.json`, obvious OpenAI API key, plaintext random canary, raw Windows SID or bearer authorization marker.

## Observed Godot crash events

The Agent trajectories also preserve six occurrences of:

```text
CrashHandlerException: Program crashed with signal 11
```

across five Godot trials:

- `godot.generic` R1, R2 and R3;
- `godot.agent` R2 and R3, with two crash events in R2.

These events are not removed, retried or converted into replacement samples. They happened inside the Agent's tool trajectory; the cohort itself continued, every scheduled trial record was appended, the final independent verifier ran for every slot, and every final workspace/verdict later reverified exactly.

They are therefore preserved as observed engine/tool-path evidence rather than treated as a reason to cherry-pick a cleaner cohort.

## B0 conclusion

B0's main claim is methodological:

```text
frozen task/model/budget
 -> hard held-out isolation
 -> fresh matched workspaces
 -> append-only raw trajectories
 -> independent verifier
 -> repeated preregistered cohort
 -> independent replay/reverification
 -> raw distributions without best-of-N
```

That path has now executed end-to-end on the owner-local Windows environment.

The cohort also exposes a concrete follow-up: the current task/model combination is too input-token-expensive for the frozen `100000` ceiling in all three lanes. B0 does not rewrite that ceiling after seeing the result. Later benchmark versions may define a new preregistered budget or more token-efficient task/Agent protocol, but this B0 cohort remains immutable evidence.
