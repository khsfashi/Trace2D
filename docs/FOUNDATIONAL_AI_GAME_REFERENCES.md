# Foundational AI-game references

Last reviewed: **2026-08-10**.

This document is the focused companion to [`REFERENCE_PROJECTS.md`](REFERENCE_PROJECTS.md) for the seven external references that first shaped Trace2D's AI-operated product and autonomous-benchmark direction:

1. `satelliteoflove/godot-mcp`,
2. OpenGame,
3. GameCraft-Bench,
4. GameDevBench,
5. DreamGarden,
6. Bitmagic,
7. Rosebud.

They are not treated as competitors to copy wholesale. Each solves a different part of the problem Trace2D is trying to unify: Agent control, interactive verification, end-to-end creation, benchmark methodology, multimodal feedback, human review and AI-first product design.

The operating rule remains [`EXTERNAL_REFERENCE_PROTOCOL.md`](EXTERNAL_REFERENCE_PROTOCOL.md): when an owning Trace2D stage activates, refresh the then-current primary sources before design freeze and classify concrete lessons as `ADOPT / ADAPT / REJECT / DEFER`.

These projects are **references, not dependencies**. No source, package, service or runtime code is vendored by this document.

---

## 1. `satelliteoflove/godot-mcp`

Primary reference:

- https://github.com/satelliteoflove/godot-mcp

### Why it matters

This is the clearest direct precedent for the question that originally motivated Trace2D:

> How can a coding Agent run a real game, control it, observe what happened and verify its own change without a human manually ferrying screenshots and logs?

The project explicitly emphasizes:

- deterministic/frozen playtesting,
- stepping controlled slices of game time,
- input injection into the running game,
- structured live entity/runtime state,
- runtime verification rather than editor mutation alone,
- an Agent-oriented evaluation harness in addition to the MCP server itself.

At the 2026-08-10 review, its public documentation exposes a broad editor/runtime tool surface and explicitly frames deterministic playtesting plus cheap structured observation as differentiators.

### `ADOPT`

Trace2D should keep:

- exact/explicit simulation stepping,
- semantic runtime inspection,
- virtual input on the same gameplay-facing path as physical input,
- self-verification without mandatory human screenshot/log shuttling,
- runtime tools only where file/source editing cannot provide equivalent reliable control.

### `ADAPT`

Godot-MCP retrofits these capabilities around an existing editor/runtime. Trace2D should instead make them protocol-independent engine contracts and expose MCP only as one adapter.

The engine remains authoritative; MCP is not allowed to become a second gameplay truth model.

### `REJECT`

Do not infer that Agent-friendliness requires:

- a large editor-action API for every property,
- graphical-editor state as the primary source of truth,
- screenshot inference when the engine already owns the semantic fact.

### Benchmark role

This is the most important initial comparison lane:

```text
Godot + generic coding tools
vs
Godot + pinned reviewed godot-mcp
vs
Trace2D
```

It isolates whether Trace2D gains anything beyond attaching a strong Agent bridge to a mature conventional engine.

### Trace2D stages to refresh it

- #98 verify/diagnose/repair,
- #102 B0 matched harness,
- #103 B1 visual/content benchmark,
- #104 B2 autonomous micro-game,
- any future MCP/Agent-surface redesign.

---

## 2. OpenGame

Primary references:

- https://github.com/leigest519/OpenGame
- https://arxiv.org/abs/2604.18394

### Why it matters

OpenGame attacks end-to-end game creation rather than isolated coding tasks. Its public framework/paper emphasizes a loop from high-level prompt to playable result and execution-grounded repair.

Two particularly relevant ideas are its reusable **Game Skill** components:

- **Template Skill** — successful project structures become reusable scaffolding,
- **Debug Skill** — verified error/fix knowledge is accumulated instead of repeatedly rediscovering the same repair.

Its evaluation direction also separates multiple qualities of the produced game rather than treating build success as sufficient.

### `ADOPT`

Trace2D should adopt the principle that:

```text
compile/build success
!=
playable, intent-satisfying game completion
```

The autonomous product loop should continue through execution, diagnosis, repair and re-verification.

### `DEFER`

