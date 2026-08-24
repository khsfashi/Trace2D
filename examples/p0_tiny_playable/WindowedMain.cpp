#include "TinyPlayableGame.hpp"

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/ui/Ui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using TextureHandle = trace2d::render::TextureHandle;
using Color = std::array<std::uint8_t, 4>;
using SpriteBuffer = std::array<trace2d::render::SpriteRenderData, 64>;

constexpr Color Transparent{0U, 0U, 0U, 0U};
constexpr Color Ink{20U, 24U, 33U, 255U};
constexpr Color DeepInk{11U, 14U, 22U, 255U};
constexpr Color StoneDark{48U, 56U, 69U, 255U};
constexpr Color Stone{78U, 88U, 101U, 255U};
constexpr Color StoneLight{112U, 122U, 132U, 255U};
constexpr Color Moss{52U, 91U, 70U, 255U};
constexpr Color MossLight{76U, 124U, 83U, 255U};
constexpr Color CoatDark{30U, 66U, 86U, 255U};
constexpr Color Coat{43U, 113U, 132U, 255U};
constexpr Color CoatLight{91U, 187U, 181U, 255U};
constexpr Color Skin{218U, 174U, 132U, 255U};
constexpr Color Leather{116U, 76U, 50U, 255U};
constexpr Color HazardDark{114U, 25U, 38U, 255U};
constexpr Color Hazard{229U, 53U, 69U, 255U};
constexpr Color HazardHot{255U, 143U, 92U, 255U};
constexpr Color GoldDark{129U, 88U, 37U, 255U};
constexpr Color Gold{236U, 184U, 65U, 255U};
constexpr Color GoldLight{255U, 231U, 139U, 255U};
constexpr Color Health{91U, 213U, 126U, 255U};

struct PixelArt final
{
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::vector<std::uint8_t> rgba{};
};

struct PresentationState final
{
    trace2d::render::Renderer* renderer{nullptr};
    TextureHandle background{};
    TextureHandle player{};
    TextureHandle shadow{};
    TextureHandle hazardActive{};
    TextureHandle hazardSafe{};
    TextureHandle beaconPending{};
    TextureHandle beaconDone{};
    TextureHandle rock{};
    TextureHandle grass{};
    TextureHandle panel{};
    TextureHandle heartFull{};
    TextureHandle heartEmpty{};
    TextureHandle objectivePending{};
    TextureHandle objectiveDone{};
    TextureHandle sparkle{};
};

PixelArt MakeArt(const std::uint32_t width, const std::uint32_t height, const Color& clear = Transparent)
{
    PixelArt art{};
    art.width = width;
    art.height = height;
    art.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    for (std::uint32_t y = 0U; y < height; ++y)
    {
        for (std::uint32_t x = 0U; x < width; ++x)
        {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4U;
            std::copy(clear.begin(), clear.end(), art.rgba.begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }
    return art;
}

void Put(PixelArt& art, const int x, const int y, const Color& color)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(art.width) || y >= static_cast<int>(art.height)) return;
    const std::size_t offset =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(art.width) + static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), art.rgba.begin() + static_cast<std::ptrdiff_t>(offset));
}

void Fill(PixelArt& art, const int x, const int y, const int width, const int height, const Color& color)
{
    for (int py = y; py < y + height; ++py)
        for (int px = x; px < x + width; ++px)
            Put(art, px, py, color);
}

void Diamond(PixelArt& art, const int centerX, const int centerY, const int radius, const Color& color)
{
    for (int y = -radius; y <= radius; ++y)
    {
        const int half = radius - std::abs(y);
        Fill(art, centerX - half, centerY + y, half * 2 + 1, 1, color);
    }
}

