# Reference projects, papers, and engineering precedents

Last reviewed: 2026-08-10.

This document is Trace2D's curated starting registry for public work that can inform the engine, Agent workflow and benchmark harness. It is intentionally broader than "projects similar to Trace2D": some of the most useful ideas come from databases, physics engines, TAS tooling, software-verification systems and agent-evaluation frameworks.

These sources are **references, not runtime dependencies**. Trace2D should borrow validated ideas and techniques, not copy architectures that conflict with its own authority, performance or product model.

`docs/EXTERNAL_REFERENCE_PROTOCOL.md` defines how every substantive future `Trace2D next/continue` task must refresh relevant current sources and convert lessons into `ADOPT / ADAPT / REJECT / DEFER` decisions before freezing design.

Any vendoring, redistribution, executable dependency or benchmark integration still requires an explicit current version/license review.

## 1. Core lesson

Trace2D should combine four bodies of prior art:

```text
AI/game-development systems
    how agents author, inspect, run and repair games
+
interactive/game benchmarks
    how playable behavior is evaluated rather than compilation alone
+
deterministic systems/replay research
    how a failure becomes reproducible authoritative evidence
+
benchmark-harness methodology
    how model, harness, verifier, budget and environment are separated fairly
```

The intended Trace2D-owned synthesis remains:

```text
AI-operated authoring/iteration
+ deterministic structured self-verification
+ authoritative replay/evidence where possible
+ bounded visual/multimodal review where necessary
+ human final judgment
+ independent matched benchmark evidence
```

## 2. AI-native / agent-operated engine references

### 2.1 satelliteoflove/godot-mcp

Reference: https://github.com/satelliteoflove/godot-mcp

Useful ideas:

- deterministic/frozen playtest control,
- structured runtime state instead of pure screenshot inference,
- live input injection,
- runtime actions focused on things ordinary file editing cannot provide reliably.

Trace2D adoption:

- exact stepping and semantic state belong in the engine-owned Agent surface,
- MCP remains an adapter rather than gameplay authority,
- structured observation precedes visual guessing.

Trace2D difference: the Agent bridge is not retrofitted around an editor-first source of truth; the underlying runtime is designed for this workflow.

### 2.2 godot-mcp-runtime

Reference: https://github.com/Erodenn/godot-mcp-runtime

Useful ideas:

- headless scene editing,
- screenshot/input/runtime control,
- live UI/runtime discovery,
- another independent Godot Agent bridge that can serve as a benchmark/control reference.

Trace2D use: useful for distinguishing "one unusually good Godot MCP implementation" from structural advantages of an engine-native Agent contract.

### 2.3 nAIVE Engine

References:

- https://naive.dev/whitepaper.html
- https://github.com/naive-engine/naive

Useful ideas:

- explicit AI-native engine thesis,
- text/declarative authored content,
- headless AI testing,
- MCP command/control,
- designing engine iteration around machine-readable state rather than treating AI as a chat add-on.

Trace2D adoption: treat AI operability as an engine architecture requirement.

Trace2D difference: avoid relying on marketing claims such as generic "100x" iteration speed; Trace2D's strongest differentiator should be measured authoritative state, exact-frame behavior and reproducible benchmark evidence.

### 2.4 Summer Engine Agent

Reference: https://github.com/SummerEngine/summer-engine-agent

Useful ideas:

- Agent skills/playbooks plus MCP/CLI rather than one monolithic magic tool,
- setup/doctor diagnostics,
- diagnose before editing,
- a verification ladder that escalates from cheap script diagnostics to screenshot/runtime verification only when needed,
- hidden disposable `RunVerification`-style execution for behavior probes,
- structured failure reasons and project identity checks.

Trace2D adoption:

- use the cheapest authoritative verification layer that answers the question,
- make diagnose/repair/re-verify explicit,
- consider verified Agent-side recipes later (#106), but only when benchmark evidence proves repeated failure classes.

Trace2D difference: engine semantic truth should remain independent of an Agent skill bundle and should not require a closed desktop runtime.

### 2.5 UnityCodeMCPServer

Reference: https://github.com/Signal-Loop/UnityCodeMCPServer

Useful ideas:

- live component/state inspection,
- Play Mode control,
- simulated input,
- screenshots,
- EditMode/PlayMode tests.

Trace2D use: useful as a later mature-engine + Agent-bridge comparison/control, not an initial required benchmark environment.

### 2.6 Roblox Studio MCP / playtest automation

Primary reference: https://github.com/Roblox/creator-docs/blob/main/content/en-us/studio/mcp.md

Useful ideas:

- official engine-side Agent integration,
- automated playtest/input workflows,
- runtime debugging becoming a first-class engine/tooling concern.

Community implementations can provide additional ideas such as per-peer logs and multiplayer test orchestration, but official Roblox documentation should carry more architectural weight.

### 2.7 RE Engine MCP

Reference: https://github.com/praydog/re-engine-mcp

Useful ideas:

- live structured inspection through reflection/runtime state,
- direct player/enemy/world state inspection instead of inferring everything from pixels,
- hot-reload/verification loops in a complex existing game runtime.

Trace2D lesson: visual games still benefit strongly from structured debugging truth.

## 3. AI game creation / interactive verification references

### 3.1 OpenGame

Reference: https://github.com/leigest519/OpenGame

Useful ideas:

- end-to-end prompt -> game -> execution -> repair loop,
- reusable template/repair knowledge,
- execution-grounded validation.

Trace2D adoption: keep verified recipes/templates as a later Agent knowledge layer, outside gameplay authority and only when benchmark evidence justifies them (#106).

### 3.2 PlayCoder / PlayEval / PlayTester

References:

- https://arxiv.org/abs/2604.19742
- https://github.com/Tencent/PlayCoder

Useful ideas:

- compilation success is weaker than interactive correctness,
- evaluate action sequences and state transitions,
- distinguish execution/test success from true playability,
- closed generate -> playtest -> targeted repair loop.

Trace2D adoption:

- preserve layered correctness instead of one aggregate success bit,
- benchmark whether a result runs, satisfies deterministic acceptance, and behaves correctly under interaction,
- use engine-owned state wherever it can replace an LLM playtester's visual inference.

### 3.3 PlaytestArena / Play2Code

Reference: https://arxiv.org/abs/2605.28258

Useful ideas:

- coding agent and playtesting agent form a continuing feedback loop,
- evaluation rubrics are based on in-play behavior,
- game generation is not complete until interaction-level behavior is checked.

Trace2D lesson: compare that pixel/GUI-agent loop against Trace2D's engine-native semantic verification to measure how much external visual reasoning can be avoided.

### 3.4 DreamGarden

Reference: https://www.microsoft.com/en-us/research/publication/dreamgarden-a-designer-assistant-for-growing-games-from-a-single-prompt/

Useful ideas:

- visible hierarchical planning,
- specialized execution modules,
- human feedback/pruning at meaningful checkpoints.

Trace2D adoption: Workspace/result review should expose explicit project/task state without exposing private model chain-of-thought.

### 3.5 Bitmagic

References:

- https://www.bitmagic.ai/
- https://www.bitmagic.ai/about/

Useful idea: explicit AI-first game-platform positioning and high-level intent-to-playable-result product framing.

Trace2D difference: the project should prove its AI-first claim through committed reproducible evidence, not prompt-demo positioning alone.

### 3.6 Rosebud

Reference: https://rosebud.ai/make-your-own-game-online

Useful ideas: prompt -> playable preview -> conversational revision with the result kept close to the feedback loop.

Trace2D adoption: Workspace should make review artifacts and revision requests low friction; preview remains presentation evidence, not semantic authority.

### 3.7 Dreamlab

Reference: https://docs.dreamlab.gg/quick-start/

Useful contrast: AI assistance can be layered onto a conventional scene/property/editor workflow.

Trace2D lesson: a useful world browser/inspector may exist, but a traditional manually editable inspector should not become the product center or a second authoritative database.

## 4. Game-development benchmark references

### 4.1 GameCraft-Bench

References:

- https://github.com/FreedomIntelligence/gamecraft-bench
- https://arxiv.org/abs/2606.17861

Useful ideas:

- complete launchable game-project evaluation,
- replayed player inputs,
- engine grounding and artifact completeness,
- interactive verification and review artifacts,
- token/cost evidence.

Trace2D extension: add **semantic verifiability** as a separate axis because engine-owned facts can be checked directly.

### 4.2 GameDevBench

Reference: https://github.com/waynchi/gamedevbench

Useful ideas:

- real game-development tasks across gameplay/UI/sprites/shaders/animation,
- multimodal feedback where visual output genuinely matters,
- reproducible task/agent/model configuration.

Trace2D adoption: content benchmarks must include visual work, but visual review is layered after structured verification.

### 4.3 JAMER / JamBench

Reference: https://arxiv.org/abs/2606.19830

Useful ideas:

- professional-engine, project-level game coding rather than snippets,
- Godot text format + headless execution used for deterministic verification,
- verification pipeline from file integrity through runtime behavior,
- separate structural completeness and behavioral alignment,
- task difficulty rises sharply with project scale.

Trace2D adoption:

- B2/B3/B4 should measure project-level coherence, not just isolated API exercises,
- grow difficulty by project scale and dependency depth,
- distinguish compilation, structural completeness and runtime behavior.

### 4.4 V-GameGym

Reference: https://aclanthology.org/2026.findings-acl.276/

Useful ideas:

- broad visual-game task corpus,
- clustered task diversity,
- multimodal evaluation of dynamic/visual output.

Trace2D use: task-taxonomy and visual-quality methodology for later B1/B4 growth, while keeping engine-owned correctness deterministic.

### 4.5 Cutscene Agent / CutsceneBench

Reference: https://arxiv.org/abs/2604.25318

Useful ideas:

- bidirectional engine state through MCP,
- long-horizon dependent tool orchestration,
- visual reasoning as part of content-production QA.

Trace2D use: later Sprite/animation/content workflows should test long dependent sequences, not only single-tool correctness.

### 4.6 OmniGameArena

Reference: https://arxiv.org/abs/2606.09826

Useful ideas:

- unified action interface across games,
- evaluate improvement across reflection/repair rounds rather than final score alone,
- measure how quickly feedback turns into performance improvement.

Trace2D adoption: retain success-vs-iteration and success-vs-cost curves instead of reporting only end-state success.

### 4.7 AgencyBench

Reference: https://github.com/GAIR-NLP/AgencyBench

Useful idea: long-context, persistent, tool-using agent scenarios with diagnostic evaluation. Use mainly as a general long-horizon Agent reference rather than a direct game-engine benchmark.

## 5. Agent harness / evaluator methodology

### 5.1 Harness-Bench

Reference: https://arxiv.org/abs/2605.27922

This is one of the most important methodological references for Trace2D.

Useful ideas:

- evaluate **model + harness configuration**, not model alone,
- hold task environments, budgets and evaluation protocols constant while varying harness behavior,
- record final artifacts, execution traces, usage and validator outputs,
- analyze execution-alignment failures rather than only final pass/fail.

Trace2D adoption: the central Godot-generic vs Godot-MCP vs Trace2D experiment is explicitly a matched harness/environment study. Agent/model identity must be pinned and reported separately from engine/adapter identity.

### 5.2 Claw-SWE-Bench

References:

- https://arxiv.org/abs/2606.12344
- https://github.com/opensquilla/claw-swe-bench

Useful ideas:

- heterogeneous Agent harnesses become comparable through a fixed prompt, runtime budget, workspace contract, patch extraction and evaluator,
- harness choice can materially change success under the same model,
- full and low-cost subsets serve different iteration needs.

Trace2D adoption:

- adapters normalize only what is required for fair execution,
- B0 should have a small calibration/smoke subset and larger publishable suite later,
- adapter design/version is first-class benchmark metadata.

### 5.3 ADK Arena

Reference: https://arxiv.org/abs/2606.05548

Useful ideas:

- hold the LLM developer constant and vary the framework,
- measure generation/repair effort as an API-usability signal,
- isolate frameworks and validate at multiple levels.

Trace2D adoption: exactly the same coding agent should learn/use each compared environment; iteration/token/tool cost is evidence about developer-facing usability, not just task correctness.

### 5.4 SWE-bench evaluation harness

Reference: https://www.swebench.com/SWE-bench/reference/harness/

Useful ideas:

- isolated reproducible task environments,
- layered cached base/environment/instance images,
- apply artifact -> execute verifier -> grade -> report,
- validate evaluator setup with gold solutions.

Trace2D adoption:

- Agent workspace and independent verifier are separate concerns,
- benchmark environment setup should be cached without contaminating task state,
- known-good solutions validate the harness before candidate runs.

### 5.5 Inspect AI

References:

- https://inspect.aisi.org.uk/
- https://inspect.aisi.org.uk/tracing.html
- https://inspect.aisi.org.uk/scoring.html

Useful ideas:

- separate task/solver/tools/sandbox/scorer/log concepts,
- JSONL traces around model calls, subprocesses, sandbox commands, tool calls and subtasks,
- scorer can be rerun independently from generation,
- preserve errored/partial runs and distinguish infrastructure failure from scored outcomes.

Trace2D adoption: benchmark execution trace, verifier/scorer and report should be separable so a run can be inspected or rescored without rerunning an expensive Agent trajectory when the underlying artifact is sufficient.

### 5.6 Meta-Agent Challenge

References:

- https://github.com/ant-research/meta-agent-challenge
- https://arxiv.org/abs/2606.04455

Useful ideas:

- sealed development/evaluation separation,
- explicit wall-clock and API budgets,
- development feedback followed by held-out verifier,
- evaluation-integrity defenses against reward hacking/test leakage.

Trace2D adoption: hidden acceptance tests must remain inaccessible to the Agent when they are intended as independent evidence; benchmark artifacts must not contain privileged shortcuts.

### 5.7 Harbor

Reference: https://github.com/harbor-framework/harbor

Useful ideas:

- arbitrary Agent adapters against versioned benchmark environments,
- Docker/local/cloud execution abstraction,
- scalable repeated trials.

Trace2D boundary: study the orchestration model; do not make Harbor a mandatory engine dependency without a concrete need/license/deployment review.

### 5.8 Exgentic

References:

- https://github.com/Exgentic/exgentic
- https://arxiv.org/abs/2602.22953

Useful ideas:

- unified protocol for evaluating different general agents,
- explicit per-run/per-session configuration,
- `trajectory.jsonl` plus OpenTelemetry-compatible traces,
- compare Agent and benchmark adapters through a common runner.

Trace2D adoption: a trial should preserve machine-readable action/observation trajectory and configuration independently of the aggregate report.

### 5.9 AgentKernelArena

Reference: https://github.com/AMD-AGI/AgentKernelArena

Useful ideas:

- controlled A/B comparisons across model/prompt/tools/MCP/skills,
- objective compilation/correctness/performance verifier independent from the optimizing Agent,
- held-out evaluation,
- resumable timestamped workspaces and structured reports.

Trace2D use: strong reference for later benchmark A/B experimentation and performance-aware agent tasks.

### 5.10 OmniaBench / AgentOmnia

References:

- https://arxiv.org/abs/2607.14989
- https://arxiv.org/abs/2607.23124

Useful ideas:

- explicit task state spaces,
- domain/capability/difficulty taxonomies,
- smaller challenging subset for cheaper iteration,
- feed evaluation failures back into targeted product requirements rather than treating benchmark results as a static leaderboard.

Trace2D adoption: benchmark failure taxonomy should inform future engine/Agent gaps, but benchmark evidence does not automatically reorder the owner-fixed core lane.

### 5.11 Frontier-Bench task validation

Reference: https://github.com/harbor-framework/frontier-bench

Useful idea: run oracle solutions repeatedly before trusting a task; current instructions run the oracle five times to expose environmental flakiness.

Trace2D adoption: benchmark tasks/verifiers need self-validation with known-good implementations and repeated runs before publication.

## 6. Deterministic simulation, replay and randomized testing

### 6.1 FoundationDB Simulation

References:

- https://apple.github.io/foundationdb/testing.html
- https://apple.github.io/foundationdb/client-testing.html

Useful ideas:

- deterministic simulation of a complete system,
- simulated time and controlled randomness/faults,
- massive repeated runs,
- failing seed enables exact investigation when nondeterminism stays controlled,
- real hardware/performance testing complements simulation rather than being replaced by it.

Trace2D adoption:

- deterministic simulation and real GPU/platform evidence are complementary tiers,
- seed is first-class evidence but only within a pinned execution identity,
- simulation should accelerate coverage without pretending to prove hardware-specific behavior.

### 6.2 Dropbox Nucleus / Trinity

Reference: https://dropbox.tech/infrastructure/-testing-our-new-sync-engine

Useful ideas:

- route randomized choices through one deterministic PRNG,
- record both seed and code revision,
- mock external filesystem/network/time for controlled randomized testing,
- rerun the same seed and assert the same final state to test the **test harness's own determinism**.

Trace2D adoption:

- future generated/random gameplay sequences should record exact action traces plus seed/revision,
- add harness self-replay checks rather than assuming determinism because a seed exists.

### 6.3 TigerBeetle VOPR / Vörtex

References:

- https://tigerbeetle.com/blog/2023-07-06-simulation-testing-for-liveness/
- https://tigerbeetle.com/blog/2025-02-13-a-descent-into-the-vortex/

Useful ideas:

- deterministic fault-injection simulation checks explicit safety/liveness properties,
- randomized/protocol-aware tests inspect invariants continuously,
- defense in depth: deterministic simulation is powerful but does not replace every real-world/non-deterministic test layer.

Trace2D adoption: deterministic headless truth should coexist with real-GPU/platform tests and explicit invariants; no one QA mode is universal proof.

### 6.4 Box2D determinism and replay

References:

- https://box2d.org/posts/2024/08/determinism/
- https://box2d.org/posts/2026/06/replay/

Critical ideas:

- determinism is a first-class engineering property and must be regression tested,
- replay can be represented as **initial full snapshot + ordered API/input operations + state hashes** rather than full snapshots every step,
- keyframes can be generated for seek speed under a memory budget,
- exact reproduction depends on deterministic application/game code too,
- generation-safe index handles and relocatable/POD-like data make snapshots practical.

Trace2D adoption:

```text
replay artifact
= execution identity
+ initial authoritative state
+ seed
+ ordered external inputs/actions
+ periodic authoritative hashes
+ optional runtime-generated keyframes
```

Visual frames/videos are review artifacts, not the authoritative replay itself.

### 6.5 libTAS

Reference: https://clementgallet.github.io/libTAS/guides/how/

Useful ideas:

- deterministic timer interception,
- frame advance,
- input recording,
- savestates/rewind for existing games.

Trace2D lesson: because Trace2D owns the runtime, features that TAS tools must inject externally can be designed directly into the authoritative simulation/test boundary.

### 6.6 BizHawk / TAS tooling

Reference: https://github.com/TASEmulators/BizHawk

Useful ideas:

- frame stepping,
- savestates and rewind,
- memory/state inspection,
- "movie" as deterministic frame/input history rather than merely encoded video.

Trace2D adoption: distinguish authoritative replay/input artifacts from screenshots/video presentation evidence.

### 6.7 Coyote

References:

- https://microsoft.github.io/coyote/concepts/concurrency-unit-testing/
- https://microsoft.github.io/coyote/get-started/using-coyote/

Useful ideas:

- take control of scheduler/non-deterministic choices,
- systematically explore executions,
- dump and deterministically replay the exact failing trace,
- unsupported uncontrolled concurrency is an explicit reproducibility boundary.

Trace2D use: future concurrency should not silently destroy deterministic tests; scheduling nondeterminism either stays outside the claimed deterministic domain or gets controlled/recorded.

### 6.8 Shuttle

Reference: https://docs.rs/shuttle/latest/shuttle/

Useful ideas:

- randomized controlled concurrency scheduling for scalability,
- encoded schedules for exact replay,
- explicit checker for uncontrolled nondeterminism that replays schedules.

Trace2D use: strong future reference if worker concurrency enters authoritative systems.

### 6.9 Hypothesis stateful testing

Reference: https://hypothesis.readthedocs.io/en/latest/stateful.html

Useful ideas:

- generate whole sequences of primitive actions, not just scalar inputs,
- assert invariants after steps,
- shrink a long failing sequence toward a short reproducer.

Trace2D adoption opportunity:

- randomized Agent/gameplay QA can eventually minimize a hundreds-frame failure into a small deterministic input/action trace that is cheaper for both humans and LLMs to debug.

### 6.10 QuickCheck family

Reference: https://hackage.haskell.org/package/QuickCheck

Useful ideas: property-based generation and shrinking/minimal counterexamples.

Trace2D lesson: preserve the concrete minimized failing action sequence as the durable reproduction artifact; do not rely only on an RNG seed whose meaning can change with generator/tool versions.

### 6.11 Mull mutation testing

Reference: https://mull.readthedocs.io/en/latest/

Useful ideas:

- deliberately mutate C/C++ behavior,
- measure whether the existing test suite kills the mutation.

Trace2D benchmark adoption: known-bad/mutated benchmark solutions can validate that an independent verifier rejects meaningful failures instead of merely producing optimistic PASS results.

## 7. Reliable performance, fairness and visual methodology

### 7.1 BenchExec

Reference: https://gitlab.com/sosy-lab/software/benchexec

Useful ideas:

- reliable CPU/wall/memory measurement including subprocesses,
- CPU/resource limits and pinning,
- process/environment isolation,
- aggregate tables/plots from repeated runs.

Trace2D adoption: benchmark timings need environment metadata and controlled resource conditions; simple `time()` measurements are not enough for publishable performance claims. BenchExec is Linux-oriented, so its methodology does not imply it should become a Windows Trace2D dependency.

### 7.2 MLPerf rules

Reference: https://github.com/mlcommons/inference_policies/blob/master/inference_rules.adoc

Useful ideas:

- fairness is explicit policy,
- fixed controlled randomness,
- benchmark detection and input-specific optimization are prohibited,
- replicability is mandatory,
- benchmark implementations/rules remain inspectable.

Trace2D adoption:

- no benchmark-only engine path,
- no task-specific hidden shortcut,
- pin versions/seeds and publish enough harness/task code to reproduce claims,
- never cherry-pick only favorable trials.

### 7.3 Playwright visual comparison

References:

- https://playwright.dev/docs/api/class-pageassertions
- https://playwright.dev/docs/test-snapshots

Useful ideas:

- stabilize visual capture before comparison,
- disable/normalize irrelevant animation where appropriate,
- separate exact/pixel-difference tolerances from semantic assertions,
- record the environment because rendering can differ across OS/hardware/configuration.

Trace2D adaptation: exact-frame authoritative capture should be stricter than browser-style "wait until visually stable" where Trace2D controls simulation; perceptual/tolerance comparison remains a presentation-layer concern.

### 7.4 Game-engine determinism research

Reference: https://arxiv.org/abs/2104.06262

Useful idea: determinism claims need a clearly defined domain and permissible variance; hardware/engine configuration can affect observed results.

Trace2D adoption: never collapse semantic deterministic state, GPU floating-point behavior and final cross-vendor pixels into one universal determinism claim.

## 8. Stage-to-reference map for `Trace2D next/continue`

This map is a **starting point**. The active Agent must still search for current/new sources according to `docs/EXTERNAL_REFERENCE_PROTOCOL.md`.

| Trace2D stage | Minimum reference classes to refresh |
| --- | --- |
| **#53 particle CPU/GPU conformance/workloads** | Box2D determinism/replay; FoundationDB simulation-vs-real evidence; BenchExec/MLPerf measurement discipline; current GPU/vendor/SDL docs |
| **#97 intent / Definition of Done** | DreamGarden; Inspect task/sample contracts; Summer project memory/playbooks; current Agent workflow literature |
| **#98 verify/diagnose/repair/WorkResult** | Inspect trace/scoring/retry; Summer verification ladder; PlayCoder repair loop; SWE-bench independent grading |
| **#99 Workspace / feedback** | GameCraft result/dashboard ideas; DreamGarden visible work; Rosebud result-near-feedback; Inspect log viewer |
| **#102 B0 benchmark harness** | Harness-Bench; Claw-SWE-Bench; ADK Arena; SWE-bench; Inspect; Meta-Agent Challenge; Harbor/Exgentic; FoundationDB/Dropbox/Box2D; BenchExec/MLPerf; Frontier-Bench; Mull |
| **#59 Sprite program** | GameDevBench; V-GameGym; Summer visual verification; Playwright visual stability; mature 2D engine sprite/atlas/import docs current at task time |
| **#103 B1 content benchmark** | GameDevBench; V-GameGym; PlayCoder; PlaytestArena/Play2Code; CutsceneBench; Box2D-style replay separation; Playwright |
| **#69/#70 external project boundary/package** | JAMER/JamBench; mature CMake/package projects; SWE-bench reproducible environment patterns; current platform packaging docs |
| **#71/#86/#87 scene/components/resources/world** | JAMER project-scale lessons; mature engine scene/resource lifecycle docs; FoundationDB/Dropbox identity/reproducibility lessons where applicable |
| **#88/#72/#73/#74/#75 camera/input/tile/text/UI** | Godot/Unity/Roblox official production docs; GameDevBench/GameCraft/JAMER tasks; accessibility/structured UI precedents; current Unicode/font libraries when selected |
| **#104 B2 autonomous micro-game** | GameCraft-Bench; JAMER; PlayCoder; PlaytestArena; Harness-Bench/Claw-SWE; AgentKernelArena; OmniaBench task taxonomy |
| **#89/#90 material/tween** | current mature 2D engine/rendering docs; GameDevBench visual tasks; Playwright/tolerance methodology; setup-resolved property precedents |
| **#76 Physics2D** | Box2D official current docs + determinism/replay; game-engine determinism research; property/stateful testing |
| **#77 Audio** | current engine/audio backend docs; semantic-vs-perceptual evaluation methodology |
| **#91 profiler/diagnostics** | BenchExec; Inspect traces; mature engine profiler schemas; AgentKernelArena performance evidence |
| **#78 Linux/non-MSVC hardening** | FoundationDB warning that seed alone is not cross-build identity; compiler/sanitizer official docs; reproducible-build practices |
| **#92 real-GPU conformance** | MLPerf-style disclosure/fairness; Box2D determinism-domain lesson; current SDL/GPU/backend/vendor documentation; platform-specific real hardware evidence |
| **#79 persistence/migration** | FoundationDB/Dropbox fault/replay thinking; mature versioned schema/migration projects; property/stateful tests |
| **#106 verified recipes/skills if promoted** | OpenGame template/debug skills; Summer skills/playbooks; benchmark evidence from #102/#103/#104 proving repeated failure classes |

## 9. Durable adoption shortlist

Across all references reviewed so far, the strongest reusable principles are:

1. **Model performance is inseparable from harness configuration** — report the model and harness/engine/adapter together.
2. **Agent and verifier must be separate** — self-declared "done" is not benchmark success.
3. **Seed is first-class but insufficient alone** — keep code/build/environment identity and the concrete ordered action/tool trace.
4. **Replay is authoritative execution evidence, not video** — initial state + ordered operations + state hashes is the preferred conceptual model.
5. **Test the harness's determinism** — replay the same recorded execution and verify the same authoritative outcome.
6. **Validate the verifier** — known-good/oracle solutions must pass repeatedly; known-bad/mutated solutions must fail.
7. **Generate action sequences and shrink failures** where randomized stateful testing provides value.
8. **Separate semantic truth, presentation evidence and human judgment** rather than blending them into one AI score.
9. **Measure process, not only completion** — iterations, repairs, token/tool cost, visual dependence, human intervention and failure class all matter.
10. **Use controlled environments for timing/resource claims** and disclose hardware/toolchain/runtime metadata.
11. **Do not optimize for the benchmark itself** — public engine paths and normal external-user contracts must be used.
12. **Feed benchmark failures back into product gaps** without allowing the benchmark to silently reorder owner-fixed engineering priorities.

## 10. Explicit avoid list

Do not design or benchmark toward:

- screenshot/VLM inference for facts already owned by the engine,
- editor-only hidden state,
- hundreds of redundant MCP mutations that safe text/source editing already solves,
- one successful demo presented as an autonomy metric,
- unfair tasks that count an unimplemented Trace2D capability as an Agent failure,
- opaque aggregate AI-quality scores that hide deterministic failure modes,
- automatic human-taste approval by a multimodal model,
- exposing hidden model chain-of-thought as required project state,
- source copying or dependencies without version/license/security review,
- benchmark-specific engine detection or special-case paths,
- claiming a borrowed idea is proven until Trace2D itself has a test/workload/result demonstrating it.

## 11. Review cadence

Refresh the applicable references:

- whenever a substantive owning subsystem begins,
- before #100/#102/#103/#104 benchmark implementation or publication,
- before adding or redistributing an external dependency,
- when a cited project materially changes,
- whenever current evidence contradicts an older Trace2D assumption.

Benchmark adapters must pin exact versions/commits for recorded runs. Architectural references may point to living documentation, but the PR should preserve the review date and material version/commit when the decision depends on it.