A reusable Trace2D recipe/skill layer is intentionally deferred to #106.

Do **not** build it merely because OpenGame demonstrates the value of templates and repair memory. Trace2D first needs benchmark evidence from #102/#103/#104 showing repeated setup/debug failure classes where a verified recipe measurably reduces failures, iterations, tool calls or tokens.

### `REJECT`

Do not let reusable Agent memory become hidden engine correctness state. A recipe may accelerate an Agent, but every result must still use public Trace2D contracts and rerun normal deterministic verification.

### Benchmark role

Useful comparison dimensions include:

- one-shot creation vs iterative repair,
- first-build success vs final verified success,
- repeated failure-class frequency,
- whether reusable verified recipes lower cost without hiding correctness gaps.

### Trace2D stages to refresh it

- #97 intent/Definition of Done,
- #98 repair loop,
- #102/#103/#104 benchmark growth,
- #106 only if promoted by evidence.

---

## 3. GameCraft-Bench

Primary references:

- https://github.com/FreedomIntelligence/gamecraft-bench
- https://arxiv.org/abs/2606.17861

### Current reviewed shape

At the 2026-08-10 review, GameCraft-Bench publicly describes:

- **140 Godot tasks** across **15 game families**,
- complete project/artifact evaluation,
- replayable player-input traces supplied with submissions,
- a verifier that launches the game and replays those traces,
- hidden rubric-based evaluation of observed behavior,
- retained gameplay evidence,
- explicit token-usage reporting rather than only provider-dependent dollar cost.

The paper frames end-to-end game evaluation around:

```text
Engine Grounding
Artifact Completeness
Interactive Verification
```

### `ADOPT`

Trace2D should adopt all three ideas and add a fourth:

```text
Semantic Verifiability
```

A game should not pass merely because it launches or looks approximately right. Where the engine owns facts such as entity state, animation events, UI semantics or deterministic gameplay outcomes, the verifier should check those facts directly.

### `ADAPT`

Replay is essential, but Trace2D replay should not mean only recording player-visible video or relying only on observed output.

For deterministic Trace2D domains, preferred evidence is conceptually:

```text
execution identity
+ initial authoritative state/fingerprint
+ seed domains
+ ordered input/action/runtime operations
+ authoritative checkpoints/hashes
```

Screenshots/video remain presentation evidence.

### `REJECT`

Do not reduce Trace2D's benchmark to one rubric/VLM score. Deterministic failures, presentation findings and human subjective judgment remain separate result layers.

### Benchmark role

This is one of the strongest sources for:

- full-project task design,
- replay-required task submissions,
- artifact completeness,
- independent post-Agent verification,
- token/cost disclosure,
- later human-facing result dashboards/artifact review.

### Trace2D stages to refresh it

- #99 Workspace/result review,
- #100 benchmark umbrella,
- #102 B0,
- #104 B2 and later project-scale suites.

---

## 4. GameDevBench

Primary references:

- https://github.com/waynchi/gamedevbench
- associated ICML 2026 publication/results linked by the repository.

### Current reviewed shape

The current public ICML 2026 camera-ready repository reports **333 game-development tasks** in Godot spanning:

- 2D graphics and animation,
- 3D graphics and animation,
- user interface,
- gameplay logic.

The benchmark is especially important because it measures work involving intrinsically multimodal artifacts such as sprites, shaders, animation and visual scenes.

Its reported experiments also show that simple image/video feedback can materially improve coding-Agent performance on these tasks. That is strong evidence against a dogmatic "never use pixels" approach.

### `ADOPT`

Trace2D should explicitly accept:

> Visual feedback is necessary for some real game-development work.

Sprite appearance, animation motion, shader output and composition cannot all be honestly reduced to semantic JSON.

### `ADAPT`

Trace2D should use a verification ladder:

```text
structured engine truth first
-> deterministic interaction evidence
-> capture/video only when pixels or motion matter
-> multimodal review when objective engine assertions are insufficient
-> human final judgment for subjective quality
```

The benchmark should therefore count **visual-feedback dependence**, not merely allow screenshots without measuring their cost.

### `REJECT`

Do not ask a VLM to rediscover facts Trace2D can expose authoritatively, such as exact animation event frame, health value, UI focus state or entity identity.