PixelArt MakeBackground()
{
    PixelArt art = MakeArt(160U, 90U, DeepInk);
    for (int y = 0; y < 90; ++y)
    {
        Color row{};
        if (y < 48)
        {
            row = Color{
                static_cast<std::uint8_t>(15 + y / 5),
                static_cast<std::uint8_t>(22 + y / 3),
                static_cast<std::uint8_t>(35 + y / 2),
                255U};
        }
        else
        {
            row = Color{
                static_cast<std::uint8_t>(20 + (y - 48) / 4),
                static_cast<std::uint8_t>(31 + (y - 48) / 2),
                static_cast<std::uint8_t>(34 + (y - 48) / 5),
                255U};
        }
        Fill(art, 0, y, 160, 1, row);
    }

    // Moon and sparse stars make the scene read as a place before any gameplay object is drawn.
    for (int y = -8; y <= 8; ++y)
        for (int x = -8; x <= 8; ++x)
            if (x * x + y * y <= 64)
                Put(art, 126 + x, 18 + y, Color{184U, 198U, 185U, 255U});
    for (const auto [x, y] : std::array<std::array<int, 2>, 10>{
             std::array<int, 2>{12, 12}, {25, 22}, {42, 9}, {56, 18}, {72, 12},
             {91, 24}, {108, 8}, {143, 28}, {151, 13}, {34, 31}})
        Put(art, x, y, Color{155U, 174U, 173U, 255U});

    // Distant ridge.
    for (int x = 0; x < 160; ++x)
    {
        const int ridge = 47 + ((x * 17 + 11) % 9);
        Fill(art, x, ridge, 1, 63 - ridge, Color{24U, 41U, 45U, 255U});
    }

    // Tree silhouettes at two depths.
    for (const int baseX : std::array<int, 8>{4, 22, 42, 64, 93, 113, 139, 153})
    {
        Fill(art, baseX, 39, 3, 33, Color{18U, 31U, 32U, 255U});
        for (int crown = 0; crown < 4; ++crown)
        {
            const int cy = 34 + crown * 7;
            const int radius = 10 - crown;
            for (int row = 0; row < radius; ++row)
            {
                const int half = row;
                Fill(art, baseX + 1 - half, cy + row, half * 2 + 1, 1, Color{19U, 43U, 38U, 255U});
            }
        }
    }

    // Mossy path and broken masonry around the playable lane.
    Fill(art, 0, 58, 160, 3, Color{45U, 72U, 56U, 255U});
    Fill(art, 0, 61, 160, 29, Color{28U, 47U, 40U, 255U});
    for (int x = 0; x < 160; x += 10)
    {
        const Color stone = (x / 10) % 2 == 0 ? Color{52U, 67U, 61U, 255U} : Color{44U, 58U, 54U, 255U};
        Fill(art, x, 66 + ((x / 10) % 3), 8, 3, stone);
    }
    for (int x = 2; x < 158; x += 13)
    {
        Put(art, x, 59, Color{76U, 118U, 76U, 255U});
        Put(art, x + 1, 58, Color{66U, 104U, 70U, 255U});
    }
    return art;
}

PixelArt MakePlayer()
{
    PixelArt art = MakeArt(24U, 32U);
    // Shadowed hood outline and face.
    Fill(art, 7, 2, 10, 2, Ink);
    Fill(art, 5, 4, 14, 7, Ink);
    Fill(art, 7, 5, 10, 6, CoatDark);
    Fill(art, 9, 7, 6, 4, Skin);
    Put(art, 13, 8, DeepInk);
    Fill(art, 6, 11, 12, 3, Ink);

    // Asymmetric courier coat and scarf give the silhouette a clear facing direction.
    Fill(art, 5, 13, 13, 11, Ink);
    Fill(art, 7, 13, 9, 10, Coat);
    Fill(art, 8, 13, 8, 3, CoatLight);
    Fill(art, 16, 14, 3, 3, Gold);
    Fill(art, 17, 17, 2, 8, Leather);
    Fill(art, 4, 16, 3, 7, CoatDark);
    Fill(art, 3, 19, 3, 3, Skin);

    // Belt, satchel and split legs.
    Fill(art, 6, 21, 11, 2, Leather);
    Fill(art, 15, 19, 5, 6, Ink);
    Fill(art, 16, 20, 3, 4, Leather);
    Fill(art, 7, 23, 4, 6, Ink);
    Fill(art, 13, 23, 4, 6, Ink);
    Fill(art, 6, 28, 6, 3, DeepInk);
    Fill(art, 13, 28, 6, 3, DeepInk);
    Put(art, 7, 14, GoldLight);
    Put(art, 8, 15, CoatLight);
    return art;
}

