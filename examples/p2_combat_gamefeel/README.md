# P2 Combat Game-Feel Product Proof

P2 is the bounded product proof for Trace2D Physics2D + Audio after #76 and #77. It is deliberately an external-style example, not a combat framework.

Representative loop:

`move -> collide -> attack -> hit -> knockback -> SFX -> HUD/presentation feedback`

## What this proves

The headless executable uses only public Trace2D surfaces and asserts structured gameplay evidence:

- player movement is issued through `PhysicsWorld2D::SetLinearVelocity` and remains blocked by authored static arena collision;
- Space performs one bounded `OverlapBox` attack query against the enemy layer;
- one accepted swing changes proof-owned HP exactly once;
- knockback uses `ApplyLinearImpulseToCenter` on the dynamic enemy;
- accepted hits publish semantic `AudioEventType2D::Started` through `AudioSystem2D` without waveform inference;
- cooldown input cannot double-hit the same authored attack interval;
- the same authored initial state/input reproduces authoritative counters and bounded floating-point body state;
- retained Physics2D query/event capacity, semantic audio voice/event capacity and resource counters remain observable.

The player is intentionally a zero-gravity, fixed-rotation **dynamic** body driven by explicit velocity commands. The initial P2 planning note suggested a kinematic player, but a prescribed kinematic body is not a meaningful proof that ordinary Box2D collision response blocks movement against static walls. This correction stays entirely in the example and adds no engine primitive.

## Audio fixture

The proof does not commit a binary sound asset. `P2GeneratedAssets.hpp` creates one deterministic 48 kHz mono PCM16 hit sound in the example build's `runtime/audio` directory before the application starts. That setup-only filesystem work is outside `AudioSystem2D::Step()` and outside the gameplay/fixed-step hot path. The generated file is then published through the ordinary `AudioClipResource` contract and used by the physical `AudioOutput2D` owner proof.

## Windowed owner proof

The windowed executable adds presentation-only physical audio through `AudioOutput2D`. `Sync()` consumes semantic audio state after each fixed step; `Pump()` performs preparation/refill work outside the gameplay fixed-step path.

Controls:

- `WASD`: move
- `Space`: attack
- `R`: restart the authored round
- `C`: capture `trace2d-p2-combat-proof.bmp`
- `Esc`: quit

The retained owner verdict should judge separately from automated correctness:

1. movement/collision feels coherent;
2. attack -> hit -> knockback reads clearly;
3. hit SFX is audible and acceptably synchronized;
4. hit flash/attack cue/health feedback reads as one intentional feedback event;
5. no repeated concrete missing engine primitive is exposed.

A rejection is evidence and must be preserved; it should become only the smallest demonstrated follow-up rather than broad Physics2D/Audio expansion.

## In-tree build

The repository root includes this example through `examples/CMakeLists.txt`. Normal Trace2D CI builds the headless and windowed targets; CTest runs the headless proof.

## Installed SDK consumer

P2 also supports a clean standalone build against an installed Trace2D package:

```powershell
$env:TRACE2D_ROOT = "<installed Trace2D prefix>"
cmake --preset windows-msvc
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Run the owner executable from the standalone build output after the installed-SDK test is green.

## Scope boundary

P2 owns its HP, cooldown and attack rules. It does not add an ability system, damage framework, character controller, spatial audio, DSP graph, or another collision/audio authority. If this proof repeatedly exposes a common missing primitive, #329 records the evidence before any engine-core follow-up is opened.