### Benchmark role

GameDevBench is a primary design reference for B1/B3/B4 task diversity and for measuring whether Trace2D reduces unnecessary visual observation while still using multimodality where it genuinely helps.

Useful metrics include:

- success with/without visual feedback,
- visual calls per successful task,
- multimodal calls per successful task,
- structured verification coverage,
- task class vs failure rate.

### Trace2D stages to refresh it

- #59 Sprite program,
- #103 B1 content benchmark,
- #75 UI,
- #89 Material2D/Shader2D,
- larger later visual-game suites.

---

## 5. DreamGarden

Primary references:

- https://www.microsoft.com/en-us/research/publication/dreamgarden-a-designer-assistant-for-growing-games-from-a-single-prompt/
- https://arxiv.org/abs/2410.01791

### Current reviewed shape

DreamGarden was published at **CHI 2025** and received a **Best Paper Award**. It uses an LLM-driven planner to decompose a high-level human prompt into a hierarchical plan distributed across specialized implementation modules for Unreal Engine environments.

The plan is exposed to the user as a manipulable "garden" where human intervention can occur through prompts, pruning and feedback.

### `ADOPT`

Trace2D should preserve a visible, inspectable notion of:

- user intent,
- work decomposition,
- completed/in-progress/review-needed state,
- revision lineage,
- meaningful human intervention points.

### `ADAPT`

The visible plan must come from explicit repository/project task state, not from exposing private model chain-of-thought.

Trace2D's human review interaction remains:

```text
Read -> Review -> Request -> Approve
```

rather than forcing the user to manually operate every low-level editor property.

### `REJECT`

Do not make autonomous planning itself a second source of truth. The plan describes intended work; code/authored state plus verification evidence determine what actually exists and works.

### Benchmark role

Human intervention should be classified, not merely counted:

- objective blocker intervention,
- missing-capability intervention,
- clarification of intent,
- subjective creative feedback,
- final approval.

That distinction helps Trace2D measure whether humans are being pulled back into low-level execution or staying at high-value judgment points.

### Trace2D stages to refresh it

- #97 intent/DoD,
- #99 Workspace,
- #104 human-feedback revision cycle,
- future higher-level Agent planning UX.

---

## 6. Bitmagic

Primary references:

- https://www.bitmagic.ai/
- https://www.bitmagic.ai/about/

### Why it matters more than a marketing comparison

Bitmagic is a particularly relevant product precedent because its own public history describes an architectural pivot.

Its September 2025 history states that the Unity-based web build became too limiting for its desired AI workflow, so the team moved to a **custom WebGL engine using TypeScript and Three.js**, explicitly to gain control over the code and let AI modify rules, environments, mechanics and assets more freely.

That is unusually direct external evidence for Trace2D's core thesis:

> Sometimes making AI the primary operator changes what the engine itself needs to own and expose.

### `ADOPT`

Trace2D should remain willing to redesign conventional engine boundaries when they materially obstruct:

- machine-readable authoring,
- deterministic execution/control,
- structured verification,
- rapid explicit revision,
- result-oriented human feedback.

### `ADAPT`

Trace2D's reason for building a custom engine is narrower and more falsifiable than "AI can modify everything."

The project should justify custom-engine cost through measurable claims such as lower verification ambiguity, fewer visual guesses, lower intervention, lower repair cost or stronger reproducibility.

### `REJECT`

Do not use "AI-first" as sufficient evidence of technical superiority. Product demos and ease of prompting are not substitutes for reproducible benchmark results, explicit determinism domains or performance evidence.

### Benchmark role

Bitmagic is mainly a **product/architecture precedent**, not an initial open benchmark baseline.

It strengthens the rationale for measuring whether an AI-native engine architecture produces benefits that an Agent bridge over a conventional engine cannot fully recover.

### Trace2D stages to refresh it

- architecture/product reviews,
- #100 benchmark interpretation,
- flagship proof / README positioning,
- any future debate about whether a conventional editor-first assumption should be preserved.

---

## 7. Rosebud

Primary references:

- https://rosebud.ai/
- https://rosebud.ai/make-your-own-game-online
- https://lab.rosebud.ai/blog/beginner-guide