PixelArt MakeShadow()
{
    PixelArt art = MakeArt(24U, 8U);
    Fill(art, 5, 2, 14, 4, Color{7U, 10U, 14U, 150U});
    Fill(art, 2, 3, 20, 2, Color{7U, 10U, 14U, 105U});
    return art;
}

PixelArt MakeHazard(const bool active)
{
    PixelArt art = MakeArt(22U, 54U);
    // Ancient side rails.
    Fill(art, 2, 3, 5, 48, DeepInk);
    Fill(art, 15, 3, 5, 48, DeepInk);
    Fill(art, 3, 5, 3, 45, StoneDark);
    Fill(art, 16, 5, 3, 45, Stone);
    Fill(art, 1, 2, 7, 4, StoneLight);
    Fill(art, 14, 2, 7, 4, StoneLight);
    Fill(art, 1, 49, 7, 4, StoneDark);
    Fill(art, 14, 49, 7, 4, StoneDark);

    if (active)
    {
        for (int y = 7; y < 49; y += 5)
        {
            Fill(art, 8, y, 6, 3, HazardDark);
            Fill(art, 9, y, 4, 2, Hazard);
            Put(art, 10 + ((y / 5) & 1), y, HazardHot);
        }
        Fill(art, 10, 5, 2, 44, Color{255U, 87U, 96U, 180U});
    }
    else
    {
        Fill(art, 9, 44, 4, 4, HazardDark);
        Put(art, 10, 45, Hazard);
    }
    return art;
}

PixelArt MakeBeacon(const bool complete)
{
    PixelArt art = MakeArt(24U, 40U);
    Fill(art, 5, 31, 14, 6, DeepInk);
    Fill(art, 7, 27, 10, 6, StoneDark);
    Fill(art, 9, 18, 6, 10, Stone);
    Fill(art, 8, 17, 8, 3, StoneLight);
    Diamond(art, 12, 10, 7, complete ? GoldLight : GoldDark);
    Diamond(art, 12, 10, 4, complete ? Gold : Color{105U, 92U, 66U, 255U});
    if (complete)
    {
        Put(art, 12, 2, GoldLight);
        Put(art, 4, 10, Gold);
        Put(art, 20, 10, Gold);
        Put(art, 12, 18, GoldLight);
    }
    return art;
}

PixelArt MakeRock()
{
    PixelArt art = MakeArt(20U, 12U);
    Fill(art, 3, 5, 14, 5, DeepInk);
    Fill(art, 5, 3, 10, 6, StoneDark);
    Fill(art, 7, 2, 6, 5, Stone);
    Fill(art, 8, 3, 4, 2, StoneLight);
    Fill(art, 4, 4, 4, 2, Moss);
    Put(art, 6, 3, MossLight);
    return art;
}

PixelArt MakeGrass()
{
    PixelArt art = MakeArt(18U, 10U);
    Fill(art, 2, 7, 14, 2, Moss);
    for (const int x : std::array<int, 7>{3, 5, 7, 9, 11, 13, 15})
    {
        Put(art, x, 6, MossLight);
        Put(art, x + ((x & 1) == 0 ? -1 : 1), 5, Moss);
    }
    Put(art, 5, 4, MossLight);
    Put(art, 12, 4, MossLight);
    return art;
}

PixelArt MakeHeart(const bool full)
{
    PixelArt art = MakeArt(12U, 11U);
    const Color core = full ? Health : StoneDark;
    const Color shine = full ? Color{157U, 241U, 169U, 255U} : Stone;
    Fill(art, 2, 2, 3, 3, Ink);
    Fill(art, 7, 2, 3, 3, Ink);
    Fill(art, 1, 3, 10, 4, Ink);
    Fill(art, 3, 7, 6, 2, Ink);
    Fill(art, 5, 9, 2, 1, Ink);
    Fill(art, 2, 3, 8, 3, core);
    Fill(art, 3, 6, 6, 2, core);
    Fill(art, 5, 8, 2, 1, core);
    Put(art, 3, 3, shine);
    return art;
}

