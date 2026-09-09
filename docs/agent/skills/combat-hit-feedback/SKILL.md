---
name: combat-hit-feedback
description: Use when an accepted deterministic combat hit must update gameplay state first, then emit non-authoritative audio and presentation feedback with separate verification.
---

# Combat hit feedback

## Canonical workflow

1. Resolve the hit in deterministic gameplay/fixed-step logic and decide the authoritative outcome first: target identity, accepted/rejected hit, damage, health/state mutation, cooldown/death rules.
2. Commit canonical gameplay state before presentation feedback. Derive a small semantic hit-feedback cue/event from the accepted outcome rather than letting visual/audio systems decide whether a hit occurred.
3. Trigger presentation from that accepted outcome. For audio, an `AudioSource2D` entity plus `AudioSystem2D::Play(entity)` produces structured command results and semantic events; inspect results/events rather than assuming playback succeeded.
4. Sprite flash, particle, camera, UI, and audio feedback are consumers of the gameplay outcome. They may be dropped/degraded without changing damage truth.
5. Verify gameplay and presentation separately: structural gameplay assertion first, then audio event/voice/presentation evidence as needed.

## Authority and ordering

`deterministic hit decision -> canonical gameplay mutation -> semantic feedback cue -> audio/presentation consumers`

## Do not

- Do not let audio, particle, animation completion, or renderer timing decide damage.
- Do not persist/transmit backend audio handles as gameplay identity.
- Do not make screenshot/VLM evidence the only oracle for health/damage state.
- Do not retry a failed feedback command by replaying the gameplay hit.

## Discovery handoff

Load `gameplay`, `scene`, and `audio` surfaces. Exact command/result enums are in `trace2d/audio/AudioSystem2D.hpp`; exact gameplay symbols remain in the public API index/header named by the selected surfaces.

## Deterministic verification

Assert the target's canonical health/state after the hit. Then, independently verify that `AudioSystem2D::Play` succeeded and/or that the expected semantic audio event was published. Presentation evidence may supplement this, but a presentation failure must not alter the gameplay assertion.
