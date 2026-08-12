# Sprite Animation Conformance, Determinism, and Workloads — SA4

Status: **SA4 implementation contract for #152.**  
Parent: #59.  
Frozen predecessors: SA0 timing/events, SA1 authoritative state, SA2 deterministic playback, SA3 Agent/MCP verification.  
Exact successor after SA4 merges green: **SPP0 deterministic Sprite processing/QA report contract.**

## 1. Purpose

SA4 closes the deterministic Sprite-animation runtime phase by proving the already-frozen runtime semantics under reproducible long-running and boundary-heavy workloads.

The authority chain does not change:

```text
prepared SpriteAnimationClip2D
 -> SpriteAnimator2D authoritative state / emissions
 -> explicit SA4 conformance workload
 -> structural evidence
 -> optional environment-labelled timing evidence
```

SA4 is evidence, not a second animation implementation. It does not make rendered pixels, wall-clock time, Agent/MCP state, workload hashes, or benchmark results authoritative.

## 2. Deterministic conformance boundary

Portable correctness is defined by exact Trace2D-owned typed facts:

- prepared frame/event counts and integer clip duration,
- authoritative integer `time_ns`, frame index, playback, loop mode and direction,
- completion state,
- canonical rational speed numerator/denominator and retained remainder,
- ordered `AuthoredEvent | Loop | Bounce | Completed` emissions,
- authored event id/ordinal/time/direction,
- transactional failure behavior.

Two fresh runs with the same prepared clip, initial state and advance sequence must produce the same semantic state and ordered emission evidence.

Pointers are ownership/lifetime implementation details and are never included in replay identity. Renderer/GPU state and pixels are derived presentation and are outside SA4 semantic conformance.

## 3. Committed workload runner

`trace2d_sprite_animation_workload` is an explicit QA/performance tool. It is not called by ordinary animation stepping.

The runner exposes three committed workloads:

| Workload | Contract stress |
| --- | --- |
| `steady_loop_rational` | 6,000 fixed advances, `2/3` retained rational speed, forward loop, offset-zero/event boundaries |
| `dense_event_ping_pong` | dense authored events including equal-time ordinals, `5/4` speed, repeated ping-pong bounces |
| `large_step_multi_wrap` | reverse loop with advances larger than three clip durations, repeated wrap/event traversal |

Normal structural mode executes the selected workload twice from fresh state. A replay mismatch is a tool failure. Successful output is one machine-readable JSON object using schema `trace2d.sprite-animation-workload.v1`.

Structural fields include:

```text
workload identity
frame/event counts
clip duration
loop mode / initial direction
exact speed numerator/denominator
step delta / step count
successful advance count
emission counts by kind
final time/frame/direction/completion/remainder
stable FNV-1a 64-bit semantic transcript digest
```

The digest hashes only explicit numeric/enumerated semantic facts in a fixed byte order. It never hashes pointers, addresses, wall-clock samples, strings from runtime hot paths, GPU resources, or platform-specific object bytes. It is a compact regression/evidence field, not a replacement for typed tests.

## 4. Focused conformance tests

`SpriteAnimationConformanceSA4Tests` lock the failure modes most likely to escape short unit cases:

- long-running boundary-heavy exact replay,
- split-step versus aggregate-step state/transcript equivalence where SA0 semantics define equivalence,
- exact long-run rational quotient/remainder behavior without floating accumulation,
- large multi-bounce ping-pong replay,
- transactional output-capacity failure under multi-wrap stress,
- restart/seek future-transcript repeatability without historical replay.

SA2's existing focused tests remain authoritative for narrow transition/event rules. SA4 intentionally does not duplicate every SA2 case; it composes them into longer reproducible workloads.

## 5. Timing evidence

Timing is optional and local:

```text
trace2d_sprite_animation_workload
  --workload <name>
  --timing
  --machine-label <label>
  [--warmup N]
  [--iterations N]
```

Timing rules:

- build and run Release when collecting performance evidence,
- clip/asset/state preparation happens before each measured window,
- report serialization happens after the measured window,
- the measured window contains repeated `SpriteAnimator2D::Advance` calls using caller-owned retained emission storage,
- warmup runs are discarded,
- repeated measured windows report average/median/p95 microseconds,
- output records OS, compiler, build configuration and caller-supplied machine label,
- structural digest is included so the measured semantic workload is identifiable.

Wall-clock values are machine-dependent evidence. Hosted/shared CI checks only that the deterministic workload path runs successfully; CI does **not** fail on a microsecond threshold.

