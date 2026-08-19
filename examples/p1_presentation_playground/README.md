# P1 Presentation Playground

Issue: #327  
Scope: bounded external product proof for #89 Material2D/Shader2D + #90 Tween/Sequence.

This sample is intentionally outside engine core. It tests whether the current public Trace2D SDK can express a compact set of reusable 2D presentation techniques without adding an effect-specific subsystem.

## Bounded recipe set

| Recipe ID | Primitive composition | Time authority |
| --- | --- | --- |
| `hit_flash` | Material2D `flashAmount` resolved once + Tween2D | presentation-only |
| `button_punch` | resolved Transform2D scale + Sequence | presentation-only |
| `panel_slide` | resolved Transform2D position + Tween2D | presentation-only |
| `hit_impact` | `hit_flash` + `button_punch` triggered together | presentation-only |

This is deliberately not the whole `docs/PRESENTATION_RECIPES.md` vocabulary. `outline`, `dissolve`, damage numbers, screen transitions and other techniques stay backlog items until a real product need pulls them in.

## Deterministic evidence

`trace2d_p1_presentation_playground_headless` proves:

- all tween clocks advance only from supplied presentation deltas;
- `hit_flash` reaches the exact 80 ms peak and returns to zero;
- `button_punch` returns to authored scale;
- `panel_slide` reaches its authored destination;
- the same 260 ms trajectory can be replayed after restart with one supplied large delta;
- Material2D writes use the resolved T2/T4 provider path;
- Sequence/Tween/Material target capacities are retained ahead of steady stepping.

The headless proof does not use pixels as gameplay or success authority.

## Windowed review

`trace2d_p1_presentation_playground_windowed` is the human-visible proof.

Controls:

- `Enter`: toggle deterministic autoplay;
- `Space`: pause autoplay and manually advance presentation by 40 ms;
- `R`: deterministic restart;
- `C`: capture `trace2d-p1-presentation-proof.bmp`;
- `Esc`: quit.

Autoplay advances with a fixed supplied `16,666,667 ns` presentation delta. The tween system never queries the wall clock.

The target sprite is small procedural pixel art owned by this sample. The flash itself uses the same canonical Shader2D -> Material2D -> prepared parameter block -> Sprite presentation path as production rendering. UI cards use ordinary Sprite presentation plus resolved transform tweening.

## Performance / ownership

The proof intentionally keeps:

- setup-time semantic/material-name resolution only;
- fixed typed Tween2D values;
- retained Tween/Sequence/material-target storage;
- canonical ResourceRegistry texture/material/shader identity;
- one prepared Material2D pipeline reused across frames;
- no arbitrary tween callbacks or hidden gameplay mutation authority;
- no new engine subsystem.

Renderer/Tween/Material counters are printed when the windowed sample exits.

## Owner verdict

The deterministic proof and build can be automated. The visual verdict cannot.

Until the owner downloads/runs the retained Windows artifact (or equivalent real-GPU build) and records a verdict on #327, this checkpoint remains **open**. A rejection must be preserved and corrected with the smallest demonstrated presentation gap; it must not be erased by rerunning for a preferred result.

## Standalone installed-SDK build

The dedicated workflow installs the candidate Trace2D package, then configures this directory as a clean `find_package(Trace2D)` consumer. This prevents the proof from depending on unexported in-tree implementation details.
