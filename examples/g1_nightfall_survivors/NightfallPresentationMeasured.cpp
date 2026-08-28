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
#include <limits>
#include <memory>
#include <span>
#include <sstream>
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

// Galmuri is a 32 px raster face. Higher PPU means a smaller world-space label.
constexpr float TitlePpu = 44.0F;
constexpr float HeadingPpu = 54.0F;
constexpr float BodyPpu = 68.0F;
constexpr float SmallPpu = 84.0F;
constexpr float TinyPpu = 104.0F;
constexpr float MicroPpu = 124.0F;

constexpr Color White{0.96F, 0.97F, 1.0F, 1.0F};
constexpr Color Muted{0.67F, 0.73F, 0.83F, 1.0F};
constexpr Color Dim{0.43F, 0.48F, 0.58F, 1.0F};
constexpr Color Ink{0.028F, 0.035F, 0.055F, 1.0F};
constexpr Color Panel{0.050F, 0.062F, 0.094F, 1.0F};
constexpr Color PanelRaised{0.075F, 0.095F, 0.145F, 1.0F};
constexpr Color Cyan{0.28F, 0.82F, 1.0F, 1.0F};
constexpr Color Gold{1.0F, 0.72F, 0.24F, 1.0F};
constexpr Color Ember{1.0F, 0.36F, 0.20F, 1.0F};
constexpr Color Moon{0.62F, 0.72F, 1.0F, 1.0F};
constexpr Color Green{0.35F, 0.92F, 0.58F, 1.0F};
constexpr Color Red{1.0F, 0.28F, 0.32F, 1.0F};

struct Rect final
{
    float left{0.0F};
    float bottom{0.0F};
    float right{0.0F};
    float top{0.0F};
};

struct Visual final
{
    std::shared_ptr<trace2d::assets::SpriteAsset> asset{};
    std::vector<trace2d::render::ResolvedSpriteRegion> selections{};
    TextureHandle texture{};
    float pixelsPerUnit{16.0F};
};

[[nodiscard]] Rect BoundsOfQuad(const trace2d::render::SpriteDrawQuad& quad) noexcept
{
    const std::array<trace2d::render::SpriteDrawVertex, 4U> vertices{
        quad.topLeft, quad.topRight, quad.bottomRight, quad.bottomLeft};
    Rect result{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };
    for (const auto& vertex : vertices)
    {
        result.left = std::min(result.left, vertex.position.x);
        result.bottom = std::min(result.bottom, vertex.position.y);
        result.right = std::max(result.right, vertex.position.x);
        result.top = std::max(result.top, vertex.position.y);
    }
    return result;
}

[[nodiscard]] Rect UnionRect(const Rect a, const Rect b) noexcept
{
    return {
        std::min(a.left, b.left),
        std::min(a.bottom, b.bottom),
        std::max(a.right, b.right),
        std::max(a.top, b.top),
    };
}

[[nodiscard]] bool Intersects(const Rect a, const Rect b, const float gap = 0.0F) noexcept
{
    return a.left < b.right + gap && a.right > b.left - gap &&
        a.bottom < b.top + gap && a.top > b.bottom - gap;
}

[[nodiscard]] bool Contains(const Rect parent, const Rect child, const float margin = 0.0F) noexcept
{
    return child.left >= parent.left + margin && child.right <= parent.right - margin &&
        child.bottom >= parent.bottom + margin && child.top <= parent.top - margin;
}

[[nodiscard]] std::string RectString(const Rect rect)
{
    std::ostringstream stream{};
    stream << '[' << rect.left << ',' << rect.bottom << " -> " << rect.right << ',' << rect.top << ']';
    return stream.str();
}

void RequireInside(
    const std::string_view childId,
    const Rect child,
    const std::string_view parentId,
    const Rect parent,
    const float margin = 0.0F)
{
    if (Contains(parent, child, margin)) return;
    throw std::runtime_error{
        "Nightfall layout violation: " + std::string{childId} + " escapes " + std::string{parentId} +
        " child=" + RectString(child) + " parent=" + RectString(parent)};
}

void RequireDisjoint(
    const std::string_view leftId,
    const Rect left,
    const std::string_view rightId,
    const Rect right,
    const float gap = 0.0F)
{
    if (!Intersects(left, right, gap)) return;
    throw std::runtime_error{
        "Nightfall layout violation: " + std::string{leftId} + " intersects " + std::string{rightId} +
        " first=" + RectString(left) + " second=" + RectString(right)};
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
    canonical.retentionReason = "Nightfall Survivors measured presentation";
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
    constexpr std::string_view Reference = "generated/nightfall-measured-white.rgba8";
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall measured UI primitive";
    canonical.canonicalRgba8.assign(Pixels.begin(), Pixels.end());
    const auto published = resources.PublishTexture(std::string{Reference}, std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"Nightfall could not publish UI primitive."};

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle,
        trace2d::render::Rgba8TextureData{1U, 1U, Pixels},
        trace2d::render::SpriteTextureEncoding::Linear);
    visual.pixelsPerUnit = 1.0F;
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = "nightfall-measured-white.sprite";
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
    if (index == 1U) return Ember;
    if (index == 2U) return Moon;
    return Gold;
}

