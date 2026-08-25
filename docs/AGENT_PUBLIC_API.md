# Agent Public C++ API Discovery

Trace2D agents and external-consumer tooling must discover public C++ APIs from repository-owned evidence instead of guessing header names from type names.

The machine-readable index is [`agent-public-api-v1.json`](agent-public-api-v1.json). It maps supported public symbols to the include path that actually declares them and to a compiling canonical external-consumer example.

For an installed SDK, discovery starts at the SDK root before CMake configure or include-tree enumeration:

```text
<Trace2D root>/trace2d.sdk.locator.json
```

That locator is intentionally tiny and points to the canonical SDK metadata at `share/Trace2D/trace2d.sdk.json`. The canonical metadata then publishes the public API guide/index/example locations, the direct downstream CMake packages required by the exported static target graph, and the MSVC configuration-matching policy. It is the machine-readable setup/discovery entry point; do not infer dependencies or build configuration by waiting for a configure/link failure.

The same public API discovery contract is packaged at:

```text
<Trace2D root>/share/Trace2D/trace2d.sdk.json
<Trace2D root>/share/Trace2D/docs/agent-public-api-v1.json
<Trace2D root>/share/Trace2D/docs/AGENT_PUBLIC_API.md
<Trace2D root>/share/Trace2D/examples/e0_external_game/
```

`find_package(Trace2D CONFIG REQUIRED)` exposes the stable CMake variables `Trace2D_SDK_ROOT_LOCATOR`, `Trace2D_SDK_METADATA`, `Trace2D_PUBLIC_API_INDEX`, `Trace2D_PUBLIC_API_GUIDE`, and `Trace2D_CANONICAL_EXTERNAL_EXAMPLE_ROOT`. A normal non-quiet configure also prints the locator, metadata, public API index and canonical example paths. The example root is packaged with the SDK, so the `canonical_example` references in the JSON remain inspectable even when the engine source checkout is unavailable.

## Discovery rule

For a public type such as `trace2d::application::Game`:

1. If using an installed SDK, read `<Trace2D root>/trace2d.sdk.locator.json`, then its canonical `sdk_metadata` target before configuring or scanning headers.
2. Query the machine-readable public API index for the fully qualified symbol.
3. Use its `include` field verbatim.
4. Inspect the referenced canonical example for normal construction/lifecycle usage.
5. If a symbol is not indexed, inspect the public include tree and current examples/tests before assuming support.
6. **Never manufacture a per-type header from the C++ type name.** Absence of `Game.hpp`, `SceneTemplate.hpp`, or a similar guessed file is not evidence that the engine is missing the type.

For the current application boundary, the canonical declaration is:

```cpp
#include <trace2d/application/Application.hpp>

class MyGame final : public trace2d::application::Game
{
public:
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override
    {
        (void)context;
        (void)update;
    }
};
```

`Game::OnFixedUpdate(GameContext&, const FixedUpdate&)` is the one required pure-virtual lifecycle callback. `OnStart(GameContext&)` and `OnStop(GameContext&)` are optional overrides. A concrete external `Game` that omits `OnFixedUpdate` is intentionally abstract and cannot be instantiated.

The canonical external-consumer implementation is under `examples/e0_external_game/`; it is ordinary public API usage, not benchmark-only scaffolding.

## Scene component registry discovery

`trace2d::scene::ComponentRegistry` is declared by the scene component header, not a per-type header:

```cpp
#include <trace2d/scene/Components.hpp>

trace2d::scene::ComponentRegistry registry{};
```

Use the machine-readable `trace2d::scene::ComponentRegistry` entry when an external host or factory needs the registry type. Do not infer `trace2d/scene/ComponentRegistry.hpp`; that file is not part of the public API. `examples/e0_external_game/ExampleGame.hpp` includes the canonical header directly and shows registry-backed component declarations without relying on transitive includes.

## Windowed presentation discovery

Do not bypass Trace2D with a second host renderer merely because the application example first discovered is headless. The same canonical external-consumer example contains a complete ordinary windowed path in `examples/e0_external_game/WindowedMain.cpp`.

For a small external 2D game, discover these stable symbols directly from the machine-readable index instead of repository-wide search:

- `trace2d::platform::Platform` for the window and native event/input host,
- `trace2d::render::Renderer` for production GPU presentation and capture,
- `trace2d::render::OrthographicCamera` and `trace2d::render::SpriteRenderData` for the minimal 2D draw path,
- `trace2d::application::Application::SetPresentationCallback` to bind presentation to the same fixed-step `Application` lifecycle as headless verification.

`WindowedMain.cpp` demonstrates the intended integration order: create the public `Platform` and `Renderer`, construct the same `Game`/`Application` used for deterministic execution, bind one presentation callback, feed `Platform` input into `Application`, step the application, and present through `Renderer`. This keeps gameplay state, input, verification, and presentation on one engine-owned path.

Higher-level retained Sprite/Text/UI presentation contracts remain independently discoverable through their public headers. Prefer those production contracts when the game needs their semantics; the minimal `SpriteRenderData` example is a compact starting point, not a replacement for retained presentation features.

## Scope

This index is intentionally small and evidence-backed. Add a symbol only when its public contract and canonical usage are stable enough to help agents avoid repository search/guessing. It is a discovery aid, not a generated reflection database, ABI promise, or substitute for compiling code and tests.
