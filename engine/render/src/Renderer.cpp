#include <trace2d/render/Renderer.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <stdexcept>
#include <string>

namespace trace2d::render
{
namespace
{
constexpr SDL_GPUShaderFormat SupportedShaderFormats = static_cast<SDL_GPUShaderFormat>(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL);

[[nodiscard]] std::runtime_error MakeSdlError(const char* context)
{
    return std::runtime_error{std::string{context} + ": " + SDL_GetError()};
}
} // namespace

class Renderer::Impl final
{
public:
    Impl(const RendererConfig& config, const platform::Platform& platform)
        : config_{config}
    {
        if (!platform.HasWindow() || platform.WindowIdValue() == platform::InvalidWindowId)
        {
            throw std::invalid_argument{"Trace2D renderer requires a windowed Platform instance."};
        }

        window_ = SDL_GetWindowFromID(platform.WindowIdValue());
        if (window_ == nullptr)
        {
            throw MakeSdlError("SDL window lookup failed");
        }

        device_ = SDL_CreateGPUDevice(SupportedShaderFormats, config_.enableDebugValidation, nullptr);
        if (device_ == nullptr)
        {
            throw MakeSdlError("SDL GPU device creation failed");
        }

        if (!SDL_ClaimWindowForGPUDevice(device_, window_))
        {
            const std::string error{SDL_GetError()};
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
            throw std::runtime_error{"SDL GPU window claim failed: " + error};
        }

        windowClaimed_ = true;

        const char* const driverName = SDL_GetGPUDeviceDriver(device_);
        if (driverName != nullptr)
        {
            driverName_ = driverName;
        }
    }

    ~Impl()
    {
        if (windowClaimed_ && device_ != nullptr && window_ != nullptr)
        {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
            windowClaimed_ = false;
        }

        if (device_ != nullptr)
        {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }

        window_ = nullptr;
    }

    void RenderFrame()
    {
        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU command buffer acquisition failed");
        }

        SDL_GPUTexture* swapchainTexture = nullptr;
        Uint32 targetWidth = 0;
        Uint32 targetHeight = 0;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                commandBuffer, window_, &swapchainTexture, &targetWidth, &targetHeight))
        {
            const std::string error{SDL_GetError()};
            SDL_CancelGPUCommandBuffer(commandBuffer);
            throw std::runtime_error{"SDL GPU swapchain acquisition failed: " + error};
        }

        bool encodedRenderPass = false;

        if (swapchainTexture != nullptr)
        {
            SDL_GPUColorTargetInfo colorTarget{};
            colorTarget.texture = swapchainTexture;
            colorTarget.clear_color = SDL_FColor{
                config_.clearColor.red,
                config_.clearColor.green,
                config_.clearColor.blue,
                config_.clearColor.alpha,
            };
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* const renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
            if (renderPass == nullptr)
            {
                const std::string error{SDL_GetError()};
                SDL_SubmitGPUCommandBuffer(commandBuffer);
                throw std::runtime_error{"SDL GPU render pass creation failed: " + error};
            }

            SDL_EndGPURenderPass(renderPass);
            encodedRenderPass = true;
        }

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            throw MakeSdlError("SDL GPU command buffer submission failed");
        }

        ++metrics_.submittedFrames;
        metrics_.lastTargetWidth = targetWidth;
        metrics_.lastTargetHeight = targetHeight;

        if (swapchainTexture != nullptr)
        {
            ++metrics_.presentedFrames;
        }

        if (encodedRenderPass)
        {
            ++metrics_.renderPasses;
        }
    }

    [[nodiscard]] const RendererConfig& Config() const noexcept
    {
        return config_;
    }

    [[nodiscard]] const RenderMetrics& Metrics() const noexcept
    {
        return metrics_;
    }

    [[nodiscard]] std::string_view DriverName() const noexcept
    {
        return driverName_;
    }

private:
    RendererConfig config_{};
    RenderMetrics metrics_{};
    SDL_Window* window_{nullptr};
    SDL_GPUDevice* device_{nullptr};
    bool windowClaimed_{false};
    std::string driverName_{};
};

Renderer::Renderer(const RendererConfig& config, const platform::Platform& platform)
    : impl_{std::make_unique<Impl>(config, platform)}
{
}

Renderer::~Renderer() = default;

void Renderer::RenderFrame()
{
    impl_->RenderFrame();
}

const RendererConfig& Renderer::Config() const noexcept
{
    return impl_->Config();
}

const RenderMetrics& Renderer::Metrics() const noexcept
{
    return impl_->Metrics();
}

std::string_view Renderer::DriverName() const noexcept
{
    return impl_->DriverName();
}
} // namespace trace2d::render