[[nodiscard]] Color StageAccent(const std::size_t index) noexcept
{
    if (index == 1U) return Ember;
    if (index == 2U) return {0.66F, 0.48F, 1.0F, 1.0F};
    return Cyan;
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
        SyncGlyphAtlas();
        frameGlyphRevision_ = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
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
            "게임 시작 프로필 업적 설정 종료 돌아가기 선택 확인 캐릭터 스테이지 잠김 해금 조건 필요 또는"
            "별가루 플레이 클리어 누적 처치 최고 레벨 최고 생존 총 횟수 효과음 볼륨 화면 흔들림 켜짐 꺼짐"
            "일시정지 계속하기 메인 메뉴 레벨 업 하나를 선택하세요 연사 화력 궤도 공격 간격 감소 투사체 공격력 증가"
            "주변 회전무기 최대 이후 체력 강화 체력 남은 시간 다시 도전 획득 새 달성 미달성 조작 이동 자동공격 재시작"
            "현재 장착 보유 전투 기록 모험 준비 완료 패배 회 분 초 상세 능력을 확인할 수 있습니다 이전 클리어하면 해금됩니다"
            "공격 방어 속도 장착 조절 뒤로 중 레벨마다 탄환 4개 캐릭터선택 스테이지선택 생존자 첫 번째 밤 최종 결과 해금 후";
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
            throw std::runtime_error{"Nightfall could not warm measured product glyph atlas."};

        auto layout = trace2d::text::PrepareTextLayoutRun({1024U, 64U});
        if (!layout.Succeeded())
            throw std::runtime_error{"Nightfall could not prepare product text layout."};
        textLayout_ = std::move(layout.run);
        SyncGlyphAtlas();
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

        if (!draws_.empty())
        {
            throw std::runtime_error{
                "Nightfall glyph atlas resync requested after frame draw emission began. "
                "Static UI corpus/prewarm is incomplete."};
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
        texture.retentionReason = "Nightfall measured Galmuri glyph atlas";
        texture.canonicalRgba8.resize(static_cast<std::size_t>(config.width) * config.height * 4U);
        std::size_t requiredBytes = 0U;
        const auto written = trace2d::text::WriteGlyphAtlasRgba8(
            *glyphAtlas_, texture.canonicalRgba8, requiredBytes);
        if (!written.Succeeded() || requiredBytes != texture.canonicalRgba8.size())
            throw std::runtime_error{"Nightfall could not rasterize measured glyph atlas."};

        const std::string reference = "generated/nightfall-measured-glyph-atlas-" +
            std::to_string(++glyphTextureVersion_) + ".rgba8";
        const auto published = game_.Resources().PublishTexture(reference, std::move(texture));
        if (!published.Succeeded())
            throw std::runtime_error{"Nightfall could not publish measured glyph texture."};
        const auto* const canonical = game_.Resources().Resolve(published.handle);
        if (canonical == nullptr)
            throw std::runtime_error{"Nightfall lost measured glyph texture resource."};
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
                "Nightfall could not bind measured glyph atlas: " +
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
        camera_.center = {center.x + shake, center.y - shake * 0.5F};
        camera_.verticalSize = viewVerticalSize_;
    }

    [[nodiscard]] Rect AddVisual(
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
            throw std::runtime_error{"Nightfall measured sprite presentation failed."};
        }
        draw.texture = visual.texture;
        draw.order.layer = layer;
        draw.order.stableOrder = order_++;
        const Rect bounds = BoundsOfQuad(draw.presentation.quad);
        draws_.push_back(draw);
        return bounds;
    }

    [[nodiscard]] Rect AddPanel(
        const float x,
        const float y,
        const float width,
        const float height,
        const Color color,
        const float opacity = 1.0F,
        const std::int32_t layer = 40)
    {
        return AddVisual(white_, {x, y}, {width, height}, 0.0F, color, opacity,
            trace2d::render::SpriteBlendMode::Normal, layer);
    }

    [[nodiscard]] Rect AddText(
        const std::string_view text,
        const float x,
        const float topY,
        const float pixelsPerUnit,
        const Color color,
        const bool centered = false,
        const std::int32_t layer = 70)
    {
        const std::uint64_t revisionBefore = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
        const trace2d::text::TextFontAtlasRef atlasRef{glyphAtlas_.get()};
        const std::span<const trace2d::text::TextFontAtlasRef> atlases(&atlasRef, 1U);
        trace2d::text::TextLayoutOptions options{};
        options.wrapMode = trace2d::text::TextWrapMode::None;
        const auto layoutResult = textLayout_->LayoutUtf8(atlases, text, options);
        if (!layoutResult.Succeeded())
            throw std::runtime_error{"Nightfall measured text layout failed for: " + std::string{text}};
        const std::uint64_t revisionAfter = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
        if (revisionAfter != revisionBefore || revisionAfter != frameGlyphRevision_)
        {
            throw std::runtime_error{
                "Nightfall text introduced an unprepared glyph after frame begin: \"" +
                std::string{text} + "\". Add the literal/character to the deterministic prewarm corpus."};
        }

        const auto metrics = textLayout_->Metrics();
        const float width = static_cast<float>(metrics.contentWidth26_6) / (64.0F * pixelsPerUnit);
        const float originX = centered ? x - width * 0.5F : x;

        trace2d::text::TextPresentationConfig2D config{};
        config.origin = {originX, topY};
        config.pixelsPerUnit = pixelsPerUnit;
        config.painterLayer = layer;
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
                "Nightfall measured text presentation failed: " +
                std::string{trace2d::text::ToString(status.error)} +
                " text=\"" + std::string{text} + "\""};
        }

        Rect bounds{};
        bool hasBounds = false;
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
            const Rect glyphBounds = BoundsOfQuad(draw.presentation.quad);
            bounds = hasBounds ? UnionRect(bounds, glyphBounds) : glyphBounds;
            hasBounds = true;
            draws_.push_back(draw);
        }
        order_ += static_cast<std::uint64_t>(textLayout_->Glyphs().size()) + 1U;
        return hasBounds ? bounds : Rect{x, topY, x, topY};
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
                static_cast<void>(AddVisual(
                    alternate ? floorsB_[stage] : floorsA_[stage],
                    {static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F},
                    {1.0F, 1.0F}, 0.0F, White, 1.0F,
                    trace2d::render::SpriteBlendMode::Normal, -20));
            }
        }
    }

    void DrawProductBackdrop()
    {
        DrawFloor({}, 0U);
        static_cast<void>(AddPanel(0.0F, 0.0F, CoreWidth, CoreHeight, Ink, 0.94F, 20));
        static_cast<void>(AddPanel(0.0F, 4.23F, CoreWidth, 0.54F, {0.02F, 0.03F, 0.06F, 1.0F}, 0.98F, 25));
        static_cast<void>(AddPanel(0.0F, -4.23F, CoreWidth, 0.54F, {0.02F, 0.03F, 0.06F, 1.0F}, 0.98F, 25));
    }

    [[nodiscard]] Rect DrawHeader(const std::string_view eyebrow, const std::string_view title)
    {
        const Rect safe{-7.45F, 2.55F, 7.45F, 4.12F};
        const Rect eyebrowBounds = AddText(eyebrow, -7.15F, 3.96F, TinyPpu, Cyan, false, 70);
        const Rect titleBounds = AddText(title, -7.15F, 3.47F, HeadingPpu, White, false, 70);
        RequireInside("header.eyebrow", eyebrowBounds, "header.safe", safe, 0.02F);
        RequireInside("header.title", titleBounds, "header.safe", safe, 0.02F);
        RequireDisjoint("header.eyebrow", eyebrowBounds, "header.title", titleBounds, 0.04F);
        static_cast<void>(AddPanel(-5.15F, 2.73F, 4.0F, 0.055F, Cyan, 0.85F, 43));
        return safe;
    }

    void DrawFooter(const std::string_view help)
    {
        const Rect footerSafe{-7.55F, -4.35F, 7.55F, -3.72F};
        const Rect bounds = AddText(help, 0.0F, -3.93F, TinyPpu, Dim, true, 75);
        RequireInside("footer.help", bounds, "footer.safe", footerSafe, 0.02F);
    }

    void DrawMainMenu()
    {
        // The title stack has 0.23 world-unit visual clearance above the panels;
        // actual panel collision is enforced independently by RequireDisjoint below.
        const Rect titleSafe{-7.45F, 2.30F, 7.45F, 4.12F};
        const Rect nightfall = AddText("NIGHTFALL", -7.15F, 3.98F, TitlePpu, White, false, 72);
        const Rect survivors = AddText("SURVIVORS", -7.12F, nightfall.bottom - 0.08F, BodyPpu, Cyan, false, 72);
        const Rect byline = AddText("Trace2D ORIGINAL SURVIVAL", -7.12F, survivors.bottom - 0.06F, MicroPpu, Dim, false, 72);
        RequireInside("main.title", nightfall, "main.titleSafe", titleSafe, 0.02F);
        RequireInside("main.subtitle", survivors, "main.titleSafe", titleSafe, 0.02F);
        RequireInside("main.byline", byline, "main.titleSafe", titleSafe, 0.02F);
        RequireDisjoint("main.title", nightfall, "main.subtitle", survivors, 0.03F);
        RequireDisjoint("main.subtitle", survivors, "main.byline", byline, 0.03F);

        const Rect menuPanel = AddPanel(-5.05F, -0.45F, 4.45F, 5.20F, Panel, 0.98F, 40);
        const Rect infoPanel = AddPanel(2.15F, -0.45F, 8.85F, 5.20F, Panel, 0.98F, 40);
        RequireDisjoint("main.titleStack", UnionRect(nightfall, byline), "main.menuPanel", menuPanel, 0.08F);
        RequireDisjoint("main.titleStack", UnionRect(nightfall, byline), "main.infoPanel", infoPanel, 0.08F);

        const Rect menuLabel = AddText("MENU", -6.86F, 1.82F, TinyPpu, Dim, false, 72);
        RequireInside("main.menuLabel", menuLabel, "main.menuPanel", menuPanel, 0.20F);
        constexpr std::array<std::string_view, 5U> Menu{
            "게임 시작", "프로필", "업적", "설정", "종료"};
        for (std::size_t i = 0U; i < Menu.size(); ++i)
        {
            const float y = 1.18F - static_cast<float>(i) * 0.88F;
            const bool selected = product_.Selection() == i;
            if (selected)
            {
                static_cast<void>(AddPanel(-5.05F, y - 0.12F, 3.88F, 0.68F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 43));
                static_cast<void>(AddPanel(-6.95F, y - 0.12F, 0.08F, 0.68F, Cyan, 1.0F, 44));
            }
            const Rect item = AddText(Menu[i], -6.60F, y + 0.13F, BodyPpu, selected ? White : Muted, false, 74);
            RequireInside("main.menuItem", item, "main.menuPanel", menuPanel, 0.18F);
        }

        const std::size_t characterIndex = static_cast<std::size_t>(product_.SelectedCharacter());
        const auto& character = NightfallProduct::Character(product_.SelectedCharacter());
        const Rect characterHeader = AddText("현재 캐릭터", -1.86F, 1.82F, TinyPpu, Dim, false, 72);
        const Rect characterName = AddText(character.nameKo, -1.86F, 1.28F, HeadingPpu, CharacterAccent(characterIndex), false, 72);
        const Rect characterRole = AddText(character.subtitleKo, -1.86F, characterName.bottom - 0.08F, MicroPpu, Muted, false, 72);
        const float bob = std::sin(static_cast<float>(game_.FrameCounter() % 120U) * 0.0523598776F) * 0.04F;
        const Rect hero = AddVisual(
            heroes_[characterIndex], {-0.28F, -1.18F + bob}, {2.65F, 2.65F}, 0.0F,
            White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 45);

        const auto& profile = product_.PlayerProfile();
        const Rect recordHeader = AddText("전투 기록", 2.65F, 1.82F, TinyPpu, Dim, false, 72);
        const Rect stars = AddText("별가루  " + std::to_string(profile.stars), 2.65F, 1.23F, BodyPpu, Gold, false, 72);
        const Rect runs = AddText("플레이  " + std::to_string(profile.totalRuns) + "회", 2.65F, 0.54F, SmallPpu, Muted, false, 72);
        const Rect clears = AddText("클리어  " + std::to_string(profile.totalClears) + "회", 2.65F, -0.05F, SmallPpu, Muted, false, 72);
        const Rect kills = AddText("누적 처치  " + std::to_string(profile.totalKills), 2.65F, -0.64F, SmallPpu, Muted, false, 72);
        const Rect level = AddText("최고 레벨  " + std::to_string(profile.bestLevel), 2.65F, -1.23F, SmallPpu, Muted, false, 72);

        RequireInside("main.characterHeader", characterHeader, "main.infoPanel", infoPanel, 0.20F);
        RequireInside("main.characterName", characterName, "main.infoPanel", infoPanel, 0.20F);
        RequireInside("main.characterRole", characterRole, "main.infoPanel", infoPanel, 0.20F);
        RequireInside("main.hero", hero, "main.infoPanel", infoPanel, 0.20F);
        RequireInside("main.recordHeader", recordHeader, "main.infoPanel", infoPanel, 0.20F);
        RequireInside("main.stats", UnionRect(stars, level), "main.infoPanel", infoPanel, 0.20F);
        RequireDisjoint("main.characterName", characterName, "main.characterRole", characterRole, 0.03F);
        RequireDisjoint("main.characterCopy", UnionRect(characterHeader, characterRole), "main.hero", hero, 0.10F);
        RequireDisjoint("main.hero", hero, "main.stats", UnionRect(recordHeader, level), 0.12F);
        RequireDisjoint("main.stat.stars", stars, "main.stat.runs", runs, 0.03F);
        RequireDisjoint("main.stat.runs", runs, "main.stat.clears", clears, 0.03F);
        RequireDisjoint("main.stat.clears", clears, "main.stat.kills", kills, 0.03F);
        RequireDisjoint("main.stat.kills", kills, "main.stat.level", level, 0.03F);

        DrawFooter("W/S 선택  ·  ENTER 확인  ·  ESC 종료");
    }

    void DrawProfile()
    {
        static_cast<void>(DrawHeader("PLAYER PROFILE", "프로필"));
        const Rect panel = AddPanel(0.0F, -0.33F, 13.5F, 5.65F, Panel, 0.98F, 40);
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
            const float x = rightColumn ? 0.72F : -5.62F;
            const float y = 1.72F - static_cast<float>(row) * 1.02F;
            const Rect label = AddText(rows[i].first, x, y, TinyPpu, Dim, false, 72);
            const Rect value = AddText(rows[i].second, x, y - 0.40F, BodyPpu, i == 5U ? Gold : White, false, 72);
            RequireInside("profile.label", label, "profile.panel", panel, 0.20F);
            RequireInside("profile.value", value, "profile.panel", panel, 0.20F);
            RequireDisjoint("profile.label", label, "profile.value", value, 0.02F);
        }
        DrawFooter("ENTER 또는 ESC  ·  돌아가기");
    }

    void DrawAchievements()
    {
        static_cast<void>(DrawHeader("ACHIEVEMENTS", "업적"));
        const Rect panel = AddPanel(0.0F, -0.30F, 14.0F, 5.68F, Panel, 0.98F, 40);
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            const auto id = static_cast<AchievementId>(i);
            const auto& definition = NightfallProduct::Achievement(id);
            const bool unlocked = product_.IsAchievementUnlocked(id);
            const bool rightColumn = i >= 3U;
            const std::size_t row = rightColumn ? i - 3U : i;
            const float x = rightColumn ? 3.52F : -3.52F;
            const float y = 1.50F - static_cast<float>(row) * 1.55F;
            const Rect card = AddPanel(x, y, 6.25F, 1.20F, unlocked ? PanelRaised : Panel, 1.0F, 42);
            static_cast<void>(AddPanel(x - 3.03F, y, 0.10F, 1.20F, unlocked ? Green : Dim, 1.0F, 43));
            const Rect state = AddText(unlocked ? "달성" : "미달성", x - 2.68F, y + 0.32F, MicroPpu, unlocked ? Green : Dim, false, 72);
            const Rect name = AddText(definition.nameKo, x - 1.62F, y + 0.34F, SmallPpu, unlocked ? White : Muted, false, 72);
            const Rect description = AddText(definition.descriptionKo, x - 2.68F, y - 0.18F, MicroPpu, Dim, false, 72);
            RequireInside("achievement.state", state, "achievement.card", card, 0.14F);
            RequireInside("achievement.name", name, "achievement.card", card, 0.14F);
            RequireInside("achievement.description", description, "achievement.card", card, 0.14F);
            RequireDisjoint("achievement.name", name, "achievement.description", description, 0.02F);
        }
        static_cast<void>(panel);
        DrawFooter("ENTER 또는 ESC  ·  돌아가기");
    }

    void DrawSettings()
    {
        static_cast<void>(DrawHeader("SETTINGS", "설정"));
        const Rect panel = AddPanel(0.0F, -0.28F, 10.6F, 5.40F, Panel, 0.98F, 40);
        const auto& profile = product_.PlayerProfile();
        const std::string volume = profile.sfxVolumeStep == 0U ? "0%" : (profile.sfxVolumeStep == 1U ? "50%" : "100%");
        const std::array<std::string, 3U> labels{{
            "효과음 볼륨   " + volume,
            std::string{"화면 흔들림   "} + (profile.cameraShakeEnabled ? "켜짐" : "꺼짐"),
            "돌아가기",
        }};
        constexpr std::array<float, 3U> Y{1.12F, -0.02F, -1.65F};
        for (std::size_t i = 0U; i < labels.size(); ++i)
        {
            const bool selected = product_.Selection() == i;
            if (selected)
            {
                static_cast<void>(AddPanel(0.0F, Y[i] - 0.12F, 8.70F, 0.80F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 43));
                static_cast<void>(AddPanel(-4.30F, Y[i] - 0.12F, 0.10F, 0.80F, Cyan, 1.0F, 44));
            }
            const Rect item = AddText(labels[i], -3.82F, Y[i] + 0.14F, BodyPpu, selected ? White : Muted, false, 72);
            RequireInside("settings.item", item, "settings.panel", panel, 0.24F);
        }
        DrawFooter("W/S 선택  ·  A/D 또는 ←/→ 조절  ·  ENTER 확인");
    }

    void DrawCharacterSelect()
    {
        static_cast<void>(DrawHeader("CHOOSE YOUR HUNTER", "캐릭터 선택"));
        constexpr std::array<float, 3U> X{-5.10F, 0.0F, 5.10F};
        std::array<Rect, 3U> cards{};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto id = static_cast<CharacterId>(i);
            const auto& definition = NightfallProduct::Character(id);
            const bool unlocked = product_.IsCharacterUnlocked(id);
            const bool selected = product_.Selection() == i;
            const Color accent = CharacterAccent(i);
            cards[i] = AddPanel(X[i], 0.08F, 4.40F, 4.55F, selected ? PanelRaised : Panel, 0.98F, 41);
            static_cast<void>(AddPanel(X[i], 2.31F, 4.40F, 0.08F, unlocked ? accent : Dim, 1.0F, 42));
            const Rect hero = AddVisual(heroes_[i], {X[i], 1.15F}, {2.04F, 2.04F}, 0.0F,
                unlocked ? White : Dim, unlocked ? 1.0F : 0.52F,
                trace2d::render::SpriteBlendMode::Normal, 45);
            const Rect name = AddText(unlocked ? definition.nameKo : "잠김", X[i], -0.20F, BodyPpu,
                unlocked ? accent : Red, true, 73);
            const Rect role = AddText(definition.subtitleKo, X[i], -0.72F, MicroPpu, unlocked ? Muted : Dim, true, 73);
            RequireInside("character.hero", hero, "character.card", cards[i], 0.16F);
            RequireInside("character.name", name, "character.card", cards[i], 0.16F);
            RequireInside("character.role", role, "character.card", cards[i], 0.16F);
            RequireDisjoint("character.hero", hero, "character.name", name, 0.08F);
            RequireDisjoint("character.name", name, "character.role", role, 0.02F);
            if (!unlocked)
            {
                const Rect locked = AddText("해금 조건 필요", X[i], -1.28F, MicroPpu, Red, true, 73);
                RequireInside("character.lock", locked, "character.card", cards[i], 0.16F);
                RequireDisjoint("character.role", role, "character.lock", locked, 0.02F);
            }
        }
        RequireDisjoint("character.card0", cards[0], "character.card1", cards[1], 0.16F);
        RequireDisjoint("character.card1", cards[1], "character.card2", cards[2], 0.16F);

        const std::size_t selectedIndex = std::min<std::size_t>(product_.Selection(), 2U);
        const auto selectedId = static_cast<CharacterId>(selectedIndex);
        const bool selectedUnlocked = product_.IsCharacterUnlocked(selectedId);
        const Rect detailPanel = AddPanel(0.0F, -2.75F, 13.65F, 0.72F, Panel, 0.98F, 42);
        const Rect detail = AddText(
            selectedUnlocked ? NightfallProduct::Character(selectedId).descriptionKo : "해금 후 상세 능력을 확인할 수 있습니다.",
            0.0F, -2.51F, MicroPpu, selectedUnlocked ? Muted : Dim, true, 74);
        RequireInside("character.detail", detail, "character.detailPanel", detailPanel, 0.10F);
        RequireDisjoint("character.cards", UnionRect(cards[0], cards[2]), "character.detailPanel", detailPanel, 0.12F);
        DrawFooter("A/D 또는 W/S 선택  ·  ENTER 확인  ·  ESC 뒤로");
    }

    void DrawStageSelect()
    {
        static_cast<void>(DrawHeader("CHOOSE THE NIGHT", "스테이지 선택"));
        constexpr std::array<float, 3U> X{-5.10F, 0.0F, 5.10F};
        std::array<Rect, 3U> cards{};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto id = static_cast<StageId>(i);
            const auto& definition = NightfallProduct::Stage(id);
            const bool unlocked = product_.IsStageUnlocked(id);
            const bool selected = product_.Selection() == i;
            const Color accent = StageAccent(i);
            cards[i] = AddPanel(X[i], 0.08F, 4.40F, 4.55F, selected ? PanelRaised : Panel, 0.98F, 41);
            static_cast<void>(AddPanel(X[i], 2.31F, 4.40F, 0.08F, unlocked ? accent : Dim, 1.0F, 42));
            const Rect preview = AddPanel(X[i], 1.18F, 3.50F, 1.45F, {0.025F, 0.03F, 0.05F, 1.0F}, 1.0F, 43);
            for (int py = 0; py < 2; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    const Visual& tile = ((px + py) & 1) == 0 ? floorsA_[i] : floorsB_[i];
                    static_cast<void>(AddVisual(tile,
                        {X[i] - 1.3125F + static_cast<float>(px) * 0.875F,
                         0.86F + static_cast<float>(py) * 0.64F},
                        {0.875F, 0.64F}, 0.0F, unlocked ? White : Dim,
                        unlocked ? 1.0F : 0.50F,
                        trace2d::render::SpriteBlendMode::Normal, 44));
                }
            }
            const Rect name = AddText(unlocked ? definition.nameKo : "잠김", X[i], -0.02F, BodyPpu,
                unlocked ? accent : Red, true, 73);
            const Rect subtitle = AddText(definition.subtitleKo, X[i], -0.68F, MicroPpu, unlocked ? White : Dim, true, 73);
            RequireInside("stage.preview", preview, "stage.card", cards[i], 0.16F);
            RequireInside("stage.name", name, "stage.card", cards[i], 0.16F);
            RequireInside("stage.subtitle", subtitle, "stage.card", cards[i], 0.16F);
            RequireDisjoint("stage.preview", preview, "stage.name", name, 0.06F);
            RequireDisjoint("stage.name", name, "stage.subtitle", subtitle, 0.02F);
        }
        RequireDisjoint("stage.card0", cards[0], "stage.card1", cards[1], 0.16F);
        RequireDisjoint("stage.card1", cards[1], "stage.card2", cards[2], 0.16F);

        const std::size_t selectedIndex = std::min<std::size_t>(product_.Selection(), 2U);
        const auto selectedStage = static_cast<StageId>(selectedIndex);
        const bool selectedUnlocked = product_.IsStageUnlocked(selectedStage);
        const Rect detailPanel = AddPanel(0.0F, -2.72F, 13.65F, 0.68F, Panel, 0.98F, 42);
        const Rect detail = AddText(
            selectedUnlocked ? NightfallProduct::Stage(selectedStage).descriptionKo : "이전 스테이지를 클리어하면 해금됩니다.",
            0.0F, -2.50F, MicroPpu, selectedUnlocked ? Muted : Dim, true, 74);
        RequireInside("stage.detail", detail, "stage.detailPanel", detailPanel, 0.09F);
        RequireDisjoint("stage.cards", UnionRect(cards[0], cards[2]), "stage.detailPanel", detailPanel, 0.12F);
        const auto selectedCharacter = product_.SelectedCharacter();
        const Rect equipped = AddText(
            "현재 장착  ·  " + std::string{NightfallProduct::Character(selectedCharacter).nameKo},
            0.0F, -3.20F, MicroPpu, Gold, true, 74);
        RequireDisjoint("stage.detailPanel", detailPanel, "stage.equipped", equipped, 0.06F);
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
            static_cast<void>(AddVisual(particle_, gem.position, {pulse, pulse}, 0.0F, Moon, 1.0F,
                trace2d::render::SpriteBlendMode::Additive, 2));
        }

        for (const auto& enemy : game_.Enemies())
        {
            if (!enemy.active) continue;
            const std::size_t kind = static_cast<std::size_t>(enemy.kind);
            const float scale = kind == 1U ? 1.28F : (kind == 2U ? 0.86F : 1.0F);
            const Color tint = enemy.hitFlashFrames > 0U ? Color{1.0F, 0.86F, 0.38F, 1.0F} : White;
            const float bob = std::sin(phase * 1.4F + static_cast<float>(enemy.stableId) * 0.31F) * 0.035F;
            static_cast<void>(AddVisual(enemies_[std::min<std::size_t>(kind, 2U)],
                {enemy.position.x, enemy.position.y + bob}, {scale, scale}, 0.0F, tint, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, 5));
        }

        for (const auto& projectile : game_.Projectiles())
        {
            if (!projectile.active) continue;
            static_cast<void>(AddVisual(skills_[0], projectile.position, {0.42F, 0.42F},
                std::atan2(projectile.velocity.y, projectile.velocity.x), Cyan, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, 9));
            static_cast<void>(AddVisual(particle_, projectile.position, {0.18F, 0.18F}, 0.0F, Cyan, 0.70F,
                trace2d::render::SpriteBlendMode::Additive, 8));
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
            static_cast<void>(AddVisual(particle_, effect.position, {scale, scale}, t * Pi, tint, 1.0F - t,
                trace2d::render::SpriteBlendMode::Additive, 11));
        }

        const std::uint32_t orbitCount = std::min<std::uint32_t>(game_.OrbitLevel(), 4U);
        for (std::uint32_t blade = 0U; blade < orbitCount; ++blade)
        {
            const float angle = game_.ElapsedSeconds() * 3.4F +
                2.0F * Pi * static_cast<float>(blade) / static_cast<float>(orbitCount);
            const float radius = 1.25F + static_cast<float>(orbitCount) * 0.08F;
            static_cast<void>(AddVisual(skills_[2],
                {playerPosition.x + std::cos(angle) * radius,
                 playerPosition.y + std::sin(angle) * radius},
                {0.62F, 0.62F}, angle, Gold, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, 13));
        }

        const float heroBob = (game_.MoveIntent().x != 0.0F || game_.MoveIntent().y != 0.0F)
            ? std::sin(phase * 2.4F) * 0.055F
            : std::sin(phase) * 0.025F;
        const float facingScale = game_.Facing().x < 0.0F ? -1.30F : 1.30F;
        const Color heroTint = game_.PlayerFlashFrames() > 0U && (game_.FrameCounter() & 1U) != 0U
            ? Color{1.0F, 0.45F, 0.45F, 1.0F}
            : White;
        static_cast<void>(AddVisual(heroes_[characterIndex],
            {playerPosition.x, playerPosition.y + heroBob}, {facingScale, 1.30F}, 0.0F,
            heroTint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 15));
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

        static_cast<void>(AddPanel(center.x, top - 0.02F, 15.25F, 0.62F, Ink, 0.92F, 30));
        static_cast<void>(AddPanel(center.x - 5.60F + 1.55F * healthRatio, top - 0.02F,
            3.10F * healthRatio, 0.10F, Green, 1.0F, 31));
        static_cast<void>(AddText("체력 " + std::to_string(game_.Health()) + "/" + std::to_string(game_.MaximumHealth()),
            center.x - 5.55F, top + 0.19F, TinyPpu, White, true, 66));
        const float remaining = std::max(0.0F, game_.RunDurationSeconds() - game_.ElapsedSeconds());
        static_cast<void>(AddText(FormatTime(remaining), center.x, top + 0.19F, BodyPpu, Cyan, true, 66));
        static_cast<void>(AddText("레벨 " + std::to_string(game_.Level()) + "  ·  처치 " + std::to_string(game_.KillCount()),
            center.x + 5.55F, top + 0.19F, TinyPpu, White, true, 66));

        static_cast<void>(AddPanel(center.x, bottom, 15.0F, 0.16F, PanelRaised, 1.0F, 31));
        if (xpRatio > 0.0F)
        {
            static_cast<void>(AddPanel(center.x - 7.5F + 7.5F * xpRatio, bottom,
                15.0F * xpRatio, 0.09F, Cyan, 1.0F, 32));
        }

        const std::array<std::uint32_t, 3U> levels{game_.RapidLevel(), game_.MightLevel(), game_.OrbitLevel()};
        const std::array<std::string_view, 3U> names{"연사", "화력", "궤도"};
        constexpr std::array<float, 3U> offsets{-4.2F, 0.0F, 4.2F};
        const std::array<Color, 3U> accents{Cyan, Ember, Gold};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const float x = center.x + offsets[i];
            static_cast<void>(AddPanel(x, bottom + 0.48F, 3.35F, 0.64F, Panel, 0.92F, 33));
            static_cast<void>(AddVisual(skills_[i], {x - 1.34F, bottom + 0.48F}, {0.48F, 0.48F}, 0.0F,
                White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 34));
            static_cast<void>(AddText(std::string{names[i]} + "  " + std::to_string(levels[i]),
                x + 0.20F, bottom + 0.67F, TinyPpu, accents[i], true, 67));
        }
    }

    void DrawLevelUp(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        static_cast<void>(AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.88F, 50));
        static_cast<void>(AddText("LEVEL UP", center.x, center.y + 3.62F, TinyPpu, Cyan, true, 73));
        static_cast<void>(AddText("레벨 업", center.x, center.y + 3.16F, HeadingPpu, White, true, 73));
        static_cast<void>(AddText("Q / E / F 중 하나를 선택하세요", center.x, center.y + 2.47F, TinyPpu, Muted, true, 73));

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
            const Rect card = AddPanel(center.x + X[i], center.y - 0.25F, 4.12F, 4.65F, PanelRaised, 0.99F, 51);
            static_cast<void>(AddPanel(center.x + X[i], center.y + 2.03F, 4.12F, 0.08F, accents[i], 1.0F, 52));
            const Rect key = AddText(keys[i], center.x + X[i] - 1.52F, center.y + 1.55F, BodyPpu, accents[i], false, 74);
            const Rect icon = AddVisual(skills_[i], {center.x + X[i], center.y + 0.90F}, {1.25F, 1.25F}, 0.0F,
                White, 1.0F, trace2d::render::SpriteBlendMode::Normal, 55);
            const Rect name = AddText(names[i], center.x + X[i], center.y + 0.03F, BodyPpu, accents[i], true, 74);
            const Rect line1 = AddText(lines1[i], center.x + X[i], center.y - 0.66F, TinyPpu, White, true, 74);
            const Rect line2 = AddText(lines2[i], center.x + X[i], center.y - 1.20F, MicroPpu, Muted, true, 74);
            RequireInside("level.key", key, "level.card", card, 0.12F);
            RequireInside("level.icon", icon, "level.card", card, 0.12F);
            RequireInside("level.name", name, "level.card", card, 0.12F);
            RequireInside("level.line1", line1, "level.card", card, 0.12F);
            RequireInside("level.line2", line2, "level.card", card, 0.12F);
        }
    }

    void DrawPause(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        static_cast<void>(AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.82F, 50));
        const Rect panel = AddPanel(center.x, center.y, 6.80F, 4.15F, Panel, 0.99F, 51);
        const Rect paused = AddText("PAUSED", center.x, center.y + 1.47F, TinyPpu, Cyan, true, 73);
        const Rect title = AddText("일시정지", center.x, center.y + 0.98F, HeadingPpu, White, true, 73);
        RequireInside("pause.paused", paused, "pause.panel", panel, 0.18F);
        RequireInside("pause.title", title, "pause.panel", panel, 0.18F);
        RequireDisjoint("pause.paused", paused, "pause.title", title, 0.03F);
        constexpr std::array<std::string_view, 2U> items{"계속하기", "메인 메뉴"};
        for (std::size_t i = 0U; i < 2U; ++i)
        {
            const float y = center.y - 0.05F - static_cast<float>(i) * 1.02F;
            const bool selected = product_.Selection() == i;
            if (selected)
                static_cast<void>(AddPanel(center.x, y - 0.12F, 4.75F, 0.76F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 53));
            const Rect item = AddText(items[i], center.x, y + 0.14F, BodyPpu, selected ? White : Muted, true, 74);
            RequireInside("pause.item", item, "pause.panel", panel, 0.18F);
        }
    }

    void DrawResult(const trace2d::application::GameContext& context)
    {
        const auto* const player = context.Scene().TryGet(game_.Player());
        if (player == nullptr) return;
        const auto center = player->LocalTransform().position;
        const auto& summary = product_.LastRunSummary();
        static_cast<void>(AddPanel(center.x, center.y, CoreWidth, CoreHeight, Ink, 0.90F, 50));
        const Rect panel = AddPanel(center.x, center.y, 11.20F, 8.40F, Panel, 0.99F, 51);
        const Rect result = AddText(summary.cleared ? "CLEAR" : "GAME OVER", center.x, center.y + 3.00F,
            TinyPpu, summary.cleared ? Gold : Red, true, 74);
        const Rect title = AddText(summary.cleared ? "스테이지 클리어" : "패배", center.x, center.y + 2.48F,
            HeadingPpu, summary.cleared ? White : Red, true, 74);
        const Rect stat = AddText("처치 " + std::to_string(summary.kills) + "  ·  레벨 " + std::to_string(summary.level) +
            "  ·  생존 " + FormatTime(summary.elapsedSeconds),
            center.x, center.y + 1.66F, TinyPpu, White, true, 74);
        const Rect reward = AddText("별가루 획득  +" + std::to_string(summary.starsEarned),
            center.x, center.y + 1.05F, BodyPpu, Gold, true, 74);
        RequireInside("result.result", result, "result.panel", panel, 0.18F);
        RequireInside("result.title", title, "result.panel", panel, 0.18F);
        RequireInside("result.stat", stat, "result.panel", panel, 0.18F);
        RequireInside("result.reward", reward, "result.panel", panel, 0.18F);
        RequireDisjoint("result.result", result, "result.title", title, 0.03F);
        RequireDisjoint("result.title", title, "result.stat", stat, 0.05F);
        RequireDisjoint("result.stat", stat, "result.reward", reward, 0.03F);

        float unlockY = center.y + 0.30F;
        for (std::size_t i = 0U; i < NightfallProduct::CharacterCount(); ++i)
        {
            if ((summary.newlyUnlockedCharactersMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            const Rect unlockLine = AddText("새 캐릭터  ·  " + std::string{NightfallProduct::Character(static_cast<CharacterId>(i)).nameKo},
                center.x, unlockY, TinyPpu, CharacterAccent(i), true, 74);
            RequireInside("result.unlock.character", unlockLine, "result.panel", panel, 0.18F);
            unlockY -= 0.40F;
        }
        for (std::size_t i = 0U; i < NightfallProduct::StageCount(); ++i)
        {
            if ((summary.newlyUnlockedStagesMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            const Rect unlockLine = AddText("새 스테이지  ·  " + std::string{NightfallProduct::Stage(static_cast<StageId>(i)).nameKo},
                center.x, unlockY, TinyPpu, StageAccent(i), true, 74);
            RequireInside("result.unlock.stage", unlockLine, "result.panel", panel, 0.18F);
            unlockY -= 0.40F;
        }
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            if ((summary.newlyUnlockedAchievementsMask & (1U << static_cast<std::uint32_t>(i))) == 0U) continue;
            const Rect unlockLine = AddText("새 업적  ·  " + std::string{NightfallProduct::Achievement(static_cast<AchievementId>(i)).nameKo},
                center.x, unlockY, TinyPpu, Green, true, 74);
            RequireInside("result.unlock.achievement", unlockLine, "result.panel", panel, 0.18F);
            unlockY -= 0.38F;
        }

        constexpr std::array<std::string_view, 2U> items{"다시 도전", "메인 메뉴"};
        for (std::size_t i = 0U; i < 2U; ++i)
        {
            const float x = center.x + (i == 0U ? -2.35F : 2.35F);
            const bool selected = product_.Selection() == i;
            if (selected)
                static_cast<void>(AddPanel(x, center.y - 3.24F, 3.90F, 0.76F, {0.10F, 0.28F, 0.44F, 1.0F}, 1.0F, 53));
            static_cast<void>(AddText(items[i], x, center.y - 3.00F, BodyPpu, selected ? White : Muted, true, 74));
        }
        static_cast<void>(AddText("W/S 선택  ·  ENTER 확인", center.x, center.y - 3.68F, TinyPpu, Dim, true, 74));
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
    std::uint64_t frameGlyphRevision_{0U};
    std::vector<trace2d::render::SpritePresentationRenderData> textScratch_{};
    std::vector<trace2d::render::SpritePresentationRenderData> draws_{};

    trace2d::render::OrthographicCamera camera_{};
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
        throw std::runtime_error{"Nightfall measured product presentation is unavailable."};
    self->impl_->Present(context);
}
