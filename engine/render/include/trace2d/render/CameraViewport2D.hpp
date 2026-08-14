#pragma once

#include <trace2d/render/RenderData.hpp>
#include <trace2d/scene/Camera2D.hpp>
#include <trace2d/scene/Scene.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace trace2d::render
{
enum class ViewportScaleMode2D : std::uint8_t
{
    Fit = 0,
    Fill,
    Stretch,
};

[[nodiscard]] std::string_view ToString(ViewportScaleMode2D mode) noexcept;

// Stable logical viewport identity. OS/window dimensions are deliberately supplied separately to
// ResolveViewport2D so a window resize never mutates authored logical dimensions implicitly.
struct Viewport2D final
{
    std::string semanticId{"main"};
    std::uint32_t logicalWidth{320U};
    std::uint32_t logicalHeight{180U};
    ViewportScaleMode2D scaleMode{ViewportScaleMode2D::Fit};

    [[nodiscard]] bool operator==(const Viewport2D&) const = default;
};

struct PresentationRect2D final
{
    Float2 origin{};
    Float2 size{};

    [[nodiscard]] bool operator==(const PresentationRect2D&) const noexcept = default;
};

struct ResolvedViewport2D final
{
    std::uint32_t logicalWidth{0U};
    std::uint32_t logicalHeight{0U};
    std::uint32_t targetWidth{0U};
    std::uint32_t targetHeight{0U};
    ViewportScaleMode2D scaleMode{ViewportScaleMode2D::Fit};
    PresentationRect2D contentRect{};
    Float2 viewportToPresentationScale{};
    Float2 presentationToViewportScale{};

    [[nodiscard]] bool operator==(const ResolvedViewport2D&) const noexcept = default;
};

enum class ViewportResolveError2D : std::uint8_t
{
    None = 0,
    EmptySemanticId,
    InvalidLogicalSize,
    InvalidTargetSize,
    InvalidScaleMode,
    InvalidMapping,
};

[[nodiscard]] std::string_view ToString(ViewportResolveError2D error) noexcept;

struct ViewportResolveResult2D final
{
    ViewportResolveError2D error{ViewportResolveError2D::None};
    ResolvedViewport2D viewport{};

    [[nodiscard]] bool Succeeded() const noexcept { return error == ViewportResolveError2D::None; }
};

[[nodiscard]] ViewportResolveResult2D ResolveViewport2D(
    const Viewport2D& viewport,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight) noexcept;

enum class ActiveCameraSelectionError2D : std::uint8_t
{
    None = 0,
    InvalidCameraType,
    InvalidViewport,
    NoActiveCamera,
};

[[nodiscard]] std::string_view ToString(ActiveCameraSelectionError2D error) noexcept;

struct ActiveCamera2D final
{
    scene::EntityId entity{};
    std::int32_t priority{0};

    [[nodiscard]] bool IsValid() const noexcept { return entity.IsValid(); }
    [[nodiscard]] bool operator==(const ActiveCamera2D&) const noexcept = default;
};

struct ActiveCameraSelectionResult2D final
{
    ActiveCameraSelectionError2D error{ActiveCameraSelectionError2D::None};
    ActiveCamera2D camera{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == ActiveCameraSelectionError2D::None && camera.IsValid();
    }
};

// Explicit selection is the only operation that scans entities or compares target viewport strings.
// Call it after structural changes or enabled/priority/target-viewport edits, then cache ActiveCamera2D.
[[nodiscard]] ActiveCameraSelectionResult2D ResolveActiveCamera2D(
    const scene::Scene& scene,
    scene::ComponentTypeHandle<scene::Camera2D> cameraType,
    const Viewport2D& viewport) noexcept;

enum class CameraFrameStateError2D : std::uint8_t
{
    None = 0,
    InvalidCameraType,
    StaleSelection,
    InactiveSelection,
    InvalidCameraState,
    InvalidWorldTransform,
};

[[nodiscard]] std::string_view ToString(CameraFrameStateError2D error) noexcept;

struct CameraFrameState2D final
{
    scene::EntityId entity{};
    Float2 center{};
    float verticalSize{0.0F};

    [[nodiscard]] bool operator==(const CameraFrameState2D&) const noexcept = default;
};

struct CameraFrameStateResult2D final
{
    CameraFrameStateError2D error{CameraFrameStateError2D::None};
    CameraFrameState2D state{};

    [[nodiscard]] bool Succeeded() const noexcept { return error == CameraFrameStateError2D::None; }
};

// O(hierarchy depth), allocation-free after component/world setup, and contains no semantic string
// lookup. Cached ActiveCamera2D becomes stale when camera selection inputs change.
[[nodiscard]] CameraFrameStateResult2D ResolveCameraFrameState2D(
    const scene::Scene& scene,
    scene::ComponentTypeHandle<scene::Camera2D> cameraType,
    ActiveCamera2D activeCamera) noexcept;

enum class PresentationSamplingMode2D : std::uint8_t
{
    AuthoritativeCurrent = 0,
    Interpolated,
};

[[nodiscard]] std::string_view ToString(PresentationSamplingMode2D mode) noexcept;

enum class PresentationViewError2D : std::uint8_t
{
    None = 0,
    InvalidViewport,
    InvalidCurrentCamera,
    MissingPreviousCamera,
    CameraHistoryMismatch,
    InvalidInterpolationAlpha,
    InvalidSamplingMode,
    InvalidProjection,
};

[[nodiscard]] std::string_view ToString(PresentationViewError2D error) noexcept;

// Fully resolved CPU view. RendererCamera is source-compatible with the existing Renderer and
// deterministically rebuilds presentationView for the same target dimensions. logicalView freezes
// world<->logical viewport conversion independently of OS/window size.
struct ResolvedPresentationView2D final
{
    scene::EntityId cameraEntity{};
    ResolvedViewport2D viewport{};
    PresentationSamplingMode2D samplingMode{PresentationSamplingMode2D::AuthoritativeCurrent};
    float interpolationAlpha{1.0F};
    OrthographicCamera rendererCamera{};
    OrthographicView logicalView{};
    OrthographicView presentationView{};

    [[nodiscard]] bool operator==(const ResolvedPresentationView2D&) const noexcept = default;
};

struct PresentationViewResult2D final
{
    PresentationViewError2D error{PresentationViewError2D::None};
    ResolvedPresentationView2D view{};

    [[nodiscard]] bool Succeeded() const noexcept { return error == PresentationViewError2D::None; }
};

// Exact-frame/headless capture should use AuthoritativeCurrent. Interactive hosts may pass the
// previous fixed state and an explicit alpha for Interpolated. The function never mutates either
// authoritative state and performs no allocation/GPU/filesystem work.
[[nodiscard]] PresentationViewResult2D ResolvePresentationView2D(
    const CameraFrameState2D& current,
    const CameraFrameState2D* previous,
    const ResolvedViewport2D& viewport,
    PresentationSamplingMode2D samplingMode = PresentationSamplingMode2D::AuthoritativeCurrent,
    float interpolationAlpha = 1.0F) noexcept;

// Continuous pixel-edge convention: logical (0,0) is the top-left viewport edge and
// (logicalWidth, logicalHeight) is the bottom-right edge. Presentation coordinates use the same
// convention against the target. Integer pixel-center policy remains the narrower SR6 contract.
[[nodiscard]] Float2 WorldToViewport(
    const ResolvedPresentationView2D& view,
    Float2 worldPosition) noexcept;
[[nodiscard]] Float2 ViewportToWorld(
    const ResolvedPresentationView2D& view,
    Float2 viewportPosition) noexcept;
[[nodiscard]] Float2 ViewportToPresentation(
    const ResolvedPresentationView2D& view,
    Float2 viewportPosition) noexcept;
[[nodiscard]] Float2 PresentationToViewport(
    const ResolvedPresentationView2D& view,
    Float2 presentationPosition) noexcept;
[[nodiscard]] Float2 WorldToPresentation(
    const ResolvedPresentationView2D& view,
    Float2 worldPosition) noexcept;
[[nodiscard]] Float2 PresentationToWorld(
    const ResolvedPresentationView2D& view,
    Float2 presentationPosition) noexcept;

// Useful for #72 pointer routing: fit-mode letterbox/pillarbox coordinates return false. Fill and
// stretch still require the point to lie inside the actual presentation target.
[[nodiscard]] bool IsPresentationPointInsideViewport(
    const ResolvedPresentationView2D& view,
    Float2 presentationPosition) noexcept;
} // namespace trace2d::render
