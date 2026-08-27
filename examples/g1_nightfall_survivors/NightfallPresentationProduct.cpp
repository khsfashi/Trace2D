#include "NightfallPresentation.hpp"

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/assets/TextureAssets.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>
#include <trace2d/render/SpriteRenderContract.hpp>
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
using Color = trace2d::render::SpriteLinearRgba;
using Screen = NightfallProduct::Screen;
using CharacterId = NightfallProduct::CharacterId;
using StageId = NightfallProduct::StageId;
using AchievementId = NightfallProduct::AchievementId;

constexpr float Pi = 3.14159265358979323846F;
constexpr float CoreWidth = NightfallSurvivorsGame::CameraHorizontalSize;
constexpr float CoreHeight = NightfallSurvivorsGame::CameraVerticalSize;
constexpr float CoreAspect = CoreWidth / CoreHeight;

constexpr float TitlePpu = 26.0F;
constexpr float HeadingPpu = 36.0F;
constexpr float SubheadingPpu = 46.0F;
constexpr float BodyPpu = 57.0F;
constexpr float SmallPpu = 70.0F;
constexpr float TinyPpu = 82.0F;
constexpr float MicroPpu = 112.0F;

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

[[nodiscard]] std::vector<std::uint8_t> ReadBinary(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
        throw std::runtime_error{"Nightfall Survivors could not read: " + path.string()};
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
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
        throw std::runtime_error{"Nightfall Survivors could not load product asset: " + reference};

    trace2d::assets::TextureResource canonical{};
    canonical.width = loaded.asset->width;
    canonical.height = loaded.asset->height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Srgb;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors product presentation";
    canonical.canonicalRgba8 = loaded.asset->rgba8;
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not publish product texture: " + reference};

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
        throw std::runtime_error{"Nightfall Survivors could not resolve product sprite region."};
    }
    return visual;
}

