# P0 Tiny Playable Product Proof

This is the deliberately narrow external product proof for Trace2D issue #315. It uses the public engine surface that already existed when B2 closed; it does not add a gameplay subsystem or a benchmark-only shortcut.

## Play

Run `trace2d_p0_tiny_playable_windowed.exe` from the retained Windows artifact.

- `A` / `D`: move left / right
- red vertical gate: active hazard; touching it costs one health segment and resets the player
- when the red gate lifts upward, cross during the safe window
- `Space` or `Enter` beside the yellow beacon: claim the objective
- `Esc` or close the window: quit

The left top bar is health. The right top bar fills when the beacon is claimed.

## Deterministic proof

`trace2d_p0_tiny_playable_headless` runs two fixed-step scenarios through ordinary `Application::ScheduleInput` and semantic `ActionMap` bindings:

1. collide with the active gate and assert health plus player reset from engine-owned `UiDocument` / `Scene` state;
2. wait for the safe pulse, cross, interact at the beacon, and assert full health plus completed objective from the same canonical state.

No screenshot is used as gameplay authority. The windowed renderer is presentation evidence for owner review; the headless executable is the deterministic gameplay gate.

## Installed-SDK consumer proof

This directory is also a standalone CMake project. With the normal Trace2D SDK environment prepared (`TRACE2D_ROOT` points at an installed Trace2D SDK and `VCPKG_ROOT` points at the pinned vcpkg checkout), it supports the same external-consumer contract as E0:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

The P0 product-proof workflow first builds and installs the candidate Trace2D SDK, then configures this directory independently through `find_package(Trace2D 0.1 CONFIG REQUIRED)`. The downloadable owner artifact is retained from that standalone consumer build rather than from the repository's in-tree example build.

## Scope

The source stays under `examples/`, outside `engine/`. It uses existing Application, Scene, Input Actions, UI progress state, ResourceRegistry, Platform and Renderer APIs. Material2D/Shader2D, Physics2D, Audio, Save, Mesh and other breadth remain intentionally out of scope until the owner loop produces concrete blocking evidence.