PixelArt MakeObjectiveBadge(const bool complete)
{
    PixelArt art = MakeArt(18U, 18U);
    Diamond(art, 9, 9, 8, Ink);
    Diamond(art, 9, 9, 6, complete ? Gold : StoneDark);
    Diamond(art, 9, 9, 3, complete ? GoldLight : Stone);
    if (complete)
    {
        Fill(art, 5, 9, 3, 2, DeepInk);
        Fill(art, 7, 11, 2, 2, DeepInk);
        Fill(art, 9, 7, 5, 2, DeepInk);
    }
    return art;
}

PixelArt MakeSparkle()
{
    PixelArt art = MakeArt(12U, 12U);
    Fill(art, 5, 1, 2, 10, GoldLight);
    Fill(art, 1, 5, 10, 2, GoldLight);
    Diamond(art, 6, 6, 2, Gold);
    return art;
}

TextureHandle CreateTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    std::string reference,
    const PixelArt& art)
{
    trace2d::assets::TextureResource canonical{};
    canonical.width = art.width;
    canonical.height = art.height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.canonicalRgba8 = art.rgba;
    const auto published = resources.PublishTexture(std::move(reference), std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"P0 could not publish authored presentation texture."};

    trace2d::render::Rgba8TextureData data{};
    data.width = art.width;
    data.height = art.height;
    data.pixels = std::span<const std::uint8_t>{art.rgba.data(), art.rgba.size()};
    return renderer.CreateTextureRgba8(published.handle, data);
}

TextureHandle CreateSolidTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    std::string reference,
    const Color& rgba)
{
    const PixelArt art = MakeArt(1U, 1U, rgba);
    return CreateTexture(resources, renderer, std::move(reference), art);
}

void ReleaseTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const TextureHandle texture) noexcept
{
    renderer.DestroyTexture(texture);
    static_cast<void>(resources.Unload(texture.Untyped()));
}

void AddSprite(
    SpriteBuffer& sprites,
    std::size_t& count,
    const float centerX,
    const float centerY,
    const float halfWidth,
    const float halfHeight,
    const TextureHandle texture,
    const std::int32_t layer,
    const std::uint64_t order)
{
    if (count >= sprites.size()) throw std::runtime_error{"P0 presentation sprite capacity exceeded."};
    sprites[count++] = trace2d::render::SpriteRenderData{
        .center = {.x = centerX, .y = centerY},
        .halfExtents = {.x = halfWidth, .y = halfHeight},
        .texture = texture,
        .layer = layer,
        .stableOrder = order,
    };
}

