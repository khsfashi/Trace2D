# Nightfall Survivors

`g1_nightfall_survivors` is a game-first Trace2D dogfood slice inspired by the broad auto-attack survival genre. It is not affiliated with or derived from Vampire Survivors artwork/audio/code.

## Play

- `WASD` — move
- attacks fire automatically at nearby enemies
- collect XP gems and level up
- while the level-up overlay is visible: `Q` = Rapid Fire, `E` = Might, `F` = Orbit Blades
- `R` — restart
- `Esc` — quit
- survive 180 seconds to win

The first run downloads a small pinned set of CC0 assets into the build runtime directory. See `CREDITS.md` for provenance.

## Build from the Trace2D repo

Configure Trace2D normally, then explicitly build the owner-playable target:

```text
cmake --build <build-dir> --target trace2d_g1_nightfall_survivors_windowed --config Release
```

Run the produced `trace2d_g1_nightfall_survivors_windowed` executable. Asset fetching is intentionally attached only to this explicit target so ordinary engine CI/builds do not depend on the network.

## What this exercises

- fixed-step movement and deterministic gameplay state
- 384-enemy fixed pool, projectiles, XP gems and timed waves
- auto-target/auto-fire combat
- three enemy archetypes
- XP/level-up/build progression
- Trace2D `SpriteAnimationClip2D` / `SpriteAnimator2D` walk animation
- region-based sprite presentation
- additive hit/death/level-up/projectile VFX
- health/XP HUD and level-up overlay
- `AudioSystem2D -> AudioOutput2D` physical SFX output
- camera follow and hit shake

No `engine/` source is changed by this game slice. Any missing engine capability found while playing should become a separate dogfood issue instead of being silently expanded inside the game PR.
