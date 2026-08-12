# Sprite Animation Agent/MCP Verification — SA3

Status: **SA3 implementation contract for #150.**  
Parent: #59.  
Frozen predecessors: SA0 timing, SA1 authoritative state, SA2 deterministic playback/events/transitions.  
Exact successor after SA3 merges green: **SA4 animation conformance, determinism, and performance workloads.**

## 1. Purpose

SA3 makes `runtime::SpriteAnimator2D` inspectable, controllable, and exactly assertable by coding agents without introducing a second animation model and without making rendered pixels, interpolation, wall-clock time, or MCP transport state authoritative.

The authority chain remains:

```text
prepared SpriteAnimationClip2D
 -> SpriteAnimator2D authoritative state / SA2 actions
 -> protocol-independent Agent inspection/action/assertion
 -> optional MCP serialization adapter
```

The Agent/MCP layers consume runtime truth. They do not reproduce playback, event-crossing, loop, ping-pong, speed, completion, or frame-resolution semantics.

## 2. Binding and lifetime

`agent::SpriteAnimatorBinding` is explicit and non-owning:

```text
entity semantic id
SpriteAnimator2D*
```

Rules:

- the caller owns animator and prepared-clip lifetime,
- a binding never extends lifetime through shared ownership,
- the Agent facade does not maintain a second animator registry or simulation,
- the MCP server receives a non-owning span of bindings supplied by its application owner,
- MCP advertises Sprite-animation tools only when that server instance has at least one configured binding,
- binding lookup is a small explicit linear scan; no speculative per-animator cache/index is added before workload evidence exists.

Future #71 scene/component integration may provide these bindings from world/component ownership without changing the public SA3 verification semantics.

## 3. Authoritative inspection

`AgentFacade::InspectSpriteAnimator(binding)` returns one scalar `SpriteAnimatorSnapshot` containing:

```text
entity_semantic_id
clip_duration_ns
clip_frame_count
clip_event_count
time_ns
frame_index
region_index
playback
loop_mode
direction
completed
speed_numerator
speed_denominator
speed_remainder
```

All values are read from the existing prepared clip and `SpriteAnimator2DState`.

Inspection:

- does not advance time,
- does not replay events,
- requires no renderer/GPU,
- performs no filesystem lookup or semantic-name resolution,
- allocates/report-formats only because inspection was explicitly requested.

`time_ns` is the exact SA0/SA2 integer-nanosecond authority. `frame_index` and `region_index` are current authoritative selections, not pixel inference.

## 4. Explicit actions

`AgentFacade::ActOnSpriteAnimator(binding, action)` delegates to the existing SA2 runtime operations:

```text
play
pause
stop
reset
restart
seek(time_ns)
set_speed(numerator, denominator)
set_direction(forward|reverse)
advance(delta_ns, emission_capacity)
```

The Agent layer does not reinterpret transition rules. Runtime errors are surfaced as structured Agent errors with the underlying finite `SpriteAnimator2DError` code retained.

### Seek/reset/restart

Seek/reset/restart preserve SA2 semantics exactly. They do not synthesize historical authored events. Event evidence exists only for boundaries actually crossed by an explicit successful `advance`.

### Exact advance

`advance` accepts an explicit integer nanosecond delta. Wall-clock time is never sampled.

The caller supplies `emission_capacity` and SA3 bounds explicit Agent output to at most **4096** emissions per request. The Agent allocates the temporary emission buffer only for that explicit request.

The underlying SA2 `Advance` already performs a dry traversal before commit. Therefore insufficient output capacity remains transactional:

```text
output_capacity_exceeded
 -> no animator state mutation
 -> no partial authoritative emission list
```

On success, SA3 returns the exact ordered typed emissions produced by SA2:

```text
authored_event
loop
bounce
completed
```

Each emission preserves event id, authored ordinal, exact timeline time and traversal direction.

## 5. Exact assertions

`AgentFacade::AssertSpriteAnimator(binding, assertion)` evaluates one finite typed field immediately against the current authoritative state.

Supported fields:

```text
clip_duration_ns
clip_frame_count
clip_event_count
time_ns
frame_index
region_index
playback
loop_mode
direction
completed
speed_numerator
speed_denominator
speed_remainder
```

Value kinds are finite:

```text
bool
int64
uint64
string
```

Assertion semantics:

- no implicit retry,
- no implicit simulation step,
- no timeout/wait loop,
- no render/capture dependency,
- exact integer/enum/bool comparison,
- type mismatch is distinct from state mismatch.

A mismatch returns expected value, observed value and bounded current-state context:

```text
entity id
time_ns
frame_index
region_index
playback
loop_mode
direction
completed
```

This makes exact-frame failures diagnosable without dumping unrelated world state or pixel evidence.

## 6. Stable Agent errors

SA3 uses a finite Agent error vocabulary:

