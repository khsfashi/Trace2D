# B1 strongest Godot Agent selection

Selection date: **2026-08-13**  
Non-scored workflow: **31622618958**  
Selected lane: **`hi-godot/godot-ai`**  
Exact pin: **`v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a`**

This is capability qualification evidence only. It is not a scored Trace2D-vs-Godot benchmark result.

## Matched fixture

Both leading candidates received the same Godot 4.7.1-stable project containing an `AnimationPlayer`, a `GPUParticles2D`, and a target node. The required authored result was fixed before the run:

- animation `content_probe`, length `1.0`,
- one method event targeting `Subject.queue_redraw()` at exactly `0.375s`,
- particle `amount=96`, `lifetime=0.8`, `emitting=false`,
- save/readback and 2D presentation-capture handoff,
- provider-independent headless known-good acceptance,
- provider-independent rejection after the committed known-bad mutation changes amount to `95`.

## hi-godot/godot-ai v3.0.6

Job **94200960755** completed successfully.

The pinned source installed and started against official Godot 4.7.1-stable, authored the exact animation event and particle constraints, saved/read the scene, provided presentation capture handoff, passed its animation validation, passed the independent headless known-good verifier, and the verifier rejected the known-bad mutation.

Result: **QUALIFIED / SELECTED**.

## satelliteoflove/godot-mcp 4.1.0

Job **94200960753** installed the exact npm package, installed/started its addon, connected to the editor after the same bounded readiness handshake used by B0, and reached the exact method-event authoring step.

Two bridge defects were exposed rather than patched in the competitor:

1. its public MCP schema documents `add_keyframe.value` as optional while the Godot addon rejects calls without `value` before checking the track type; the qualification driver supplied a neutral placeholder that the method-track branch does not consume,
2. the method-track branch then calls `Animation.method_track_add_key()`, which does not exist in Godot 4.7.1. The editor raises `Invalid call. Nonexistent function 'method_track_add_key' in base 'Animation'`, so the bridge cannot complete the preregistered exact method-event task through its advertised animation authoring path on the frozen engine version.

The candidate itself was not modified. Generic coding tools remain allowed for a normal benchmark lane, but they do not make this bridge the strongest content-authoring Agent baseline when another reviewed bridge completes the same fixture through its ordinary public authoring surface.

Result: **NOT QUALIFIED FOR B1 STRONGEST-AGENT SELECTION**.

## Decision

Select `hi-godot/godot-ai v3.0.6` for B1 `godot.agent` before any scored task membership, budgets, known-bad mutations, or comparative results are frozen.

This opens the scored-suite freeze gate; it does not itself define or run the scored B1 suite.
