# B2 task: one-room top-down combat slice

Build a small, coherent and playable top-down combat micro-game from the
provided lane starter project. Use only normal public engine/project workflows
available to an external user.

## Gameplay contract

Create one room with exactly one controllable player and one enemy.

- The player has stable semantic identity `player`.
- The enemy has stable semantic identity `enemy`.
- The player starts at `(0, 0)` in benchmark gameplay units.
- The enemy starts at `(64, 0)`.
- The player starts with 3 HP.
- The enemy starts with 2 HP.
- Semantic actions must include `move_left`, `move_right`, `move_up`,
  `move_down`, and `attack`.
- Holding `move_right` for eight fixed gameplay steps from the initial state
  must move the player to `(32, 0)`.
- Player movement is four gameplay units per fixed step and must go through the
  normal input/gameplay path.
- A normal `attack` action from `(32, 0)` must hit the enemy at `(64, 0)`.
- One successful attack deals exactly 1 damage.
- Attack cooldown is six fixed gameplay steps. An attack during cooldown must
  not deal damage.
- After the cooldown, a second successful attack must reduce enemy HP to 0 and
  transition the enemy to a dead/inactive state exactly once.
- Player HP must remain 3 during the frozen acceptance sequence.

Do not implement verifier-only state assignment, special benchmark detection, or
a shortcut that writes an expected-result file instead of exercising gameplay.

## Presentation contract

Use each lane's normal 2D production surface.

- Player and enemy must have visually distinct sprites.
- The player must have visible idle/move/attack animation intent.
- Enemy damage must trigger a hit particle effect.
- Enemy death must trigger a distinct death particle effect.
- A small HUD must show player HP and enemy HP/state.
- The final room must be readable in a normal presentation preview.

Deterministic gameplay truth is verified independently. Screenshots/captures are
presentation evidence only and cannot repair a deterministic failure.

## Completion evidence

Before declaring the initial build complete:

1. run the lane's deterministic/headless verifier,
2. produce the requested presentation evidence,
3. leave the project in a reproducible state that can replay the frozen input
   sequence without the original Agent.

Do not modify benchmark task, verifier, policy, or harness files.