```text
animator_unavailable
animator_state_unavailable
clip_unavailable
invalid_action
invalid_assertion
type_mismatch
state_mismatch
runtime_rejected
output_capacity_exceeded
```

When a runtime action rejects a request, the response also retains the exact `SpriteAnimator2DError` string such as `negative_delta`, `invalid_speed`, `invalid_playback_transition`, or `output_capacity_exceeded`.

## 7. MCP adapter

MCP owns serialization only. The configured tool surface is deliberately small:

```text
trace2d.sprite_animation.inspect
trace2d.sprite_animation.action
trace2d.sprite_animation.assert
```

Playback operations are an `action` enum instead of one MCP tool per runtime method. This keeps discovery compact and keeps semantic authority in the Agent/runtime layers.

Each tool has a finite JSON Schema input. Tool results keep Trace2D's modern MCP shape:

```text
structuredContent = authoritative machine-readable result
content[TextContent] = serialized compatibility representation
```

Inspection/assertion are annotated read-only; action is not. These annotations are transport hints only and do not replace server-side semantics or validation.

The repository remains on MCP protocol `2026-07-28`. SA3 does not reintroduce handshake/session state.

## 8. Exact-frame workflow

SA3 deliberately separates **advancing** from **asserting**:

```text
explicit fixed simulation step and/or explicit animator advance
 -> inspect/assert current authoritative animation state
 -> optional exact-frame render/capture as derived evidence
```

An assertion never advances until it passes. This differs intentionally from browser/UI auto-retrying assertion libraries: exact animation QA must fail at the requested state rather than silently observe a later one.

When future world/component integration advances `SpriteAnimator2D` from fixed simulation ticks, the existing `trace2d.runtime.step` tool can drive those ticks through the application's frame update. SA3 itself does not couple `SpriteAnimator2D` to `GameplayScenario` or create Sprite-only world ownership.

## 9. Performance contract

Ordinary `SpriteAnimator2D` update remains unchanged by SA3.

Forbidden in the normal animation hot path:

- Agent snapshot construction,
- JSON serialization,
- string formatting,
- filesystem work,
- renderer/GPU access,
- semantic-name lookup,
- mandatory heap allocation,
- background fingerprint/report maintenance.

Costs introduced by SA3 occur only on explicit Agent/MCP QA calls:

- inspection/assertion: fixed scalar snapshot/report work,
- binding lookup in MCP: O(configured bindings),
- advance evidence: O(crossed boundaries) runtime traversal plus O(returned emissions) explicit report materialization, bounded to 4096 emissions per Agent request.

No unmeasured cache/index is introduced merely to optimize a QA lookup.

## 10. External reference decisions

Reference pass performed 2026-08-12 before implementation:

- Model Context Protocol `2026-07-28`: **ADAPT** stateless/self-describing requests, deterministic cacheable tool catalogs and full JSON Schema tool inputs while preserving Trace2D's protocol-independent Agent facade.
- MCP Tools contract: **ADOPT** schema-defined tool inputs and structured results; **ADAPT** `structuredContent` plus serialized text compatibility to the repository's existing server shape.
- MCP tool annotations: **ADAPT** read-only/action hints for discovery; do not treat annotations as runtime authority.
- Playwright assertion guidance: **ADAPT** useful expected/observed diagnostic shape; **REJECT** auto-retry for authoritative Sprite animation assertions because retrying would implicitly move the observed state.
- Trace2D Particle Agent verification (#50/PR #83): **ADOPT** explicit-request snapshots, finite typed assertions, structured errors and zero normal-step reporting work.

Primary references:

- https://blog.modelcontextprotocol.io/posts/2026-07-28/
- https://modelcontextprotocol.io/specification/2025-11-25/server/tools
- https://playwright.dev/docs/test-assertions
- [`PARTICLE_AGENT_VERIFICATION.md`](PARTICLE_AGENT_VERIFICATION.md)
- [`adr/0001-protocol-independent-agent-facade.md`](adr/0001-protocol-independent-agent-facade.md)

## 11. SA3 acceptance / handoff

SA3 is complete when the exact PR head proves:

- headless authoritative state inspection,
- all exposed state-changing operations delegate to SA2 runtime authority,
- successful advance returns exact ordered authored/structural emission evidence,
- output-capacity failure is explicit and transactional,
- exact typed assertions return expected/observed/current-state context,
- malformed/unavailable bindings and runtime-rejected actions have stable errors,
- MCP exposes the same capabilities only as an adapter,
- existing no-binding MCP servers retain their existing tool catalog,
- normal hosted configure/build/test/audits are green,
- no new presentation-GPU behavior is introduced.

SA3 adds no presentation path and therefore requires no new local real-GPU acceptance gate. After SA3 merges green, **SA4** is the exact next Sprite child; do not begin SA4 in the SA3 PR.
