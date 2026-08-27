# Nightfall Survivors asset credits

Nightfall Survivors uses free/open runtime assets fetched from pinned public GitHub commits. Game art and SFX are CC0; the Korean UI font is under SIL Open Font License 1.1.

## Visuals

- Player walk sheet: `rpg_sprite_walk.png`, OpenGameArt “2D RPG character walk spritesheet”, CC0. Pinned mirror: `Tiddybub/2d-assets@e0cbe0d995554a490d4c182fe9beb8769ffbb606`.
- Floor/enemy tiles: Kenney Tiny Dungeon 1.0, CC0. Pinned public sample repository: `wyatt-raex/2d_survivors_game@72db959453fedc08409416ef60567955955f9e2b`.
- Particle sprite: Kenney Puzzle Pack 2, CC0. Pinned mirror: `Tiddybub/2d-assets@e0cbe0d995554a490d4c182fe9beb8769ffbb606`.

## Font

- `NotoSansKR.ttf`: Noto Sans KR from Google Fonts, SIL Open Font License 1.1. Pinned source: `google/fonts@4efc2774c63917927efe769ca845def6bd6debae`.
- The package also carries `runtime/licenses/NotoSansKR-OFL.txt` beside the font.
- Nightfall routes this font through Trace2D `FontResource -> GlyphAtlas -> TextLayoutRun -> TextPresentation2D`; Korean labels are not baked into images.

## Audio

The five SFX are pinned MP3 fallbacks from `manuel-palacio/brickstorm@9327739b5a85b4819b95145ccf08d6664eab8f3c`. That repository’s `CREDITS.md` identifies their Kenney source packs as CC0:

- `laser.mp3` — Kenney Sci-Fi Sounds
- `brick-hit.mp3` — Kenney Impact Sounds
- `brick-break.mp3` — Kenney Sci-Fi Sounds
- `powerup-get.mp3` — Kenney Interface Sounds
- `life-lost.mp3` — Kenney Music Jingles

No Vampire Survivors assets, audio, names, code, or data are included.
