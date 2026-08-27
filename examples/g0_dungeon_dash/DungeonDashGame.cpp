#include "DungeonDashGame.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
constexpr trace2d::scene::Vector2 PlayerStart{-5.6F, 0.0F};
constexpr trace2d::scene::Vector2 HunterStart{5.7F, -2.7F};
constexpr std::array<trace2d::scene::Vector2, DungeonDashGame::RelicCount> RelicStarts{{
    {-4.8F, 2.5F},
    {-2.4F, -2.6F},
    {0.1F, 2.8F},
    {2.8F, -2.3F},
    {5.3F, 2.2F},
}};

trace2d::scene::EntityId CreateEntity(
    trace2d::scene::Scene& scene,
    std::string semanticId,
    std::string name,
    const trace2d::scene::Vector2 position)
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.name = std::move(name);
    descriptor.transform.position = position;
    return scene.CreateEntity(std::move(descriptor));
}

float DistanceSquared(
    const trace2d::scene::Vector2 left,
    const trace2d::scene::Vector2 right) noexcept
{
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    return dx * dx + dy * dy;
}
} // namespace

void DungeonDashGame::OnStart(trace2d::application::GameContext& context)
{
    player_ = CreateEntity(context.Scene(), "dungeon.player", "Explorer", PlayerStart);
    hunter_ = CreateEntity(context.Scene(), "dungeon.hunter", "Dungeon Hunter", HunterStart);

    for (std::size_t index = 0U; index < RelicCount; ++index)
    {
        relics_[index] = CreateEntity(
            context.Scene(),
            "dungeon.relic." + std::to_string(index),
            "Relic " + std::to_string(index + 1U),
            RelicStarts[index]);
    }

    horizontalAction_ = context.Actions().AddAxis1DAction(
        "dungeon.move.horizontal",
        trace2d::input::InputControl::KeyA,
        trace2d::input::InputControl::KeyD);
    verticalAction_ = context.Actions().AddAxis1DAction(
        "dungeon.move.vertical",
        trace2d::input::InputControl::KeyS,
        trace2d::input::InputControl::KeyW);
    restartAction_ = context.Actions().AddButtonAction("dungeon.restart");
    context.Actions().BindButton(restartAction_, trace2d::input::InputControl::KeyR);

    ResetRound(context);
}

void DungeonDashGame::ResetRound(trace2d::application::GameContext& context)
{
    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    trace2d::scene::Entity* const hunter = context.Scene().TryGet(hunter_);
    if (player == nullptr || hunter == nullptr)
        throw std::runtime_error{"Dungeon Dash could not reset canonical player/hunter state."};

    player->LocalTransform().position = PlayerStart;
    hunter->LocalTransform().position = HunterStart;

    for (std::size_t index = 0U; index < RelicCount; ++index)
    {
        trace2d::scene::Entity* const relic = context.Scene().TryGet(relics_[index]);
        if (relic == nullptr) throw std::runtime_error{"Dungeon Dash could not reset a canonical relic."};
        relic->LocalTransform().position = RelicStarts[index];
        collected_[index] = false;
    }

    collectedCount_ = 0U;
    state_ = State::Running;
}

void DungeonDashGame::OnFixedUpdate(
    trace2d::application::GameContext& context,
    const trace2d::application::FixedUpdate& update)
{
    if (context.Actions().Pressed(restartAction_))
    {
        ResetRound(context);
        return;
    }
    if (state_ != State::Running) return;

    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    trace2d::scene::Entity* const hunter = context.Scene().TryGet(hunter_);
    if (player == nullptr || hunter == nullptr)
        throw std::runtime_error{"Dungeon Dash lost canonical player/hunter state."};

    const float dt = std::chrono::duration<float>{update.fixedDelta}.count();
    float horizontal = context.Actions().Axis1D(horizontalAction_);
    float vertical = context.Actions().Axis1D(verticalAction_);
    const float inputLengthSquared = horizontal * horizontal + vertical * vertical;
    if (inputLengthSquared > 1.0F)
    {
        const float inverseLength = 1.0F / std::sqrt(inputLengthSquared);
        horizontal *= inverseLength;
        vertical *= inverseLength;
    }

    auto& playerPosition = player->LocalTransform().position;
    playerPosition.x = std::clamp(
        playerPosition.x + horizontal * PlayerSpeed * dt,
        -WorldHalfWidth + 0.65F,
        WorldHalfWidth - 0.65F);
    playerPosition.y = std::clamp(
        playerPosition.y + vertical * PlayerSpeed * dt,
        -WorldHalfHeight + 0.65F,
        WorldHalfHeight - 0.65F);

    const float relicCollectRadius = PlayerRadius + RelicRadius;
    const float relicCollectRadiusSquared = relicCollectRadius * relicCollectRadius;
    for (std::size_t index = 0U; index < RelicCount; ++index)
    {
        if (collected_[index]) continue;
        const trace2d::scene::Entity* const relic = context.Scene().TryGet(relics_[index]);
        if (relic == nullptr) throw std::runtime_error{"Dungeon Dash lost canonical relic state."};
        if (DistanceSquared(playerPosition, relic->LocalTransform().position) > relicCollectRadiusSquared) continue;

        collected_[index] = true;
        ++collectedCount_;
    }

    if (collectedCount_ == RelicCount)
    {
        state_ = State::Won;
        return;
    }

    auto& hunterPosition = hunter->LocalTransform().position;
    const float dx = playerPosition.x - hunterPosition.x;
    const float dy = playerPosition.y - hunterPosition.y;
    const float distanceSquared = dx * dx + dy * dy;
    if (distanceSquared > 0.0001F)
    {
        const float distance = std::sqrt(distanceSquared);
        const float hunterSpeed = HunterBaseSpeed + HunterSpeedPerRelic * static_cast<float>(collectedCount_);
        hunterPosition.x += (dx / distance) * hunterSpeed * dt;
        hunterPosition.y += (dy / distance) * hunterSpeed * dt;
    }

    hunterPosition.x = std::clamp(
        hunterPosition.x,
        -WorldHalfWidth + 0.65F,
        WorldHalfWidth - 0.65F);
    hunterPosition.y = std::clamp(
        hunterPosition.y,
        -WorldHalfHeight + 0.65F,
        WorldHalfHeight - 0.65F);

    const float hitRadius = PlayerRadius + HunterRadius;
    if (DistanceSquared(playerPosition, hunterPosition) <= hitRadius * hitRadius)
        state_ = State::Lost;
}

DungeonDashGame::State DungeonDashGame::CurrentState() const noexcept { return state_; }
std::size_t DungeonDashGame::CollectedRelicCount() const noexcept { return collectedCount_; }
bool DungeonDashGame::RelicCollected(const std::size_t index) const noexcept
{
    return index < RelicCount && collected_[index];
}
trace2d::scene::EntityId DungeonDashGame::Player() const noexcept { return player_; }
trace2d::scene::EntityId DungeonDashGame::Hunter() const noexcept { return hunter_; }
trace2d::scene::EntityId DungeonDashGame::Relic(const std::size_t index) const noexcept
{
    return index < RelicCount ? relics_[index] : trace2d::scene::EntityId{};
}
