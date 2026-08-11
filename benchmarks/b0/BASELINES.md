# Benchmark B0 Godot baseline candidates

Review date: **2026-08-11**

This file records the external-reference refresh required by #102. It is selection rationale, not a scored benchmark result. Qualification evidence lives under [`qualification/`](qualification/).

## Frozen Godot engine

Pinned engine: **Godot `4.7.1-stable`**.

Primary reference:

- <https://github.com/godotengine/godot/releases/tag/4.7.1-stable>

Why this version is frozen for B0:

- it was the current latest stable Godot release at the 2026-08-11 reference refresh,
- the selected MCP candidate requires Godot 4.5 or newer,
- one exact stable version removes an uncontrolled engine variable across `godot.generic` and `godot.agent`.

B0 classification: **PIN / QUALIFIED**. Hosted CI downloaded the official Linux x86_64 build, verified it against the release `SHA512-SUMS.txt`, verified the reported version, and exercised both Godot lane oracles.

## Selected baseline — satelliteoflove/godot-mcp

Pinned selected package: **`@satelliteoflove/godot-mcp@4.1.0`**.

Primary references:

- <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- <https://github.com/satelliteoflove/godot-mcp>

Why it was qualified first:

- frozen and predicate-controlled game-time control,
- structured live runtime state rather than screenshot-only observation,
- real timed input injection,
- editor/runtime inspection and screenshots,
- ordinary project authoring remains available instead of forcing every edit through task-shaped custom tools.

B0 classification: **ADOPT AS SELECTED QUALIFIED `godot.agent` BASELINE**.

The hosted qualification proved, against Godot 4.7.1-stable and the exact npm package integrity recorded in [`qualification/godot-agent.json`](qualification/godot-agent.json):

1. reversible real editor authoring/save/readback,
2. structured runtime inspection while launch-frozen,
3. raw `D` key input affecting ordinary gameplay input code,
4. deterministic replay by stopping on the fixture's authoritative `physics_ticks == 12` predicate through public `step_until`,
5. independent known-good acceptance and wrong-position known-bad rejection.

The accepted two clean runs both stopped at exactly 12 physics ticks and `Player.position_x == 2`. Their uncapped render-frame counts differed (`267` vs `271`); that difference is retained and deliberately excluded from the deterministic domain.

Two weaker measurement criteria were rejected during qualification. Fixed render-frame stepping did not map to fixed physics progress on an uncapped hosted renderer. A fixed 200 ms game-time criterion then passed once but exposed 12-vs-13 physics-tick phase variance on a later clean rerun, so it was also rejected rather than normalized away. The selected baseline is qualified only under the stronger authoritative physics-tick boundary.

This selection was made on the predefined qualification contract, before any scored Trace2D-vs-Godot result exists.

## Alternative — Erodenn/godot-mcp-runtime

Pinned reproducible candidate: **`godot-mcp-runtime@3.2.1`**.

Primary references:

- <https://www.npmjs.com/package/godot-mcp-runtime>
- <https://github.com/Erodenn/godot-mcp-runtime>

Strengths:

- zero committed project footprint,
- headless authoring plus live-game screenshots/input/UI discovery/script execution,
- stock Godot runtime and simple `npx` setup.

B0 classification: **ADAPT / KEEP AS FALLBACK**. It remains attractive when project-footprint neutrality matters. It is not promoted simply to change the baseline after the primary candidate successfully qualified.

## Alternative — hi-godot/godot-ai

Reproducible reviewed release: **`v2.5.7`**; current `main` was also reviewed for capability direction.

Primary reference:

- <https://github.com/hi-godot/godot-ai>

Strengths:

- broad editor authoring surface,
- scene/node/script/UI/material/animation/particle operations,
- active plugin + MCP workflow.

B0 classification: **ADAPT / AUTHORING-BREADTH FALLBACK**. This is a strong authoring baseline, but B0's qualification contract values independent runtime observation/input/controlled time in addition to editor operation breadth.

## Alternative — n24q02m/better-godot-mcp

Pinned reproducible candidate: **`@n24q02m/better-godot-mcp@1.21.0`**.

Primary references:

- <https://www.npmjs.com/package/@n24q02m/better-godot-mcp>
- <https://github.com/n24q02m/better-godot-mcp>

Strengths:

- compact composite authoring tools,
- broad scene/script/UI/input-map/animation/physics authoring,
- local stdio mode and straightforward environment diagnostics.

B0 classification: **ADAPT / NOT PRIMARY FOR RUNTIME B0**. Its reviewed public surface was stronger as an authoring bridge than as the frozen-time structured-runtime baseline needed by this B0 contract.

## Alternative — IvanMurzak/Godot-MCP

Primary reference:

- <https://github.com/IvanMurzak/Godot-MCP>

Strengths:

- broad editor operations,
- screenshots/resources/scripts,
- opt-in runtime extension model.

Tradeoffs relevant to a matched benchmark:

- requires Godot .NET/Mono and .NET 8,
- default workflow adds an addon/server/cloud-or-self-hosted connection surface,
- therefore changes the environment more materially than the stock-Godot candidates above.

B0 classification: **DEFER FROM FIRST QUALIFICATION ROUND**. This is not a quality judgment; the selected candidate already satisfied the frozen B0 requirements without adding that environment variable.

## Selection rule

The `godot.agent` baseline was not chosen by stars, tool count, or marketing claims. Selection required:

1. exact Godot 4.7.1-stable binary/environment identity,
2. authoring in a fresh project,
3. structured runtime inspection,
4. timed player input,
5. deterministic/frozen game-time control with an authoritative replay boundary,
6. no task-specific solution logic,
7. exact bridge/package integrity recording,
8. independent gold/known-bad verifier behavior.

Those checks now pass and the candidate is frozen. A future baseline change requires a new versioned benchmark cohort; it must not be changed after observing which lane wins.