[[nodiscard]] Visual MakeWhiteVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer)
{
    constexpr std::array<std::uint8_t, 4U> Pixels{255U, 255U, 255U, 255U};
    constexpr std::string_view Reference = "generated/nightfall-product-white-v2.rgba8";
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors product UI primitive";
    canonical.canonicalRgba8.assign(Pixels.begin(), Pixels.end());
    const auto published = resources.PublishTexture(std::string{Reference}, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall could not publish UI primitive."};

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle,
        trace2d::render::Rgba8TextureData{1U, 1U, Pixels},
        trace2d::render::SpriteTextureEncoding::Linear);
    visual.pixelsPerUnit = 1.0F;
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = "nightfall-product-white-v2.sprite";
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
            "textures/hero-star.png",
            "textures/hero-ember.png",
            "textures/hero-moon.png",
        };
        const std::array<std::string, 3U> enemyRefs{
            "textures/enemy-ghoul.png",
            "textures/enemy-brute.png",
            "textures/enemy-wisp.png",
        };
        const std::array<std::string, 3U> floorARefs{
            "textures/stage-moon-floor-a.png",
            "textures/stage-ember-floor-a.png",
            "textures/stage-astral-floor-a.png",
        };
        const std::array<std::string, 3U> floorBRefs{
            "textures/stage-moon-floor-b.png",
            "textures/stage-ember-floor-b.png",
            "textures/stage-astral-floor-b.png",
        };
        const std::array<std::string, 3U> skillRefs{
            "textures/skill-rapid.png",
            "textures/skill-might.png",
            "textures/skill-orbit.png",
        };

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
        PrepareText();
    }

    ~Impl()
    {
        if (glyphTexture_.generation != 0U)
        {
            renderer_.DestroyTexture(glyphTexture_);
            static_cast<void>(game_.Resources().Unload(glyphTexture_.Untyped()));
        }
        textLayout_.reset();
        glyphAtlas_.reset();
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

        const Screen screen = product_.CurrentScreen();
        if (screen == Screen::Playing || screen == Screen::Pause || screen == Screen::Result)
        {
            DrawWorld(context);
            DrawHud(context);
            if (game_.CurrentState() == NightfallSurvivorsGame::State::LevelUp && screen == Screen::Playing)
                DrawLevelUp(context);
            if (screen == Screen::Pause) DrawPause(context);
            if (screen == Screen::Result) DrawResult(context);
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

        auto atlas = trace2d::text::PrepareGlyphAtlas(
            game_.Resources(),
            fontPublished.handle,
            trace2d::text::GlyphAtlasConfig{2048U, 2048U, 32U, 1U, 2048U});
        if (!atlas.Succeeded())
            throw std::runtime_error{"Nightfall could not prepare Galmuri glyph atlas."};
        glyphAtlas_ = std::move(atlas.atlas);

        std::string corpus =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 %/:+-.[]()·<>,←→"
            "NIGHTFALL SURVIVORS Trace2D WASD QEF ESC ENTER SPACE R"
            "게임 시작 프로필 업적 설정 종료 돌아가기 선택 확인 캐릭터 스테이지 잠김 해금 조건 필요"
            "별가루 플레이 클리어 누적 처치 최고 레벨 최고 생존 총 횟수 효과음 볼륨 화면 흔들림 켜짐 꺼짐"
            "일시정지 계속하기 메인 메뉴 레벨 업 하나를 선택하세요 연사 화력 궤도 공격 간격 감소 투사체 공격력 증가"
            "주변 회전무기 최대 이후 체력 강화 체력 남은 시간 레벨 처치 다시 도전 획득 새 캐릭터 새 스테이지 새 업적"
            "달성 미달성 조작 이동 자동공격 재시작 시작 뒤로 현재 장착 보유 별 전투 기록 모험 준비 완료 패배";
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
            throw std::runtime_error{"Nightfall could not warm product glyph atlas."};

        SyncGlyphAtlas();

        auto layout = trace2d::text::PrepareTextLayoutRun({1024U, 64U});
        if (!layout.Succeeded())
            throw std::runtime_error{"Nightfall could not prepare product text layout."};
        textLayout_ = std::move(layout.run);
    }

    void SyncGlyphAtlas()
    {
        if (glyphAtlas_ == nullptr) return;
        const std::uint64_t revision = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
        if (glyphBinding_.atlas == glyphAtlas_.get() && glyphBinding_.pixelRevision == revision &&
            glyphTexture_.generation != 0U)
        {
            return;
        }

        if (glyphTexture_.generation != 0U)
        {
            renderer_.DestroyTexture(glyphTexture_);
            static_cast<void>(game_.Resources().Unload(glyphTexture_.Untyped()));
            glyphTexture_ = {};
            glyphBinding_ = {};
        }

        const auto config = glyphAtlas_->Config();
        trace2d::assets::TextureResource texture{};
        texture.width = config.width;
        texture.height = config.height;
        texture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
        texture.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
        texture.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
        texture.retentionReason = "Nightfall Galmuri glyph atlas";
        texture.canonicalRgba8.resize(static_cast<std::size_t>(config.width) * config.height * 4U);
        std::size_t requiredBytes = 0U;
        const auto written = trace2d::text::WriteGlyphAtlasRgba8(
            *glyphAtlas_, texture.canonicalRgba8, requiredBytes);
        if (!written.Succeeded() || requiredBytes != texture.canonicalRgba8.size())
            throw std::runtime_error{"Nightfall could not rasterize product glyph atlas."};

        const std::string reference = "generated/nightfall-product-glyph-atlas-" +
            std::to_string(++glyphTextureVersion_) + ".rgba8";
        const auto published = game_.Resources().PublishTexture(reference, std::move(texture));
        if (!published.Succeeded())
            throw std::runtime_error{"Nightfall could not publish product glyph texture."};
        const auto* const canonical = game_.Resources().Resolve(published.handle);
        if (canonical == nullptr)
            throw std::runtime_error{"Nightfall lost product glyph texture resource."};
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
            throw std::runtime_error{
                "Nightfall could not bind product glyph atlas: " +
                std::string{trace2d::text::ToString(bindingStatus.error)}};
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
        if (!glyphAtlas_->WarmUtf8(text).Succeeded())
            throw std::runtime_error{"Nightfall product text warm failed."};
        SyncGlyphAtlas();

        const trace2d::text::TextFontAtlasRef atlasRef{glyphAtlas_.get()};
        const std::span<const trace2d::text::TextFontAtlasRef> atlases(&atlasRef, 1U);
        trace2d::text::TextLayoutOptions options{};
        options.wrapMode = trace2d::text::TextWrapMode::None;
        const auto layoutResult = textLayout_->LayoutUtf8(atlases, text, options);
        if (!layoutResult.Succeeded())
            throw std::runtime_error{"Nightfall product text layout failed."};

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
        config.sampler = trace2d::render::SpriteSamplerCompatibility::Nearest;

        std::size_t requiredCount = 0U;
        trace2d::text::TextPresentationMeasurement2D measurement{};
        auto status = trace2d::text::BuildTextPresentation2D(
            *textLayout_, atlases,
            std::span<const trace2d::text::GlyphAtlasTextureBinding2D>(&glyphBinding_, 1U),
            config, textScratch_, requiredCount, measurement);
        if (status.error == trace2d::text::TextPresentationError::StaleAtlasBinding)
        {
            SyncGlyphAtlas();
            status = trace2d::text::BuildTextPresentation2D(
                *textLayout_, atlases,
                std::span<const trace2d::text::GlyphAtlasTextureBinding2D>(&glyphBinding_, 1U),
                config, textScratch_, requiredCount, measurement);
        }
        if (!status.Succeeded() || requiredCount > textScratch_.size())
        {
            throw std::runtime_error{
                "Nightfall product text presentation failed: " +
                std::string{trace2d::text::ToString(status.error)} +
                " glyph=" + std::to_string(status.glyphIndex) +
                " required=" + std::to_string(requiredCount)};
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

    void AddCardFrame(const float x, const float y, const float width, const float height, const Color accent, const bool selected)
    {
        AddPanel(x, y, width, height, selected ? PanelRaised : Panel, 0.97F, 41);
        AddPanel(x, y + height * 0.5F - 0.045F, width, 0.09F, selected ? accent : Dim, 1.0F, 42);
        if (selected)
        {
            AddPanel(x - width * 0.5F + 0.045F, y, 0.09F, height, accent, 1.0F, 42);
        }
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
        AddPanel(0.0F, 0.0F, CoreWidth, CoreHeight, Ink, 0.90F, 20);
        AddPanel(0.0F, 4.18F, CoreWidth, 0.64F, {0.02F, 0.03F, 0.06F, 1.0F}, 0.98F, 25);
        AddPanel(0.0F, -4.18F, CoreWidth, 0.64F, {0.02F, 0.03F, 0.06F, 1.0F}, 0.98F, 25);
    }

    void DrawHeader(std::string_view eyebrow, std::string_view title)
    {
        AddText(eyebrow, -7.15F, 3.94F, SmallPpu, Cyan, false, 70);
        AddText(title, -7.15F, 3.44F, HeadingPpu, White, false, 70);
        AddPanel(-5.15F, 2.94F, 4.0F, 0.055F, Cyan, 0.85F, 43);
    }

    void DrawFooter(std::string_view help)
    {
        AddText(help, 0.0F, -4.02F, SmallPpu, Dim, true, 75);
    }

    void DrawMainMenu()
    {
        AddText("NIGHTFALL", -7.12F, 3.78F, TitlePpu, White, false, 72);
        AddText("SURVIVORS", -7.08F, 3.02F, SubheadingPpu, Cyan, false, 72);
        AddText("Trace2D ORIGINAL SURVIVAL", -7.08F, 2.54F, TinyPpu, Dim, false, 72);

        AddPanel(-4.92F, -0.52F, 4.62F, 5.25F, Panel, 0.98F, 40);
        AddText("MENU", -6.75F, 1.63F, TinyPpu, Dim, false, 72);
        constexpr std::array<std::string_view, 5U> Menu{
            "게임 시작", "프로필", "업적", "설정", "종료"};
        for (std::size_t i = 0U; i < Menu.size(); ++i)
        {
            const float y = 1.02F - static_cast<float>(i) * 0.88F;
            const bool selected = product_.Selection() == i;
            if (selected)
            {
                AddPanel(-4.92F, y - 0.12F, 4.10F, 0.68F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 43);
                AddPanel(-6.89F, y - 0.12F, 0.08F, 0.68F, Cyan, 1.0F, 44);
            }
            AddText(Menu[i], -6.55F, y + 0.14F, BodyPpu, selected ? White : Muted, false, 74);
        }

        const std::size_t characterIndex = static_cast<std::size_t>(product_.SelectedCharacter());
        const auto& character = NightfallProduct::Character(product_.SelectedCharacter());
        AddPanel(2.55F, -0.52F, 8.10F, 5.25F, Panel, 0.98F, 40);
        AddText("현재 캐릭터", -0.95F, 1.63F, TinyPpu, Dim, false, 72);
        AddText(character.nameKo, -0.95F, 1.10F, HeadingPpu, CharacterAccent(characterIndex), false, 72);
        AddText(character.subtitleKo, -0.95F, 0.52F, SmallPpu, Muted, false, 72);
        const float bob = std::sin(static_cast<float>(game_.FrameCounter() % 120U) * 0.0523598776F) * 0.06F;
        AddVisual(heroes_[characterIndex], {1.08F, -0.52F + bob}, {3.6F, 3.6F}, 0.0F, White, 1.0F,
            trace2d::render::SpriteBlendMode::Normal, 45);

        const auto& profile = product_.PlayerProfile();
        AddText("전투 기록", 3.65F, 1.63F, TinyPpu, Dim, false, 72);
        AddText("별가루  " + std::to_string(profile.stars), 3.65F, 1.02F, BodyPpu, Gold, false, 72);
        AddText("플레이  " + std::to_string(profile.totalRuns) + "회", 3.65F, 0.38F, SmallPpu, Muted, false, 72);
        AddText("클리어  " + std::to_string(profile.totalClears) + "회", 3.65F, -0.18F, SmallPpu, Muted, false, 72);
        AddText("누적 처치  " + std::to_string(profile.totalKills), 3.65F, -0.74F, SmallPpu, Muted, false, 72);
        AddText("최고 레벨  " + std::to_string(profile.bestLevel), 3.65F, -1.30F, SmallPpu, Muted, false, 72);

        DrawFooter("W/S 선택  ·  ENTER 확인  ·  ESC 종료");
    }

    void DrawProfile()
    {
        DrawHeader("PLAYER PROFILE", "프로필");
        AddPanel(0.0F, -0.35F, 13.4F, 5.80F, Panel, 0.98F, 40);
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
            const float x = rightColumn ? 0.7F : -5.7F;
            const float y = 1.78F - static_cast<float>(row) * 1.02F;
            AddText(rows[i].first, x, y, SmallPpu, Dim, false, 72);
            AddText(rows[i].second, x, y - 0.42F, SubheadingPpu, i == 5U ? Gold : White, false, 72);
        }
        DrawFooter("ENTER 또는 ESC  ·  돌아가기");
    }

    void DrawAchievements()
    {
        DrawHeader("ACHIEVEMENTS", "업적");
        AddPanel(0.0F, -0.35F, 14.0F, 5.80F, Panel, 0.98F, 40);
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            const auto id = static_cast<AchievementId>(i);
            const auto& definition = NightfallProduct::Achievement(id);
            const bool unlocked = product_.IsAchievementUnlocked(id);
            const bool rightColumn = i >= 3U;
            const std::size_t row = rightColumn ? i - 3U : i;
            const float x = rightColumn ? 0.55F : -6.25F;
            const float y = 1.72F - static_cast<float>(row) * 1.52F;
            const Color card = unlocked ? PanelRaised : Color{0.045F, 0.052F, 0.072F, 1.0F};
            AddPanel(x + 0.20F, y - 0.30F, 6.05F, 1.20F, card, 1.0F, 42);
            AddPanel(x - 2.64F, y - 0.30F, 0.12F, 1.20F, unlocked ? Green : Dim, 1.0F, 43);
            AddText(unlocked ? "달성" : "미달성", x - 2.35F, y + 0.12F, TinyPpu, unlocked ? Green : Dim, false, 72);
            AddText(definition.nameKo, x - 1.20F, y + 0.15F, BodyPpu, unlocked ? White : Muted, false, 72);
            AddText(definition.descriptionKo, x - 2.35F, y - 0.43F, MicroPpu, Dim, false, 72);
        }
        DrawFooter("ENTER 또는 ESC  ·  돌아가기");
    }

    void DrawSettings()
    {
        DrawHeader("SETTINGS", "설정");
        AddPanel(0.0F, -0.28F, 10.6F, 5.55F, Panel, 0.98F, 40);
        const auto& profile = product_.PlayerProfile();
        const std::string volume = profile.sfxVolumeStep == 0U ? "0%" : (profile.sfxVolumeStep == 1U ? "50%" : "100%");
        const std::array<std::string, 3U> labels{{
            "효과음 볼륨   " + volume,
            std::string{"화면 흔들림   "} + (profile.cameraShakeEnabled ? "켜짐" : "꺼짐"),
            "돌아가기",
        }};
        constexpr std::array<float, 3U> Y{1.15F, 0.05F, -1.55F};
        for (std::size_t i = 0U; i < labels.size(); ++i)
        {
            const bool selected = product_.Selection() == i;
            if (selected)
            {
                AddPanel(0.0F, Y[i] - 0.12F, 8.7F, 0.80F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 43);
                AddPanel(-4.30F, Y[i] - 0.12F, 0.10F, 0.80F, Cyan, 1.0F, 44);
            }
            AddText(labels[i], -3.85F, Y[i] + 0.15F, BodyPpu, selected ? White : Muted, false, 72);
        }
        DrawFooter("W/S 선택  ·  A/D 또는 ←/→ 조절  ·  ENTER 확인");
    }

    void DrawCharacterSelect()
    {
        DrawHeader("CHOOSE YOUR HUNTER", "캐릭터 선택");
        constexpr std::array<float, 3U> X{-5.15F, 0.0F, 5.15F};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto id = static_cast<CharacterId>(i);
            const auto& definition = NightfallProduct::Character(id);
            const bool unlocked = product_.IsCharacterUnlocked(id);
            const bool selected = product_.Selection() == i;
            const Color accent = CharacterAccent(i);
            AddCardFrame(X[i], -0.05F, 4.45F, 4.65F, unlocked ? accent : Dim, selected);
            AddVisual(heroes_[i], {X[i], 1.12F}, {2.45F, 2.45F}, 0.0F,
                unlocked ? White : Dim, unlocked ? 1.0F : 0.52F,
                trace2d::render::SpriteBlendMode::Normal, 45);
            AddText(unlocked ? definition.nameKo : "잠김", X[i], -0.12F, SubheadingPpu,
                unlocked ? accent : Red, true, 73);
            AddText(definition.subtitleKo, X[i], -0.78F, TinyPpu, unlocked ? Muted : Dim, true, 73);
            if (!unlocked)
                AddText("해금 조건 필요", X[i], -1.48F, MicroPpu, Red, true, 73);
        }
        const std::size_t selectedIndex = std::min<std::size_t>(product_.Selection(), 2U);
        const auto selectedId = static_cast<CharacterId>(selectedIndex);
        const bool selectedUnlocked = product_.IsCharacterUnlocked(selectedId);
        AddPanel(0.0F, -2.92F, 13.7F, 0.68F, Panel, 0.98F, 42);
        AddText(
            selectedUnlocked ? NightfallProduct::Character(selectedId).descriptionKo : "해금 후 상세 능력을 확인할 수 있습니다.",
            0.0F, -2.70F, MicroPpu, selectedUnlocked ? Muted : Dim, true, 74);
        DrawFooter("A/D 또는 W/S 선택  ·  ENTER 확인  ·  ESC 뒤로");
    }

    void DrawStageSelect()
    {
        DrawHeader("CHOOSE THE NIGHT", "스테이지 선택");
        constexpr std::array<float, 3U> X{-5.15F, 0.0F, 5.15F};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto id = static_cast<StageId>(i);
            const auto& definition = NightfallProduct::Stage(id);
            const bool unlocked = product_.IsStageUnlocked(id);
            const bool selected = product_.Selection() == i;
            const Color accent = StageAccent(i);
            AddCardFrame(X[i], -0.05F, 4.45F, 4.65F, unlocked ? accent : Dim, selected);
            AddPanel(X[i], 1.22F, 3.55F, 1.48F, {0.025F, 0.03F, 0.05F, 1.0F}, 1.0F, 43);
            for (int py = 0; py < 2; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    const Visual& tile = ((px + py) & 1) == 0 ? floorsA_[i] : floorsB_[i];
                    AddVisual(tile,
                        {X[i] - 1.35F + static_cast<float>(px) * 0.90F,
                         0.90F + static_cast<float>(py) * 0.64F},
                        {0.90F, 0.64F}, 0.0F, unlocked ? White : Dim,
                        unlocked ? 1.0F : 0.50F,
                        trace2d::render::SpriteBlendMode::Normal, 44);
                }
            }
            AddText(unlocked ? definition.nameKo : "잠김", X[i], -0.12F, SubheadingPpu,
                unlocked ? accent : Red, true, 73);
            AddText(definition.subtitleKo, X[i], -0.78F, TinyPpu, unlocked ? White : Dim, true, 73);
        }
        const std::size_t selectedIndex = std::min<std::size_t>(product_.Selection(), 2U);
        const auto selectedStage = static_cast<StageId>(selectedIndex);
        const bool selectedUnlocked = product_.IsStageUnlocked(selectedStage);
        AddPanel(0.0F, -2.92F, 13.7F, 0.68F, Panel, 0.98F, 42);
        AddText(
            selectedUnlocked ? NightfallProduct::Stage(selectedStage).descriptionKo : "이전 스테이지를 클리어하면 해금됩니다.",
            0.0F, -2.70F, MicroPpu, selectedUnlocked ? Muted : Dim, true, 74);
        const auto selectedCharacter = product_.SelectedCharacter();
        AddText(
            "현재 장착  ·  " + std::string{NightfallProduct::Character(selectedCharacter).nameKo},
            0.0F, -3.42F, TinyPpu, Gold, true, 74);
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
        const float top = center.y + 4.10F;
        const float bottom = center.y - 4.12F;
        const float healthRatio = static_cast<float>(game_.Health()) /
            static_cast<float>(std::max<std::uint32_t>(1U, game_.MaximumHealth()));
        const float xpRatio = static_cast<float>(game_.Experience()) /
            static_cast<float>(std::max<std::uint32_t>(1U, game_.ExperienceToNextLevel()));

        AddPanel(center.x, top - 0.02F, 15.25F, 0.62F, Ink, 0.90F, 30);
        AddText("체력 " + std::to_string(game_.Health()) + "/" + std::to_string(game_.MaximumHealth()),
            center.x - 5.55F, top + 0.20F, SmallPpu, White, true, 66);
        const float remaining = std::max(0.0F, game_.RunDurationSeconds() - game_.ElapsedSeconds());
        AddText(FormatTime(remaining), center.x, top + 0.22F, SubheadingPpu, Cyan, true, 66);
        AddText("레벨 " + std::to_string(game_.Level()) + "  ·  처치 " + std::to_string(game_.KillCount()),
            center.x + 5.55F, top + 0.20F, SmallPpu, White, true, 66);

        AddPanel(center.x, bottom, 15.0F, 0.16F, PanelRaised, 1.0F, 31);
        if (xpRatio > 0.0F)
        {
            AddPanel(center.x - 7.5F + 7.5F * xpRatio, bottom, 15.0F * xpRatio, 0.09F, Cyan, 1.0F, 32);
        }

        const std::array<std::uint32_t, 3U> levels{game_.RapidLevel(), game_.MightLevel(), game_.OrbitLevel()};
        const std::array<std::string_view, 3U> names{"연사", "화력", "궤도"};
        constexpr std::array<float, 3U> offsets{-4.2F, 0.0F, 4.2F};
        const std::array<Color, 3U> accents{Cyan, Ember, Gold};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const float x = center.x + offsets[i];
            AddPanel(x, bottom + 0.48F, 3.35F, 0.64F, Panel, 0.90F, 33);
            AddVisual(skills_[i], {x - 1.34F, bottom + 0.48F}, {0.48F, 0.48F}, 0.0F,
                White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 34);
            AddText(std::string{names[i]} + "  " + std::to_string(levels[i]),
                x + 0.20F, bottom + 0.67F, SmallPpu, accents[i], true, 67);
        }
        (void)healthRatio;
    }

    void DrawLevelUp(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.86F, 50);
        AddText("LEVEL UP", center.x, center.y + 3.63F, TinyPpu, Cyan, true, 73);
        AddText("레벨 업", center.x, center.y + 3.12F, HeadingPpu, White, true, 73);
        AddText("Q / E / F 중 하나를 선택하세요", center.x, center.y + 2.47F, SmallPpu, Muted, true, 73);

        constexpr std::array<float, 3U> X{-4.75F, 0.0F, 4.75F};
        const std::array<Color, 3U> accents{Cyan, Ember, Gold};
        const std::array<std::string_view, 3U> keys{"Q", "E", "F"};
        const std::array<std::string_view, 3U> names{"연사", "화력", "궤도"};
        const std::array<std::string_view, 3U> lines1{
            "공격 간격 감소",
            "투사체 공격력 증가",
            "주변 회전무기 +1",
        };
        const std::array<std::string_view, 3U> lines2{
            "3레벨마다 탄환 +1",
            "궤도 공격력도 증가",
            "최대 4개 · 이후 체력 강화",
        };
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            AddCardFrame(center.x + X[i], center.y - 0.35F, 4.15F, 4.75F, accents[i], false);
            AddText(keys[i], center.x + X[i] - 1.55F, center.y + 1.53F, SubheadingPpu, accents[i], false, 74);
            AddVisual(skills_[i], {center.x + X[i], center.y + 0.95F}, {1.35F, 1.35F}, 0.0F,
                White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 55);
            AddText(names[i], center.x + X[i], center.y + 0.06F, HeadingPpu, accents[i], true, 74);
            AddText(lines1[i], center.x + X[i], center.y - 0.68F, SmallPpu, White, true, 74);
            AddText(lines2[i], center.x + X[i], center.y - 1.25F, TinyPpu, Muted, true, 74);
        }
    }

    void DrawPause(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.80F, 50);
        AddPanel(center.x, center.y, 6.8F, 4.25F, Panel, 0.99F, 51);
        AddText("PAUSED", center.x, center.y + 1.50F, TinyPpu, Cyan, true, 73);
        AddText("일시정지", center.x, center.y + 1.03F, HeadingPpu, White, true, 73);
        constexpr std::array<std::string_view, 2U> items{"계속하기", "메인 메뉴"};
        for (std::size_t i = 0U; i < 2U; ++i)
        {
            const float y = center.y + 0.05F - static_cast<float>(i) * 1.02F;
            const bool selected = product_.Selection() == i;
            if (selected) AddPanel(center.x, y - 0.12F, 4.75F, 0.76F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 53);
            AddText(items[i], center.x, y + 0.15F, BodyPpu, selected ? White : Muted, true, 74);
        }
    }

    void DrawResult(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        const auto& summary = product_.LastRunSummary();
        AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.88F, 50);
        AddPanel(center.x, center.y, 11.2F, 7.05F, Panel, 0.99F, 51);
        AddText(summary.cleared ? "CLEAR" : "GAME OVER", center.x, center.y + 3.02F,
            TinyPpu, summary.cleared ? Gold : Red, true, 74);
        AddText(summary.cleared ? "스테이지 클리어" : "패배", center.x, center.y + 2.50F,
            HeadingPpu, summary.cleared ? White : Red, true, 74);
        AddText("처치 " + std::to_string(summary.kills) + "  ·  레벨 " + std::to_string(summary.level) +
            "  ·  생존 " + FormatTime(summary.elapsedSeconds),
            center.x, center.y + 1.68F, SmallPpu, White, true, 74);
        AddText("별가루 획득  +" + std::to_string(summary.starsEarned),
            center.x, center.y + 1.05F, BodyPpu, Gold, true, 74);

        float unlockY = center.y + 0.30F;
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
            if (selected) AddPanel(x, center.y - 2.30F, 3.90F, 0.76F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 53);
            AddText(items[i], x, center.y - 2.03F, BodyPpu, selected ? White : Muted, true, 74);
        }
        AddText("W/S 선택  ·  ENTER 확인", center.x, center.y - 3.15F, SmallPpu, Dim, true, 74);
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

    std::unique_ptr<trace2d::text::GlyphAtlas> glyphAtlas_{};
    std::unique_ptr<trace2d::text::TextLayoutRun> textLayout_{};
    TextureHandle glyphTexture_{};
    trace2d::text::GlyphAtlasTextureBinding2D glyphBinding_{};
    std::uint32_t glyphTextureVersion_{0U};
    std::vector<trace2d::render::SpritePresentationRenderData> textScratch_{};
    std::vector<trace2d::render::SpritePresentationRenderData> draws_{};

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
