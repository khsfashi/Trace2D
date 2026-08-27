#include "NightfallPresentation.hpp"

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/assets/TextureAssets.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>
#include <trace2d/render/SpriteRenderContract.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>
#include <trace2d/scene/SpriteTransform2D.hpp>
#include <trace2d/text/Text.hpp>
#include <trace2d/text/TextLayout.hpp>
#include <trace2d/text/TextPresentation2D.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using TextureHandle = trace2d::render::TextureHandle;
using Color = trace2d::render::SpriteLinearRgba;
using Screen = NightfallProduct::Screen;
using CharacterId = NightfallProduct::CharacterId;
using StageId = NightfallProduct::StageId;
using AchievementId = NightfallProduct::AchievementId;
constexpr float Pi = 3.14159265358979323846F;
constexpr float CoreWidth = NightfallSurvivorsGame::CameraHorizontalSize;
constexpr float CoreHeight = NightfallSurvivorsGame::CameraVerticalSize;
constexpr float CoreAspect = CoreWidth / CoreHeight;

constexpr Color White{1.0F, 1.0F, 1.0F, 1.0F};
constexpr Color Muted{0.67F, 0.72F, 0.82F, 1.0F};
constexpr Color Dim{0.38F, 0.42F, 0.52F, 1.0F};
constexpr Color Cyan{0.34F, 0.82F, 1.0F, 1.0F};
constexpr Color Gold{1.0F, 0.76F, 0.24F, 1.0F};
constexpr Color Red{1.0F, 0.30F, 0.34F, 1.0F};
constexpr Color Green{0.38F, 0.92F, 0.58F, 1.0F};
constexpr Color Panel{0.035F, 0.045F, 0.075F, 1.0F};
constexpr Color PanelLight{0.10F, 0.13F, 0.20F, 1.0F};

struct Visual final
{
    std::shared_ptr<trace2d::assets::SpriteAsset> asset{};
    std::vector<trace2d::render::ResolvedSpriteRegion> selections{};
    TextureHandle texture{};
    float pixelsPerUnit{1.0F};
};

[[nodiscard]] std::vector<std::uint8_t> ReadBinary(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input) throw std::runtime_error{"Nightfall Survivors could not read: " + path.string()};
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
}

[[nodiscard]] Visual MakeVisual(
    trace2d::assets::TextureAssetCache& cache,
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const std::string& reference,
    const std::uint32_t columns,
    const std::uint32_t rows,
    const float pixelsPerUnit)
{
    const auto loaded = cache.Load(reference);
    if (!loaded.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not load asset: " + reference};
    if (columns == 0U || rows == 0U || loaded.asset->width % columns != 0U ||
        loaded.asset->height % rows != 0U)
    {
        throw std::runtime_error{"Nightfall Survivors sprite grid does not divide source texture."};
    }

    trace2d::assets::TextureResource canonical{};
    canonical.width = loaded.asset->width;
    canonical.height = loaded.asset->height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Srgb;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors product asset";
    canonical.canonicalRgba8 = loaded.asset->rgba8;
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not publish texture."};

    trace2d::render::Rgba8TextureData gpuData{};
    gpuData.width = loaded.asset->width;
    gpuData.height = loaded.asset->height;
    gpuData.pixels = std::span<const std::uint8_t>{loaded.asset->rgba8.data(), loaded.asset->rgba8.size()};

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle, gpuData, trace2d::render::SpriteTextureEncoding::Srgb);
    visual.pixelsPerUnit = pixelsPerUnit;
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = reference + ".sprite";
    visual.asset->sampling = trace2d::assets::SpriteSampling::Nearest;
    visual.asset->pages.push_back({
        .id = "page",
        .textureReference = reference,
        .size = {loaded.asset->width, loaded.asset->height},
        .colorSpace = trace2d::assets::SpriteColorSpace::Srgb,
    });

    const std::uint32_t cellWidth = loaded.asset->width / columns;
    const std::uint32_t cellHeight = loaded.asset->height / rows;
    visual.asset->regions.reserve(static_cast<std::size_t>(columns) * rows);
    for (std::uint32_t row = 0U; row < rows; ++row)
    {
        for (std::uint32_t column = 0U; column < columns; ++column)
        {
            trace2d::assets::SpriteRegion region{};
            region.id = "region-" + std::to_string(row * columns + column);
            region.pageId = "page";
            region.sourceSize = {cellWidth, cellHeight};
            region.trimSize = {cellWidth, cellHeight};
            region.packedRect = {column * cellWidth, row * cellHeight, cellWidth, cellHeight};
            region.pivot = {
                static_cast<std::int64_t>(cellWidth),
                static_cast<std::int64_t>(cellHeight),
                2,
            };
            visual.asset->regions.push_back(std::move(region));
        }
    }

    visual.selections.resize(visual.asset->regions.size());
    for (std::size_t index = 0U; index < visual.selections.size(); ++index)
    {
        if (!trace2d::render::ResolveSpriteRegionByIndices(
                visual.asset.get(), 0U, index, visual.selections[index]).Succeeded())
        {
            throw std::runtime_error{"Nightfall Survivors could not resolve sprite region."};
        }
    }
    return visual;
}

[[nodiscard]] Visual MakeWhiteVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer)
{
    constexpr std::array<std::uint8_t, 4U> Pixels{255U, 255U, 255U, 255U};
    constexpr std::string_view Reference = "generated/nightfall-product-white.rgba8";
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors product UI primitive";
    canonical.canonicalRgba8.assign(Pixels.begin(), Pixels.end());
    const auto published = resources.PublishTexture(Reference, std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"Nightfall could not publish UI primitive."};

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle,
        trace2d::render::Rgba8TextureData{1U, 1U, Pixels},
        trace2d::render::SpriteTextureEncoding::Linear);
    visual.pixelsPerUnit = 1.0F;
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = "nightfall-product-white.sprite";
    visual.asset->sampling = trace2d::assets::SpriteSampling::Nearest;
    visual.asset->pages.push_back({
        .id = "page",
        .textureReference = std::string{Reference},
        .size = {1U, 1U},
        .colorSpace = trace2d::assets::SpriteColorSpace::Linear,
    });
    trace2d::assets::SpriteRegion region{};
    region.id = "region-0";
    region.pageId = "page";
    region.sourceSize = {1U, 1U};
    region.trimSize = {1U, 1U};
    region.packedRect = {0U, 0U, 1U, 1U};
    region.pivot = {1, 1, 2};
    visual.asset->regions.push_back(std::move(region));
    visual.selections.resize(1U);
    if (!trace2d::render::ResolveSpriteRegionByIndices(
            visual.asset.get(), 0U, 0U, visual.selections[0]).Succeeded())
    {
        throw std::runtime_error{"Nightfall could not resolve UI primitive."};
    }
    return visual;
}

void ReleaseVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    Visual& visual) noexcept
{
    if (visual.texture.generation != 0U)
    {
        renderer.DestroyTexture(visual.texture);
        static_cast<void>(resources.Unload(visual.texture.Untyped()));
    }
    visual.selections.clear();
    visual.asset.reset();
    visual.texture = {};
}

[[nodiscard]] Color CharacterTint(const std::uint32_t index) noexcept
{
    switch (index)
    {
    case 1U: return {1.0F, 0.58F, 0.34F, 1.0F};
    case 2U: return {0.66F, 0.80F, 1.0F, 1.0F};
    default: return White;
    }
}

[[nodiscard]] Color StageTint(const std::uint32_t index) noexcept
{
    switch (index)
    {
    case 1U: return {0.88F, 0.53F, 0.42F, 1.0F};
    case 2U: return {0.58F, 0.48F, 0.86F, 1.0F};
    default: return {0.82F, 0.88F, 1.0F, 1.0F};
    }
}

[[nodiscard]] std::string FormatTime(const float seconds)
{
    const std::uint32_t total = static_cast<std::uint32_t>(std::max(0.0F, std::floor(seconds)));
    const std::uint32_t minutes = total / 60U;
    const std::uint32_t remainder = total % 60U;
    std::string result = std::to_string(minutes) + ":";
    if (remainder < 10U) result += "0";
    result += std::to_string(remainder);
    return result;
}

[[nodiscard]] std::uint32_t DirectionRow(const NightfallSurvivorsGame& game) noexcept
{
    // rpg_sprite_walk.png row order is down, up, left, right.
    const auto facing = game.Facing();
    if (std::abs(facing.x) > std::abs(facing.y)) return facing.x < 0.0F ? 2U : 3U;
    return facing.y > 0.0F ? 1U : 0U;
}

[[nodiscard]] std::uint32_t CountBits(std::uint32_t value) noexcept
{
    std::uint32_t count = 0U;
    while (value != 0U)
    {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}
} // namespace

class NightfallPresentation::Impl final
{
public:
    Impl(
        trace2d::render::Renderer& renderer,
        NightfallSurvivorsGame& game,
        NightfallProduct& product,
        std::filesystem::path runtimeRoot)
        : renderer_{renderer}
        , game_{game}
        , product_{product}
        , runtimeRoot_{std::move(runtimeRoot)}
        , textureCache_{runtimeRoot_}
    {
        floor_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/floor.png", 1U, 1U, 16.0F);
        floorAlt_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/floor-alt.png", 1U, 1U, 16.0F);
        hero_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/warden-walk.png", 8U, 4U, 32.0F);
        ghoul_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/ghoul.png", 1U, 1U, 16.0F);
        brute_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/brute.png", 1U, 1U, 16.0F);
        particle_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/particle.png", 1U, 1U, 96.0F);
        white_ = MakeWhiteVisual(game_.Resources(), renderer_);
        draws_.reserve(4096U);
        textScratch_.resize(1024U);
        PrepareWalkAnimation();
        PrepareText();
    }

    ~Impl()
    {
        textLayout_.reset();
        glyphAtlas_.reset();
        if (glyphTexture_.generation != 0U)
        {
            renderer_.DestroyTexture(glyphTexture_);
            static_cast<void>(game_.Resources().Unload(glyphTexture_.Untyped()));
        }
        ReleaseVisual(game_.Resources(), renderer_, white_);
        ReleaseVisual(game_.Resources(), renderer_, particle_);
        ReleaseVisual(game_.Resources(), renderer_, brute_);
        ReleaseVisual(game_.Resources(), renderer_, ghoul_);
        ReleaseVisual(game_.Resources(), renderer_, hero_);
        ReleaseVisual(game_.Resources(), renderer_, floorAlt_);
        ReleaseVisual(game_.Resources(), renderer_, floor_);
    }

    void Present(const trace2d::application::GameContext& context)
    {
        draws_.clear();
        order_ = 0U;
        UpdateView();

        const Screen screen = product_.CurrentScreen();
        const bool worldVisible = screen == Screen::Playing || screen == Screen::Pause || screen == Screen::Result;
        if (worldVisible)
        {
            DrawWorld(context);
            DrawHud(context);
            if (game_.CurrentState() == NightfallSurvivorsGame::State::LevelUp && screen == Screen::Playing)
                DrawLevelUp(context);
            if (screen == Screen::Pause) DrawPause(context);
            else if (screen == Screen::Result) DrawResult(context);
        }
        else
        {
            DrawMenuBackdrop();
            switch (screen)
            {
            case Screen::MainMenu: DrawMainMenu(); break;
            case Screen::Profile: DrawProfile(); break;
            case Screen::Achievements: DrawAchievements(); break;
            case Screen::Settings: DrawSettings(); break;
            case Screen::CharacterSelect: DrawCharacterSelect(); break;
            case Screen::StageSelect: DrawStageSelect(); break;
            case Screen::Playing:
            case Screen::Pause:
            case Screen::Result:
                break;
            }
        }

        renderer_.RenderFrame(
            camera_,
            std::span<const trace2d::render::SpritePresentationRenderData>{draws_.data(), draws_.size()});
    }

private:
    void PrepareWalkAnimation()
    {
        constexpr auto FrameDuration = std::chrono::milliseconds{70};
        for (std::uint32_t row = 0U; row < 4U; ++row)
        {
            std::array<trace2d::runtime::SpriteAnimationFrame2D, 8U> frames{};
            for (std::uint32_t frame = 0U; frame < frames.size(); ++frame)
            {
                frames[frame].regionIndex = row * 8U + frame;
                frames[frame].duration = FrameDuration;
            }
            if (!trace2d::runtime::SpriteAnimationClip2D::Prepare(
                    hero_.asset.get(),
                    static_cast<std::uint32_t>(hero_.asset->regions.size()),
                    frames,
                    walkClips_[row]).Succeeded())
            {
                throw std::runtime_error{"Nightfall could not prepare walk animation."};
            }

            trace2d::runtime::SpriteAnimator2DState animatorState{};
            if (!trace2d::runtime::MakeSpriteAnimator2DState(
                    walkClips_[row], {},
                    trace2d::runtime::SpriteAnimationPlaybackState::Playing,
                    trace2d::runtime::SpriteAnimationLoopMode::Loop,
                    trace2d::runtime::SpriteAnimationDirection::Forward,
                    false, {1U, 1U}, animatorState).Succeeded() ||
                !walkAnimators_[row].RestoreState(animatorState).Succeeded())
            {
                throw std::runtime_error{"Nightfall could not initialize walk animator."};
            }
        }
    }

