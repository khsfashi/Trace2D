# Dungeon Dash

`g0_dungeon_dash` is intentionally a **game-first** Trace2D example rather than another engine product-proof subsystem.

The goal is simple: prove that an Agent can take the existing public engine surface, import ordinary external art, and make a small complete game loop without first expanding Trace2D itself.

## Game

You are the explorer in a tiny dungeon. Collect all five relics before the hunter catches you.

- `WASD` — move
- `R` — restart the round after a win/loss (or at any time)
- `Esc` — quit
- five gold HUD markers track the relics
- the small top-right marker is blue while running, green on victory, red on defeat

The hunter gets slightly faster after every relic, so the shortest-looking route is not always the safest route.

## Build

From a normal Trace2D root build, build and run the target:

```text
trace2d_g0_dungeon_dash_windowed
```

The source directory is compiled into the example as its asset root, so the executable does not depend on being launched from a particular working directory.

The example can also be configured as its own CMake project against an installed Trace2D 0.1 package.

## Why this example exists

Dungeon Dash deliberately consumes only existing Trace2D facilities:

- `Application` and fixed-step input for gameplay,
- canonical `Scene` entities for the player, hunter, and relics,
- `TextureAssetCache` for project-relative PNG decoding,
- `ResourceRegistry` plus `Renderer` for GPU texture residency and presentation,
- the normal `Platform` event loop.

No new engine subsystem is introduced for the game.

The 768x432 window and 16x9 world tile grid also make each 16x16 source tile land at an integer 3x scale.

## External art

The four committed PNGs are renamed selections from **Kenney Tiny Dungeon 1.0**:

| Local file | Original pack file |
| --- | --- |
| `floor.png` | `tile_0048.png` |
| `floor-alt.png` | `tile_0049.png` |
| `hunter.png` | `tile_0084.png` |
| `player.png` | `tile_0085.png` |

Tiny Dungeon is released under **Creative Commons Zero (CC0)**. See `assets/kenney-tiny-dungeon/LICENSE.txt` for the pack notice retained with the example.
