# Benchmark B1 Godot baseline refresh

Review date: **2026-08-13**

This refresh is required before B1 task freeze by #100. It is capability-selection evidence, not a scored Trace2D-vs-Godot result.

B1 reuses Godot **4.7.1-stable** and the accepted B0 coding-agent profile unless qualification proves that a change is required. The Godot Agent bridge is re-opened because B1 changes the relevant task mix from semantic scene/runtime interaction to Sprite, animation and particle authoring.

## Candidate carried from B0 — satelliteoflove/godot-mcp

Reviewed pin: **`@satelliteoflove/godot-mcp@4.1.0`**.

Primary references:

- <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- <https://github.com/satelliteoflove/godot-mcp>

The B0 qualification already proved ordinary editor authoring, structured runtime inspection, real input and predicate-controlled deterministic stepping against Godot 4.7.1-stable. Its current public surface also includes animation editing and resource inspection.

B1 classification: **CARRY FORWARD AS A QUALIFICATION CANDIDATE**, not automatically selected. B1 must not assume that the B0 winner remains strongest for a content-authoring-heavy suite.

## Materially updated candidate — hi-godot/godot-ai

Reviewed release: **`v3.0.6`**, tag commit **`f3d99dfbd38c9e095edf1467f85bee507ace2c3a`**.

Primary references:

- <https://github.com/hi-godot/godot-ai/tree/v3.0.6>
- <https://github.com/hi-godot/godot-ai/blob/v3.0.6/docs/TOOLS.md>

The reviewed v3.0.6 surface exposes ordinary scene/script/resource editing plus dedicated animation and particle operations, runtime/game inspection/input, and editor/running-game capture. Those are materially relevant to B1 and broader than the v2.5.7 snapshot recorded during B0.

B1 classification: **MUST QUALIFY BEFORE FREEZE**. Dedicated content-authoring breadth is potentially stronger for B1, but public capability descriptions are not enough to declare a winner. Reproducible install/start, save/readback, verifier compatibility and clean known-good/known-bad behavior must be exercised first.

## Carry-forward fallback — Erodenn/godot-mcp-runtime

Reviewed pin carried from B0: **`godot-mcp-runtime@3.2.1`**.

Primary references:

- <https://www.npmjs.com/package/godot-mcp-runtime>
- <https://github.com/Erodenn/godot-mcp-runtime>

B1 classification: **FALLBACK CANDIDATE**. Its stock-Godot/headless/runtime footprint remains useful, but the first B1 qualification round should compare the B0-selected satellite bridge with the materially updated hi-godot content-authoring bridge.

## Selection rule

The selected `godot.agent` lane must be the strongest credible ordinary external-user workflow for the committed B1 task classes, considering:

1. reproducible pin/install on the frozen Godot version,
2. Sprite/resource authoring and save/readback,
3. exact animation/event authoring,
4. particle authoring under explicit structural/performance constraints,
5. structured or headless verification support,
6. presentation capture handoff,
7. coding-agent compatibility,
8. maintenance and license/distribution constraints,
9. absence of task-specific solution logic.

A complementary generic coding-tool path is allowed because #100 explicitly allows a normal stronger external-user stack. The final selection must be recorded **before** scored B1 task membership, budgets, known-bad mutations or results are frozen.
