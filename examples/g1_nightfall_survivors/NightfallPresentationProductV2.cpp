#include "NightfallPresentation.hpp"
#include "NightfallLayoutAudit.hpp"

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/assets/TextureAssets.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>
#include <trace2d/scene/SpriteTransform2D.hpp>
#include <trace2d/text/Text.hpp>
#include <trace2d/text/TextLayout.hpp>
#include <trace2d/text/TextPresentation2D.hpp>

#include <algorithm>
#include <array>
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
using FontHandle = trace2d::assets::ResourceHandle<trace2d::assets::FontResource>;
using Color = trace2d::render::SpriteLinearRgba;
using Screen = NightfallProduct::Screen;
using CharacterId = NightfallProduct::CharacterId;
using StageId = NightfallProduct::StageId;
using AchievementId = NightfallProduct::AchievementId;

constexpr float Pi = 3.14159265358979323846F;
constexpr float CoreWidth = NightfallSurvivorsGame::CameraHorizontalSize;
constexpr float CoreHeight = NightfallSurvivorsGame::CameraVerticalSize;
constexpr float CoreAspect = CoreWidth / CoreHeight;

// Galmuri11 is rasterized at 32 px. Larger PPU means smaller logical text.
// These roles deliberately leave enough vertical rhythm for deterministic bounds validation.
constexpr float TitlePpu = 40.0F;
constexpr float HeadingPpu = 50.0F;
constexpr float SubheadingPpu = 60.0F;
constexpr float BodyPpu = 70.0F;
constexpr float SmallPpu = 84.0F;
constexpr float TinyPpu = 98.0F;
constexpr float MicroPpu = 122.0F;

constexpr Color White{0.96F, 0.97F, 1.0F, 1.0F};
constexpr Color Muted{0.68F, 0.73F, 0.82F, 1.0F};
constexpr Color Dim{0.42F, 0.46F, 0.55F, 1.0F};
constexpr Color Ink{0.035F, 0.045F, 0.07F, 1.0F};
constexpr Color Panel{0.055F, 0.070F, 0.105F, 1.0F};
constexpr Color PanelRaised{0.085F, 0.105F, 0.155F, 1.0F};
constexpr Color Cyan{0.28F, 0.82F, 1.0F, 1.0F};
constexpr Color Gold{1.0F, 0.72F, 0.24F, 1.0F};
constexpr Color Ember{1.0F, 0.36F, 0.20F, 1.0F};
constexpr Color Moon{0.62F, 0.72F, 1.0F, 1.0F};
constexpr Color Green{0.35F, 0.92F, 0.58F, 1.0F};
constexpr Color Red{1.0F, 0.28F, 0.32F, 1.0F};

struct Visual final
{
    std::shared_ptr<trace2d::assets::SpriteAsset> asset{};
    std::vector<trace2d::render::ResolvedSpriteRegion> selections{};
    TextureHandle texture{};
    float pixelsPerUnit{16.0F};
};

[[nodiscard]] std::string_view ScreenName(const Screen screen) noexcept
{
    switch (screen)
    {
    case Screen::MainMenu: return "main_menu";
    case Screen::Profile: return "profile";
    case Screen::Achievements: return "achievements";
    case Screen::Settings: return "settings";
    case Screen::CharacterSelect: return "character_select";
    case Screen::StageSelect: return "stage_select";
    case Screen::Playing: return "playing";
    case Screen::Pause: return "pause";
    case Screen::Result: return "result";
    }
    return "unknown";
}

[[nodiscard]] std::vector<std::uint8_t> ReadBinary(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
        throw std::runtime_error{"Nightfall Survivors could not read: " + path.string()};
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
}

void RetainOrThrow(
    trace2d::assets::ResourceRegistry& resources,
    const trace2d::assets::ResourceHandleUntyped handle,
    const std::string_view label)
{
    const auto retained = resources.Retain(handle);
    if (!retained.Succeeded())
        throw std::runtime_error{"Nightfall could not retain " + std::string{label} + "."};
}

[[nodiscard]] Visual MakeVisual(
    trace2d::assets::TextureAssetCache& cache,
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const std::string& reference,
    const float pixelsPerUnit)
{
    const auto loaded = cache.Load(reference);
    if (!loaded.Succeeded())
        throw std::runtime_error{"Nightfall could not load product asset: " + reference};

    trace2d::assets::TextureResource canonical{};
    canonical.width = loaded.asset->width;
    canonical.height = loaded.asset->height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Srgb;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall product presentation";
    canonical.canonicalRgba8 = loaded.asset->rgba8;
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall could not publish product texture: " + reference};
    RetainOrThrow(resources, published.handle.Untyped(), reference);

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle,
        trace2d::render::Rgba8TextureData{
            loaded.asset->width,
            loaded.asset->height,
            std::span<const std::uint8_t>{loaded.asset->rgba8.data(), loaded.asset->rgba8.size()},
        },
        trace2d::render::SpriteTextureEncoding::Srgb);
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
    trace2d::assets::SpriteRegion region{};
    region.id = "region-0";
    region.pageId = "page";
    region.sourceSize = {loaded.asset->width, loaded.asset->height};
    region.trimSize = {loaded.asset->width, loaded.asset->height};
    region.packedRect = {0U, 0U, loaded.asset->width, loaded.asset->height};
    region.pivot = {
        static_cast<std::int64_t>(loaded.asset->width),
        static_cast<std::int64_t>(loaded.asset->height),
        2,
    };
    visual.asset->regions.push_back(std::move(region));
    visual.selections.resize(1U);
    if (!trace2d::render::ResolveSpriteRegionByIndices(
            visual.asset.get(), 0U, 0U, visual.selections[0]).Succeeded())
    {
        throw std::runtime_error{"Nightfall could not resolve product sprite region."};
    }
    return visual;
}

[[nodiscard]] Visual MakeWhiteVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer)
{
    constexpr std::array<std::uint8_t, 4U> Pixels{255U, 255U, 255U, 255U};
    constexpr std::string_view Reference = "generated/nightfall-product-white-v3.rgba8";
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall product UI primitive";
    canonical.canonicalRgba8.assign(Pixels.begin(), Pixels.end());
    const auto published = resources.PublishTexture(std::string{Reference}, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall could not publish UI primitive."};
    RetainOrThrow(resources, published.handle.Untyped(), Reference);

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle,
        trace2d::render::Rgba8TextureData{1U, 1U, Pixels},
        trace2d::render::SpriteTextureEncoding::Linear);
    visual.pixelsPerUnit = 1.0F;
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = "nightfall-product-white-v3.sprite";
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
        static_cast<void>(resources.Release(visual.texture.Untyped()));
        static_cast<void>(resources.Unload(visual.texture.Untyped()));
    }
    visual.selections.clear();
    visual.asset.reset();
    visual.texture = {};
}