No Google Benchmark dependency is added: the existing small dependency-free workload-runner pattern is sufficient for this bounded evidence path.

## 6. Hot-path contract

SA4 adds no work to ordinary `SpriteAnimator2D::Advance` beyond the already-frozen SA2 implementation.

Forbidden as mandatory normal-step behavior:

- heap allocation for reporting/transcripts,
- filesystem access,
- JSON/string formatting,
- semantic-name lookup,
- Agent/MCP snapshot maintenance,
- renderer/GPU access,
- wall-clock reads,
- background hash/fingerprint maintenance.

Arrays/vectors, hashing, JSON and timing samples in the SA4 runner/tests are explicit QA tooling costs only.

## 7. Complexity and allocation expectations

SA4 does not change runtime algorithmic complexity.

For a successful existing SA2 advance:

- traversal cost remains proportional to crossed semantic boundaries/events,
- state observation remains fixed scalar work,
- caller owns emission capacity,
- clip preparation/cached boundaries remain setup-time work.

The workload runner adds O(returned emissions) digest/count work only after explicit workload requests. Timing mode deliberately excludes digest/report generation from its measured advance window.

No speculative animator lookup cache or per-animator background index is introduced. SA3's explicit small binding lookup remains unchanged because SA4 presents no evidence that a new runtime index is required.

## 8. External reference decisions

Primary-source pass refreshed 2026-08-12 before freezing SA4:

### FoundationDB Simulation — ADOPT / ADAPT

Official FoundationDB testing documentation emphasizes deterministic simulation because identical controlled runs can be reproduced for diagnosis, while performance testing is handled separately.

Trace2D adopts the replay principle and separation of correctness from machine performance. It adapts the technique to a small single-threaded Sprite-animation workload over an already deterministic runtime instead of importing a general simulation framework.

Reference: https://apple.github.io/foundationdb/testing.html

### Google Benchmark user guide — ADAPT / REJECT

The official guide supports warmup, repeated measurements, aggregate statistics and machine/context metadata.

Trace2D adapts those measurement practices for optional local Release timing. It rejects shared-CI wall-clock thresholds as correctness gates and rejects a new benchmark dependency for this narrow runner because the repository can express the required measurement directly.

Reference: https://google.github.io/benchmark/user_guide.html

### Godot SpriteFrames / AnimatedSprite2D — ADAPT / REJECT

Current Godot documentation exposes variable frame durations, none/linear/ping-pong loop modes, reverse playback and endpoint behavior. These are useful mature-engine comparison cases.

Trace2D adapts the behavioral coverage but retains SA0-SA2 integer-nanosecond time and exact rational speed/remainder as authority. Godot float frame progress/speed is not adopted as deterministic Trace2D truth.

References:

- https://docs.godotengine.org/en/stable/classes/class_spriteframes.html
- https://docs.godotengine.org/en/stable/classes/class_animatedsprite2d.html

### Aseprite file format — ADOPT / ADAPT

Aseprite's official format stores per-frame integer duration and animation tag direction/repeat metadata including forward, reverse and ping-pong variants.

Trace2D retains those authoring precedents as useful input semantics while SA4 tests Trace2D's own runtime contract rather than claiming Aseprite runtime equivalence.

Reference: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md

No external runtime dependency is introduced.

## 9. CI and local validation

Normal CI must compile/run:

- all runtime tests including `SpriteAnimationConformanceSA4Tests`,
- workload discovery,
- all three deterministic structural workload invocations,
- existing repository audits and tests.

The workload CTest entries assert only successful deterministic execution and stable machine-readable status. They contain no timing thresholds.

SA4 introduces no new presentation/GPU behavior. Existing renderer/GPU evidence remains valid and SA4 requires no new local real-GPU gate.

## 10. Completion / handoff

SA4 is complete only when one exact PR head proves:

1. all three committed workloads replay identically from fresh state,
2. focused long-run/rational/boundary/capacity/restart conformance tests pass,
3. optional timing remains explicitly local/environment-labelled and outside CI thresholds,
4. normal repository CI/audits are green,
5. no runtime hot-path reporting/hash/timing work or renderer/GPU dependency was introduced,
6. `docs/SPRITES.md`, `PROJECT_STATUS.md`, #152 and this contract agree.

After those gates pass, record exact final-head validation evidence, mark the SA4 PR ready, merge it, confirm #152 closes, and stop. **SPP0 is created only by the following continuation.**
