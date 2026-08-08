# Public Alpha Vertical Sample

This sample is the first complete Trace2D agent-first vertical loop. It deliberately composes existing engine surfaces instead of introducing a broader gameplay framework.

Source scene:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

The authored scene contains seven entities. `#player` is the stable semantic controlled entity; six marker entities provide a representative multi-sprite renderer workload.

## Deterministic sample behavior

The `trace2d public-alpha` command uses the existing `Trace2D::Testing::GameplayScenario` path.

For the default eight-frame run:

1. load the authored TOML scene,
2. schedule virtual `KeyD` press at simulation frame 2,
3. schedule virtual `KeyD` release at simulation frame 6,
4. move `#player` by exactly `+1.0` world unit on each frame where `KeyD` is held,
5. assert `#player` -> `Transform2D.position.x` at the requested final frame,
6. compose the resulting scene state into renderer data,
7. measure visible sprites and contiguous texture runs without changing painter order,
8. when windowed, render that same authoritative final state,
9. when capture is requested, capture the exact final simulation frame.

With the committed scene and `--frames 8`, the exact deterministic CPU-side result is:

```text
#player.position.x = 4.0
visible sprites = 7
culled sprites = 0
contiguous texture runs = 2
unbatched baseline draws = 7
ordered instanced draw target = 2
measured draw-call saving = 5
```

PR #34 implements that measured optimization. The renderer keeps seven submitted sprite instances but emits one draw per contiguous visible texture run, so the windowed sample reports `submitted_sprites = 7` and `draw_calls = 2` after a successful GPU submission.

Hosted CI does not require interactive GPU presentation; it validates the renderer build plus the backend-independent scene/input/assertion/batching contracts. The windowed command remains the explicit smoke surface for real presentation metrics and capture on a machine with an available SDL GPU backend.

## Complete agent workflow

The commands below assume a configured Windows build in `out/build/windows-msvc` through the repository presets.

### 1. Edit the text-authored scene

Edit:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

The scene is ordinary version-1 Trace2D TOML. No graphical editor is required.

### 2. Build

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
```

### 3. Inspect the authored scene

```powershell
out/build/windows-msvc/tools/trace2d/Debug/trace2d.exe inspect `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --frames 0 `
  --seed 42 `
  --json
```

This verifies that the scene parses and exposes deterministic structured state.

### 4. Query the semantic controlled entity

Quote `#player` in shells where `#` has special meaning.

```powershell
out/build/windows-msvc/tools/trace2d/Debug/trace2d.exe query `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --selector "#player" `
  --one `
  --json
```

The query must resolve exactly one entity.

### 5. Run virtual input and the exact-frame gameplay assertion headlessly

```powershell
out/build/windows-msvc/tools/trace2d/Debug/trace2d.exe public-alpha `
  --headless `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --frames 8 `
  --seed 42 `
  --json
```

Expected key fields:

```json
{
  "frame": 8,
  "selector": "#player",
  "input": {
    "press_frame": 2,
    "release_frame": 6
  },
  "assertion_passed": true,
  "player_x": 4,
  "visible_sprites": 7,
  "culled_sprites": 0,
  "contiguous_texture_runs": 2,
  "estimated_draw_call_saving": 5
}
```

This path is part of CTest as `trace2d_cli_public_alpha_headless`, so CI proves scene load -> scheduled virtual input -> deterministic movement -> semantic component assertion without initializing the GPU.

### 6. Render and capture the same final state

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null

out/build/windows-msvc/tools/trace2d/Debug/trace2d.exe public-alpha `
  --windowed `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --frames 8 `
  --seed 42 `
  --capture artifacts/public-alpha-frame-8.bmp `
  --json
```

Expected renderer fields after successful presentation:

```json
{
  "rendered_frames": 1,
  "draw_calls": 2,
  "submitted_sprites": 7,
  "capture_frame": 8
}
```

The command runs the same deterministic gameplay scenario first, then renders its final scene state. The capture request uses `GameplayScenario::Runtime().State().frame`, so the artifact is explicitly bound to simulation frame 8 rather than wall-clock timing.

Normal headless execution performs no renderer creation, GPU upload/readback, fence wait, image normalization, or file I/O.

## Why the sample is intentionally narrow

The sample does not add a generic movement component, scripting VM, asset database, editor, render graph, or new protocol layer. Those would obscure the Public Alpha claim.

The release claim is smaller and testable: Trace2D can take text-authored state through deterministic execution, semantic observation, virtual input, exact-frame gameplay assertion, ordered/culling-aware 2D rendering, measured contiguous batching, and frame-specific visual capture with one coherent sample.

## Batching evidence

`MeasureContiguousTextureBatching` sees the committed sample as seven visible sprites in two contiguous texture runs. PR #34 uses exactly that visible-run contract for GPU instancing: it does not sort by texture, does not build a per-frame visible-sprite vector, and reuses persistent capacity-managed instance/upload buffers.

The important distinction is:

```text
before PR #34: 7 visible sprites -> 7 sprite draws
PR #34 path:   7 visible sprites -> 2 contiguous instanced draws
```

`submittedSprites` remains seven because seven sprites are still rendered; `drawCalls` falls to two because adjacent same-texture visible sprites share a draw.
