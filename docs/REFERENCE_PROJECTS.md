# Reference projects and benchmark lessons

Last reviewed: 2026-08-10.

This document records public projects that are relevant to Trace2D's AI-operated product direction. They are **references, not runtime dependencies**. Trace2D should borrow validated ideas, not copy architectures that conflict with its own authority model.

Any future vendoring, redistribution, executable dependency or benchmark integration still requires an explicit version/license review.

## 1. godot-mcp

Reference: https://github.com/satelliteoflove/godot-mcp

Relevant ideas:

- let an agent run and verify its own work rather than requiring a human to shuttle screenshots/logs,
- deterministic playtesting through freeze/step semantics,
- structured runtime observation for positions, velocities, animation/custom state,
- input injection into a live game,
- runtime tools focused on capabilities that ordinary file editing cannot provide cheaply or reliably.

Trace2D lesson:

- preserve a small protocol-independent Agent surface,
- expose exact stepping/semantic state directly from the engine,
- let MCP remain an adapter,
- prefer structured observation over screenshot inference.

Trace2D difference:

`godot-mcp` adds an Agent/editor/runtime bridge around an existing engine. Trace2D designs the engine's authoritative runtime, authored formats and testing boundaries around AI operability from the start.

Do **not** copy the assumption that a graphical editor must remain the primary source of truth.

## 2. GameCraft-Bench

Reference: https://github.com/FreedomIntelligence/gamecraft-bench

Relevant ideas:

- evaluate complete launchable game projects rather than isolated code snippets,
- require replayable player-input traces,
- launch the game and evaluate observed behavior,
- retain artifacts and per-task evidence,
- expose a dashboard for inspecting/playing benchmark outputs,
- report token usage rather than hiding agent cost.

Trace2D lesson:

Use the benchmark concepts:

```text
Engine grounding
Artifact completeness
Interactive verification
```

and add a Trace2D-specific fourth axis:

```text
Semantic verifiability
```

because Trace2D should be able to verify engine-owned facts directly rather than relying only on observed pixels/gameplay recordings.

The benchmark dashboard is also useful inspiration for the Trace2D Workspace's result-first review UX.

Trace2D difference:

The Trace2D benchmark must separate deterministic semantic truth from presentation/multimodal judgment and final human review rather than reducing everything to one observed-gameplay score.

## 3. GameDevBench

Reference: https://github.com/waynchi/gamedevbench

Relevant ideas:

- use real game-development tasks spanning gameplay, UI and multimodal assets,
- treat sprites, shaders, animation and visual scenes as first-class agent tasks,
- allow visual feedback/capture tooling for tasks where code-only feedback is insufficient,
- record task/agent/model configuration in a reproducible harness.

Trace2D lesson:

Visual feedback has real value for content work, but Trace2D should use it **after** structured engine-owned verification has handled everything that can be checked deterministically.

This reinforces Trace2D's judgment rule:

> Deterministic where possible. Multimodal where necessary. Human judgment at the end.

## 4. OpenGame

Reference: https://github.com/leigest519/OpenGame

Relevant ideas:

- end-to-end prompt-to-playable-game workflow,
- reusable Template Skill for stable project structures,
- Debug Skill that accumulates known repair strategies instead of repeatedly patching from scratch,
- execution-grounded game validation.

Trace2D lesson:

Consider an **Agent-side skill/recipe layer** for verified project patterns and repair knowledge once enough successful Trace2D projects exist.

Conceptually:

```text
Trace2D agent knowledge
- verified game patterns
- project templates
- diagnostic recipes
- repair recipes
```

This stays outside the engine runtime. Engine correctness must never depend on one model's hidden memory.

## 5. DreamGarden

Reference: https://www.microsoft.com/en-us/research/publication/dreamgarden-a-designer-assistant-for-growing-games-from-a-single-prompt/

Relevant ideas:

- decompose one high-level prompt into a visible hierarchical plan,
- let specialized modules execute parts of the plan,
- expose the plan/work to the human rather than hiding all orchestration,
- support human intervention through feedback/pruning rather than requiring manual low-level authoring.

Trace2D lesson:

The Workspace should make current intent, completed work, work in progress and review-needed items understandable to the user.

The human interaction should center on:

```text
Read -> Review -> Request -> Approve
```

not on redoing the AI's work manually.

## 6. Bitmagic

References:

- https://www.bitmagic.ai/
- https://www.bitmagic.ai/about/

Relevant ideas:

- explicit AI-first game-development positioning,
- high-level natural-language intent to playable result,
- recognition that an engine/platform may need to be redesigned when an existing engine constrains AI-driven modification.

Trace2D lesson:

Be clear and ambitious about the product identity instead of marketing Trace2D merely as an "agent-friendly C++ engine."

Trace2D difference:

The core differentiator is not prompt generation alone. It is the combination of:

```text
AI-operated authoring/iteration
+ deterministic structured self-verification
+ bounded multimodal review
+ human final judgment
+ measured autonomous benchmark evidence
```

## 7. Rosebud

Reference: https://rosebud.ai/make-your-own-game-online

Relevant ideas:

- prompt -> immediate playable preview,
- conversational revision while keeping the result visible,
- low-friction result-oriented iteration.

Trace2D lesson:

The Workspace should keep the generated/modified result close to the feedback interaction.

Trace2D difference:

A playable preview is presentation evidence, not the only verification mechanism. Trace2D should not require the user or AI to infer engine-owned correctness from the preview.

## 8. Dreamlab

Reference: https://docs.dreamlab.gg/quick-start/

Relevant observation:

AI assistance can be added to a conventional editor composed of scene graph, properties, behaviors, prefabs and assistant panels.

Trace2D lesson:

This is a useful contrast case. Trace2D intentionally does **not** make a traditional editor hierarchy/property-tuning loop its primary product interaction.

A limited world browser or inspector can exist, but the product center remains result review and natural-language revision over shared structured state.

## 9. Combined benchmark/product lesson

Trace2D should combine the strongest ideas without inheriting the wrong authority model:

```text
OpenGame
  end-to-end autonomous creation / reusable repair knowledge
+
godot-mcp
  deterministic playtest + structured runtime observation
+
GameCraft-Bench
  complete-project evaluation + replay + result dashboard
+
GameDevBench
  multimodal game-development task methodology
+
DreamGarden
  visible plan + human feedback/pruning
+
Trace2D-owned difference
  engine-native semantic truth + deterministic verification boundary
```

The result should not be "another game engine with an AI chat panel." It should be an engine where AI operation, verification and human result review are architectural requirements.

## 10. What Trace2D should explicitly avoid

Do not benchmark or design toward:

- screenshot/VLM inference for facts already owned by the engine,
- editor-only hidden state,
- hundreds of redundant MCP actions that duplicate safe text/source editing,
- one successful demo presented as an autonomy metric,
- unfair benchmark tasks that count missing engine capability as an agent failure,
- opaque aggregate AI quality scores that hide deterministic failures,
- automatic human-taste approval by a multimodal model,
- copying external dependencies without pinning/reviewing license/version/security implications.

## 11. Review cadence

Re-check these references when implementing #100/#102/#103/#104 or when the external projects materially change. Benchmark adapters must pin exact versions/commits instead of following mutable latest behavior.