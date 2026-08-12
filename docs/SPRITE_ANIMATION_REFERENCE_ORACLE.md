# Sprite animation independent reference oracle

Issue: #159  
Related runtime contract: SA0-SA4 under #59

This hardening layer is test-only. It does not change `SpriteAnimator2D` runtime semantics, public API, ownership, hot-path allocation, or renderer behavior.

## Why a second model exists

SA0-SA4 already have focused regression and conformance tests, but implementation and expected examples can still repeat the same mistaken assumption. The reference oracle therefore derives the same authoritative result with a deliberately different algorithm.

```text
deterministic generated scenario
        ├──> production SpriteAnimator2D bulk traversal
        └──> test-only 1 ns reference traversal
                         ↓
          exact final state + emission transcript equality
```

The reference model is not a second engine authority. It exists only as independent test evidence.

## Independence rules

The reference model:

- lives only under `tests/runtime/reference/`,
- receives the same already-prepared clip/state as the production animator,
- never calls `SpriteAnimator2D::Advance`,
- never copies/calls the production bulk `ScaleAdvance` / traversal / event-range helpers,
- advances input time one nanosecond at a time,
- advances produced animation time one nanosecond at a time,
- linearly scans authored events at each crossed timeline point,
- linearly resolves the final frame from authored frame durations,
- preserves the frozen SA0 boundary/event semantics including forward zero-offset loop events and reverse endpoint handling.

The production implementation remains optimized around quotient/remainder scaling, boundary-sized traversal and binary-searched event ranges. The test oracle intentionally trades performance for algorithmic simplicity and independence.

## Generated cases

The fixed base seed is `0x5452414345324401`. Each case derives its own deterministic seed so a failure report can name both the base/case identity and the complete scenario.

The normal oracle test runs 5,000 bounded cases across:

- 1..6 frames with variable integer-nanosecond durations,
- zero through ten authored events,
- explicit offset-zero events and equal-time event groups,
- shuffled authored event input order,
- `Once`, `Loop`, and `PingPong`,
- forward and reverse playback,
- zero and non-zero exact rational speeds,
- start positions including exact endpoints,
- bounded advances including forced multi-wrap/multi-bounce cases.

Every case compares:

- `time`, `frameIndex`, playback, loop mode, direction, completion,
- canonical rational speed and retained remainder,
- emission count,
- emission kind, event ID, authored ordinal, time, direction,
- exact emission order.

A second 1,000-case generated workload gives production an output buffer one slot smaller than the reference transcript and requires `OutputCapacityExceeded` with exact pre-call state preservation.

## Reproduction policy

A failing case prints seed, case number, frame durations, authored events, start time, delta, loop mode, direction, and speed. Once a generated failure reveals a real defect, promote the reduced/understood scenario into an explicit permanent regression rather than relying only on the generator.

## External-reference pass — 2026-08-12

- FoundationDB Simulation — **ADAPT** deterministic seeded stress/replay as a correctness technique; Trace2D keeps this bounded and test-only rather than introducing a simulation framework.
- Hypothesis property-based testing/replay guidance — **ADAPT** generated edge-case exploration, deterministic reproduction identity, and promotion of important failures into explicit examples; **REJECT** adding a Python/property-testing dependency to the C++ runtime tests for this bounded need.

References:
- https://apple.github.io/foundationdb/testing.html
- https://apple.github.io/foundationdb/client-testing.html
- https://hypothesis.readthedocs.io/en/latest/
- https://hypothesis.readthedocs.io/en/latest/tutorial/replaying-failures.html

No external dependency is added.
