# Nightfall Survivors asset credits

Nightfall Survivors uses free/open runtime assets fetched from pinned public GitHub commits. Game art and SFX are CC0; the Korean pixel UI font is under SIL Open Font License 1.1.

## Visuals

Nightfall deliberately uses separate source resources for playable characters, enemy archetypes, skill icons, and each stage floor identity instead of reusing one sprite with tint-only variants.

- Playable characters: three separate Kenney Tiny Dungeon tiles (`hero-star.png`, `hero-ember.png`, `hero-moon.png`), CC0.
- Enemy archetypes: three separate Kenney Tiny Dungeon tiles (`enemy-ghoul.png`, `enemy-brute.png`, `enemy-wisp.png`), CC0.
- Stage floors: six separate Kenney Tiny Dungeon tiles, two per stage (`stage-moon-*`, `stage-ember-*`, `stage-astral-*`), CC0.
- Skill icons: three separate Kenney Tiny Dungeon tiles (`skill-rapid.png`, `skill-might.png`, `skill-orbit.png`), CC0.
- Kenney Tiny Dungeon source is pinned through `wyatt-raex/2d_survivors_game@72db959453fedc08409416ef60567955955f9e2b`.
- Particle sprite: Kenney Puzzle Pack 2, CC0. Pinned mirror: `Tiddybub/2d-assets@e0cbe0d995554a490d4c182fe9beb8769ffbb606`.

## Font

- `Galmuri11-Bold.ttf`: Galmuri11 Bold by Minseo Lee / quiple, SIL Open Font License 1.1.
- Pinned source: `quiple/galmuri@71e1cacf1437a11220307120e63e30bc275312d4` (v2.404).
- The package carries `runtime/licenses/Galmuri-OFL.txt` beside the font.
- Nightfall routes the font through Trace2D `FontResource -> GlyphAtlas -> TextLayoutRun -> TextPresentation2D`; Korean labels are real text, not baked images.

## Audio

The five SFX are pinned MP3 fallbacks from `manuel-palacio/brickstorm@9327739b5a85b4819b95145ccf08d6664eab8f3c`. That repository’s `CREDITS.md` identifies their Kenney source packs as CC0:

- `laser.mp3` — Kenney Sci-Fi Sounds
- `brick-hit.mp3` — Kenney Impact Sounds
- `brick-break.mp3` — Kenney Sci-Fi Sounds
- `powerup-get.mp3` — Kenney Interface Sounds
- `life-lost.mp3` — Kenney Music Jingles

No Vampire Survivors assets, audio, names, code, or data are included.
