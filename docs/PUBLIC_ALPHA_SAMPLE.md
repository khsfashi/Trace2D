# Public Alpha Vertical Sample

This sample is the first complete Trace2D agent-first vertical loop. It deliberately composes existing engine surfaces instead of introducing a broader gameplay framework.

Source scene:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

The authored scene contains seven entities. `#player` is the stable semantic controlled entity; six marker entities make the renderer workload large enough to measure the existing contiguous-texture batching candidate.

## Deterministic sample behavior

The `trace2d public-alpha` command uses the existing `Trace2D::Testing::GameplayScenario` path.

For the default eight-frame run:

1. load the authored TOML scene
2. schedule virtual `KeyD` press at simulation frame 2
3. schedule virtual `KeyD` release at simulation frame 6
4. move `#player` by exactly `+1.0` world unit on each frame where `KeyD` is held
5. assert `#player` -> `Transform2D.position.x` at the requested final frame
6. compose the resulting scene state into the renderer sample
7. measure contiguous texture runs without changing painter order
8. when windowed, render that same authoritative final state
9. when capture is requested, capture the exact final simulation frame

With the committed scene and `--frames 8`, the exact result is:

```text
#player.position.x = 4.0
visible sprites = 7
contiguous texture runs = 2
measured candidate draw-call saving = 5
```

The current renderer remains unbatched, so a windowed frame still submits one draw per visible sprite. The measurement is evidence for the next batching decision; it is not presented as an already-implemented optimization.

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

The command runs the same deterministic gameplay scenario first, then renders its final scene state. The capture request uses `GameplayScenario::Runtime().State().frame`, so the artifact is explicitly bound to simulation frame 8 rather than wall-clock timing.

Normal headless execution performs no renderer creation, GPU readback, fence wait, image normalization, or file I/O.

## Why the sample is intentionally narrow

The sample does not add a generic movement component, scripting VM, asset database, editor, render graph, or new protocol layer. Those would obscure the Public Alpha claim.

The release claim is smaller and testable: Trace2D can take text-authored state through deterministic execution, semantic observation, virtual input, exact-frame gameplay assertion, renderer composition, and frame-specific visual capture with one coherent sample.

## Batching evidence

`MeasureContiguousTextureBatching` sees the committed sample as seven visible sprites in two contiguous texture runs. The theoretical ordered draw count therefore falls from seven unbatched draws to two contiguous same-texture batches, a five-draw saving for this workload.

This is enough evidence to revisit the previously deferred contiguous same-texture instancing candidate. Any implementation must still preserve painter order and reuse persistent GPU/transfer resources rather than adding per-frame allocation or sorting.
