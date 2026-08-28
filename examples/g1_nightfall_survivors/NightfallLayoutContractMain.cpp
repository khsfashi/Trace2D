#include "NightfallProduct.hpp"
#include "NightfallSurvivorsGame.hpp"

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/assets/TextureAssets.hpp>
#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>
#include <trace2d/scene/SpriteTransform2D.hpp>
#include <trace2d/text/Text.hpp>
#include <trace2d/text/TextLayout.hpp>
#include <trace2d/text/TextPresentation2D.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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

#ifndef TRACE2D_G1_RUNTIME_DIR
#define TRACE2D_G1_RUNTIME_DIR "runtime"
#endif

namespace
{
using CharacterId = NightfallProduct::CharacterId;
using StageId = NightfallProduct::StageId;
using AchievementId = NightfallProduct::AchievementId;
using Color = trace2d::render::SpriteLinearRgba;

constexpr float CoreWidth = NightfallSurvivorsGame::CameraHorizontalSize;
constexpr float CoreHeight = NightfallSurvivorsGame::CameraVerticalSize;
constexpr float TitlePpu = 44.0F;
constexpr float HeadingPpu = 54.0F;
constexpr float BodyPpu = 68.0F;
constexpr float SmallPpu = 84.0F;
constexpr float TinyPpu = 104.0F;
constexpr float MicroPpu = 124.0F;

constexpr Color White{0.96F, 0.97F, 1.0F, 1.0F};

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
    float pixelsPerUnit{16.0F};
};

[[nodiscard]] Rect PanelRect(const float x, const float y, const float width, const float height) noexcept
{
    return {x - width * 0.5F, y - height * 0.5F, x + width * 0.5F, y + height * 0.5F};
}

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
    const std::string_view scenario,
    const std::string_view childId,
    const Rect child,
    const std::string_view parentId,
    const Rect parent,
    const float margin = 0.0F)
{
    if (Contains(parent, child, margin)) return;
    throw std::runtime_error{
        "Nightfall headless layout violation [" + std::string{scenario} + "]: " +
        std::string{childId} + " escapes " + std::string{parentId} +
        " child=" + RectString(child) + " parent=" + RectString(parent)};
}

void RequireDisjoint(
    const std::string_view scenario,
    const std::string_view leftId,
    const Rect left,
    const std::string_view rightId,
    const Rect right,
    const float gap = 0.0F)
{
    if (!Intersects(left, right, gap)) return;
    throw std::runtime_error{
        "Nightfall headless layout violation [" + std::string{scenario} + "]: " +
        std::string{leftId} + " intersects " + std::string{rightId} +
        " first=" + RectString(left) + " second=" + RectString(right)};
}

[[nodiscard]] std::vector<std::uint8_t> ReadBinary(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input) throw std::runtime_error{"Nightfall contract could not read: " + path.string()};
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
}

class Contract final
{
public:
    explicit Contract(std::filesystem::path runtimeRoot)
        : runtimeRoot_{std::move(runtimeRoot)}
        , resources_{runtimeRoot_}
        , textureCache_{runtimeRoot_}
    {
        const std::array<std::string, 3U> heroRefs{
            "textures/hero-star.png", "textures/hero-ember.png", "textures/hero-moon.png"};
        const std::array<std::string, 3U> floorARefs{
            "textures/stage-moon-floor-a.png", "textures/stage-ember-floor-a.png", "textures/stage-astral-floor-a.png"};
        const std::array<std::string, 3U> floorBRefs{
            "textures/stage-moon-floor-b.png", "textures/stage-ember-floor-b.png", "textures/stage-astral-floor-b.png"};
        const std::array<std::string, 3U> skillRefs{
            "textures/skill-rapid.png", "textures/skill-might.png", "textures/skill-orbit.png"};
        for (std::size_t index = 0U; index < 3U; ++index)
        {
            heroes_[index] = LoadVisual(heroRefs[index], 16.0F);
            floorsA_[index] = LoadVisual(floorARefs[index], 16.0F);
            floorsB_[index] = LoadVisual(floorBRefs[index], 16.0F);
            skills_[index] = LoadVisual(skillRefs[index], 16.0F);
        }
        PrepareText();
    }

    void Run()
    {
        constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 3U> Viewports{{
            {960U, 540U},
            {1024U, 768U},
            {2560U, 1080U},
        }};
        for (const auto [width, height] : Viewports)
        {
            viewportWidth_ = width;
            viewportHeight_ = height;
            const std::string suffix = std::to_string(width) + "x" + std::to_string(height);
            CheckMain("main/default/" + suffix, 0U, false);
            CheckMain("main/large-profile/" + suffix, 2U, true);
            CheckProfile("profile/" + suffix);
            CheckAchievements("achievements/" + suffix);
            CheckSettings("settings/" + suffix);
            CheckCharacterSelect("characters/locked/" + suffix, false);
            CheckCharacterSelect("characters/unlocked/" + suffix, true);
            CheckStageSelect("stages/locked/" + suffix, false);
            CheckStageSelect("stages/unlocked/" + suffix, true);
            CheckHud("hud/" + suffix);
            CheckLevelUp("levelup/" + suffix);
            CheckPause("pause/" + suffix);
            CheckResult("result/normal/" + suffix, false);
            CheckResult("result/worst-unlocks/" + suffix, true);
        }
    }

