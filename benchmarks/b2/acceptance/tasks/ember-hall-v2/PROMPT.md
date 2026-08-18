# B2 non-scored acceptance v2: Ember Hall playable presentation

This task is a **new post-score acceptance version** for Trace2D issue #104.
It does not replace, rescore, reinterpret, or repair the immutable nine-slot B2
scored cohort or the consumed acceptance-v1 evidence.

Build a small, coherent and playable top-down combat micro-game from the
provided Trace2D lane starter project using only normal public engine/project
workflows available to an external user.

## Gameplay contract

The independent deterministic verifier intentionally reuses the already-qualified
`b2-topdown-combat-v1` gameplay semantics. Presentation criteria are additive
acceptance requirements and are never allowed to override deterministic failure.

Create one room with exactly one controllable player and one enemy.

- The player has stable semantic identity `player`.
- The enemy has stable semantic identity `enemy`.
- The player starts at `(0, 0)` in benchmark gameplay units.
- The enemy starts at `(64, 0)`.
- The player starts with 3 HP.
- The enemy starts with 2 HP.
- Semantic actions include `move_left`, `move_right`, `move_up`, `move_down`,
  and `attack`.
- Holding `move_right` for eight fixed gameplay steps moves the player to
  `(32, 0)`.
- Player movement is four gameplay units per fixed step and goes through the
  normal input/gameplay path.
- A normal `attack` action from `(32, 0)` hits the enemy at `(64, 0)`.
- One successful attack deals exactly 1 damage.
- Attack cooldown is six fixed gameplay steps. An attack during cooldown does
  not deal damage.
- After cooldown, a second successful attack reduces enemy HP to 0 and
  transitions the enemy to a dead/inactive state exactly once.
- Player HP remains 3 during the acceptance sequence.

Do not implement verifier-only state assignment, acceptance detection, or any
shortcut that writes expected results instead of exercising gameplay.

## Playable presentation contract

The result must read as an actual top-down combat game, not as a debug view,
test harness, text dump, or a mostly-empty render target.

Use a 16:9-ish game presentation of at least 640x360 pixels.

### Visual identity

- The room is a dark stone/slate **EMBER HALL** with a clearly visible floor,
  four-sided room boundary, and warm ember environmental accents.
- The player is visually identifiable with a **cool cyan/blue** accent.
- The enemy is visually identifiable with a **hostile red/magenta** accent.
- Ember/environmental accents use **orange/amber**, visually distinct from both
  the player and enemy.
- The player, enemy, room boundary and HUD must all be simultaneously legible
  in the overview capture.
- Do not use a full-screen solid color, giant diagnostic text, or a presentation
  whose primary purpose is reporting verifier state.

### HUD

Reserve a compact top HUD zone that does not cover the central combat space.

- player HP is readable on the left,
- `EMBER HALL` is readable near the top center,
- enemy HP/state is readable on the right.

### Combat feedback

The game must visibly communicate the combat state through the ordinary
presentation path.

- attack intent has a visible attack arc/projectile/effect,
- enemy damage has a readable hit flash/particle effect,
- enemy death has a larger and distinct death effect and a visibly dead or
  inactive enemy state.

### Required retained captures

Retain these exact PNG files under `.trace2d-b2-evidence/presentation/`:

1. `ember-hall-overview.png` — normal combat overview before enemy death.
2. `ember-hall-attack.png` — player attack presentation visible.
3. `ember-hall-hit.png` — enemy hit feedback visible.
4. `ember-hall-death.png` — enemy death/inactive state and death effect visible.

All four captures must come from the authored game presentation, must be at
least 640x360, and must be visually different from one another where the state
changes. The overview must independently read as a game screen without relying
on captions outside the render.

The acceptance harness applies an independent machine presentation gate to the
PNG bytes before any perceptual review. It checks image shape, non-trivial scene
composition, dark-room occupancy, the required cyan/blue, red/magenta and
orange/amber visual families, HUD-region contrast, and visible state changes
between retained captures. Do not attempt to detect or special-case the gate;
satisfy the presentation contract naturally.

## Completion evidence

Before declaring the initial build complete:

1. make the candidate compile/link through normal public Trace2D APIs,
2. leave it replayable by the independent deterministic verifier,
3. produce all four required retained captures from the actual game
   presentation,
4. keep all authored game changes inside the provided workspace,
5. do not modify benchmark task, verifier, policy, scored records, acceptance-v1
   evidence, or harness files.

Deterministic gameplay truth, machine presentation checks, perceptual review and
real human feedback remain separate evidence layers.