    void AdvanceWalkAnimation()
    {
        const std::uint64_t frame = game_.FrameCounter();
        if (frame <= lastAnimationFrame_) return;
        const auto delta = std::chrono::nanoseconds{
            static_cast<std::int64_t>(frame - lastAnimationFrame_) * 16'666'667LL};
        lastAnimationFrame_ = frame;
        std::array<trace2d::runtime::SpriteAnimationEmission2D, 32U> emissions{};
        for (auto& animator : walkAnimators_)
        {
            if (!animator.Advance(delta, emissions).Succeeded())
                throw std::runtime_error{"Nightfall walk animation advance failed."};
        }
    }

    void PrepareText()
    {
        trace2d::assets::FontResource font{};
        font.canonicalBytes = ReadBinary(runtimeRoot_ / "fonts/NotoSansKR.ttf");
        const auto fontPublished = game_.Resources().PublishFont("fonts/NotoSansKR.ttf", std::move(font));
        if (!fontPublished.Succeeded()) throw std::runtime_error{"Nightfall could not publish Korean font."};

        auto atlas = trace2d::text::PrepareGlyphAtlas(
            game_.Resources(),
            fontPublished.handle,
            trace2d::text::GlyphAtlasConfig{2048U, 2048U, 32U, 1U, 1024U});
        if (!atlas.Succeeded()) throw std::runtime_error{"Nightfall could not prepare Korean glyph atlas."};
        glyphAtlas_ = std::move(atlas.atlas);

        // Prewarm every static Hangul/ASCII symbol used by menus plus numeric runtime HUD output.
        constexpr std::string_view Corpus =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 %/:+-.[]()·<>"
            "NIGHTFALL SURVIVORS Trace2D WASD QEF ESC ENTER SPACE"
            "게임 시작 프로필 업적 설정 종료 돌아가기 캐릭터 선택 스테이지 선택 잠김 해금 달성 미달성"
            "별의 전사 균형형 자동 사격 기본에 충실한 전사 이동 화력 체력이 모두 안정적입니다"
            "잿불 기사 고화력 중장갑 느리지만 더 강하고 튼튼합니다 첫 스테이지 클리어로 해금됩니다"
            "달빛 사냥꾼 고속 연사 특화 빠르게 움직이고 더 자주 사격합니다 누적 킬로 해금됩니다"
            "월광 폐허 생존 시간 분 첫 번째 밤 적의 밀도가 서서히 증가합니다"
            "잿불 지하묘지 초 더 질기고 빠른 적이 몰려옵니다 클리어로 해금됩니다"
            "성운 심연 최종 스테이지 강한 적과 높은 밀도를 버티면 완주입니다"
            "첫 사냥 적을 처음으로 처치하세요 밤의 생존자 아무 스테이지나 처음 클리어하세요"
            "백인참 누적 킬을 달성하세요 성장하는 별 한 번의 런에서 레벨 에 도달하세요"
            "심연 정복 성운 심연을 클리어하세요 건재한 귀환 체력 이상을 남기고 클리어하세요"
            "총 플레이 횟수 누적 처치 클리어 최고 레벨 최고 생존 별가루 캐릭터 스테이지"
            "효과음 볼륨 화면 흔들림 켜짐 꺼짐 좌우 조절 선택 확인 일시정지 계속하기 메인 메뉴"
            "레벨 업 연사 공격 간격 감소 레벨마다 탄환 화력 투사체와 궤도 공격력 증가 궤도 주변 회전무기 최대 개"
            "체력 경험치 남은 시간 레벨 처치 현재 강화 패배 스테이지 클리어 다시 도전 획득 새 캐릭터 새 스테이지 새 업적"
            "조작 이동 자동공격 ESC 일시정지 R 재시작";
        if (!glyphAtlas_->WarmUtf8(Corpus).Succeeded())
            throw std::runtime_error{"Nightfall could not warm Korean glyph atlas."};

        const trace2d::text::GlyphAtlasConfig atlasConfig = glyphAtlas_->Config();
        trace2d::assets::TextureResource texture{};
        texture.width = atlasConfig.width;
        texture.height = atlasConfig.height;
        texture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
        texture.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
        texture.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
        texture.retentionReason = "Nightfall Korean glyph atlas";
        texture.canonicalRgba8.resize(
            static_cast<std::size_t>(atlasConfig.width) * atlasConfig.height * 4U);
        std::size_t requiredBytes = 0U;
        if (!trace2d::text::WriteGlyphAtlasRgba8(
                *glyphAtlas_, texture.canonicalRgba8, requiredBytes).Succeeded() ||
            requiredBytes != texture.canonicalRgba8.size())
        {
            throw std::runtime_error{"Nightfall could not rasterize Korean glyph atlas."};
        }

        const auto texturePublished = game_.Resources().PublishTexture(
            "generated/nightfall-korean-glyph-atlas.rgba8", std::move(texture));
        if (!texturePublished.Succeeded()) throw std::runtime_error{"Nightfall could not publish glyph texture."};
        const auto* const canonical = game_.Resources().Resolve(texturePublished.handle);
        if (canonical == nullptr) throw std::runtime_error{"Nightfall lost glyph texture resource."};
        glyphTexture_ = renderer_.CreateSpriteTextureRgba8(
            texturePublished.handle,
            trace2d::render::Rgba8TextureData{
                canonical->width,
                canonical->height,
                std::span<const std::uint8_t>{canonical->canonicalRgba8.data(), canonical->canonicalRgba8.size()},
            },
            trace2d::render::SpriteTextureEncoding::Linear);
        if (!trace2d::text::ResolveGlyphAtlasTextureBinding2D(
                *glyphAtlas_, game_.Resources(), glyphTexture_, glyphBinding_).Succeeded())
        {
            throw std::runtime_error{"Nightfall could not bind Korean glyph atlas."};
        }

        auto layout = trace2d::text::PrepareTextLayoutRun({1024U, 64U});
        if (!layout.Succeeded()) throw std::runtime_error{"Nightfall could not prepare text layout."};
        textLayout_ = std::move(layout.run);
    }