    [[nodiscard]] std::size_t ScenarioCount() const noexcept
    {
        return scenarioCount_;
    }

private:
    [[nodiscard]] Visual LoadVisual(const std::string& reference, const float pixelsPerUnit)
    {
        const auto loaded = textureCache_.Load(reference);
        if (!loaded.Succeeded())
            throw std::runtime_error{"Nightfall contract could not load product asset: " + reference};

        Visual visual{};
        visual.pixelsPerUnit = pixelsPerUnit;
        visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
        visual.asset->id = reference + ".contract.sprite";
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
            throw std::runtime_error{"Nightfall contract could not resolve sprite region: " + reference};
        }
        return visual;
    }

    void PrepareText()
    {
        trace2d::assets::FontResource font{};
        font.canonicalBytes = ReadBinary(runtimeRoot_ / "fonts/Galmuri11-Bold.ttf");
        const auto fontPublished = resources_.PublishFont("fonts/Galmuri11-Bold.ttf", std::move(font));
        if (!fontPublished.Succeeded()) throw std::runtime_error{"Nightfall contract could not publish Galmuri font."};

        auto atlas = trace2d::text::PrepareGlyphAtlas(
            resources_, fontPublished.handle,
            trace2d::text::GlyphAtlasConfig{2048U, 2048U, 32U, 1U, 2048U});
        if (!atlas.Succeeded()) throw std::runtime_error{"Nightfall contract could not prepare glyph atlas."};
        glyphAtlas_ = std::move(atlas.atlas);

        std::string corpus =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 %/:+-.[]()·<>,←→"
            "게임 시작 프로필 업적 설정 종료 돌아가기 선택 확인 캐릭터 스테이지 잠김 해금 조건 필요 또는"
            "별가루 플레이 클리어 누적 처치 최고 레벨 최고 생존 총 횟수 효과음 볼륨 화면 흔들림 켜짐 꺼짐"
            "일시정지 계속하기 메인 메뉴 레벨 업 하나를 선택하세요 연사 화력 궤도 공격 간격 감소 투사체 공격력 증가"
            "주변 회전무기 최대 이후 체력 강화 체력 남은 시간 다시 도전 획득 새 달성 미달성 조작 이동 자동공격 재시작"
            "현재 장착 보유 전투 기록 모험 준비 완료 패배 회 분 초 상세 능력을 확인할 수 있습니다 이전 클리어하면 해금됩니다"
            "공격 방어 속도 장착 캐릭터선택 스테이지선택 생존자 첫 번째 밤 최종 결과 해금 후";
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
            throw std::runtime_error{"Nightfall contract could not prewarm glyph atlas."};
        fixedGlyphRevision_ = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);

        auto layout = trace2d::text::PrepareTextLayoutRun({1024U, 64U});
        if (!layout.Succeeded()) throw std::runtime_error{"Nightfall contract could not prepare text layout."};
        textLayout_ = std::move(layout.run);
        textScratch_.resize(1024U);

        const auto config = glyphAtlas_->Config();
        trace2d::assets::TextureResource texture{};
        texture.width = config.width;
        texture.height = config.height;
        texture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
        texture.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
        texture.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
        texture.retentionReason = "Nightfall headless layout contract glyph atlas";
        texture.canonicalRgba8.resize(static_cast<std::size_t>(config.width) * config.height * 4U);
        std::size_t requiredBytes = 0U;
        const auto written = trace2d::text::WriteGlyphAtlasRgba8(*glyphAtlas_, texture.canonicalRgba8, requiredBytes);
        if (!written.Succeeded() || requiredBytes != texture.canonicalRgba8.size())
            throw std::runtime_error{"Nightfall contract could not materialize glyph atlas."};
        const auto published = resources_.PublishTexture("generated/nightfall-contract-glyph-atlas.rgba8", std::move(texture));
        if (!published.Succeeded()) throw std::runtime_error{"Nightfall contract could not publish glyph atlas texture."};
        const auto status = trace2d::text::ResolveGlyphAtlasTextureBinding2D(
            *glyphAtlas_, resources_, published.handle, glyphBinding_);
        if (!status.Succeeded())
            throw std::runtime_error{"Nightfall contract could not bind glyph atlas: " + std::string{trace2d::text::ToString(status.error)}};
    }

    [[nodiscard]] Rect Text(
        const std::string_view value,
        const float x,
        const float topY,
        const float pixelsPerUnit,
        const bool centered = false)
    {
        const std::uint64_t before = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
        const trace2d::text::TextFontAtlasRef atlasRef{glyphAtlas_.get()};
        const std::span<const trace2d::text::TextFontAtlasRef> atlases(&atlasRef, 1U);
        trace2d::text::TextLayoutOptions options{};
        options.wrapMode = trace2d::text::TextWrapMode::None;
        const auto layoutStatus = textLayout_->LayoutUtf8(atlases, value, options);
        if (!layoutStatus.Succeeded())
            throw std::runtime_error{"Nightfall contract text layout failed for: " + std::string{value}};
        const std::uint64_t after = trace2d::text::GlyphAtlasPixelRevision(*glyphAtlas_);
        if (after != before || after != fixedGlyphRevision_)
        {
            throw std::runtime_error{
                "Nightfall headless glyph contract violation: unprepared text \"" + std::string{value} + "\""};
        }

        const auto metrics = textLayout_->Metrics();
        const float width = static_cast<float>(metrics.contentWidth26_6) / (64.0F * pixelsPerUnit);
        const float originX = centered ? x - width * 0.5F : x;
        trace2d::text::TextPresentationConfig2D config{};
        config.origin = {originX, topY};
        config.pixelsPerUnit = pixelsPerUnit;
        config.tint = White;
        config.opacity = 1.0F;
        config.sampler = trace2d::render::SpriteSamplerCompatibility::Nearest;

        std::size_t requiredCount = 0U;
        trace2d::text::TextPresentationMeasurement2D measurement{};
        const auto status = trace2d::text::BuildTextPresentation2D(
            *textLayout_, atlases,
            std::span<const trace2d::text::GlyphAtlasTextureBinding2D>(&glyphBinding_, 1U),
            config, textScratch_, requiredCount, measurement);
        if (!status.Succeeded() || requiredCount > textScratch_.size())
            throw std::runtime_error{"Nightfall contract text presentation failed for: " + std::string{value}};

        Rect bounds{};
        bool hasBounds = false;
        for (std::size_t index = 0U; index < requiredCount; ++index)
        {
            auto quad = textScratch_[index].presentation.quad;
            const auto flip = [topY](trace2d::render::SpriteDrawVertex& vertex) {
                vertex.position.y = 2.0F * topY - vertex.position.y;
            };
            flip(quad.topLeft);
            flip(quad.topRight);
            flip(quad.bottomRight);
            flip(quad.bottomLeft);
            const Rect glyph = BoundsOfQuad(quad);
            bounds = hasBounds ? UnionRect(bounds, glyph) : glyph;
            hasBounds = true;
        }
        return hasBounds ? bounds : Rect{x, topY, x, topY};
    }

    [[nodiscard]] Rect Sprite(
        const Visual& visual,
        const float x,
        const float y,
        const float scaleX,
        const float scaleY)
    {
        trace2d::scene::SpritePose2D pose{};
        pose.transform.position = {x, y};
        pose.transform.scale = {scaleX, scaleY};
        trace2d::render::SpriteAppearance2D appearance{};
        appearance.tint = White;
        appearance.opacity = 1.0F;
        appearance.sampling = trace2d::render::SpriteAppearanceSampling::Nearest;
        trace2d::render::SpritePresentation2D presentation{};
        if (!trace2d::render::BuildSpritePresentation2D(
                visual.selections[0], pose, visual.pixelsPerUnit, appearance, presentation).Succeeded())
        {
            throw std::runtime_error{"Nightfall contract sprite presentation failed."};
        }
        return BoundsOfQuad(presentation.quad);
    }

    void Header(const std::string_view scenario, const std::string_view eyebrow, const std::string_view title)
    {
        const Rect safe{-7.45F, 2.55F, 7.45F, 4.12F};
        const Rect eyebrowBounds = Text(eyebrow, -7.15F, 3.96F, TinyPpu);
        const Rect titleBounds = Text(title, -7.15F, 3.47F, HeadingPpu);
        RequireInside(scenario, "header.eyebrow", eyebrowBounds, "header.safe", safe, 0.02F);
        RequireInside(scenario, "header.title", titleBounds, "header.safe", safe, 0.02F);
        RequireDisjoint(scenario, "header.eyebrow", eyebrowBounds, "header.title", titleBounds, 0.04F);
    }

    void Footer(const std::string_view scenario, const std::string_view help)
    {
        const Rect safe{-7.55F, -4.35F, 7.55F, -3.72F};
        const Rect bounds = Text(help, 0.0F, -3.93F, TinyPpu, true);
        RequireInside(scenario, "footer.help", bounds, "footer.safe", safe, 0.02F);
    }

    void CheckMain(const std::string& scenario, const std::size_t characterIndex, const bool largeProfile)
    {
        ++scenarioCount_;
        const Rect titleSafe{-7.45F, 2.30F, 7.45F, 4.12F};
        const Rect nightfall = Text("NIGHTFALL", -7.15F, 3.98F, TitlePpu);
        const Rect survivors = Text("SURVIVORS", -7.12F, nightfall.bottom - 0.08F, BodyPpu);
        const Rect byline = Text("Trace2D ORIGINAL SURVIVAL", -7.12F, survivors.bottom - 0.06F, MicroPpu);
        RequireInside(scenario, "main.title", nightfall, "main.titleSafe", titleSafe, 0.02F);
        RequireInside(scenario, "main.subtitle", survivors, "main.titleSafe", titleSafe, 0.02F);
        RequireInside(scenario, "main.byline", byline, "main.titleSafe", titleSafe, 0.02F);
        RequireDisjoint(scenario, "main.title", nightfall, "main.subtitle", survivors, 0.03F);
        RequireDisjoint(scenario, "main.subtitle", survivors, "main.byline", byline, 0.03F);

        const Rect menuPanel = PanelRect(-5.05F, -0.45F, 4.45F, 5.20F);
        const Rect infoPanel = PanelRect(2.15F, -0.45F, 8.85F, 5.20F);
        RequireDisjoint(scenario, "main.titleStack", UnionRect(nightfall, byline), "main.menuPanel", menuPanel, 0.08F);
        RequireDisjoint(scenario, "main.titleStack", UnionRect(nightfall, byline), "main.infoPanel", infoPanel, 0.08F);
        const Rect menuLabel = Text("MENU", -6.86F, 1.82F, TinyPpu);
        RequireInside(scenario, "main.menuLabel", menuLabel, "main.menuPanel", menuPanel, 0.20F);
        constexpr std::array<std::string_view, 5U> Menu{"게임 시작", "프로필", "업적", "설정", "종료"};
        for (std::size_t i = 0U; i < Menu.size(); ++i)
        {
            const float y = 1.18F - static_cast<float>(i) * 0.88F;
            const Rect item = Text(Menu[i], -6.60F, y + 0.13F, BodyPpu);
            RequireInside(scenario, "main.menuItem", item, "main.menuPanel", menuPanel, 0.18F);
        }

        const auto& character = NightfallProduct::Character(static_cast<CharacterId>(characterIndex));
        const Rect characterHeader = Text("현재 캐릭터", -1.86F, 1.82F, TinyPpu);
        const Rect characterName = Text(character.nameKo, -1.86F, 1.28F, HeadingPpu);
        const Rect characterRole = Text(character.subtitleKo, -1.86F, characterName.bottom - 0.08F, MicroPpu);
        const Rect hero = Sprite(heroes_[characterIndex], -0.28F, -1.14F, 2.65F, 2.65F);
        const Rect recordHeader = Text("전투 기록", 2.65F, 1.82F, TinyPpu);
        const std::string starsText = largeProfile ? "별가루  999999" : "별가루  0";
        const std::string runsText = largeProfile ? "플레이  999회" : "플레이  0회";
        const std::string clearsText = largeProfile ? "클리어  999회" : "클리어  0회";
        const std::string killsText = largeProfile ? "누적 처치  999999" : "누적 처치  0";
        const std::string levelText = largeProfile ? "최고 레벨  99" : "최고 레벨  0";
        const Rect stars = Text(starsText, 2.65F, 1.23F, BodyPpu);
        const Rect runs = Text(runsText, 2.65F, 0.54F, SmallPpu);
        const Rect clears = Text(clearsText, 2.65F, -0.05F, SmallPpu);
        const Rect kills = Text(killsText, 2.65F, -0.64F, SmallPpu);
        const Rect level = Text(levelText, 2.65F, -1.23F, SmallPpu);
        RequireInside(scenario, "main.characterHeader", characterHeader, "main.infoPanel", infoPanel, 0.20F);
        RequireInside(scenario, "main.characterName", characterName, "main.infoPanel", infoPanel, 0.20F);
        RequireInside(scenario, "main.characterRole", characterRole, "main.infoPanel", infoPanel, 0.20F);
        RequireInside(scenario, "main.hero", hero, "main.infoPanel", infoPanel, 0.20F);
        RequireInside(scenario, "main.recordHeader", recordHeader, "main.infoPanel", infoPanel, 0.20F);
        RequireInside(scenario, "main.stats", UnionRect(stars, level), "main.infoPanel", infoPanel, 0.20F);
        RequireDisjoint(scenario, "main.characterName", characterName, "main.characterRole", characterRole, 0.03F);
        RequireDisjoint(scenario, "main.characterCopy", UnionRect(characterHeader, characterRole), "main.hero", hero, 0.10F);
        RequireDisjoint(scenario, "main.hero", hero, "main.stats", UnionRect(recordHeader, level), 0.12F);
        RequireDisjoint(scenario, "main.stat.stars", stars, "main.stat.runs", runs, 0.03F);
        RequireDisjoint(scenario, "main.stat.runs", runs, "main.stat.clears", clears, 0.03F);
        RequireDisjoint(scenario, "main.stat.clears", clears, "main.stat.kills", kills, 0.03F);
        RequireDisjoint(scenario, "main.stat.kills", kills, "main.stat.level", level, 0.03F);
        Footer(scenario, "W/S 선택  ·  ENTER 확인  ·  ESC 종료");
    }

    void CheckProfile(const std::string& scenario)
    {
        ++scenarioCount_;
        Header(scenario, "PLAYER PROFILE", "프로필");
        const Rect panel = PanelRect(0.0F, -0.33F, 13.5F, 5.65F);
        const std::array<std::pair<std::string, std::string>, 8U> rows{{
            {"총 플레이 횟수", "999999"}, {"누적 처치", "999999"}, {"클리어", "999999"}, {"최고 레벨", "99"},
            {"최고 생존", "99:59"}, {"별가루", "999999"}, {"캐릭터 해금", "3/3"}, {"스테이지 해금", "3/3"},
        }};
        for (std::size_t i = 0U; i < rows.size(); ++i)
        {
            const bool rightColumn = i >= 4U;
            const std::size_t row = rightColumn ? i - 4U : i;
            const float x = rightColumn ? 0.72F : -5.62F;
            const float y = 1.72F - static_cast<float>(row) * 1.02F;
            const Rect label = Text(rows[i].first, x, y, TinyPpu);
            const Rect value = Text(rows[i].second, x, y - 0.40F, BodyPpu);
            RequireInside(scenario, "profile.label", label, "profile.panel", panel, 0.20F);
            RequireInside(scenario, "profile.value", value, "profile.panel", panel, 0.20F);
            RequireDisjoint(scenario, "profile.label", label, "profile.value", value, 0.02F);
        }
        Footer(scenario, "ENTER 또는 ESC  ·  돌아가기");
    }

    void CheckAchievements(const std::string& scenario)
    {
        ++scenarioCount_;
        Header(scenario, "ACHIEVEMENTS", "업적");
        for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
        {
            const auto& definition = NightfallProduct::Achievement(static_cast<AchievementId>(i));
            const bool rightColumn = i >= 3U;
            const std::size_t row = rightColumn ? i - 3U : i;
            const float x = rightColumn ? 3.52F : -3.52F;
            const float y = 1.50F - static_cast<float>(row) * 1.55F;
            const Rect card = PanelRect(x, y, 6.25F, 1.20F);
            const Rect state = Text(i % 2U == 0U ? "달성" : "미달성", x - 2.68F, y + 0.32F, MicroPpu);
            const Rect name = Text(definition.nameKo, x - 1.62F, y + 0.34F, SmallPpu);
            const Rect description = Text(definition.descriptionKo, x - 2.68F, y - 0.18F, MicroPpu);
            RequireInside(scenario, "achievement.state", state, "achievement.card", card, 0.14F);
            RequireInside(scenario, "achievement.name", name, "achievement.card", card, 0.14F);
            RequireInside(scenario, "achievement.description", description, "achievement.card", card, 0.14F);
            RequireDisjoint(scenario, "achievement.name", name, "achievement.description", description, 0.02F);
        }
        Footer(scenario, "ENTER 또는 ESC  ·  돌아가기");
    }

    void CheckSettings(const std::string& scenario)
    {
        ++scenarioCount_;
        Header(scenario, "SETTINGS", "설정");
        const Rect panel = PanelRect(0.0F, -0.28F, 10.6F, 5.40F);
        constexpr std::array<std::string_view, 3U> labels{
            "효과음 볼륨   100%", "화면 흔들림   꺼짐", "돌아가기"};
        constexpr std::array<float, 3U> y{1.12F, -0.02F, -1.65F};
        for (std::size_t i = 0U; i < labels.size(); ++i)
        {
            const Rect item = Text(labels[i], -3.82F, y[i] + 0.14F, BodyPpu);
            RequireInside(scenario, "settings.item", item, "settings.panel", panel, 0.24F);
        }
        Footer(scenario, "W/S 선택  ·  A/D 또는 ←/→ 조절  ·  ENTER 확인");
    }

    void CheckCharacterSelect(const std::string& scenario, const bool unlocked)
    {
        ++scenarioCount_;
        Header(scenario, "CHOOSE YOUR HUNTER", "캐릭터 선택");
        constexpr std::array<float, 3U> x{-5.10F, 0.0F, 5.10F};
        std::array<Rect, 3U> cards{};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto& definition = NightfallProduct::Character(static_cast<CharacterId>(i));
            cards[i] = PanelRect(x[i], 0.08F, 4.40F, 4.55F);
            const Rect hero = Sprite(heroes_[i], x[i], 1.15F, 2.25F, 2.25F);
            const Rect name = Text(unlocked || i == 0U ? definition.nameKo : "잠김", x[i], -0.05F, BodyPpu, true);
            const Rect role = Text(definition.subtitleKo, x[i], -0.66F, MicroPpu, true);
            RequireInside(scenario, "character.hero", hero, "character.card", cards[i], 0.16F);
            RequireInside(scenario, "character.name", name, "character.card", cards[i], 0.16F);
            RequireInside(scenario, "character.role", role, "character.card", cards[i], 0.16F);
            RequireDisjoint(scenario, "character.hero", hero, "character.name", name, 0.08F);
            RequireDisjoint(scenario, "character.name", name, "character.role", role, 0.02F);
            if (!unlocked && i != 0U)
            {
                const Rect locked = Text("해금 조건 필요", x[i], -1.28F, MicroPpu, true);
                RequireInside(scenario, "character.lock", locked, "character.card", cards[i], 0.16F);
                RequireDisjoint(scenario, "character.role", role, "character.lock", locked, 0.02F);
            }
        }
        RequireDisjoint(scenario, "character.card0", cards[0], "character.card1", cards[1], 0.16F);
        RequireDisjoint(scenario, "character.card1", cards[1], "character.card2", cards[2], 0.16F);
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const Rect detailPanel = PanelRect(0.0F, -2.63F, 13.65F, 0.72F);
            const Rect detail = Text(
                unlocked || i == 0U ? NightfallProduct::Character(static_cast<CharacterId>(i)).descriptionKo
                                     : "해금 후 상세 능력을 확인할 수 있습니다.",
                0.0F, -2.39F, MicroPpu, true);
            RequireInside(scenario, "character.detail", detail, "character.detailPanel", detailPanel, 0.10F);
            RequireDisjoint(scenario, "character.cards", UnionRect(cards[0], cards[2]), "character.detailPanel", detailPanel, 0.12F);
        }
        Footer(scenario, "A/D 또는 W/S 선택  ·  ENTER 확인  ·  ESC 뒤로");
    }

    void CheckStageSelect(const std::string& scenario, const bool unlocked)
    {
        ++scenarioCount_;
        Header(scenario, "CHOOSE THE NIGHT", "스테이지 선택");
        constexpr std::array<float, 3U> x{-5.10F, 0.0F, 5.10F};
        std::array<Rect, 3U> cards{};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const auto& definition = NightfallProduct::Stage(static_cast<StageId>(i));
            cards[i] = PanelRect(x[i], 0.08F, 4.40F, 4.55F);
            const Rect preview = PanelRect(x[i], 1.18F, 3.50F, 1.45F);
            for (int py = 0; py < 2; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    const Visual& tile = ((px + py) & 1) == 0 ? floorsA_[i] : floorsB_[i];
                    const Rect tileBounds = Sprite(
                        tile,
                        x[i] - 1.31F + static_cast<float>(px) * 0.875F,
                        0.86F + static_cast<float>(py) * 0.64F,
                        0.875F, 0.64F);
                    RequireInside(scenario, "stage.tile", tileBounds, "stage.preview", preview, 0.0F);
                }
            }
            const Rect name = Text(unlocked || i == 0U ? definition.nameKo : "잠김", x[i], -0.02F, BodyPpu, true);
            const Rect subtitle = Text(definition.subtitleKo, x[i], -0.68F, MicroPpu, true);
            RequireInside(scenario, "stage.preview", preview, "stage.card", cards[i], 0.16F);
            RequireInside(scenario, "stage.name", name, "stage.card", cards[i], 0.16F);
            RequireInside(scenario, "stage.subtitle", subtitle, "stage.card", cards[i], 0.16F);
            RequireDisjoint(scenario, "stage.preview", preview, "stage.name", name, 0.06F);
            RequireDisjoint(scenario, "stage.name", name, "stage.subtitle", subtitle, 0.02F);
        }
        RequireDisjoint(scenario, "stage.card0", cards[0], "stage.card1", cards[1], 0.16F);
        RequireDisjoint(scenario, "stage.card1", cards[1], "stage.card2", cards[2], 0.16F);
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const Rect detailPanel = PanelRect(0.0F, -2.60F, 13.65F, 0.68F);
            const Rect detail = Text(
                unlocked || i == 0U ? NightfallProduct::Stage(static_cast<StageId>(i)).descriptionKo
                                     : "이전 스테이지를 클리어하면 해금됩니다.",
                0.0F, -2.38F, MicroPpu, true);
            RequireInside(scenario, "stage.detail", detail, "stage.detailPanel", detailPanel, 0.09F);
            const Rect equipped = Text("현재 장착  ·  달빛 사냥꾼", 0.0F, -3.20F, MicroPpu, true);
            RequireDisjoint(scenario, "stage.detailPanel", detailPanel, "stage.equipped", equipped, 0.06F);
        }
        Footer(scenario, "A/D 또는 W/S 선택  ·  ENTER 시작  ·  ESC 뒤로");
    }

    void CheckHud(const std::string& scenario)
    {
        ++scenarioCount_;
        const Rect topPanel = PanelRect(0.0F, 4.08F, 15.25F, 0.62F);
        const Rect hp = Text("체력 999/999", -5.55F, 4.29F, TinyPpu, true);
        const Rect time = Text("99:59", 0.0F, 4.29F, BodyPpu, true);
        const Rect level = Text("레벨 99  ·  처치 999999", 5.55F, 4.29F, TinyPpu, true);
        RequireInside(scenario, "hud.hp", hp, "hud.top", topPanel, 0.02F);
        RequireInside(scenario, "hud.time", time, "hud.top", topPanel, 0.02F);
        RequireInside(scenario, "hud.level", level, "hud.top", topPanel, 0.02F);
        RequireDisjoint(scenario, "hud.hp", hp, "hud.time", time, 0.10F);
        RequireDisjoint(scenario, "hud.time", time, "hud.level", level, 0.10F);
        constexpr std::array<float, 3U> offsets{-4.2F, 0.0F, 4.2F};
        constexpr std::array<std::string_view, 3U> labels{"연사  99", "화력  99", "궤도  99"};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const Rect panel = PanelRect(offsets[i], -3.64F, 3.35F, 0.64F);
            const Rect icon = Sprite(skills_[i], offsets[i] - 1.34F, -3.64F, 0.48F, 0.48F);
            const Rect label = Text(labels[i], offsets[i] + 0.20F, -3.45F, TinyPpu, true);
            RequireInside(scenario, "hud.skill.icon", icon, "hud.skill.panel", panel, 0.02F);
            RequireInside(scenario, "hud.skill.label", label, "hud.skill.panel", panel, 0.02F);
            RequireDisjoint(scenario, "hud.skill.icon", icon, "hud.skill.label", label, 0.02F);
        }
    }

    void CheckLevelUp(const std::string& scenario)
    {
        ++scenarioCount_;
        const Rect title = Text("레벨 업", 0.0F, 3.16F, HeadingPpu, true);
        const Rect prompt = Text("Q / E / F 중 하나를 선택하세요", 0.0F, 2.47F, TinyPpu, true);
        RequireDisjoint(scenario, "level.title", title, "level.prompt", prompt, 0.04F);
        constexpr std::array<float, 3U> x{-4.75F, 0.0F, 4.75F};
        constexpr std::array<std::string_view, 3U> keys{"Q", "E", "F"};
        constexpr std::array<std::string_view, 3U> names{"연사", "화력", "궤도"};
        constexpr std::array<std::string_view, 3U> line1{"공격 간격 감소", "투사체 공격력 증가", "주변 회전무기 +1"};
        constexpr std::array<std::string_view, 3U> line2{"3레벨마다 탄환 +1", "궤도 공격력도 증가", "최대 4개 · 이후 체력 강화"};
        for (std::size_t i = 0U; i < 3U; ++i)
        {
            const Rect card = PanelRect(x[i], -0.25F, 4.12F, 4.65F);
            const Rect key = Text(keys[i], x[i] - 1.52F, 1.55F, BodyPpu);
            const Rect icon = Sprite(skills_[i], x[i], 0.90F, 1.25F, 1.25F);
            const Rect name = Text(names[i], x[i], 0.03F, BodyPpu, true);
            const Rect first = Text(line1[i], x[i], -0.66F, TinyPpu, true);
            const Rect second = Text(line2[i], x[i], -1.20F, MicroPpu, true);
            RequireInside(scenario, "level.key", key, "level.card", card, 0.12F);
            RequireInside(scenario, "level.icon", icon, "level.card", card, 0.12F);
            RequireInside(scenario, "level.name", name, "level.card", card, 0.12F);
            RequireInside(scenario, "level.line1", first, "level.card", card, 0.12F);
            RequireInside(scenario, "level.line2", second, "level.card", card, 0.12F);
        }
    }

    void CheckPause(const std::string& scenario)
    {
        ++scenarioCount_;
        const Rect panel = PanelRect(0.0F, 0.0F, 6.80F, 4.15F);
        const Rect paused = Text("PAUSED", 0.0F, 1.47F, TinyPpu, true);
        const Rect title = Text("일시정지", 0.0F, 0.98F, HeadingPpu, true);
        RequireInside(scenario, "pause.paused", paused, "pause.panel", panel, 0.18F);
        RequireInside(scenario, "pause.title", title, "pause.panel", panel, 0.18F);
        RequireDisjoint(scenario, "pause.paused", paused, "pause.title", title, 0.03F);
        constexpr std::array<std::string_view, 2U> items{"계속하기", "메인 메뉴"};
        for (std::size_t i = 0U; i < 2U; ++i)
        {
            const float y = -0.05F - static_cast<float>(i) * 1.02F;
            const Rect item = Text(items[i], 0.0F, y + 0.14F, BodyPpu, true);
            RequireInside(scenario, "pause.item", item, "pause.panel", panel, 0.18F);
        }
    }

    void CheckResult(const std::string& scenario, const bool worstCase)
    {
        ++scenarioCount_;
        const Rect panel = PanelRect(0.0F, 0.0F, 11.20F, 7.00F);
        const Rect result = Text(worstCase ? "CLEAR" : "GAME OVER", 0.0F, 3.00F, TinyPpu, true);
        const Rect title = Text(worstCase ? "스테이지 클리어" : "패배", 0.0F, 2.48F, HeadingPpu, true);
        const Rect stat = Text(
            worstCase ? "처치 999999  ·  레벨 99  ·  생존 99:59" : "처치 0  ·  레벨 1  ·  생존 0:00",
            0.0F, 1.66F, TinyPpu, true);
        const Rect reward = Text(worstCase ? "별가루 획득  +999999" : "별가루 획득  +0", 0.0F, 1.05F, BodyPpu, true);
        RequireInside(scenario, "result.result", result, "result.panel", panel, 0.18F);
        RequireInside(scenario, "result.title", title, "result.panel", panel, 0.18F);
        RequireInside(scenario, "result.stat", stat, "result.panel", panel, 0.18F);
        RequireInside(scenario, "result.reward", reward, "result.panel", panel, 0.18F);
        RequireDisjoint(scenario, "result.result", result, "result.title", title, 0.03F);
        RequireDisjoint(scenario, "result.title", title, "result.stat", stat, 0.05F);
        RequireDisjoint(scenario, "result.stat", stat, "result.reward", reward, 0.03F);

        float unlockY = 0.28F;
        if (worstCase)
        {
            for (std::size_t i = 0U; i < NightfallProduct::CharacterCount(); ++i)
            {
                const Rect line = Text("새 캐릭터  ·  " + std::string{NightfallProduct::Character(static_cast<CharacterId>(i)).nameKo},
                    0.0F, unlockY, TinyPpu, true);
                RequireInside(scenario, "result.unlock.character", line, "result.panel", panel, 0.18F);
                unlockY -= 0.48F;
            }
            for (std::size_t i = 0U; i < NightfallProduct::StageCount(); ++i)
            {
                const Rect line = Text("새 스테이지  ·  " + std::string{NightfallProduct::Stage(static_cast<StageId>(i)).nameKo},
                    0.0F, unlockY, TinyPpu, true);
                RequireInside(scenario, "result.unlock.stage", line, "result.panel", panel, 0.18F);
                unlockY -= 0.48F;
            }
            for (std::size_t i = 0U; i < NightfallProduct::AchievementCount(); ++i)
            {
                const Rect line = Text("새 업적  ·  " + std::string{NightfallProduct::Achievement(static_cast<AchievementId>(i)).nameKo},
                    0.0F, unlockY, TinyPpu, true);
                RequireInside(scenario, "result.unlock.achievement", line, "result.panel", panel, 0.18F);
                unlockY -= 0.44F;
            }
        }
        const Rect retry = Text("다시 도전", -2.35F, -2.03F, BodyPpu, true);
        const Rect menu = Text("메인 메뉴", 2.35F, -2.03F, BodyPpu, true);
        const Rect help = Text("W/S 선택  ·  ENTER 확인", 0.0F, -3.15F, TinyPpu, true);
        RequireInside(scenario, "result.retry", retry, "result.panel", panel, 0.18F);
        RequireInside(scenario, "result.menu", menu, "result.panel", panel, 0.18F);
        RequireInside(scenario, "result.help", help, "result.panel", panel, 0.18F);
    }

    std::filesystem::path runtimeRoot_{};
    trace2d::assets::ResourceRegistry resources_;
    trace2d::assets::TextureAssetCache textureCache_;
    std::array<Visual, 3U> heroes_{};
    std::array<Visual, 3U> floorsA_{};
    std::array<Visual, 3U> floorsB_{};
    std::array<Visual, 3U> skills_{};
    std::unique_ptr<trace2d::text::GlyphAtlas> glyphAtlas_{};
    std::unique_ptr<trace2d::text::TextLayoutRun> textLayout_{};
    trace2d::text::GlyphAtlasTextureBinding2D glyphBinding_{};
    std::vector<trace2d::render::SpritePresentationRenderData> textScratch_{};
    std::uint64_t fixedGlyphRevision_{0U};
    std::uint32_t viewportWidth_{960U};
    std::uint32_t viewportHeight_{540U};
    std::size_t scenarioCount_{0U};
};
} // namespace

int main()
{
    try
    {
        Contract contract{TRACE2D_G1_RUNTIME_DIR};
        contract.Run();
        std::cout << "Nightfall headless product contract sweep passed: "
                  << contract.ScenarioCount() << " scenarios\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
