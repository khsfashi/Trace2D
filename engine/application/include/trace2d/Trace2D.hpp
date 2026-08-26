#pragma once

// Trace2D external gameplay-authoring umbrella.
//
// Prefer this include when starting an ordinary game/application integration:
//
//     #include <trace2d/Trace2D.hpp>
//
// It intentionally exposes only the high-level gameplay surface already owned
// transitively by Trace2D::Application: Game/Application lifecycle, fixed-step
// input/actions, Scene/ComponentRegistry, and semantic UI state. Rendering,
// assets, audio, physics, profiling, platform hosting, and other specialized
// systems remain explicit includes so consumers do not pay unnecessary compile
// context for subsystems they do not use.
//
// Installed-SDK Agent discovery should prefer the tiny deterministic index
// before reading larger guides/examples/headers:
//   trace2d.agent-index.json
// `find_package(Trace2D CONFIG REQUIRED)` exposes its exact relocatable path as
// `Trace2D_AGENT_INDEX`. Canonical setup metadata and exact symbol/header
// mappings remain in:
//   trace2d.sdk.locator.json
//   share/Trace2D/trace2d.sdk.json
//   share/Trace2D/docs/agent-public-api-v1.json
// This header is the conventional include-tree entry point, not a reflection
// registry and not a runtime object.

#include <trace2d/application/Application.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/scene/Components.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/ui/Ui.hpp>