void AddHud(
    SpriteBuffer& sprites,
    std::size_t& count,
    const trace2d::ui::UiElement& health,
    const trace2d::ui::UiElement& objective,
    const PresentationState& state,
    std::uint64_t& order)
{
    AddSprite(sprites, count, 68.0F, 27.0F, 54.0F, 18.0F, state.panel, 20, order++);
    const std::uint32_t currentHealth = health.progress.Active() ? health.progress.Value() : 0U;
    for (std::uint32_t index = 0U; index < TinyPlayableGame::MaximumHealth; ++index)
    {
        const TextureHandle heart = index < currentHealth ? state.heartFull : state.heartEmpty;
        AddSprite(sprites, count, 38.0F + static_cast<float>(index) * 30.0F, 27.0F, 12.0F, 11.0F, heart, 21, order++);
    }

    const bool complete = objective.progress.Active() && objective.progress.Value() == 1U;
    AddSprite(sprites, count, 596.0F, 27.0F, 30.0F, 18.0F, state.panel, 20, order++);
    AddSprite(
        sprites,
        count,
        596.0F,
        27.0F,
        14.0F,
        14.0F,
        complete ? state.objectiveDone : state.objectivePending,
        21,
        order++);
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr) throw std::runtime_error{"P0 presentation state is unavailable."};

    const auto playerId = context.Scene().FindBySemanticId("game.player");
    const auto hazardId = context.Scene().FindBySemanticId("game.hazard");
    const auto beaconId = context.Scene().FindBySemanticId("game.beacon");
    const trace2d::ui::UiElement* const health = context.Ui().Find("hud.health");
    const trace2d::ui::UiElement* const objective = context.Ui().Find("hud.objective");
    if (!playerId.has_value() || !hazardId.has_value() || !beaconId.has_value() || health == nullptr || objective == nullptr)
        throw std::runtime_error{"P0 presentation could not resolve canonical gameplay state."};

    const trace2d::scene::Entity* const player = context.Scene().TryGet(*playerId);
    const trace2d::scene::Entity* const hazard = context.Scene().TryGet(*hazardId);
    const trace2d::scene::Entity* const beacon = context.Scene().TryGet(*beaconId);
    if (player == nullptr || hazard == nullptr || beacon == nullptr)
        throw std::runtime_error{"P0 presentation resolved a stale gameplay entity."};

    const bool objectiveComplete = objective->progress.Active() && objective->progress.Value() == 1U;
    const bool hazardActive = std::abs(hazard->LocalTransform().position.y - TinyPlayableGame::GroundY) < 0.5F;
    const bool canInteract = !objectiveComplete &&
        std::abs(player->LocalTransform().position.x - beacon->LocalTransform().position.x) <=
            TinyPlayableGame::GoalInteractionDistance;

    SpriteBuffer sprites{};
    std::size_t count = 0U;
    std::uint64_t order = 0U;

    // One authored backdrop establishes a readable location. Decorative sprites stay presentation-only.
    AddSprite(sprites, count, 320.0F, 180.0F, 320.0F, 180.0F, state->background, -20, order++);
    AddSprite(sprites, count, 158.0F, 207.0F, 20.0F, 12.0F, state->rock, -5, order++);
    AddSprite(sprites, count, 468.0F, 207.0F, 18.0F, 10.0F, state->grass, -5, order++);
    AddSprite(sprites, count, 250.0F, 205.0F, 15.0F, 8.0F, state->grass, -5, order++);

    AddSprite(
        sprites,
        count,
        player->LocalTransform().position.x,
        player->LocalTransform().position.y + 23.0F,
        24.0F,
        8.0F,
        state->shadow,
        0,
        order++);
    AddSprite(
        sprites,
        count,
        player->LocalTransform().position.x,
        player->LocalTransform().position.y,
        22.0F,
        30.0F,
        state->player,
        3,
        order++);
    AddSprite(
        sprites,
        count,
        hazard->LocalTransform().position.x,
        hazard->LocalTransform().position.y,
        18.0F,
        58.0F,
        hazardActive ? state->hazardActive : state->hazardSafe,
        2,
        order++);
    AddSprite(
        sprites,
        count,
        beacon->LocalTransform().position.x,
        beacon->LocalTransform().position.y,
        22.0F,
        36.0F,
        objectiveComplete ? state->beaconDone : state->beaconPending,
        2,
        order++);

    if (canInteract)
    {
        AddSprite(
            sprites,
            count,
            beacon->LocalTransform().position.x,
            beacon->LocalTransform().position.y - 48.0F,
            10.0F,
            10.0F,
            state->sparkle,
            5,
            order++);
    }

    AddHud(sprites, count, *health, *objective, *state, order);
    if (objectiveComplete)
    {
        AddSprite(sprites, count, 320.0F, 44.0F, 22.0F, 22.0F, state->objectiveDone, 22, order++);
        AddSprite(sprites, count, 292.0F, 44.0F, 7.0F, 7.0F, state->sparkle, 22, order++);
        AddSprite(sprites, count, 348.0F, 44.0F, 7.0F, 7.0F, state->sparkle, 22, order++);
    }

    trace2d::render::OrthographicCamera camera{};
    camera.center = {.x = TinyPlayableGame::CanvasWidth * 0.5F, .y = TinyPlayableGame::CanvasHeight * 0.5F};
    camera.verticalSize = TinyPlayableGame::CanvasHeight;
    state->renderer->RenderFrame(camera, std::span<const trace2d::render::SpriteRenderData>{sprites.data(), count});
}
} // namespace

