#pragma once

#include "NightfallProduct.hpp"
#include "NightfallSurvivorsGame.hpp"

#include <trace2d/application/Application.hpp>
#include <trace2d/render/Renderer.hpp>

#include <filesystem>
#include <memory>

class NightfallPresentation final
{
public:
    NightfallPresentation(
        trace2d::render::Renderer& renderer,
        NightfallSurvivorsGame& game,
        NightfallProduct& product,
        std::filesystem::path runtimeRoot);
    ~NightfallPresentation();

    NightfallPresentation(const NightfallPresentation&) = delete;
    NightfallPresentation& operator=(const NightfallPresentation&) = delete;
    NightfallPresentation(NightfallPresentation&&) = delete;
    NightfallPresentation& operator=(NightfallPresentation&&) = delete;

    static void Present(const trace2d::application::GameContext& context, void* userData);

private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
