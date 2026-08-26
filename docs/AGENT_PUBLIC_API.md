# Agent Public C++ API Discovery

Trace2D agents and external-consumer tooling must discover public C++ APIs from repository-owned evidence instead of guessing header names from type names.

For ordinary external gameplay authoring, the conventional public include-tree entry point is:

```cpp
#include <trace2d/Trace2D.hpp>
```

This umbrella intentionally covers the high-level gameplay surface already owned by `Trace2D::Application`: the Game/Application lifecycle, fixed-step input/actions, Scene/ComponentRegistry, and semantic UI state. Renderer, assets, audio, physics, profiling, platform hosting, and other specialized systems remain explicit includes so compile context does not grow for subsystems a consumer does not use. The umbrella adds no runtime object or frame work.

## Thin Agent information entry point

The installed SDK also publishes a deliberately small deterministic routing map:

```text
<Trace2D root>/trace2d.agent-index.json
```

`find_package(Trace2D CONFIG REQUIRED)` exposes its exact relocatable path as `Trace2D_AGENT_INDEX` and prints that path during a normal non-quiet configure. The root SDK locator and canonical SDK metadata both point to the same index identity.

The index is an information entry point, not a second API authority. It routes a finite ordinary authoring vocabulary to small surface metadata under `share/Trace2D/agent/`. A caller should select at most the task-relevant surfaces first, consume those compact contracts, and only then open an exact declaration header or canonical example when a missing signature/behavior still requires confirmation. Do not recursively enumerate the installed SDK as the default discovery strategy.

The first bounded surfaces are:

```text
gameplay -> share/Trace2D/agent/gameplay-v1.json
scene    -> share/Trace2D/agent/scene-v1.json
input    -> share/Trace2D/agent/input-v1.json
ui       -> share/Trace2D/agent/ui-v1.json
```

This is intentionally static and cheap: no vector database, daemon, network service, embedding step, runtime reflection registry, or frame-loop work is required. If deterministic surface routing cannot answer the question, fall back to the exact public symbol index and then the narrow public headers/examples.

The machine-readable exact-symbol index is [`agent-public-api-v1.json`](agent-public-api-v1.json). Its `policy.preferred_authoring_include` names the umbrella, while supported public symbols still map to the exact narrow header that declares them and to a compiling canonical external-consumer example.

For an installed SDK, discovery may also start at the SDK root before CMake configure or include-tree enumeration when the caller is inspecting package metadata:

```text
<Trace2D root>/trace2d.sdk.locator.json
```

That locator is intentionally tiny and points to both the canonical SDK metadata at `share/Trace2D/trace2d.sdk.json` and the root `trace2d.agent-index.json`. The canonical metadata publishes `discovery.agent_index_root_relative`, `discovery.public_authoring_include`, the public API guide/index/example locations, the direct downstream CMake packages required by the exported static target graph, and the MSVC configuration-matching policy. It is the machine-readable setup/discovery entry point; do not infer dependencies or build configuration by waiting for a configure/link failure.

If an Agent instead begins from the public include tree, `include/trace2d/Trace2D.hpp` points back to the same Agent index contract. This gives the observed CMake/package, root-metadata, and public-header discovery paths one bounded information entry point without requiring a repository checkout or benchmark-specific knowledge.

The same public API discovery contract is packaged at:

```text
<Trace2D root>/trace2d.agent-index.json
<Trace2D root>/include/trace2d/Trace2D.hpp
<Trace2D root>/share/Trace2D/trace2d.sdk.json
<Trace2D root>/share/Trace2D/agent/*.json
<Trace2D root>/share/Trace2D/docs/agent-public-api-v1.json
<Trace2D root>/share/Trace2D/docs/AGENT_PUBLIC_API.md
<Trace2D root>/share/Trace2D/examples/e0_external_game/
```

`find_package(Trace2D CONFIG REQUIRED)` exposes the stable CMake variables `Trace2D_SDK_ROOT_LOCATOR`, `Trace2D_SDK_METADATA`, `Trace2D_AGENT_INDEX`, `Trace2D_PUBLIC_AUTHORING_HEADER`, `Trace2D_PUBLIC_API_INDEX`, `Trace2D_PUBLIC_API_GUIDE`, and `Trace2D_CANONICAL_EXTERNAL_EXAMPLE_ROOT`. A normal non-quiet configure also prints the locator, metadata, Agent index, public authoring header, public API index and canonical example paths. The example root is packaged with the SDK, so the `canonical_example` references in the JSON remain inspectable even when the engine source checkout is unavailable.

