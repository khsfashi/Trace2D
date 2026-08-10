# Benchmark B0 Godot baseline candidates

Review date: **2026-08-11**

This file records the external-reference refresh required by #102. It is not itself qualification evidence. A scored B0 run remains blocked until the selected lane has committed positive qualification evidence matching the exact versions in `suite.json`.

## Frozen Godot engine qualification candidate

Pinned engine candidate: **Godot `4.6.3-stable`**.

Primary reference:

- <https://github.com/godotengine/godot/releases/tag/4.6.3-stable>

Why this version is pinned for the first qualification round:

- it is the current latest stable Godot release at the 2026-08-11 reference refresh,
- the primary MCP candidate requires Godot 4.5 or newer,
- pinning one exact stable engine version removes an otherwise uncontrolled engine variable across `godot.generic` and `godot.agent`.

B0 classification: **PIN / QUALIFY FIRST**. Version compatibility on paper is not qualification evidence. Both Godot lanes must exercise the actual frozen binary/environment before the suite can become scored-eligible.

## Primary candidate — satelliteoflove/godot-mcp

Pinned qualification candidate: **`@satelliteoflove/godot-mcp@4.1.0`**.

Primary references:

- <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- <https://github.com/satelliteoflove/godot-mcp>

Why it is the current first qualification target:

- exact/frozen game-time stepping and step-until,
- structured live runtime state rather than screenshot-only observation,
- real timed input injection,
- editor/runtime inspection and screenshots,
- ordinary project-file authoring remains available instead of forcing every edit through custom tools.

B0 classification: **ADAPT / QUALIFY FIRST**. The runtime verification surface is a strong match for Trace2D's deterministic Agent thesis, but the benchmark must prove the exact package/version works with the pinned Godot 4.6.3 environment before selecting it as the scored `godot.agent` lane.

## Alternative — Erodenn/godot-mcp-runtime

Pinned reproducible candidate: **`godot-mcp-runtime@3.2.1`**.

Primary references:

- <https://www.npmjs.com/package/godot-mcp-runtime>
- <https://github.com/Erodenn/godot-mcp-runtime>

Strengths:

- zero committed project footprint,
- headless authoring plus live-game screenshots/input/UI discovery/script execution,
- stock Godot runtime and simple `npx` setup.

B0 classification: **ADAPT / KEEP AS FALLBACK**. It is attractive when project-footprint neutrality matters. The first B0 task is structurally simple, but later B0 runtime tasks should compare its determinism controls against the primary candidate before a baseline switch.

## Alternative — hi-godot/godot-ai

Reproducible reviewed release: **`v2.5.7`**; current `main` was also reviewed for capability direction.

Primary reference:

- <https://github.com/hi-godot/godot-ai>

Strengths:

- broad editor authoring surface,
- scene/node/script/UI/material/animation/particle operations,
- active plugin + MCP workflow.

B0 classification: **ADAPT / AUTHORING-BREADTH FALLBACK**. This is a strong authoring baseline, but B0 prioritizes independent runtime observation/input/deterministic stepping over editor operation count alone.

## Newly reviewed — n24q02m/better-godot-mcp

Pinned reproducible candidate: **`@n24q02m/better-godot-mcp@1.21.0`**.

Primary references:

- <https://www.npmjs.com/package/@n24q02m/better-godot-mcp>
- <https://github.com/n24q02m/better-godot-mcp>

Strengths:

- compact composite authoring tools,
- broad scene/script/UI/input-map/animation/physics authoring,
- local stdio mode and straightforward environment diagnostics.

B0 classification: **ADAPT / NOT PRIMARY FOR RUNTIME B0**. Its public surface is currently stronger as an authoring bridge than as a frozen-time structured-runtime verifier, so tool-count breadth alone must not displace a stronger runtime baseline.

## Newly reviewed — IvanMurzak/Godot-MCP

Primary reference:

- <https://github.com/IvanMurzak/Godot-MCP>

Strengths:

- broad editor operations,
- screenshots/resources/scripts,
- opt-in runtime extension model.

Tradeoffs relevant to a matched benchmark:

- requires Godot .NET/Mono and .NET 8,
- default workflow includes an additional addon/server/cloud-or-self-hosted connection surface,
- therefore changes the environment more materially than the stock-Godot candidates above.

B0 classification: **DEFER FROM FIRST QUALIFICATION ROUND**. This is not a quality judgment; it avoids adding a .NET/cloud-or-server environment variable to the smallest first matched harness unless its capabilities prove uniquely necessary.

## Selection rule

The scored `godot.agent` lane is not chosen by stars, tool count, or marketing claims. Before changing `suite.json` from `qualification_required` to `eligible`, run the same bridge qualification fixture and require:

1. both Godot lanes use the exact pinned Godot 4.6.3-stable binary/environment,
2. authoring works in a fresh starter project,
3. structured runtime inspection works,
4. timed player input works,
5. deterministic/frozen stepping works when the selected bridge claims it,
6. the bridge does not inject task-specific solution logic,
7. the exact bridge and Godot versions are recorded,
8. the independent gold/known-bad verifier still behaves correctly.

If the primary candidate fails these checks, record the failure as qualification evidence and evaluate the next candidate. Do not silently choose the easiest bridge for Trace2D to beat.
