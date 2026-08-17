# B2 non-scored acceptance task: Ember Hall combat slice

This task is a post-score acceptance validation for Trace2D issue #104. It is
**not scored** and must never write to or reinterpret the immutable nine-slot B2
scored cohort.

Build a small, coherent and playable top-down combat micro-game from the
provided Trace2D lane starter project. Use only normal public engine/project
workflows available to an external user.

## Gameplay contract

The independent verifier intentionally reuses the already-qualified
`b2-topdown-combat-v1` gameplay semantics so this acceptance variant does not
introduce a new verifier authority.

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

## Held-out presentation variant: Ember Hall

Make this room visually identifiable as **Ember Hall**, distinct from the
scored task presentation.

- Use a dark stone/slate room with a warm ember/orange environmental accent.
- Keep the player and enemy clearly distinguishable at a glance.
- Give the player visible idle/move/attack animation intent.
- Enemy damage triggers a readable hit particle effect.
- Enemy death triggers a larger, distinct death particle effect.
- A compact HUD shows player HP and enemy HP/state without covering gameplay.
- Include a small visible `EMBER HALL` room label.
- Retain at least one final presentation capture under
  `.trace2d-b2-evidence/presentation/` as PNG/JPEG/WebP/BMP.

Deterministic gameplay truth is verified independently. Presentation evidence is
for perceptual and human review only and cannot repair deterministic failure.

## Completion evidence

Before declaring the initial build complete:

1. make the candidate compile/link through normal public Trace2D APIs,
2. leave it replayable by the independent deterministic verifier,
3. produce the requested presentation capture,
4. keep all authored game changes inside the provided workspace.

Do not modify benchmark task, verifier, policy, scored records, or harness files.
