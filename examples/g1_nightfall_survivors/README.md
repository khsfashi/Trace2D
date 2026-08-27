# Nightfall Survivors

`g1_nightfall_survivors` is a small but closed-loop game built as Trace2D product dogfood. It uses the broad auto-attack survival genre as a design reference, but contains no Vampire Survivors artwork, audio, code, names, or data.

## Product loop

The executable now owns the full loop instead of booting directly into a combat demo:

1. main menu
2. profile / achievements / settings
3. character selection
4. stage selection
5. survival run
6. result and unlock rewards
7. persistent progression back into the menu

Progress is saved to `save/nightfall_profile.v1` beside the packaged game working directory.

## Characters

- **별의 전사** — balanced starter
- **잿불 기사** — slower, harder-hitting, higher-health character; unlocked by clearing 월광 폐허
- **달빛 사냥꾼** — fast rapid-fire character; unlocked at 150 cumulative kills

## Stages

- **월광 폐허** — survive 3:00
- **잿불 지하묘지** — survive 3:30; unlocked by clearing 월광 폐허
- **성운 심연** — survive 4:00; unlocked by clearing 잿불 지하묘지

Each stage changes enemy health, speed, and spawn pressure rather than only changing the displayed timer.

## Controls

### Menus

- `W/S` or arrow up/down — move selection
- `A/D` or arrow left/right — adjust settings / browse selection screens
- `Enter` or `Space` — confirm
- `Esc` — back; from the main menu, quit

### Run

- `WASD` — move
- attacks fire automatically at nearby enemies
- collect XP gems and level up
- level-up choice: `Q` **연사**, `E` **화력**, `F` **궤도**
- `Esc` — pause
- `R` — restart the current run

The Korean level-up overlay explains what each upgrade changes instead of relying on unexplained English labels.

## Progression and achievements

The profile records total runs, kills, clears, best level, best survival time, and earned stars. Six achievements are tracked and persisted, including first kill, first clear, 100 cumulative kills, reaching level 8, clearing the final stage, and clearing with at least 75% health.

## Presentation

- real 8x4 walk spritesheet through `SpriteAnimationClip2D` / `SpriteAnimator2D`
- correct down/up/left/right direction mapping
- automatic aspect-ratio-safe camera fitting: the authored 16:9 gameplay core is never cropped on narrower or wider windows; extra world is revealed instead
- dynamic floor coverage for the actual visible camera extent
- health, XP, timer, kill, level, and build HUD
- Korean menu/HUD/result text rendered through Trace2D production text (`FontResource -> GlyphAtlas -> TextLayoutRun -> TextPresentation2D`)
- Noto Sans KR fetched from a pinned Google Fonts commit under SIL OFL 1.1
- hit/death/level-up/projectile additive VFX and camera hit shake
- physical SFX through `AudioSystem2D -> AudioOutput2D`

## Build from the Trace2D repo

Configure Trace2D normally, then explicitly build the owner-playable target:

```text
cmake --build <build-dir> --target trace2d_g1_nightfall_survivors_windowed --config Release
```

The explicit playable target fetches pinned free/open runtime assets and places the complete `runtime/` directory beside the executable. Ordinary Trace2D builds still compile all Nightfall product code without requiring network downloads.

On Windows the playable is built as a GUI executable, so normal double-click play does not keep a console window open. If startup fails, `nightfall_error.txt` is written for diagnosis.

## Why this exists

Nightfall is intentionally a real product-shaped consumer of existing engine facilities. `engine/` source is not expanded just to make the sample impressive. Friction discovered while building or playing becomes a separate dogfood issue only when it represents a reusable engine need.
