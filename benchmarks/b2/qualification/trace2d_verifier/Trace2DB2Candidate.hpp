#pragma once

#include <trace2d/application/Application.hpp>
#include <trace2d/scene/Components.hpp>

#include <memory>

namespace trace2d::benchmark::b2
{
// Qualification/scored candidate boundary for the Trace2D lane. The candidate registers its
// ordinary runtime component types before the registry is frozen and returns a normal Game.
// The independent verifier owns Application construction, physical input scheduling and all
// observation through public Trace2D runtime/Agent/UI surfaces.
[[nodiscard]] std::unique_ptr<application::Game> CreateCandidate(
    scene::ComponentRegistry& registry);
} // namespace trace2d::benchmark::b2
