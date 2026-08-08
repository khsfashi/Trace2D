# Trace2D v0.1.0-alpha.1 — Public Alpha

Trace2D is a deterministic and observable C++20 2D engine prototype designed around AI coding-agent workflows.

This first Public Alpha proves one complete text-first automation loop: edit a scene, build, run headlessly, step simulation frames explicitly, inspect authoritative state, query semantic entities, inject virtual input, assert gameplay behavior, render, and capture a visual artifact at a known simulation frame.

## Highlights

- deterministic fixed-step runtime with explicit frame stepping
- text-authored TOML scenes with stable semantic entity identity
- structured runtime inspection and deterministic semantic queries
- frame-indexed virtual input and deterministic gameplay assertions
- SDL3 GPU orthographic sprite rendering and explicit-frame BMP capture
- measured contiguous same-texture instancing that reduces the committed seven-sprite sample from 7 draws to 2 ordered instanced draws without global texture sorting
- repeatable repository/history release audit, clean-checkout Windows/MSVC CI, and pinned vcpkg baseline
- MIT License with documented third-party source-license review

## Public Alpha sample

The committed sample is `samples/public_alpha/public_alpha.trace2d.toml`.

Default deterministic contract:

```text
frames:                     8
seed:                       42
KeyD press frame:           2
KeyD release frame:         6
#player.position.x:          4.0
visible sprites:             7
contiguous texture runs:     2
unbatched baseline draws:    7
ordered instanced draws:     2
```

See `docs/PUBLIC_ALPHA_SAMPLE.md` for the complete edit -> build -> inspect -> query -> input -> assert -> capture workflow.

## Scope

This release is intentionally narrow. It is a proof of architecture and workflow, not a feature-complete Godot-like engine.

Not included in this alpha: graphical editor, MCP transport, broad physics/UI/audio/networking systems, advanced renderer/lighting, job system/custom allocator, and Linux/macOS/mobile support.

See `docs/PUBLIC_ALPHA_LIMITATIONS.md` for the explicit release boundaries and non-claims.

## Build

Initial supported toolchain:

- Windows x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.28+
- repository-pinned vcpkg baseline

The README contains the clean-clone Quick Start, and CI validates the same configure/build/test path.

## License

Trace2D is licensed under the MIT License. Third-party components retain their own licenses; see `docs/THIRD_PARTY.md`.