    void UpdateView()
    {
        const auto metrics = renderer_.Metrics();
        const std::uint32_t width = metrics.lastTargetWidth == 0U
            ? static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasWidth)
            : metrics.lastTargetWidth;
        const std::uint32_t height = metrics.lastTargetHeight == 0U
            ? static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasHeight)
            : metrics.lastTargetHeight;
        targetAspect_ = static_cast<float>(width) / static_cast<float>(height);
        viewVerticalSize_ = targetAspect_ >= CoreAspect ? CoreHeight : CoreWidth / targetAspect_;
        viewHalfHeight_ = viewVerticalSize_ * 0.5F;
        viewHalfWidth_ = viewHalfHeight_ * targetAspect_;

        trace2d::scene::Vector2 center{};
        const Screen screen = product_.CurrentScreen();
        if (screen == Screen::Playing || screen == Screen::Pause || screen == Screen::Result)
        {
            const trace2d::scene::Entity* const player = currentContext_->Scene().TryGet(game_.Player());
            if (player != nullptr) center = player->LocalTransform().position;
        }
        cameraCenter_ = center;
        const float shake = product_.CameraShakeEnabled() && game_.ScreenShakeFrames() > 0U
            ? (((game_.FrameCounter() & 1U) != 0U) ? 0.08F : -0.08F)
            : 0.0F;
        camera_.center = {center.x + shake, center.y - shake * 0.5F};
        camera_.verticalSize = viewVerticalSize_;
    }

    void AddVisual(
        const Visual& visual,
        const std::size_t regionIndex,
        const trace2d::scene::Vector2 position,
        const trace2d::scene::Vector2 scale,
        const float rotation,
        const Color tint,
        const float opacity,
        const trace2d::render::SpriteBlendMode blend,
        const std::int32_t layer)
    {
        if (regionIndex >= visual.selections.size()) throw std::runtime_error{"Nightfall region overflow."};
        trace2d::scene::SpritePose2D pose{};
        pose.transform.position = position;
        pose.transform.rotationRadians = rotation;
        pose.transform.scale = scale;
        trace2d::render::SpriteAppearance2D appearance{};
        appearance.tint = tint;
        appearance.opacity = opacity;
        appearance.sampling = trace2d::render::SpriteAppearanceSampling::Nearest;
        appearance.blend = blend;

        trace2d::render::SpritePresentationRenderData draw{};
        if (!trace2d::render::BuildSpritePresentation2D(
                visual.selections[regionIndex], pose, visual.pixelsPerUnit, appearance, draw.presentation).Succeeded())
        {
            throw std::runtime_error{"Nightfall sprite presentation failed."};
        }
        draw.texture = visual.texture;
        draw.order.layer = layer;
        draw.order.stableOrder = order_++;
        draws_.push_back(draw);
    }

    void AddPanel(
        const float x,
        const float y,
        const float width,
        const float height,
        const Color color,
        const float opacity,
        const std::int32_t layer)
    {
        AddVisual(white_, 0U, {x, y}, {width, height}, 0.0F, color, opacity,
            trace2d::render::SpriteBlendMode::Normal, layer);
    }

    void AddText(
        const std::string_view text,
        const float x,
        const float topY,
        const float pixelsPerUnit,
        const Color color,
        const bool centered = false,
        const std::int32_t layer = 70)
    {
        const trace2d::text::TextFontAtlasRef atlasRef{glyphAtlas_.get()};
        const std::span<const trace2d::text::TextFontAtlasRef> atlases(&atlasRef, 1U);
        trace2d::text::TextLayoutOptions options{};
        options.wrapMode = trace2d::text::TextWrapMode::None;
        const auto layoutResult = textLayout_->LayoutUtf8(atlases, text, options);
        if (!layoutResult.Succeeded()) throw std::runtime_error{"Nightfall Korean text layout failed."};

        const auto metrics = textLayout_->Metrics();
        const float width = static_cast<float>(metrics.contentWidth26_6) / (64.0F * pixelsPerUnit);
        const float originX = centered ? x - width * 0.5F : x;

        trace2d::text::TextPresentationConfig2D config{};
        config.origin = {originX, topY};
        config.pixelsPerUnit = pixelsPerUnit;
        config.painterLayer = layer;
        config.painterOrder = 0;
        config.stableOrderBase = order_;
        config.tint = color;
        config.opacity = 1.0F;
        config.sampler = trace2d::render::SpriteSamplerCompatibility::Linear;

        std::size_t requiredCount = 0U;
        trace2d::text::TextPresentationMeasurement2D measurement{};
        const auto status = trace2d::text::BuildTextPresentation2D(
            *textLayout_, atlases,
            std::span<const trace2d::text::GlyphAtlasTextureBinding2D>(&glyphBinding_, 1U),
            config,
            textScratch_,
            requiredCount,
            measurement);
        if (!status.Succeeded() || requiredCount > textScratch_.size())
            throw std::runtime_error{"Nightfall Korean text presentation failed."};

        for (std::size_t index = 0U; index < requiredCount; ++index)
        {
            auto draw = textScratch_[index];
            auto flipY = [topY](trace2d::render::SpriteDrawVertex& vertex) {
                vertex.position.y = 2.0F * topY - vertex.position.y;
            };
            flipY(draw.presentation.quad.topLeft);
            flipY(draw.presentation.quad.topRight);
            flipY(draw.presentation.quad.bottomRight);
            flipY(draw.presentation.quad.bottomLeft);
            draws_.push_back(draw);
        }
        order_ += static_cast<std::uint64_t>(textLayout_->Glyphs().size()) + 1U;
    }

    void DrawFloor(const trace2d::scene::Vector2 center, const Color tint, const float opacity)
    {
        const int minX = static_cast<int>(std::floor(center.x - viewHalfWidth_)) - 2;
        const int maxX = static_cast<int>(std::ceil(center.x + viewHalfWidth_)) + 2;
        const int minY = static_cast<int>(std::floor(center.y - viewHalfHeight_)) - 2;
        const int maxY = static_cast<int>(std::ceil(center.y + viewHalfHeight_)) + 2;
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const bool accent = ((x * 17 + y * 31) & 15) == 0;
                AddVisual(
                    accent ? floorAlt_ : floor_, 0U,
                    {static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F},
                    {1.0F, 1.0F}, 0.0F, tint, opacity,
                    trace2d::render::SpriteBlendMode::Normal, -20);
            }
        }
    }

    void DrawMenuBackdrop()
    {
        DrawFloor({}, {0.30F, 0.38F, 0.56F, 1.0F}, 0.55F);
        AddPanel(0.0F, 0.0F, CoreWidth, CoreHeight, {0.012F, 0.018F, 0.038F, 1.0F}, 0.76F, 20);
    }

    void DrawWorld(const trace2d::application::GameContext& context)
    {
        const trace2d::scene::Entity* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto playerPosition = player->LocalTransform().position;
        const auto run = game_.CurrentRunConfig();
        DrawFloor(playerPosition, StageTint(run.stageIndex), 0.92F);

        const float phase = static_cast<float>(game_.FrameCounter() % 120U) * 0.0523598776F;
        for (const auto& gem : game_.Gems())
        {
            if (!gem.active) continue;
            const float pulse = 0.30F + std::sin(phase + static_cast<float>(gem.stableId) * 0.31F) * 0.04F;
            AddVisual(particle_, 0U, gem.position, {pulse, pulse}, 0.0F,
                {0.40F, 0.55F, 1.0F, 1.0F}, 1.0F,
                trace2d::render::SpriteBlendMode::Additive, 0);
        }

        for (const auto& enemy : game_.Enemies())
        {
            if (!enemy.active) continue;
            const Visual& visual = enemy.kind == NightfallSurvivorsGame::EnemyKind::Brute ? brute_ : ghoul_;
            const float scale = enemy.kind == NightfallSurvivorsGame::EnemyKind::Brute ? 1.20F : 0.92F;
            Color tint = enemy.kind == NightfallSurvivorsGame::EnemyKind::Wisp
                ? Color{0.62F, 0.55F, 1.0F, 0.92F}
                : White;
            if (enemy.hitFlashFrames > 0U) tint = {1.0F, 0.92F, 0.48F, 1.0F};
            const float bob = std::sin(phase * 1.5F + static_cast<float>(enemy.stableId) * 0.37F) * 0.04F;
            AddVisual(visual, 0U, {enemy.position.x, enemy.position.y + bob}, {scale, scale}, 0.0F,
                tint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 4);
        }

        for (const auto& projectile : game_.Projectiles())
        {
            if (!projectile.active) continue;
            AddVisual(particle_, 0U, projectile.position, {0.28F, 0.16F},
                std::atan2(projectile.velocity.y, projectile.velocity.x),
                {0.25F, 0.88F, 1.0F, 1.0F}, 1.0F,
                trace2d::render::SpriteBlendMode::Additive, 8);
        }

        for (const auto& effect : game_.Effects())
        {
            if (!effect.active || effect.lifetimeSeconds <= 0.0F) continue;
            const float t = std::clamp(effect.ageSeconds / effect.lifetimeSeconds, 0.0F, 1.0F);
            const float scale = effect.startScale + (effect.endScale - effect.startScale) * t;
            Color tint{1.0F, 0.78F, 0.30F, 1.0F};
            if (effect.kind == NightfallSurvivorsGame::EffectKind::Death) tint = {1.0F, 0.34F, 0.24F, 1.0F};
            else if (effect.kind == NightfallSurvivorsGame::EffectKind::LevelUp) tint = Cyan;
            else if (effect.kind == NightfallSurvivorsGame::EffectKind::PlayerHurt) tint = Red;
            AddVisual(particle_, 0U, effect.position, {scale, scale}, t * Pi,
                tint, 1.0F - t, trace2d::render::SpriteBlendMode::Additive, 10);
            if (effect.kind == NightfallSurvivorsGame::EffectKind::Death)
            {
                for (std::uint32_t burst = 0U; burst < 4U; ++burst)
                {
                    const float angle = static_cast<float>(burst) * Pi * 0.5F +
                        static_cast<float>(effect.stableId) * 0.19F;
                    const float distance = t * 0.55F;
                    AddVisual(particle_, 0U,
                        {effect.position.x + std::cos(angle) * distance,
                         effect.position.y + std::sin(angle) * distance},
                        {scale * 0.38F, scale * 0.38F}, angle, tint, 1.0F - t,
                        trace2d::render::SpriteBlendMode::Additive, 10);
                }
            }
        }

        const std::uint32_t orbitCount = std::min<std::uint32_t>(game_.OrbitLevel(), 4U);
        for (std::uint32_t blade = 0U; blade < orbitCount; ++blade)
        {
            const float angle = game_.ElapsedSeconds() * 3.4F +
                2.0F * Pi * static_cast<float>(blade) / static_cast<float>(orbitCount);
            const float radius = 1.25F + static_cast<float>(orbitCount) * 0.08F;
            AddVisual(particle_, 0U,
                {playerPosition.x + std::cos(angle) * radius, playerPosition.y + std::sin(angle) * radius},
                {0.34F, 0.18F}, angle, Gold, 1.0F,
                trace2d::render::SpriteBlendMode::Additive, 12);
        }

        AdvanceWalkAnimation();
        const std::uint32_t row = DirectionRow(game_);
        std::uint32_t heroRegion = row * 8U;
        if (game_.MoveIntent().x != 0.0F || game_.MoveIntent().y != 0.0F)
        {
            if (!walkAnimators_[row].TryGetCurrentRegionIndex(heroRegion))
                throw std::runtime_error{"Nightfall could not resolve walk frame."};
        }
        Color heroTint = CharacterTint(run.characterIndex);
        if (game_.PlayerFlashFrames() > 0U && (game_.FrameCounter() & 1U) != 0U)
            heroTint = {1.0F, 0.42F, 0.42F, 1.0F};
        AddVisual(hero_, heroRegion, playerPosition, {1.15F, 1.15F}, 0.0F,
            heroTint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 15);
    }

    void DrawHud(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        const float healthRatio = static_cast<float>(game_.Health()) /
            static_cast<float>(std::max<std::uint32_t>(1U, game_.MaximumHealth()));
        const float xpRatio = static_cast<float>(game_.Experience()) /
            static_cast<float>(std::max<std::uint32_t>(1U, game_.ExperienceToNextLevel()));
        const float left = center.x - 7.45F;
        const float top = center.y + 4.12F;
        const float bottom = center.y - 4.15F;

        AddPanel(center.x, top - 0.04F, 15.2F, 0.55F, Panel, 0.86F, 30);
        AddPanel(left + 1.55F, top - 0.02F, 3.10F, 0.17F, PanelLight, 1.0F, 31);
        AddPanel(left + 1.55F * healthRatio, top - 0.02F, 3.10F * healthRatio, 0.11F, Red, 1.0F, 32);
        AddText(
            "체력 " + std::to_string(game_.Health()) + "/" + std::to_string(game_.MaximumHealth()),
            left, top + 0.20F, 62.0F, White, false, 65);

        const StageId stage = static_cast<StageId>(std::min<std::uint32_t>(
            game_.CurrentRunConfig().stageIndex,
            static_cast<std::uint32_t>(NightfallProduct::StageCount() - 1U)));
        AddText(NightfallProduct::Stage(stage).nameKo, center.x, top + 0.22F, 46.0F, White, true, 65);
        const float remaining = std::max(0.0F, game_.RunDurationSeconds() - game_.ElapsedSeconds());
        AddText("남은 시간 " + FormatTime(remaining), center.x, top - 0.17F, 60.0F, Cyan, true, 65);
        AddText(
            "레벨 " + std::to_string(game_.Level()) + " · 처치 " + std::to_string(game_.KillCount()),
            center.x + 7.35F, top + 0.20F, 62.0F, White, false, 65);

        AddPanel(center.x, bottom, 15.0F, 0.18F, PanelLight, 1.0F, 31);
        if (xpRatio > 0.0F)
        {
            AddPanel(
                center.x - 7.5F + 15.0F * xpRatio * 0.5F,
                bottom,
                15.0F * xpRatio,
                0.10F,
                Cyan,
                1.0F,
                32);
        }
        AddText(
            "연사 " + std::to_string(game_.RapidLevel()) +
            "   화력 " + std::to_string(game_.MightLevel()) +
            "   궤도 " + std::to_string(game_.OrbitLevel()),
            center.x,
            bottom + 0.36F,
            60.0F,
            Muted,
            true,
            65);
    }

    void DrawLevelUp(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, {0.01F, 0.015F, 0.035F, 1.0F}, 0.84F, 50);
        AddText("레벨 업", center.x, center.y + 3.45F, 25.0F, Gold, true, 72);
        AddText("Q / E / F 중 하나를 선택하세요", center.x, center.y + 2.70F, 52.0F, Muted, true, 72);

        constexpr std::array<float, 3U> X{-4.8F, 0.0F, 4.8F};
        constexpr std::array<Color, 3U> Colors{{Cyan, Red, Gold}};
        constexpr std::array<std::string_view, 3U> Titles{{"Q  연사", "E  화력", "F  궤도"}};
        constexpr std::array<std::string_view, 3U> Lines1{{
            "공격 간격 감소",
            "투사체 공격력 증가",
            "주변 회전무기 +1",
        }};
        constexpr std::array<std::string_view, 3U> Lines2{{
            "3레벨마다 탄환 +1",
            "궤도 공격력도 증가",
            "최대 4개 · 이후 체력 강화",
        }};
        for (std::size_t i = 0U; i < X.size(); ++i)
        {
            const float x = center.x + X[i];
            AddPanel(x, center.y, 4.15F, 4.8F, PanelLight, 0.97F, 51);
            AddPanel(x, center.y + 1.98F, 4.15F, 0.10F, Colors[i], 1.0F, 52);
            AddText(Titles[i], x, center.y + 1.45F, 38.0F, Colors[i], true, 73);
            AddText(Lines1[i], x, center.y + 0.40F, 55.0F, White, true, 73);
            AddText(Lines2[i], x, center.y - 0.22F, 64.0F, Muted, true, 73);
        }
    }

    void DrawMainMenu()
    {
        AddText("NIGHTFALL SURVIVORS", -7.0F, 3.55F, 21.5F, White, false, 70);
        AddText("Trace2D 생존 액션", -6.95F, 2.63F, 48.0F, Cyan, false, 70);
        AddPanel(-4.65F, -0.65F, 5.2F, 5.4F, Panel, 0.88F, 40);

        constexpr std::array<std::string_view, 5U> Menu{{"게임 시작", "프로필", "업적", "설정", "종료"}};
        for (std::size_t i = 0U; i < Menu.size(); ++i)
        {
            const float y = 1.05F - static_cast<float>(i) * 0.92F;
            const bool selected = product_.Selection() == i;
            if (selected) AddPanel(-4.65F, y - 0.12F, 4.55F, 0.72F, {0.12F, 0.30F, 0.48F, 1.0F}, 1.0F, 42);
            AddText(Menu[i], -6.45F, y + 0.16F, 43.0F, selected ? White : Muted, false, 70);
        }

        AddPanel(3.55F, -0.55F, 6.2F, 5.55F, {0.05F, 0.07F, 0.12F, 1.0F}, 0.94F, 40);
        AddText("별의 전사", 3.55F, 1.93F, 37.0F, Gold, true, 70);
        AddVisual(hero_, 0U, {3.55F, 0.65F}, {2.8F, 2.8F}, 0.0F, White, 1.0F,
            trace2d::render::SpriteBlendMode::Normal, 44);
        const auto& profile = product_.PlayerProfile();
        AddText("별가루  " + std::to_string(profile.stars), 1.15F, -0.75F, 55.0F, White, false, 70);
        AddText("플레이  " + std::to_string(profile.totalRuns) + "회", 1.15F, -1.35F, 58.0F, Muted, false, 70);
        AddText("클리어  " + std::to_string(profile.totalClears) + "회", 1.15F, -1.90F, 58.0F, Muted, false, 70);
        AddText("W/S 선택 · ENTER 확인 · ESC 종료", 0.0F, -4.02F, 61.0F, Dim, true, 70);
    }

    void DrawProfile()
    {
        AddText("프로필", 0.0F, 3.45F, 25.0F, White, true, 70);
        AddPanel(0.0F, -0.25F, 12.5F, 6.2F, Panel, 0.91F, 40);
        const auto& p = product_.PlayerProfile();
        constexpr float Left = -5.2F;
        float y = 2.15F;
        const auto line = [this, &y](const std::string& value) {
            AddText(value, Left, y, 48.0F, White, false, 70);
            y -= 0.72F;
        };
        line("총 플레이 횟수   " + std::to_string(p.totalRuns));
        line("누적 처치        " + std::to_string(p.totalKills));
        line("클리어           " + std::to_string(p.totalClears));
        line("최고 레벨        " + std::to_string(p.bestLevel));
        line("최고 생존        " + FormatTime(p.bestSurvivalSeconds));
        line("별가루           " + std::to_string(p.stars));
        line("캐릭터 해금      " + std::to_string(CountBits(p.unlockedCharactersMask)) + "/" +
            std::to_string(NightfallProduct::CharacterCount()));
        line("스테이지 해금    " + std::to_string(CountBits(p.unlockedStagesMask)) + "/" +
            std::to_string(NightfallProduct::StageCount()));
        AddText("ENTER 또는 ESC로 돌아가기", 0.0F, -3.72F, 60.0F, Dim, true, 70);
    }

    void DrawAchievements()
    {
        AddText("업적", 0.0F, 3.55F, 25.0F, White, true, 70);
        AddPanel(0.0F, -0.20F, 14.0F, 6.3F, Panel, 0.92F, 40);
        float y = 2.30F;
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            const auto id = static_cast<AchievementId>(i);
            const bool unlocked = product_.IsAchievementUnlocked(id);
            const auto& definition = NightfallProduct::Achievement(id);
            AddPanel(-5.95F, y - 0.13F, 0.62F, 0.48F, unlocked ? Green : PanelLight, 1.0F, 42);
            AddText(unlocked ? "달성" : "미달성", -5.58F, y + 0.08F, 68.0F, unlocked ? Green : Dim, false, 70);
            AddText(definition.nameKo, -4.35F, y + 0.14F, 51.0F, unlocked ? White : Muted, false, 70);
            AddText(definition.descriptionKo, -1.25F, y + 0.11F, 72.0F, Dim, false, 70);
            y -= 0.86F;
        }
        AddText("ENTER 또는 ESC로 돌아가기", 0.0F, -3.72F, 60.0F, Dim, true, 70);
    }

    void DrawSettings()
    {
        AddText("설정", 0.0F, 3.45F, 25.0F, White, true, 70);
        AddPanel(0.0F, -0.10F, 10.5F, 5.7F, Panel, 0.92F, 40);
        constexpr std::array<float, 3U> Y{1.35F, 0.25F, -1.55F};
        const auto& profile = product_.PlayerProfile();
        const std::string volume = profile.sfxVolumeStep == 0U ? "0%" : (profile.sfxVolumeStep == 1U ? "50%" : "100%");
        const std::array<std::string, 3U> labels{{
            "효과음 볼륨     " + volume,
            std::string{"화면 흔들림     "} + (profile.cameraShakeEnabled ? "켜짐" : "꺼짐"),
            "돌아가기",
        }};
        for (std::size_t i = 0U; i < labels.size(); ++i)
        {
            const bool selected = product_.Selection() == i;
            if (selected) AddPanel(0.0F, Y[i] - 0.12F, 8.7F, 0.82F, {0.13F, 0.29F, 0.46F, 1.0F}, 1.0F, 42);
            AddText(labels[i], -3.75F, Y[i] + 0.16F, 43.0F, selected ? White : Muted, false, 70);
        }
        AddText("W/S 선택 · A/D 또는 ←/→ 조절 · ENTER 확인", 0.0F, -3.20F, 66.0F, Dim, true, 70);
    }

    void DrawCharacterSelect()
    {
        AddText("캐릭터 선택", 0.0F, 3.55F, 25.0F, White, true, 70);
        constexpr std::array<float, 3U> X{-5.0F, 0.0F, 5.0F};
        for (std::size_t i = 0U; i < X.size(); ++i)
        {
            const auto id = static_cast<CharacterId>(i);
            const auto& definition = NightfallProduct::Character(id);
            const bool unlocked = product_.IsCharacterUnlocked(id);
            const bool selected = product_.Selection() == i;
            AddPanel(X[i], 0.15F, 4.25F, 5.65F,
                selected ? Color{0.10F, 0.20F, 0.34F, 1.0F} : Panel,
                0.96F, 40);
            if (selected) AddPanel(X[i], 2.93F, 4.25F, 0.08F, unlocked ? Cyan : Red, 1.0F, 42);
            AddVisual(hero_, 0U, {X[i], 1.25F}, {2.1F, 2.1F}, 0.0F,
                unlocked ? CharacterTint(static_cast<std::uint32_t>(i)) : Dim,
                unlocked ? 1.0F : 0.65F,
                trace2d::render::SpriteBlendMode::Normal, 44);
            AddText(unlocked ? definition.nameKo : "잠김", X[i], 0.00F, 43.0F,
                unlocked ? White : Red, true, 70);
            AddText(definition.subtitleKo, X[i], -0.72F, 65.0F, unlocked ? Muted : Dim, true, 70);
            if (!unlocked) AddText("해금 조건 필요", X[i], -1.38F, 68.0F, Dim, true, 70);
        }
        const auto selected = static_cast<CharacterId>(std::min<std::size_t>(product_.Selection(), X.size() - 1U));
        AddText(NightfallProduct::Character(selected).descriptionKo, 0.0F, -3.10F, 72.0F, Muted, true, 70);
        AddText("A/D 또는 W/S 선택 · ENTER 확인 · ESC 뒤로", 0.0F, -3.82F, 66.0F, Dim, true, 70);
    }

    void DrawStageSelect()
    {
        AddText("스테이지 선택", 0.0F, 3.55F, 25.0F, White, true, 70);
        constexpr std::array<float, 3U> X{-5.0F, 0.0F, 5.0F};
        for (std::size_t i = 0U; i < X.size(); ++i)
        {
            const auto id = static_cast<StageId>(i);
            const auto& definition = NightfallProduct::Stage(id);
            const bool unlocked = product_.IsStageUnlocked(id);
            const bool selected = product_.Selection() == i;
            AddPanel(X[i], 0.20F, 4.30F, 5.55F,
                selected ? Color{0.11F, 0.18F, 0.31F, 1.0F} : Panel,
                0.96F, 40);
            AddPanel(X[i], 1.12F, 3.45F, 1.55F, unlocked ? StageTint(static_cast<std::uint32_t>(i)) : Dim, 0.38F, 42);
            AddVisual(floorAlt_, 0U, {X[i], 1.12F}, {2.5F, 2.5F}, 0.0F,
                unlocked ? StageTint(static_cast<std::uint32_t>(i)) : Dim,
                1.0F, trace2d::render::SpriteBlendMode::Normal, 44);
            AddText(unlocked ? definition.nameKo : "잠김", X[i], -0.12F, 43.0F,
                unlocked ? White : Red, true, 70);
            AddText(definition.subtitleKo, X[i], -0.88F, 62.0F, unlocked ? Cyan : Dim, true, 70);
            if (selected) AddPanel(X[i], 2.93F, 4.30F, 0.08F, unlocked ? Gold : Red, 1.0F, 42);
        }
        const auto selected = static_cast<StageId>(std::min<std::size_t>(product_.Selection(), X.size() - 1U));
        AddText(NightfallProduct::Stage(selected).descriptionKo, 0.0F, -2.95F, 72.0F, Muted, true, 70);
        AddText(
            "선택 캐릭터: " + std::string{NightfallProduct::Character(product_.SelectedCharacter()).nameKo},
            0.0F, -3.48F, 66.0F, Gold, true, 70);
        AddText("A/D 또는 W/S 선택 · ENTER 시작 · ESC 뒤로", 0.0F, -3.94F, 68.0F, Dim, true, 70);
    }

    void DrawPause(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, {0.01F, 0.015F, 0.03F, 1.0F}, 0.78F, 50);
        AddPanel(center.x, center.y, 6.4F, 4.2F, Panel, 0.98F, 51);
        AddText("일시정지", center.x, center.y + 1.45F, 29.0F, White, true, 72);
        constexpr std::array<std::string_view, 2U> Items{{"계속하기", "메인 메뉴"}};
        for (std::size_t i = 0U; i < Items.size(); ++i)
        {
            const float y = center.y + 0.30F - static_cast<float>(i) * 1.05F;
            const bool selected = product_.Selection() == i;
            if (selected) AddPanel(center.x, y - 0.12F, 4.8F, 0.78F, {0.13F, 0.29F, 0.46F, 1.0F}, 1.0F, 52);
            AddText(Items[i], center.x, y + 0.15F, 43.0F, selected ? White : Muted, true, 73);
        }
    }

    void DrawResult(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        const auto& summary = product_.LastRunSummary();
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, {0.01F, 0.015F, 0.03F, 1.0F}, 0.84F, 50);
        AddPanel(center.x, center.y, 10.8F, 7.3F, Panel, 0.98F, 51);
        AddText(summary.cleared ? "스테이지 클리어" : "패배", center.x, center.y + 3.05F, 28.0F,
            summary.cleared ? Gold : Red, true, 73);
        AddText(
            "처치 " + std::to_string(summary.kills) + " · 레벨 " + std::to_string(summary.level) +
            " · 생존 " + FormatTime(summary.elapsedSeconds),
            center.x, center.y + 2.18F, 53.0F, White, true, 73);
        AddText("별가루 획득  +" + std::to_string(summary.starsEarned), center.x, center.y + 1.50F, 51.0F, Cyan, true, 73);

        float unlockY = center.y + 0.62F;
        for (std::size_t i = 0U; i < NightfallProduct::CharacterCount(); ++i)
        {
            if ((summary.newlyUnlockedCharactersMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            AddText("새 캐릭터 해금: " + std::string{NightfallProduct::Character(static_cast<CharacterId>(i)).nameKo},
                center.x, unlockY, 60.0F, Gold, true, 73);
            unlockY -= 0.55F;
        }
        for (std::size_t i = 0U; i < NightfallProduct::StageCount(); ++i)
        {
            if ((summary.newlyUnlockedStagesMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            AddText("새 스테이지 해금: " + std::string{NightfallProduct::Stage(static_cast<StageId>(i)).nameKo},
                center.x, unlockY, 60.0F, Gold, true, 73);
            unlockY -= 0.55F;
        }
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            if ((summary.newlyUnlockedAchievementsMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            AddText("새 업적: " + std::string{NightfallProduct::Achievement(static_cast<AchievementId>(i)).nameKo},
                center.x, unlockY, 64.0F, Green, true, 73);
            unlockY -= 0.52F;
        }

        constexpr std::array<std::string_view, 2U> Items{{"다시 도전", "메인 메뉴"}};
        for (std::size_t i = 0U; i < Items.size(); ++i)
        {
            const float x = center.x + (i == 0U ? -2.3F : 2.3F);
            const bool selected = product_.Selection() == i;
            if (selected) AddPanel(x, center.y - 2.50F, 3.9F, 0.76F, {0.13F, 0.29F, 0.46F, 1.0F}, 1.0F, 52);
            AddText(Items[i], x, center.y - 2.24F, 45.0F, selected ? White : Muted, true, 73);
        }
        AddText("W/S 선택 · ENTER 확인", center.x, center.y - 3.35F, 67.0F, Dim, true, 73);
    }

    trace2d::render::Renderer& renderer_;
    NightfallSurvivorsGame& game_;
    NightfallProduct& product_;
    std::filesystem::path runtimeRoot_{};
    trace2d::assets::TextureAssetCache textureCache_;

    Visual floor_{};
    Visual floorAlt_{};
    Visual hero_{};
    Visual ghoul_{};
    Visual brute_{};
    Visual particle_{};
    Visual white_{};

    std::array<trace2d::runtime::SpriteAnimationClip2D, 4U> walkClips_{};
    std::array<trace2d::runtime::SpriteAnimator2D, 4U> walkAnimators_{};
    std::uint64_t lastAnimationFrame_{0U};

    std::unique_ptr<trace2d::text::GlyphAtlas> glyphAtlas_{};
    std::unique_ptr<trace2d::text::TextLayoutRun> textLayout_{};
    TextureHandle glyphTexture_{};
    trace2d::text::GlyphAtlasTextureBinding2D glyphBinding_{};
    std::vector<trace2d::render::SpritePresentationRenderData> textScratch_{};
    std::vector<trace2d::render::SpritePresentationRenderData> draws_{};

    const trace2d::application::GameContext* currentContext_{nullptr};
    trace2d::render::OrthographicCamera camera_{};
    trace2d::scene::Vector2 cameraCenter_{};
    float targetAspect_{CoreAspect};
    float viewVerticalSize_{CoreHeight};
    float viewHalfWidth_{CoreWidth * 0.5F};
    float viewHalfHeight_{CoreHeight * 0.5F};
    std::uint64_t order_{0U};

    friend class NightfallPresentation;
};

NightfallPresentation::NightfallPresentation(
    trace2d::render::Renderer& renderer,
    NightfallSurvivorsGame& game,
    NightfallProduct& product,
    std::filesystem::path runtimeRoot)
    : impl_{std::make_unique<Impl>(renderer, game, product, std::move(runtimeRoot))}
{
}

NightfallPresentation::~NightfallPresentation() = default;

void NightfallPresentation::Present(
    const trace2d::application::GameContext& context,
    void* const userData)
{
    auto* const self = static_cast<NightfallPresentation*>(userData);
    if (self == nullptr || self->impl_ == nullptr)
        throw std::runtime_error{"Nightfall product presentation is unavailable."};
    self->impl_->currentContext_ = &context;
    self->impl_->Present(context);
}