int main()
{
    using namespace std::chrono_literals;
    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(TinyPlayableGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(TinyPlayableGame::CanvasHeight);
        platformConfig.windowTitle = "Trace2D P0 - Ruins Courier | A/D move | Space/Enter interact | Esc quit";

        trace2d::platform::Platform platform{platformConfig};
        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.025F, .green = 0.035F, .blue = 0.050F, .alpha = 1.0F};
        trace2d::render::Renderer renderer{rendererConfig, platform};
        trace2d::assets::ResourceRegistry resources{"."};

        std::vector<TextureHandle> textures{};
        textures.reserve(15U);
        const auto retain = [&textures](const TextureHandle texture) {
            textures.push_back(texture);
            return texture;
        };

        PresentationState presentation{
            .renderer = &renderer,
            .background = retain(CreateTexture(resources, renderer, "p0/revision/background.rgba8", MakeBackground())),
            .player = retain(CreateTexture(resources, renderer, "p0/revision/player.rgba8", MakePlayer())),
            .shadow = retain(CreateTexture(resources, renderer, "p0/revision/shadow.rgba8", MakeShadow())),
            .hazardActive = retain(CreateTexture(resources, renderer, "p0/revision/hazard-active.rgba8", MakeHazard(true))),
            .hazardSafe = retain(CreateTexture(resources, renderer, "p0/revision/hazard-safe.rgba8", MakeHazard(false))),
            .beaconPending = retain(CreateTexture(resources, renderer, "p0/revision/beacon-pending.rgba8", MakeBeacon(false))),
            .beaconDone = retain(CreateTexture(resources, renderer, "p0/revision/beacon-done.rgba8", MakeBeacon(true))),
            .rock = retain(CreateTexture(resources, renderer, "p0/revision/rock.rgba8", MakeRock())),
            .grass = retain(CreateTexture(resources, renderer, "p0/revision/grass.rgba8", MakeGrass())),
            .panel = retain(CreateSolidTexture(resources, renderer, "p0/revision/hud-panel.rgba8", Color{19U, 26U, 34U, 235U})),
            .heartFull = retain(CreateTexture(resources, renderer, "p0/revision/heart-full.rgba8", MakeHeart(true))),
            .heartEmpty = retain(CreateTexture(resources, renderer, "p0/revision/heart-empty.rgba8", MakeHeart(false))),
            .objectivePending = retain(CreateTexture(resources, renderer, "p0/revision/objective-pending.rgba8", MakeObjectiveBadge(false))),
            .objectiveDone = retain(CreateTexture(resources, renderer, "p0/revision/objective-done.rgba8", MakeObjectiveBadge(true))),
            .sparkle = retain(CreateTexture(resources, renderer, "p0/revision/sparkle.rgba8", MakeSparkle())),
        };

        trace2d::application::ApplicationConfig applicationConfig{};
        applicationConfig.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
        applicationConfig.runtime.seed = 315U;
        applicationConfig.scene.semanticId = "p0.tiny-playable";
        applicationConfig.scene.name = "P0 Tiny Playable";
        applicationConfig.uiWidth = static_cast<std::uint32_t>(TinyPlayableGame::CanvasWidth);
        applicationConfig.uiHeight = static_cast<std::uint32_t>(TinyPlayableGame::CanvasHeight);

        TinyPlayableGame game{};
        trace2d::application::Application application{game, applicationConfig};
        application.SetPresentationCallback(&Present, &presentation);
        application.Start();

        bool quitRequested = false;
        Clock::time_point previous = Clock::now();
        while (!quitRequested)
        {
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                {
                    quitRequested = true;
                }
                else if (event.type == trace2d::platform::PlatformEventType::Input)
                {
                    if (event.input.control == trace2d::input::InputControl::Escape &&
                        event.input.type == trace2d::input::InputEventType::Press)
                        quitRequested = true;
                    else
                        application.ApplyInput(event.input);
                }
            }

            if (quitRequested) break;
            const Clock::time_point now = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
            previous = now;
            elapsed = std::min(
                elapsed,
                std::chrono::duration_cast<std::chrono::nanoseconds>(250ms));
            static_cast<void>(application.AdvanceElapsed(elapsed));
            static_cast<void>(application.Present());
            std::this_thread::sleep_for(1ms);
        }
        application.Stop();

        for (auto it = textures.rbegin(); it != textures.rend(); ++it)
            ReleaseTexture(resources, renderer, *it);
        return 0;
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
