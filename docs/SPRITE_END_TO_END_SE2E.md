# Sprite SE2E End-to-End Proof Contract

Status: **active via #170**  
Parent: **#59 Complete Sprite program**  
Prerequisite: **S0/S1, SR0-SR8, SA0-SA4, and SPP0-SPP5 complete**  
Exact next stage after merge: **SPERF — final reproducible Sprite performance evidence**

SE2E proves that the already-owned Sprite contracts compose into one reviewable workflow. It does not add a second Sprite authority model, a production orchestration layer, or a new GPU truth model.

## 1. Complete flow

```text
request / import
 -> generated or raw pixels
 -> deterministic cleanup / QA
 -> canonical SpriteAsset
 -> deterministic animation
 -> headless exact-frame Agent QA
 -> renderer-facing canonical region selection
 -> explicit capture artifact
 -> #98 WorkResult evidence
 -> multimodal / human perceptual review
```

Each arrow is a handoff between existing contracts. SE2E verifies identity and evidence continuity across those handoffs.

## 2. Authority boundaries

- SPP0-SPP5 own deterministic Sprite processing, validation, interoperability, and canonical import after a concrete external candidate exists.
- SA0-SA4 own animation time, state, frame/region selection, playback, exact-frame inspection/assertion, and animation conformance.
- SR0-SR8 own render extraction, presentation, batching, capture behavior, and trusted presentation-GPU conformance.
- `CaptureRequest` / `CapturedFrame` own explicit capture artifact packaging. Capture/readback/filesystem work is never ordinary-frame work.
- #98 `WorkResult` owns structured verification/review state. SE2E must not invent a Sprite-only result authority.
- Deterministic facts remain deterministic. Visual or motion quality remains multimodal/human judgment.

A test-authored capture fixture can prove simulation-frame/artifact handoff. It is **not** evidence that a production GPU rendered those pixels. SR8 remains the real-GPU presentation authority.

## 3. Deterministic hosted proof

`trace2d_sprite_e2e_tests` is intentionally backend-independent and uses a recorded/fake generation provider.

The positive fixture proves:

1. one provider request produces a concrete two-frame candidate,
2. existing SPP5 validation/processing exposes a canonical `SpriteAsset` only after all required gates pass,
3. an SA clip references that exact asset and crosses an authored frame boundary,
4. the existing SA3 Agent inspection/assertion surface observes the authoritative frame/region,
5. `SpriteAnimator2D` returns the same asset pointer and selected region index,
6. SR0 resolves and extracts that region through the setup-time O(1) index contract without semantic re-lookup,
7. explicit capture packaging binds an artifact to the same logical simulation-frame id,
8. a #98-compatible `WorkResult` records deterministic evidence while leaving perceptual review as `review_needed`.

The negative fixture supplies an invalid generation plan and proves that failure occurs before provider execution and before canonical/downstream authority exists.

## 4. Review boundary

The SE2E WorkSpec/WorkResult fixture deliberately contains two different acceptance classes:

- `deterministic-flow`: deterministic and expected to pass in hosted CI,
- `visual-motion-review`: human review and expected to remain `review_needed` until perceptual judgment occurs.

This separation prevents a green structural test from being misreported as approval of animation feel, readability, style, silhouettes, timing aesthetics, or other perceptual qualities.

## 5. Performance boundary

SE2E adds test/tooling code only. It adds no production hot-path work and no runtime dependency broadening.

Forbidden in ordinary Sprite frames:

- provider calls,
- generation/verification polling,
- filesystem access,
- capture/readback or fence waits,
- JSON/TOML construction,
- semantic-name lookup added to rendering,
- test-only caches or reporting allocations,
- automatic visual-quality inference.

Existing SPP, SA, SR, capture, and SR8 GPU contracts remain the performance authorities for their layers.

## 6. Validation

Focused backend-independent validation:

```text
trace2d_sprite_e2e_tests
```

Final SE2E completion also requires the repository's existing project-state, Sprite S0, Sprite SA0, release, benchmark, content, platform/build, and clean-clone qualification gates to remain green on the exact PR head.

SE2E creates no new real-GPU test suite. Real-GPU Sprite rendering/capture correctness was already frozen by SR8 and must not be duplicated here.

## 7. Completion rule

SE2E is complete only when:

1. #170 acceptance is implemented without adding a production SE2E authority,
2. focused and full hosted CI is green on the exact implementation head,
3. the result/review boundary remains #98-compatible and perceptual quality is not mislabeled deterministic,
4. `config/trace2d.core-lane.json`, `PROJECT_STATUS.md`, and Sprite documentation agree that #170 is the active SE2E child,
5. the SE2E PR merges and #170 closes.

After merge, stop. Do not create SPERF in the same completion continuation. The following `@GitHub Trace2D 다음 진행해줘` continuation creates exactly one SPERF child.
