#pragma once

#include <trace2d/platform/Platform.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace trace2d::render
{
struct ClearColor
{
    float red{0.08F};
    float green{0.09F};
    float blue{0.12F};
    float alpha{1.0F};
};

struct RendererConfig
{
    ClearColor clearColor{};
    bool enableDebugValidation{false};
};

struct RenderMetrics
{
    std::uint64_t submittedFrames{0};
    std::uint64_t presentedFrames{0};
    std::uint64_t renderPasses{0};
    std::uint64_t drawCalls{0};
    std::uint64_t submittedSprites{0};
    std::uint32_t lastTargetWidth{0};
    std::uint32_t lastTargetHeight{0};
};

class Renderer final
{
public:
    Renderer(const RendererConfig& config, const platform::Platform& platform);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void RenderFrame();

    [[nodiscard]] const RendererConfig& Config() const noexcept;
    [[nodiscard]] const RenderMetrics& Metrics() const noexcept;
    [[nodiscard]] std::string_view DriverName() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace trace2d::render