[[nodiscard]] std::string FormatTime(const float seconds)
{
    const std::uint32_t total = static_cast<std::uint32_t>(std::max(0.0F, std::floor(seconds)));
    const std::uint32_t minutes = total / 60U;
    const std::uint32_t remainder = total % 60U;
    return std::to_string(minutes) + ":" + (remainder < 10U ? "0" : "") + std::to_string(remainder);
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

[[nodiscard]] Color CharacterAccent(const std::size_t index) noexcept
{
    switch (index)
    {
    case 1U: return Ember;
    case 2U: return Moon;
    default: return Gold;
    }
}

[[nodiscard]] Color StageAccent(const std::size_t index) noexcept
{
    switch (index)
    {
    case 1U: return Ember;
    case 2U: return {0.66F, 0.48F, 1.0F, 1.0F};
    default: return Cyan;
    }
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
        const std::array<std::string, 3U> heroRefs{
            "textures/hero-star.png", "textures/hero-ember.png", "textures/hero-moon.png"};
        const std::array<std::string, 3U> enemyRefs{
            "textures/enemy-ghoul.png", "textures/enemy-brute.png", "textures/enemy-wisp.png"};
        const std::array<std::string, 3U> floorARefs{
            "textures/stage-moon-floor-a.png", "textures/stage-ember-floor-a.png", "textures/stage-astral-floor-a.png"};
        const std::array<std::string, 3U> floorBRefs{
            "textures/stage-moon-floor-b.png", "textures/stage-ember-floor-b.png", "textures/stage-astral-floor-b.png"};
        const std::array<std::string, 3U> skillRefs{
            "textures/skill-rapid.png", "textures/skill-might.png", "textures/skill-orbit.png"};

        for (std::size_t index = 0U; index < 3U; ++index)
        {
            heroes_[index] = MakeVisual(textureCache_, game_.Resources(), renderer_, heroRefs[index], 16.0F);
            enemies_[index] = MakeVisual(textureCache_, game_.Resources(), renderer_, enemyRefs[index], 16.0F);
            floorsA_[index] = MakeVisual(textureCache_, game_.Resources(), renderer_, floorARefs[index], 16.0F);
            floorsB_[index] = MakeVisual(textureCache_, game_.Resources(), renderer_, floorBRefs[index], 16.0F);
            skills_[index] = MakeVisual(textureCache_, game_.Resources(), renderer_, skillRefs[index], 16.0F);
        }
        particle_ = MakeVisual(textureCache_, game_.Resources(), renderer_, "textures/particle.png", 96.0F);
        white_ = MakeWhiteVisual(game_.Resources(), renderer_);
        draws_.reserve(4096U);
        textScratch_.resize(1024U);
        layoutAudit_.Reserve(160U);
        PrepareText();
    }

    ~Impl()
    {
        if (glyphTexture_.generation != 0U)
        {
            renderer_.DestroyTexture(glyphTexture_);
            static_cast<void>(game_.Resources().Release(glyphTexture_.Untyped()));
            static_cast<void>(game_.Resources().Unload(glyphTexture_.Untyped()));
        }
        textLayout_.reset();
        glyphAtlas_.reset();
        if (fontHandle_.generation != 0U)
        {
            static_cast<void>(game_.Resources().Release(fontHandle_.Untyped()));
            static_cast<void>(game_.Resources().Unload(fontHandle_.Untyped()));
        }
        ReleaseVisual(game_.Resources(), renderer_, white_);
        ReleaseVisual(game_.Resources(), renderer_, particle_);
        for (auto& visual : skills_) ReleaseVisual(game_.Resources(), renderer_, visual);
        for (auto& visual : floorsB_) ReleaseVisual(game_.Resources(), renderer_, visual);
        for (auto& visual : floorsA_) ReleaseVisual(game_.Resources(), renderer_, visual);
        for (auto& visual : enemies_) ReleaseVisual(game_.Resources(), renderer_, visual);
        for (auto& visual : heroes_) ReleaseVisual(game_.Resources(), renderer_, visual);
    }

    void Present(const trace2d::application::GameContext& context)
    {
        draws_.clear();
        order_ = 0U;
        UpdateView(context);
        ResetLayoutAudit();

        const Screen screen = product_.CurrentScreen();
        if (screen == Screen::Playing || screen == Screen::Pause || screen == Screen::Result)
        {
            DrawWorld(context);
            DrawHud(context);
            if (game_.CurrentState() == NightfallSurvivorsGame::State::LevelUp && screen == Screen::Playing)
            {
                ResetLayoutAudit();
                DrawLevelUp(context);
            }
            if (screen == Screen::Pause)
            {
                ResetLayoutAudit();
                DrawPause(context);
            }
            if (screen == Screen::Result)
            {
                ResetLayoutAudit();
                DrawResult(context);
            }
        }
        else
        {
            DrawProductBackdrop();
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

        if (const auto diagnostic = layoutAudit_.Validate(); diagnostic.has_value())
        {
            throw std::runtime_error{
                "Nightfall layout validation failed [" + std::string{ScreenName(screen)} + "]: " + *diagnostic};
        }
        if (trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_) != frozenGlyphRevision_)
        {
            throw std::runtime_error{
                "Nightfall glyph atlas mutated after frame construction began; prewarm corpus is incomplete."};
        }

        renderer_.RenderFrame(
            camera_,
            std::span<const trace2d::render::SpritePresentationRenderData>{draws_.data(), draws_.size()});
    }

private:
    void PrepareText()
    {
        trace2d::assets::FontResource font{};
        font.canonicalBytes = ReadBinary(runtimeRoot_ / "fonts/Galmuri11-Bold.ttf");
        const auto fontPublished = game_.Resources().PublishFont("fonts/Galmuri11-Bold.ttf", std::move(font));
        if (!fontPublished.Succeeded())
            throw std::runtime_error{"Nightfall could not publish Galmuri Korean font."};
        fontHandle_ = fontPublished.handle;
        RetainOrThrow(game_.Resources(), fontHandle_.Untyped(), "Galmuri font");

        auto atlas = trace2d::text::PrepareGlyphAtlas(
            game_.Resources(),
            fontHandle_,
            trace2d::text::GlyphAtlasConfig{2048U, 2048U, 32U, 1U, 2048U});
        if (!atlas.Succeeded())
            throw std::runtime_error{"Nightfall could not prepare Galmuri glyph atlas."};
        glyphAtlas_ = std::move(atlas.atlas);

        std::string corpus =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 %/:+-.[]()·<>,←→"
            "NIGHTFALL SURVIVORS Trace2D ORIGINAL SURVIVAL PLAYER PROFILE ACHIEVEMENTS SETTINGS "
            "CHOOSE YOUR HUNTER CHOOSE THE NIGHT WASD QEF ESC ENTER SPACE R LEVEL UP PAUSED CLEAR GAME OVER "
            "게임 시작 프로필 업적 설정 종료 돌아가기 선택 확인 캐릭터 스테이지 잠김 해금 조건 필요 "
            "별가루 플레이 클리어 누적 처치 최고 레벨 최고 생존 총 횟수 효과음 볼륨 화면 흔들림 켜짐 꺼짐 "
            "일시정지 계속하기 메인 메뉴 레벨 업 하나를 선택하세요 연사 화력 궤도 공격 간격 감소 투사체 공격력 증가 "
            "주변 회전무기 최대 이후 체력 강화 체력 남은 시간 다시 도전 획득 새 캐릭터 새 스테이지 새 업적 "
            "달성 미달성 조작 이동 자동공격 재시작 뒤로 현재 장착 보유 별 전투 기록 모험 준비 완료 패배 "
            "해금 후 상세 능력을 확인할 수 있습니다 이전 스테이지를 클리어하면 해금됩니다 "
            "중 하나를 선택하세요 레벨마다 탄환 궤도 공격력도 증가 최대 개 이후 체력 강화 "
            "스테이지 클리어 총 플레이 횟수 캐릭터 해금 스테이지 해금 생존 시간 분 초 회";
        for (std::size_t i = 0U; i < NightfallProduct::CharacterCount(); ++i)
        {
            const auto& definition = NightfallProduct::Character(static_cast<CharacterId>(i));
            corpus.append(definition.nameKo);
            corpus.append(definition.subtitleKo);
            corpus.append(definition.descriptionKo);
        }
        for (std::size_t i = 0U; i < NightfallProduct::StageCount(); ++i)
        {
            const auto& definition = NightfallProduct::Stage(static_cast<StageId>(i));
            corpus.append(definition.nameKo);
            corpus.append(definition.subtitleKo);
            corpus.append(definition.descriptionKo);
        }
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            const auto& definition = NightfallProduct::Achievement(static_cast<AchievementId>(i));
            corpus.append(definition.nameKo);
            corpus.append(definition.descriptionKo);
        }
        if (!glyphAtlas_->WarmUtf8(corpus).Succeeded())
            throw std::runtime_error{"Nightfall could not warm complete product glyph corpus."};

        auto layout = trace2d::text::PrepareTextLayoutRun({1024U, 64U});
        if (!layout.Succeeded())
            throw std::runtime_error{"Nightfall could not prepare product text layout."};
        textLayout_ = std::move(layout.run);

        UploadFrozenGlyphAtlas();
        frozenGlyphRevision_ = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
    }

    void UploadFrozenGlyphAtlas()
    {
        const auto config = glyphAtlas_->Config();
        trace2d::assets::TextureResource texture{};
        texture.width = config.width;
        texture.height = config.height;
        texture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
        texture.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
        texture.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
        texture.retentionReason = "Nightfall frozen Galmuri glyph atlas";
        texture.canonicalRgba8.resize(static_cast<std::size_t>(config.width) * config.height * 4U);
        std::size_t requiredBytes = 0U;
        const auto written = trace2d::text::WriteGlyphAtlasRgba8(
            *glyphAtlas_, texture.canonicalRgba8, requiredBytes);
        if (!written.Succeeded() || requiredBytes != texture.canonicalRgba8.size())
            throw std::runtime_error{"Nightfall could not rasterize frozen glyph atlas."};

        constexpr std::string_view Reference = "generated/nightfall-product-glyph-atlas-v3.rgba8";
        const auto published = game_.Resources().PublishTexture(std::string{Reference}, std::move(texture));
        if (!published.Succeeded())
            throw std::runtime_error{"Nightfall could not publish frozen glyph texture."};
        RetainOrThrow(game_.Resources(), published.handle.Untyped(), Reference);
        const auto* const canonical = game_.Resources().Resolve(published.handle);
        if (canonical == nullptr)
            throw std::runtime_error{"Nightfall lost frozen glyph texture resource."};
        glyphTexture_ = renderer_.CreateSpriteTextureRgba8(
            published.handle,
            trace2d::render::Rgba8TextureData{
                canonical->width,
                canonical->height,
                std::span<const std::uint8_t>{canonical->canonicalRgba8.data(), canonical->canonicalRgba8.size()},
            },
            trace2d::render::SpriteTextureEncoding::Linear);
        const auto bindingStatus = trace2d::text::ResolveGlyphAtlasTextureBinding2D(
            *glyphAtlas_, game_.Resources(), glyphTexture_, glyphBinding_);
        if (!bindingStatus.Succeeded())
        {
            throw std::runtime_error{
                "Nightfall could not bind frozen glyph atlas: " +
                std::string{trace2d::text::ToString(bindingStatus.error)}};
        }
    }

    void UpdateView(const trace2d::application::GameContext& context)
    {
        const auto metrics = renderer_.Metrics();
        const std::uint32_t width = metrics.lastTargetWidth == 0U
            ? static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasWidth)
            : metrics.lastTargetWidth;
        const std::uint32_t height = metrics.lastTargetHeight == 0U
            ? static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasHeight)
            : metrics.lastTargetHeight;
        const float targetAspect = static_cast<float>(width) / static_cast<float>(height);
        viewVerticalSize_ = targetAspect >= CoreAspect ? CoreHeight : CoreWidth / targetAspect;
        viewHalfHeight_ = viewVerticalSize_ * 0.5F;
        viewHalfWidth_ = viewHalfHeight_ * targetAspect;

        trace2d::scene::Vector2 center{};
        const Screen screen = product_.CurrentScreen();
        if (screen == Screen::Playing || screen == Screen::Pause || screen == Screen::Result)
        {
            const auto* const player = context.Scene().TryGet(game_.Player());
            if (player != nullptr) center = player->LocalTransform().position;
        }
        const float shake = product_.CameraShakeEnabled() && game_.ScreenShakeFrames() > 0U
            ? (((game_.FrameCounter() & 1U) != 0U) ? 0.07F : -0.07F)
            : 0.0F;
        cameraCenter_ = center;
        camera_.center = {center.x + shake, center.y - shake * 0.5F};
        camera_.verticalSize = viewVerticalSize_;
    }

    void ResetLayoutAudit()
    {
        constexpr float Margin = 0.13F;
        layoutAudit_.Reset(
            cameraCenter_.x - viewHalfWidth_ + Margin,
            cameraCenter_.x + viewHalfWidth_ - Margin,
            cameraCenter_.y - viewHalfHeight_ + Margin,
            cameraCenter_.y + viewHalfHeight_ - Margin);
    }

    void AddVisual(
        const Visual& visual,
        const trace2d::scene::Vector2 position,
        const trace2d::scene::Vector2 scale,
        const float rotation,
        const Color tint,
        const float opacity,
        const trace2d::render::SpriteBlendMode blend,
        const std::int32_t layer)
    {
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
                visual.selections[0], pose, visual.pixelsPerUnit, appearance, draw.presentation).Succeeded())
        {
            throw std::runtime_error{"Nightfall product sprite presentation failed."};
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
        const float opacity = 1.0F,
        const std::int32_t layer = 40)
    {
        AddVisual(white_, {x, y}, {width, height}, 0.0F, color, opacity,
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
        const std::uint64_t revisionBefore = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
        if (revisionBefore != frozenGlyphRevision_)
            throw std::runtime_error{"Nightfall frozen glyph atlas was stale before text layout."};

        const trace2d::text::TextFontAtlasRef atlasRef{glyphAtlas_.get()};
        const std::span<const trace2d::text::TextFontAtlasRef> atlases(&atlasRef, 1U);
        trace2d::text::TextLayoutOptions options{};
        options.wrapMode = trace2d::text::TextWrapMode::None;
        const auto layoutResult = textLayout_->LayoutUtf8(atlases, text, options);
        if (!layoutResult.Succeeded())
        {
            const std::string detail = layoutResult.diagnostic.has_value()
                ? std::string{trace2d::text::ToString(layoutResult.diagnostic->code)}
                : std::string{"unknown"};
            throw std::runtime_error{
                "Nightfall product text layout failed for '" + std::string{text} + "': " + detail};
        }
        if (trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_) != frozenGlyphRevision_)
        {
            throw std::runtime_error{
                "Nightfall late glyph mutation while laying out '" + std::string{text} +
                "'. Add the text to the startup corpus instead of mutating GPU residency mid-frame."};
        }

        const auto metrics = textLayout_->Metrics();
        const float width = static_cast<float>(metrics.contentWidth26_6) / (64.0F * pixelsPerUnit);
        const float height = static_cast<float>(metrics.contentHeight26_6) / (64.0F * pixelsPerUnit);
        const float originX = centered ? x - width * 0.5F : x;
        layoutAudit_.RecordText(text, originX, originX + width, topY - height, topY);

        trace2d::text::TextPresentationConfig2D config{};
        config.origin = {originX, topY};
        config.pixelsPerUnit = pixelsPerUnit;
        config.painterLayer = layer;
        config.painterOrder = 0;
        config.stableOrderBase = order_;
        config.tint = color;
        config.opacity = 1.0F;
        config.sampler = trace2d::render::SpriteSamplerCompatibility::Nearest;

        std::size_t requiredCount = 0U;
        trace2d::text::TextPresentationMeasurement2D measurement{};
        const auto status = trace2d::text::BuildTextPresentation2D(
            *textLayout_, atlases,
            std::span<const trace2d::text::GlyphAtlasTextureBinding2D>(&glyphBinding_, 1U),
            config, textScratch_, requiredCount, measurement);
        if (!status.Succeeded() || requiredCount > textScratch_.size())
        {
            throw std::runtime_error{
                "Nightfall product text presentation failed for '" + std::string{text} + "': " +
                std::string{trace2d::text::ToString(status.error)}};
        }

        for (std::size_t index = 0U; index < requiredCount; ++index)
        {
            auto draw = textScratch_[index];
            const auto flipY = [topY](trace2d::render::SpriteDrawVertex& vertex) {
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

    void AddCardFrame(
        const float x,
        const float y,
        const float width,
        const float height,
        const Color accent,
        const bool selected)
    {
        AddPanel(x, y, width, height, selected ? PanelRaised : Panel, 0.97F, 41);
        AddPanel(x, y + height * 0.5F - 0.04F, width, 0.08F, selected ? accent : Dim, 1.0F, 42);
        if (selected)
            AddPanel(x - width * 0.5F + 0.04F, y, 0.08F, height, accent, 1.0F, 42);
    }

    void DrawFloor(const trace2d::scene::Vector2 center, const std::size_t stageIndex)
    {
        const std::size_t stage = std::min<std::size_t>(stageIndex, 2U);
        const int minX = static_cast<int>(std::floor(center.x - viewHalfWidth_)) - 2;
        const int maxX = static_cast<int>(std::ceil(center.x + viewHalfWidth_)) + 2;
        const int minY = static_cast<int>(std::floor(center.y - viewHalfHeight_)) - 2;
        const int maxY = static_cast<int>(std::ceil(center.y + viewHalfHeight_)) + 2;
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const bool alternate = ((x * 13 + y * 29) & 7) == 0;
                AddVisual(
                    alternate ? floorsB_[stage] : floorsA_[stage],
                    {static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F},
                    {1.0F, 1.0F}, 0.0F, White, 1.0F,
                    trace2d::render::SpriteBlendMode::Normal, -20);
            }
        }
    }

    void DrawProductBackdrop()
    {
        DrawFloor({}, 0U);
        AddPanel(0.0F, 0.0F, CoreWidth, CoreHeight, Ink, 0.92F, 20);
        AddPanel(0.0F, 4.20F, CoreWidth, 0.58F, {0.02F, 0.03F, 0.06F, 1.0F}, 0.98F, 25);
        AddPanel(0.0F, -4.20F, CoreWidth, 0.58F, {0.02F, 0.03F, 0.06F, 1.0F}, 0.98F, 25);
    }

    void DrawHeader(const std::string_view eyebrow, const std::string_view title)
    {
        AddText(eyebrow, -7.10F, 4.02F, SmallPpu, Cyan, false, 70);
        AddText(title, -7.10F, 3.42F, HeadingPpu, White, false, 70);
        AddPanel(-5.05F, 2.65F, 4.10F, 0.055F, Cyan, 0.85F, 43);
    }

    void DrawFooter(const std::string_view help)
    {
        AddText(help, 0.0F, -4.00F, TinyPpu, Dim, true, 75);
    }

    void DrawMainMenu()
    {
        AddText("NIGHTFALL", -7.10F, 4.05F, TitlePpu, White, false, 72);
        AddText("SURVIVORS", -7.08F, 3.08F, SubheadingPpu, Cyan, false, 72);
        AddText("Trace2D ORIGINAL SURVIVAL", -7.08F, 2.40F, TinyPpu, Dim, false, 72);

        AddPanel(-5.05F, -0.58F, 4.35F, 4.80F, Panel, 0.98F, 40);
        AddText("MENU", -6.72F, 1.52F, TinyPpu, Dim, false, 72);
        constexpr std::array<std::string_view, 5U> Menu{
            "게임 시작", "프로필", "업적", "설정", "종료"};
        for (std::size_t i = 0U; i < Menu.size(); ++i)
        {
            const float y = 0.98F - static_cast<float>(i) * 0.84F;
            const bool selected = product_.Selection() == i;
            if (selected)
            {
                AddPanel(-5.05F, y - 0.16F, 3.86F, 0.62F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 43);
                AddPanel(-6.94F, y - 0.16F, 0.08F, 0.62F, Cyan, 1.0F, 44);
            }
            AddText(Menu[i], -6.52F, y + 0.08F, BodyPpu, selected ? White : Muted, false, 74);
        }

        const std::size_t characterIndex = static_cast<std::size_t>(product_.SelectedCharacter());
        const auto& character = NightfallProduct::Character(product_.SelectedCharacter());
        AddPanel(2.05F, -0.58F, 8.55F, 4.80F, Panel, 0.98F, 40);
        AddPanel(2.05F, -0.58F, 0.055F, 4.18F, Dim, 0.65F, 42);

        AddText("현재 캐릭터", -1.82F, 1.52F, TinyPpu, Dim, false, 72);
        AddText(character.nameKo, -0.15F, 0.99F, SubheadingPpu,
            CharacterAccent(characterIndex), true, 72);
        AddText(character.subtitleKo, -0.15F, 0.32F, TinyPpu, Muted, true, 72);
        const float bob = std::sin(static_cast<float>(game_.FrameCounter() % 120U) * 0.0523598776F) * 0.05F;
        AddVisual(heroes_[characterIndex], {-0.15F, -1.18F + bob}, {2.10F, 2.10F}, 0.0F,
            White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 45);

        const auto& profile = product_.PlayerProfile();
        AddText("전투 기록", 2.68F, 1.52F, TinyPpu, Dim, false, 72);
        AddText("별가루  " + std::to_string(profile.stars), 2.68F, 0.98F, BodyPpu, Gold, false, 72);
        AddText("플레이  " + std::to_string(profile.totalRuns) + "회", 2.68F, 0.28F, SmallPpu, Muted, false, 72);
        AddText("클리어  " + std::to_string(profile.totalClears) + "회", 2.68F, -0.30F, SmallPpu, Muted, false, 72);
        AddText("누적 처치  " + std::to_string(profile.totalKills), 2.68F, -0.88F, SmallPpu, Muted, false, 72);
        AddText("최고 레벨  " + std::to_string(profile.bestLevel), 2.68F, -1.46F, SmallPpu, Muted, false, 72);
        AddText("최고 생존  " + FormatTime(profile.bestSurvivalSeconds), 2.68F, -2.04F, SmallPpu, Muted, false, 72);

        DrawFooter("W/S 선택  ·  ENTER 확인  ·  ESC 종료");
    }

    void DrawProfile()
    {
        DrawHeader("PLAYER PROFILE", "프로필");
        AddPanel(0.0F, -0.30F, 13.45F, 5.30F, Panel, 0.98F, 40);
        const auto& p = product_.PlayerProfile();
        const std::array<std::pair<std::string, std::string>, 8U> rows{{
            {"총 플레이 횟수", std::to_string(p.totalRuns)},
            {"누적 처치", std::to_string(p.totalKills)},
            {"클리어", std::to_string(p.totalClears)},
            {"최고 레벨", std::to_string(p.bestLevel)},
            {"최고 생존", FormatTime(p.bestSurvivalSeconds)},
            {"별가루", std::to_string(p.stars)},
            {"캐릭터 해금", std::to_string(CountBits(p.unlockedCharactersMask)) + "/3"},
            {"스테이지 해금", std::to_string(CountBits(p.unlockedStagesMask)) + "/3"},
        }};
        for (std::size_t i = 0U; i < rows.size(); ++i)
        {
            const bool rightColumn = i >= 4U;
            const std::size_t row = rightColumn ? i - 4U : i;
            const float x = rightColumn ? 0.75F : -5.75F;
            const float y = 1.72F - static_cast<float>(row) * 1.14F;
            AddText(rows[i].first, x, y, SmallPpu, Dim, false, 72);
            AddText(rows[i].second, x, y - 0.48F, SubheadingPpu, i == 5U ? Gold : White, false, 72);
        }
        DrawFooter("ENTER 또는 ESC  ·  돌아가기");
    }

    void DrawAchievements()
    {
        DrawHeader("ACHIEVEMENTS", "업적");
        AddPanel(0.0F, -0.28F, 14.15F, 5.35F, Panel, 0.98F, 40);
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            const auto id = static_cast<AchievementId>(i);
            const auto& definition = NightfallProduct::Achievement(id);
            const bool unlocked = product_.IsAchievementUnlocked(id);
            const bool rightColumn = i >= 3U;
            const std::size_t row = rightColumn ? i - 3U : i;
            const float cardCenterX = rightColumn ? 3.45F : -3.45F;
            const float cardCenterY = 1.45F - static_cast<float>(row) * 1.58F;
            const float left = cardCenterX - 2.92F;
            AddPanel(cardCenterX, cardCenterY, 6.15F, 1.28F,
                unlocked ? PanelRaised : Color{0.045F, 0.052F, 0.072F, 1.0F}, 1.0F, 42);
            AddPanel(cardCenterX - 3.00F, cardCenterY, 0.10F, 1.28F, unlocked ? Green : Dim, 1.0F, 43);
            AddText(unlocked ? "달성" : "미달성", left + 0.20F, cardCenterY + 0.34F,
                MicroPpu, unlocked ? Green : Dim, false, 72);
            AddText(definition.nameKo, left + 1.25F, cardCenterY + 0.36F,
                BodyPpu, unlocked ? White : Muted, false, 72);
            AddText(definition.descriptionKo, left + 0.20F, cardCenterY - 0.28F,
                MicroPpu, Dim, false, 72);
        }
        DrawFooter("ENTER 또는 ESC  ·  돌아가기");
    }

    void DrawSettings()
    {
        DrawHeader("SETTINGS", "설정");
        AddPanel(0.0F, -0.25F, 10.6F, 5.25F, Panel, 0.98F, 40);
        const auto& profile = product_.PlayerProfile();
        const std::string volume = profile.sfxVolumeStep == 0U
            ? "0%" : (profile.sfxVolumeStep == 1U ? "50%" : "100%");
        const std::array<std::string, 3U> labels{{
            "효과음 볼륨   " + volume,
            std::string{"화면 흔들림   "} + (profile.cameraShakeEnabled ? "켜짐" : "꺼짐"),
            "돌아가기",
        }};
        constexpr std::array<float, 3U> Y{1.18F, 0.00F, -1.65F};
        for (std::size_t i = 0U; i < labels.size(); ++i)
        {
            const bool selected = product_.Selection() == i;
            if (selected)
            {
                AddPanel(0.0F, Y[i] - 0.15F, 8.65F, 0.72F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 43);
                AddPanel(-4.28F, Y[i] - 0.15F, 0.09F, 0.72F, Cyan, 1.0F, 44);
            }
            AddText(labels[i], -3.82F, Y[i] + 0.10F, BodyPpu, selected ? White : Muted, false, 72);
        }
        DrawFooter("W/S 선택  ·  A/D 또는 ←/→ 조절  ·  ENTER 확인");
    }

    void DrawCharacterSelect()
    {
        DrawHeader("CHOOSE YOUR HUNTER", "캐릭터 선택");
        constexpr std::array<float, 3U> X{-5.10F, 0.0F, 5.10F};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto id = static_cast<CharacterId>(i);
            const auto& definition = NightfallProduct::Character(id);
            const bool unlocked = product_.IsCharacterUnlocked(id);
            const bool selected = product_.Selection() == i;
            const Color accent = CharacterAccent(i);
            AddCardFrame(X[i], 0.12F, 4.35F, 4.08F, unlocked ? accent : Dim, selected);
            AddVisual(heroes_[i], {X[i], 1.10F}, {1.78F, 1.78F}, 0.0F,
                unlocked ? White : Dim, unlocked ? 1.0F : 0.52F,
                trace2d::render::SpriteBlendMode::Normal, 45);
            AddText(unlocked ? definition.nameKo : "잠김", X[i], 0.20F, SubheadingPpu,
                unlocked ? accent : Red, true, 73);
            AddText(definition.subtitleKo, X[i], -0.45F, TinyPpu, unlocked ? Muted : Dim, true, 73);
            if (!unlocked)
                AddText("해금 조건 필요", X[i], -1.10F, MicroPpu, Red, true, 73);
        }
        const std::size_t selectedIndex = std::min<std::size_t>(product_.Selection(), 2U);
        const auto selectedId = static_cast<CharacterId>(selectedIndex);
        const bool selectedUnlocked = product_.IsCharacterUnlocked(selectedId);
        AddPanel(0.0F, -2.70F, 13.65F, 0.86F, Panel, 0.98F, 42);
        AddText(
            selectedUnlocked ? NightfallProduct::Character(selectedId).descriptionKo
                             : "해금 후 상세 능력을 확인할 수 있습니다.",
            0.0F, -2.50F, MicroPpu, selectedUnlocked ? Muted : Dim, true, 74);
        DrawFooter("A/D 또는 W/S 선택  ·  ENTER 확인  ·  ESC 뒤로");
    }

    void DrawStageSelect()
    {
        DrawHeader("CHOOSE THE NIGHT", "스테이지 선택");
        constexpr std::array<float, 3U> X{-5.10F, 0.0F, 5.10F};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto id = static_cast<StageId>(i);
            const auto& definition = NightfallProduct::Stage(id);
            const bool unlocked = product_.IsStageUnlocked(id);
            const bool selected = product_.Selection() == i;
            const Color accent = StageAccent(i);
            AddCardFrame(X[i], 0.12F, 4.35F, 4.08F, unlocked ? accent : Dim, selected);
            AddPanel(X[i], 1.20F, 3.45F, 1.32F, {0.025F, 0.03F, 0.05F, 1.0F}, 1.0F, 43);
            for (int py = 0; py < 2; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    const Visual& tile = ((px + py) & 1) == 0 ? floorsA_[i] : floorsB_[i];
                    AddVisual(tile,
                        {X[i] - 1.28F + static_cast<float>(px) * 0.85F,
                         0.92F + static_cast<float>(py) * 0.57F},
                        {0.85F, 0.57F}, 0.0F, unlocked ? White : Dim,
                        unlocked ? 1.0F : 0.50F,
                        trace2d::render::SpriteBlendMode::Normal, 44);
                }
            }
            AddText(unlocked ? definition.nameKo : "잠김", X[i], 0.18F, SubheadingPpu,
                unlocked ? accent : Red, true, 73);
            AddText(definition.subtitleKo, X[i], -0.50F, TinyPpu, unlocked ? White : Dim, true, 73);
        }
        const std::size_t selectedIndex = std::min<std::size_t>(product_.Selection(), 2U);
        const auto selectedStage = static_cast<StageId>(selectedIndex);
        const bool selectedUnlocked = product_.IsStageUnlocked(selectedStage);
        AddPanel(0.0F, -2.65F, 13.65F, 0.86F, Panel, 0.98F, 42);
        AddText(
            selectedUnlocked ? NightfallProduct::Stage(selectedStage).descriptionKo
                             : "이전 스테이지를 클리어하면 해금됩니다.",
            0.0F, -2.45F, MicroPpu, selectedUnlocked ? Muted : Dim, true, 74);
        const auto selectedCharacter = product_.SelectedCharacter();
        AddText("현재 장착  ·  " + std::string{NightfallProduct::Character(selectedCharacter).nameKo},
            0.0F, -3.32F, TinyPpu, Gold, true, 74);
        DrawFooter("A/D 또는 W/S 선택  ·  ENTER 시작  ·  ESC 뒤로");
    }

    void DrawWorld(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto playerPosition = player->LocalTransform().position;
        const std::size_t stageIndex = std::min<std::size_t>(game_.CurrentRunConfig().stageIndex, 2U);
        const std::size_t characterIndex = std::min<std::size_t>(game_.CurrentRunConfig().characterIndex, 2U);
        DrawFloor(playerPosition, stageIndex);

        const float phase = static_cast<float>(game_.FrameCounter() % 120U) * 0.0523598776F;
        for (const auto& gem : game_.Gems())
        {
            if (!gem.active) continue;
            const float pulse = 0.28F + std::sin(phase + static_cast<float>(gem.stableId) * 0.17F) * 0.04F;
            AddVisual(particle_, gem.position, {pulse, pulse}, 0.0F, Moon, 1.0F,
                trace2d::render::SpriteBlendMode::Additive, 2);
        }
        for (const auto& enemy : game_.Enemies())
        {
            if (!enemy.active) continue;
            const std::size_t kind = static_cast<std::size_t>(enemy.kind);
            const float scale = kind == 1U ? 1.28F : (kind == 2U ? 0.86F : 1.0F);
            const Color tint = enemy.hitFlashFrames > 0U ? Color{1.0F, 0.86F, 0.38F, 1.0F} : White;
            const float bob = std::sin(phase * 1.4F + static_cast<float>(enemy.stableId) * 0.31F) * 0.035F;
            AddVisual(enemies_[std::min<std::size_t>(kind, 2U)],
                {enemy.position.x, enemy.position.y + bob}, {scale, scale}, 0.0F, tint, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, 5);
        }
        for (const auto& projectile : game_.Projectiles())
        {
            if (!projectile.active) continue;
            AddVisual(skills_[0], projectile.position, {0.42F, 0.42F},
                std::atan2(projectile.velocity.y, projectile.velocity.x), Cyan, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, 9);
            AddVisual(particle_, projectile.position, {0.18F, 0.18F}, 0.0F, Cyan, 0.70F,
                trace2d::render::SpriteBlendMode::Additive, 8);
        }
        for (const auto& effect : game_.Effects())
        {
            if (!effect.active || effect.lifetimeSeconds <= 0.0F) continue;
            const float t = std::clamp(effect.ageSeconds / effect.lifetimeSeconds, 0.0F, 1.0F);
            const float scale = effect.startScale + (effect.endScale - effect.startScale) * t;
            Color tint = Gold;
            if (effect.kind == NightfallSurvivorsGame::EffectKind::Death) tint = Ember;
            else if (effect.kind == NightfallSurvivorsGame::EffectKind::LevelUp) tint = Cyan;
            else if (effect.kind == NightfallSurvivorsGame::EffectKind::PlayerHurt) tint = Red;
            AddVisual(particle_, effect.position, {scale, scale}, t * Pi, tint, 1.0F - t,
                trace2d::render::SpriteBlendMode::Additive, 11);
        }

        const std::uint32_t orbitCount = std::min<std::uint32_t>(game_.OrbitLevel(), 4U);
        for (std::uint32_t blade = 0U; blade < orbitCount; ++blade)
        {
            const float angle = game_.ElapsedSeconds() * 3.4F +
                2.0F * Pi * static_cast<float>(blade) / static_cast<float>(orbitCount);
            const float radius = 1.25F + static_cast<float>(orbitCount) * 0.08F;
            AddVisual(skills_[2],
                {playerPosition.x + std::cos(angle) * radius,
                 playerPosition.y + std::sin(angle) * radius},
                {0.62F, 0.62F}, angle, Gold, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, 13);
        }

        const float heroBob = (game_.MoveIntent().x != 0.0F || game_.MoveIntent().y != 0.0F)
            ? std::sin(phase * 2.4F) * 0.055F
            : std::sin(phase) * 0.025F;
        const float facingScale = game_.Facing().x < 0.0F ? -1.30F : 1.30F;
        const Color heroTint = game_.PlayerFlashFrames() > 0U && (game_.FrameCounter() & 1U) != 0U
            ? Color{1.0F, 0.45F, 0.45F, 1.0F}
            : White;
        AddVisual(heroes_[characterIndex],
            {playerPosition.x, playerPosition.y + heroBob}, {facingScale, 1.30F}, 0.0F,
            heroTint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 15);
    }

    void DrawHud(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        const float top = center.y + 4.08F;
        const float bottom = center.y - 4.05F;
        const float xpRatio = static_cast<float>(game_.Experience()) /
            static_cast<float>(std::max<std::uint32_t>(1U, game_.ExperienceToNextLevel()));

        AddPanel(center.x, top - 0.08F, 15.25F, 0.62F, Ink, 0.92F, 30);
        AddText("체력 " + std::to_string(game_.Health()) + "/" + std::to_string(game_.MaximumHealth()),
            center.x - 6.55F, top + 0.16F, SmallPpu, White, false, 66);
        const float remaining = std::max(0.0F, game_.RunDurationSeconds() - game_.ElapsedSeconds());
        AddText(FormatTime(remaining), center.x, top + 0.17F, SubheadingPpu, Cyan, true, 66);
        AddText("레벨 " + std::to_string(game_.Level()) + "  ·  처치 " + std::to_string(game_.KillCount()),
            center.x + 6.55F, top + 0.16F, SmallPpu, White, true, 66);

        AddPanel(center.x, bottom, 15.0F, 0.15F, PanelRaised, 1.0F, 31);
        if (xpRatio > 0.0F)
            AddPanel(center.x - 7.5F + 7.5F * xpRatio, bottom, 15.0F * xpRatio, 0.08F, Cyan, 1.0F, 32);

        const std::array<std::uint32_t, 3U> levels{game_.RapidLevel(), game_.MightLevel(), game_.OrbitLevel()};
        const std::array<std::string_view, 3U> names{"연사", "화력", "궤도"};
        constexpr std::array<float, 3U> offsets{-4.3F, 0.0F, 4.3F};
        const std::array<Color, 3U> accents{Cyan, Ember, Gold};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const float x = center.x + offsets[i];
            AddPanel(x, bottom + 0.48F, 3.40F, 0.64F, Panel, 0.92F, 33);
            AddVisual(skills_[i], {x - 1.35F, bottom + 0.48F}, {0.46F, 0.46F}, 0.0F,
                White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 34);
            AddText(std::string{names[i]} + "  " + std::to_string(levels[i]),
                x + 0.18F, bottom + 0.66F, SmallPpu, accents[i], true, 67);
        }
    }

    void DrawLevelUp(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.88F, 50);
        AddText("LEVEL UP", center.x, center.y + 3.65F, TinyPpu, Cyan, true, 73);
        AddText("레벨 업", center.x, center.y + 3.10F, HeadingPpu, White, true, 73);
        AddText("Q / E / F 중 하나를 선택하세요", center.x, center.y + 2.30F, SmallPpu, Muted, true, 73);

        constexpr std::array<float, 3U> X{-4.75F, 0.0F, 4.75F};
        const std::array<Color, 3U> accents{Cyan, Ember, Gold};
        const std::array<std::string_view, 3U> keys{"Q", "E", "F"};
        const std::array<std::string_view, 3U> names{"연사", "화력", "궤도"};
        const std::array<std::string_view, 3U> lines1{
            "공격 간격 감소", "투사체 공격력 증가", "주변 회전무기 +1"};
        const std::array<std::string_view, 3U> lines2{
            "3레벨마다 탄환 +1", "궤도 공격력도 증가", "최대 4개 · 이후 체력 강화"};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            AddCardFrame(center.x + X[i], center.y - 0.45F, 4.15F, 4.65F, accents[i], false);
            AddText(keys[i], center.x + X[i] - 1.55F, center.y + 1.48F, SubheadingPpu, accents[i], false, 74);
            AddVisual(skills_[i], {center.x + X[i], center.y + 0.94F}, {1.20F, 1.20F}, 0.0F,
                White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 55);
            AddText(names[i], center.x + X[i], center.y + 0.10F, HeadingPpu, accents[i], true, 74);
            AddText(lines1[i], center.x + X[i], center.y - 0.72F, SmallPpu, White, true, 74);
            AddText(lines2[i], center.x + X[i], center.y - 1.36F, TinyPpu, Muted, true, 74);
        }
    }

    void DrawPause(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.82F, 50);
        AddPanel(center.x, center.y, 6.8F, 4.25F, Panel, 0.99F, 51);
        AddText("PAUSED", center.x, center.y + 1.52F, TinyPpu, Cyan, true, 73);
        AddText("일시정지", center.x, center.y + 0.94F, HeadingPpu, White, true, 73);
        constexpr std::array<std::string_view, 2U> items{"계속하기", "메인 메뉴"};
        for (std::size_t i = 0U; i < 2U; ++i)
        {
            const float y = center.y - 0.02F - static_cast<float>(i) * 1.05F;
            const bool selected = product_.Selection() == i;
            if (selected)
                AddPanel(center.x, y - 0.16F, 4.75F, 0.70F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 53);
            AddText(items[i], center.x, y + 0.08F, BodyPpu, selected ? White : Muted, true, 74);
        }
    }

    void DrawResult(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        const auto& summary = product_.LastRunSummary();
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.90F, 50);
        AddPanel(center.x, center.y, 11.25F, 7.00F, Panel, 0.99F, 51);
        AddText(summary.cleared ? "CLEAR" : "GAME OVER", center.x, center.y + 3.02F,
            TinyPpu, summary.cleared ? Gold : Red, true, 74);
        AddText(summary.cleared ? "스테이지 클리어" : "패배", center.x, center.y + 2.42F,
            HeadingPpu, summary.cleared ? White : Red, true, 74);
        AddText("처치 " + std::to_string(summary.kills) + "  ·  레벨 " + std::to_string(summary.level) +
            "  ·  생존 " + FormatTime(summary.elapsedSeconds),
            center.x, center.y + 1.57F, SmallPpu, White, true, 74);
        AddText("별가루 획득  +" + std::to_string(summary.starsEarned),
            center.x, center.y + 0.95F, BodyPpu, Gold, true, 74);

        float unlockY = center.y + 0.18F;
        for (std::size_t i = 0U; i < NightfallProduct::CharacterCount(); ++i)
        {
            if ((summary.newlyUnlockedCharactersMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            AddText("새 캐릭터  ·  " + std::string{NightfallProduct::Character(static_cast<CharacterId>(i)).nameKo},
                center.x, unlockY, SmallPpu, CharacterAccent(i), true, 74);
            unlockY -= 0.50F;
        }
        for (std::size_t i = 0U; i < NightfallProduct::StageCount(); ++i)
        {
            if ((summary.newlyUnlockedStagesMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            AddText("새 스테이지  ·  " + std::string{NightfallProduct::Stage(static_cast<StageId>(i)).nameKo},
                center.x, unlockY, SmallPpu, StageAccent(i), true, 74);
            unlockY -= 0.50F;
        }
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            if ((summary.newlyUnlockedAchievementsMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            AddText("새 업적  ·  " + std::string{NightfallProduct::Achievement(static_cast<AchievementId>(i)).nameKo},
                center.x, unlockY, SmallPpu, Green, true, 74);
            unlockY -= 0.46F;
        }

        constexpr std::array<std::string_view, 2U> items{"다시 도전", "메인 메뉴"};
        for (std::size_t i = 0U; i < 2U; ++i)
        {
            const float x = center.x + (i == 0U ? -2.35F : 2.35F);
            const bool selected = product_.Selection() == i;
            if (selected)
                AddPanel(x, center.y - 2.35F, 3.90F, 0.70F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 53);
            AddText(items[i], x, center.y - 2.10F, BodyPpu, selected ? White : Muted, true, 74);
        }
        AddText("W/S 선택  ·  ENTER 확인", center.x, center.y - 3.22F, TinyPpu, Dim, true, 74);
    }

    trace2d::render::Renderer& renderer_;
    NightfallSurvivorsGame& game_;
    NightfallProduct& product_;
    std::filesystem::path runtimeRoot_{};
    trace2d::assets::TextureAssetCache textureCache_;

    std::array<Visual, 3U> heroes_{};
    std::array<Visual, 3U> enemies_{};
    std::array<Visual, 3U> floorsA_{};
    std::array<Visual, 3U> floorsB_{};
    std::array<Visual, 3U> skills_{};
    Visual particle_{};
    Visual white_{};

    FontHandle fontHandle_{};
    std::unique_ptr<trace2d::text::GlyphAtlas> glyphAtlas_{};
    std::unique_ptr<trace2d::text::TextLayoutRun> textLayout_{};
    TextureHandle glyphTexture_{};
    trace2d::text::GlyphAtlasTextureBinding2D glyphBinding_{};
    std::uint64_t frozenGlyphRevision_{0U};
    std::vector<trace2d::render::SpritePresentationRenderData> textScratch_{};
    std::vector<trace2d::render::SpritePresentationRenderData> draws_{};
    NightfallLayoutAudit layoutAudit_{};

    trace2d::render::OrthographicCamera camera_{};
    trace2d::scene::Vector2 cameraCenter_{};
    float viewVerticalSize_{CoreHeight};
    float viewHalfWidth_{CoreWidth * 0.5F};
    float viewHalfHeight_{CoreHeight * 0.5F};
    std::uint64_t order_{0U};
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
    self->impl_->Present(context);
}