### Current reviewed shape

Rosebud's public workflow emphasizes a low-friction loop:

```text
prompt
-> generated playable result
-> play/preview immediately
-> describe changes conversationally
-> see the updated game close to the conversation
```

Its product materials explicitly place AI conversation and the playable result together rather than separating generation from review by a long manual tool chain.

### `ADOPT`

Trace2D Workspace should minimize the distance between:

- what the Agent changed,
- deterministic verification results,
- screenshots/video/presentation artifacts,
- known failures/limitations,
- the user's revision request.

### `ADAPT`

A playable preview is valuable UX but remains **presentation evidence**.

Trace2D's stronger loop is:

```text
AI change
-> structured verification
-> presentation artifact
-> human review
-> revision request
```

The user should not need to infer objective correctness merely by playing or looking at the preview.

### `REJECT`

Do not make browser-style instant preview the engine's only correctness oracle, and do not optimize away reproducible build/test/evidence merely to make the feedback loop feel immediate.

### Benchmark role

Rosebud is most useful for evaluating review-loop friction:

- time/tool calls from Agent completion to reviewable result,
- number of manual steps required to inspect output,
- revision-turn cost,
- whether objective verification evidence is visible beside presentation artifacts.

### Trace2D stages to refresh it

- #99 Workspace,
- #104 human-feedback revision cycle,
- future distribution/preview UX.

---

## 8. Combined lesson from the foundational seven

These seven references cover different layers and should not be flattened into one "competitor" category.

| Reference | Primary lesson for Trace2D | Main role |
| --- | --- | --- |
| `godot-mcp` | deterministic play/control + structured state + Agent self-verification | direct Agent-tooling comparison |
| OpenGame | end-to-end generation + execution-grounded repair + reusable verified knowledge | autonomous creation/repair |
| GameCraft-Bench | complete artifact + replay + independent interactive evaluation + cost evidence | benchmark methodology |
| GameDevBench | real multimodal game-development tasks + visual feedback has measurable value | multimodal benchmark methodology |
| DreamGarden | visible plan + meaningful human feedback/pruning | human/Agent workflow |
| Bitmagic | AI-first requirements can justify custom engine architecture | product/architecture precedent |
| Rosebud | prompt/play/revise loop with result kept near feedback | review UX precedent |

Trace2D's intended synthesis is not to imitate any one of them:

```text
godot-mcp
    Agent can control and inspect a real engine
+
OpenGame
    Agent owns creation/repair loops
+
GameCraft-Bench
    complete playable result is independently evaluated
+
GameDevBench
    multimodal feedback is used when visual work genuinely needs it
+
DreamGarden
    human intent/feedback stays visible and meaningful
+
Bitmagic
    AI operation is allowed to shape engine architecture
+
Rosebud
    review and revision stay close to the result
+
Trace2D-specific contract
    deterministic semantic authority
    + exact-frame input/assert/replay
    + explicit verification escalation
    + independent matched harness evidence
    + human final judgment
```

---

## 9. What these seven must *not* cause

Do not use these precedents to justify:

- feature-chasing every external Agent tool,
- cloning a conventional editor because another system has one,
- creating hundreds of narrow MCP mutation actions,
- adding an engine-integrated proprietary LLM dependency,
- letting multimodal judgment override deterministic failure,
- hiding task-specific benchmark shortcuts in Trace2D,
- building #106 recipe/skill infrastructure before evidence justifies it,
- claiming AI-first superiority before matched multi-run benchmark evidence exists.

The current owner-fixed development order remains authoritative. External references improve the implementation of the active stage; they do not silently reorder it.

---

## 10. Refresh rule

These seven are foundational, not frozen truth.

Before using one to justify a substantive design or published comparison:

1. re-open the current primary project/paper/product source,
2. record the reviewed version/commit/date when material,
3. check whether its architecture or benchmark methodology changed,
4. update the `ADOPT / ADAPT / REJECT / DEFER` decision if new evidence warrants it,
5. pin exact versions/commits for any actual benchmark adapter/run.

In particular, benchmark statistics and product capabilities are time-sensitive and must not be quoted from memory after the cited project has materially changed.