## Discovery rule

For ordinary installed-SDK authoring:

1. If `find_package(Trace2D)` has run, read the exact `Trace2D_AGENT_INDEX` path printed/exported by the package before scanning the SDK.
2. If starting from the SDK root, read `trace2d.sdk.locator.json` and follow its `agent_index` path. The same index is also named by canonical SDK metadata.
3. Select only the one-to-three deterministic surface entries relevant to the task and read their small L1 metadata.
4. Prefer `#include <trace2d/Trace2D.hpp>` for ordinary gameplay authoring.
5. Query the machine-readable public API index for the fully qualified symbol only when an exact declaration header is needed.
6. Use its `include` field verbatim.
7. Inspect only the referenced canonical example or exact header needed to resolve remaining uncertainty.
8. If a symbol is not indexed, inspect the public include tree and current examples/tests before assuming support.
9. **Never manufacture a per-type header from the C++ type name.** Absence of `Game.hpp`, `SceneTemplate.hpp`, or a similar guessed file is not evidence that the engine is missing the type.

For the current application boundary, the concise gameplay-authoring form is:

```cpp
#include <trace2d/Trace2D.hpp>

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

The exact declaration remains `trace2d/application/Application.hpp`. `Game::OnFixedUpdate(GameContext&, const FixedUpdate&)` is the one required pure-virtual lifecycle callback. `OnStart(GameContext&)` and `OnStop(GameContext&)` are optional overrides. A concrete external `Game` that omits `OnFixedUpdate` is intentionally abstract and cannot be instantiated.

The canonical external-consumer implementation is under `examples/e0_external_game/`; it is ordinary public API usage, not benchmark-only scaffolding.

## Scene component registry discovery

`trace2d::scene::ComponentRegistry` is available from the gameplay umbrella and declared canonically by the scene component header:

```cpp
#include <trace2d/Trace2D.hpp>
// Exact narrow declaration: <trace2d/scene/Components.hpp>

trace2d::scene::ComponentRegistry registry{};
```

Use the `scene` Agent surface metadata first for ordinary registry/entity/component authoring, then the machine-readable `trace2d::scene::ComponentRegistry` public-index entry when a tool needs the exact declaration path. Do not infer `trace2d/scene/ComponentRegistry.hpp`; that file is not part of the public API. `examples/e0_external_game/ExampleGame.hpp` uses the authoring umbrella and shows registry-backed component declarations without relying on accidental transitive includes.

## Windowed presentation discovery

Do not bypass Trace2D with a second host renderer merely because the application example first discovered is headless. The same canonical external-consumer example contains a complete ordinary windowed path in `examples/e0_external_game/WindowedMain.cpp`.

Presentation remains deliberately outside `trace2d/Trace2D.hpp`. For a small external 2D game, discover these stable symbols directly from the machine-readable public API index and include their specialized headers:

- `trace2d::platform::Platform` for the window and native event/input host,
- `trace2d::render::Renderer` for production GPU presentation and capture,
- `trace2d::render::OrthographicCamera` and `trace2d::render::SpriteRenderData` for the minimal 2D draw path,
- `trace2d::application::Application::SetPresentationCallback` to bind presentation to the same fixed-step `Application` lifecycle as headless verification.

`WindowedMain.cpp` demonstrates the intended integration order: create the public `Platform` and `Renderer`, construct the same `Game`/`Application` used for deterministic execution, bind one presentation callback, feed `Platform` input into `Application`, step the application, and present through `Renderer`. This keeps gameplay state, input, verification, and presentation on one engine-owned path.

Higher-level retained Sprite/Text/UI presentation contracts remain independently discoverable through their public headers. Prefer those production contracts when the game needs their semantics; the minimal `SpriteRenderData` example is a compact starting point, not a replacement for retained presentation features.

## Scope

The umbrella, thin Agent index, surface metadata, and exact-symbol index are intentionally small and evidence-backed. Add a surface or symbol only when its public contract and canonical usage are stable enough to help agents avoid repository search/guessing. They are discovery aids, not generated reflection databases, ABI promises, benchmark-specific hints, or substitutes for compiling code and tests.
