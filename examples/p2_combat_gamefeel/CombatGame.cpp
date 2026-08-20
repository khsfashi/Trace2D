#include "CombatGame.hpp"

#include <trace2d/ui/Ui.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace trace2d::examples
{
namespace
{
constexpr char HitClipReference[] = "audio/p2_hit.wav";

[[nodiscard]] physics::Collider2D MakeCircleCollider(
    std::string semanticId,
    const float radius,
    const std::uint32_t layerBits,
    const std::uint32_t maskBits)
{
    physics::Collider2D collider{};
    collider.semanticId = std::move(semanticId);
    collider.shape = physics::ColliderShape2D::Circle;
    collider.radius = radius;
    collider.layerBits = layerBits;
    collider.maskBits = maskBits;
    collider.friction = 0.2F;
    return collider;
}

[[nodiscard]] physics::Collider2D MakeBoxCollider(
    std::string semanticId,
    const scene::Vector2 halfExtents,
    const std::uint32_t layerBits,
    const std::uint32_t maskBits)
{
    physics::Collider2D collider{};
    collider.semanticId = std::move(semanticId);
    collider.shape = physics::ColliderShape2D::Box;
    collider.halfExtents = halfExtents;
    collider.layerBits = layerBits;
    collider.maskBits = maskBits;
    collider.friction = 0.4F;
    return collider;
}

void RequirePhysicsAttach(const physics::PhysicsAttachResult2D result, const char* const message)
{
    if (result != physics::PhysicsAttachResult2D::Success) throw std::runtime_error{message};
}

void RequireBodyCommand(const physics::PhysicsBodyCommandResult2D result, const char* const message)
{
    if (result != physics::PhysicsBodyCommandResult2D::Success) throw std::runtime_error{message};
}

void RequireProgress(const ui::UiProgressResult result, const char* const message)
{
    if (result != ui::UiProgressResult::Success) throw std::runtime_error{message};
}
} // namespace

CombatGame::CombatGame(
    const physics::PhysicsComponentTypes2D physicsTypes,
    const audio::AudioComponentTypes2D audioTypes,
    std::filesystem::path projectRoot)
    : physicsTypes_{physicsTypes}
    , audioTypes_{audioTypes}
    , resources_{std::move(projectRoot)}
{
}

scene::EntityId CombatGame::CreatePhysicalEntity(
    scene::Scene& scene,
    const char* const semanticId,
    const scene::Vector2 position,
    physics::RigidBody2D body,
    physics::Collider2D collider)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = semanticId;
    descriptor.name = semanticId;
    descriptor.transform.position = position;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));
    static_cast<void>(scene.AddComponent(entity, physicsTypes_.rigidBody, std::move(body)));
    static_cast<void>(scene.AddComponent(entity, physicsTypes_.collider, std::move(collider)));
    return entity;
}

void CombatGame::OnStart(application::GameContext& context)
{
    horizontalAction_ = context.Actions().AddAxis1DAction(
        "combat.move.horizontal",
        input::InputControl::KeyA,
        input::InputControl::KeyD);
    verticalAction_ = context.Actions().AddAxis1DAction(
        "combat.move.vertical",
        input::InputControl::KeyS,
        input::InputControl::KeyW);
    attackAction_ = context.Actions().AddButtonAction("combat.attack");
    context.Actions().BindButton(attackAction_, input::InputControl::Space);
    restartAction_ = context.Actions().AddButtonAction("combat.restart");
    context.Actions().BindButton(restartAction_, input::InputControl::KeyR);

    const std::filesystem::path hitPath = resources_.ProjectRoot() / HitClipReference;
    std::error_code fileError{};
    const std::uint64_t encodedBytes = static_cast<std::uint64_t>(std::filesystem::file_size(hitPath, fileError));
    if (fileError || encodedBytes == 0U) throw std::runtime_error{"P2 hit SFX asset is unavailable."};

    assets::AudioClipResource clip{};
    clip.loadPolicy = assets::AudioClipLoadPolicy::Preload;
    clip.sampleRateHz = 48000U;
    clip.channelCount = 1U;
    clip.frameCount = 15360U;
    clip.encodedByteSize = encodedBytes;
    if (!resources_.PublishAudioClip(HitClipReference, clip).Succeeded())
        throw std::runtime_error{"P2 could not publish hit SFX metadata."};

    physics::RigidBody2D playerBody{};
    playerBody.type = physics::RigidBodyType2D::Dynamic;
    playerBody.gravityScale = 0.0F;
    playerBody.fixedRotation = true;
    playerBody.linearDamping = 0.0F;
    player_ = CreatePhysicalEntity(
        context.Scene(),
        "combat.player",
        {PlayerStartX, PlayerStartY},
        playerBody,
        MakeCircleCollider("combat.player.collider", PlayerRadius, 1U, 2U | 4U));

    physics::RigidBody2D enemyBody{};
    enemyBody.type = physics::RigidBodyType2D::Dynamic;
    enemyBody.gravityScale = 0.0F;
    enemyBody.fixedRotation = true;
    enemyBody.linearDamping = 2.0F;
    enemy_ = CreatePhysicalEntity(
        context.Scene(),
        "combat.enemy",
        {EnemyStartX, EnemyStartY},
        enemyBody,
        MakeCircleCollider("combat.enemy.collider", EnemyRadius, 2U, 1U | 4U));

    physics::RigidBody2D staticBody{};
    staticBody.type = physics::RigidBodyType2D::Static;
    const std::array<scene::EntityId, 5U> arena{
        CreatePhysicalEntity(
            context.Scene(), "combat.wall.left", {-7.75F, 0.0F}, staticBody,
            MakeBoxCollider("combat.wall.left.collider", {0.25F, 4.5F}, 4U, 1U | 2U)),
        CreatePhysicalEntity(
            context.Scene(), "combat.wall.right", {7.75F, 0.0F}, staticBody,
            MakeBoxCollider("combat.wall.right.collider", {0.25F, 4.5F}, 4U, 1U | 2U)),
        CreatePhysicalEntity(
            context.Scene(), "combat.wall.top", {0.0F, 4.25F}, staticBody,
            MakeBoxCollider("combat.wall.top.collider", {8.0F, 0.25F}, 4U, 1U | 2U)),
        CreatePhysicalEntity(
            context.Scene(), "combat.wall.bottom", {0.0F, -4.25F}, staticBody,
            MakeBoxCollider("combat.wall.bottom.collider", {8.0F, 0.25F}, 4U, 1U | 2U)),
        CreatePhysicalEntity(
            context.Scene(), "combat.obstacle", {-0.4F, -2.25F}, staticBody,
            MakeBoxCollider("combat.obstacle.collider", {1.0F, 0.40F}, 4U, 1U | 2U)),
    };

    scene::EntityDescriptor audioDescriptor{};
    audioDescriptor.semanticId = "combat.hit-audio";
    audioDescriptor.name = "Combat hit SFX";
    hitAudioEntity_ = context.Scene().CreateEntity(std::move(audioDescriptor));
    audio::AudioSource2D hitSource{};
    hitSource.clipReference = HitClipReference;
    hitSource.volume = 0.85F;
    hitSource.pitch = 1.0F;
    hitSource.group = audio::AudioGroup2D::Sfx;
    static_cast<void>(context.Scene().AddComponent(hitAudioEntity_, audioTypes_.source, std::move(hitSource)));

    physics::PhysicsWorldConfig2D physicsConfig{};
    physicsConfig.gravity = {0.0F, 0.0F};
    physics_ = std::make_unique<physics::PhysicsWorld2D>(context.Scene(), physicsTypes_, physicsConfig);
    physics_->Reserve(8U, 0U);
    physics_->ReserveOverlap(4U);
    physics_->ReserveEvents(16U, 4U);
    RequirePhysicsAttach(physics_->AttachEntity(player_), "P2 could not attach player physics.");
    RequirePhysicsAttach(physics_->AttachEntity(enemy_), "P2 could not attach enemy physics.");
    for (const scene::EntityId entity : arena)
        RequirePhysicsAttach(physics_->AttachEntity(entity), "P2 could not attach arena physics.");

    audio_ = std::make_unique<audio::AudioSystem2D>(context.Scene(), resources_, audioTypes_.source);
    if (!audio_->ReserveVoices(4U) || !audio_->ReserveEvents(32U))
        throw std::runtime_error{"P2 could not reserve bounded semantic audio storage."};
    audio::AudioVoiceLimits2D limits{};
    limits.globalLimit = 4U;
    limits.groupLimits[static_cast<std::size_t>(audio::AudioGroup2D::Sfx)] = 4U;
    limits.overflowPolicy = audio::AudioVoiceOverflowPolicy2D::RejectNew;
    if (!audio_->SetVoiceLimits(limits)) throw std::runtime_error{"P2 could not apply audio voice limits."};

    context.Ui().ReserveElements(2U);
    ui::UiElement health{};
    health.id = "hud.enemy-health";
    health.kind = ui::UiElementKind::Panel;
    health.bounds = {.x = 420U, .y = 18U, .width = 190U, .height = 18U};
    health.name = "Enemy health";
    if (context.Ui().AddElement(std::move(health)) != ui::UiActionResult::Success)
        throw std::runtime_error{"P2 could not create enemy health HUD."};
    RequireProgress(
        context.Ui().ConfigureProgress("hud.enemy-health", MaximumEnemyHealth, MaximumEnemyHealth),
        "P2 could not configure enemy health HUD.");

    ui::UiElement hits{};
    hits.id = "hud.hits";
    hits.kind = ui::UiElementKind::Panel;
    hits.bounds = {.x = 28U, .y = 18U, .width = 150U, .height = 18U};
    hits.name = "Accepted hits";
    if (context.Ui().AddElement(std::move(hits)) != ui::UiActionResult::Success)
        throw std::runtime_error{"P2 could not create hit counter HUD."};
    RequireProgress(context.Ui().ConfigureProgress("hud.hits", 0U, MaximumEnemyHealth),
        "P2 could not configure hit counter HUD.");
}

void CombatGame::ResetRound(application::GameContext& context)
{
    if (physics_ == nullptr || audio_ == nullptr) throw std::runtime_error{"P2 reset requested before startup."};

    if (lastHitVoice_.IsValid() && audio_->InspectVoice(lastHitVoice_).has_value())
        static_cast<void>(audio_->Stop(lastHitVoice_));
    lastHitVoice_ = {};

    RequireBodyCommand(physics_->Teleport(player_, {PlayerStartX, PlayerStartY}, 0.0F), "P2 player reset teleport failed.");
    RequireBodyCommand(physics_->SetLinearVelocity(player_, {}), "P2 player reset velocity failed.");
    RequireBodyCommand(physics_->Teleport(enemy_, {EnemyStartX, EnemyStartY}, 0.0F), "P2 enemy reset teleport failed.");
    RequireBodyCommand(physics_->SetLinearVelocity(enemy_, {}), "P2 enemy reset velocity failed.");

    facing_ = {1.0F, 0.0F};
    enemyHealth_ = MaximumEnemyHealth;
    acceptedHitCount_ = 0U;
    acceptedSwingCount_ = 0U;
    cooldownFramesRemaining_ = 0U;
    hitFlashFramesRemaining_ = 0U;
    lastSwingHitEnemy_ = false;
    UpdateEnemyHealthUi(context);
    RequireProgress(context.Ui().SetProgress("hud.hits", 0U, MaximumEnemyHealth),
        "P2 could not reset hit counter HUD.");
}

void CombatGame::UpdateEnemyHealthUi(application::GameContext& context)
{
    RequireProgress(
        context.Ui().SetProgress("hud.enemy-health", enemyHealth_, MaximumEnemyHealth),
        "P2 could not update enemy health HUD.");
}

void CombatGame::TryAttack(application::GameContext& context)
{
    if (physics_ == nullptr || audio_ == nullptr || cooldownFramesRemaining_ != 0U) return;

    ++acceptedSwingCount_;
    cooldownFramesRemaining_ = AttackCooldownFrames;
    lastSwingHitEnemy_ = false;

    physics::PhysicsBodyState2D playerState{};
    if (!physics_->TryGetBodyState(player_, playerState)) throw std::runtime_error{"P2 attack could not inspect player body."};

    physics::PhysicsBoxOverlapQuery2D query{};
    query.center = {
        playerState.position.x + facing_.x * AttackReach,
        playerState.position.y + facing_.y * AttackReach,
    };
    query.halfExtents = {AttackHalfWidth, AttackHalfHeight};
    query.layerBits = 1U;
    query.maskBits = 2U;

    std::array<physics::PhysicsOverlapHit2D, 4U> hits{};
    const physics::PhysicsOverlapReport2D report = physics_->OverlapBox(query, hits);
    if (report.result != physics::PhysicsQueryResult2D::Success)
        throw std::runtime_error{"P2 attack overlap query failed."};

    for (std::size_t index = 0U; index < report.hitCount; ++index)
    {
        if (hits[index].entity != enemy_ || enemyHealth_ == 0U) continue;

        --enemyHealth_;
        ++acceptedHitCount_;
        lastSwingHitEnemy_ = true;
        hitFlashFramesRemaining_ = HitFlashFrames;
        RequireBodyCommand(
            physics_->ApplyLinearImpulseToCenter(
                enemy_,
                {facing_.x * KnockbackImpulse, facing_.y * KnockbackImpulse}),
            "P2 enemy knockback impulse failed.");

        const audio::AudioPlayResult2D play = audio_->Play(hitAudioEntity_);
        if (play.result != audio::AudioCommandResult2D::Success)
            throw std::runtime_error{"P2 semantic hit SFX play failed."};
        lastHitVoice_ = play.voice;
        for (const audio::AudioEvent2D& event : audio_->Events())
        {
            if (event.voice == play.voice && event.type == audio::AudioEventType2D::Started)
            {
                ++semanticStartedEventCount_;
                break;
            }
        }

        UpdateEnemyHealthUi(context);
        RequireProgress(
            context.Ui().SetProgress("hud.hits", acceptedHitCount_, MaximumEnemyHealth),
            "P2 could not update hit counter HUD.");
        break;
    }
}

void CombatGame::OnFixedUpdate(application::GameContext& context, const application::FixedUpdate& update)
{
    if (physics_ == nullptr || audio_ == nullptr) throw std::runtime_error{"P2 systems are unavailable."};

    if (cooldownFramesRemaining_ > 0U) --cooldownFramesRemaining_;
    if (hitFlashFramesRemaining_ > 0U) --hitFlashFramesRemaining_;

    if (context.Actions().Pressed(restartAction_))
    {
        ResetRound(context);
    }

    float horizontal = context.Actions().Axis1D(horizontalAction_);
    float vertical = context.Actions().Axis1D(verticalAction_);
    const float lengthSquared = horizontal * horizontal + vertical * vertical;
    if (lengthSquared > 1.0F)
    {
        const float inverseLength = 1.0F / std::sqrt(lengthSquared);
        horizontal *= inverseLength;
        vertical *= inverseLength;
    }
    if (horizontal != 0.0F || vertical != 0.0F)
    {
        const float length = std::sqrt(horizontal * horizontal + vertical * vertical);
        facing_ = {horizontal / length, vertical / length};
    }

    RequireBodyCommand(
        physics_->SetLinearVelocity(player_, {horizontal * MoveSpeed, vertical * MoveSpeed}),
        "P2 player velocity command failed.");

    if (context.Actions().Pressed(attackAction_)) TryAttack(context);

    if (physics_->Step(std::chrono::duration<float>{update.fixedDelta}.count()) != physics::PhysicsStepResult2D::Success)
        throw std::runtime_error{"P2 Physics2D fixed step failed."};
    if (audio_->Step(update.fixedDelta).result != audio::AudioStepResult2D::Success)
        throw std::runtime_error{"P2 semantic audio fixed step failed."};
}

void CombatGame::OnStop(application::GameContext&)
{
    if (audio_ != nullptr && lastHitVoice_.IsValid() && audio_->InspectVoice(lastHitVoice_).has_value())
        static_cast<void>(audio_->Stop(lastHitVoice_));
}

std::uint32_t CombatGame::EnemyHealth() const noexcept { return enemyHealth_; }
std::uint32_t CombatGame::AcceptedHitCount() const noexcept { return acceptedHitCount_; }
std::uint32_t CombatGame::AcceptedSwingCount() const noexcept { return acceptedSwingCount_; }
std::uint32_t CombatGame::CooldownFramesRemaining() const noexcept { return cooldownFramesRemaining_; }
std::uint32_t CombatGame::HitFlashFramesRemaining() const noexcept { return hitFlashFramesRemaining_; }
scene::Vector2 CombatGame::Facing() const noexcept { return facing_; }
bool CombatGame::LastSwingHitEnemy() const noexcept { return lastSwingHitEnemy_; }
std::uint64_t CombatGame::SemanticStartedEventCount() const noexcept { return semanticStartedEventCount_; }

bool CombatGame::TryPlayerBodyState(physics::PhysicsBodyState2D& out) const noexcept
{
    return physics_ != nullptr && physics_->TryGetBodyState(player_, out);
}

bool CombatGame::TryEnemyBodyState(physics::PhysicsBodyState2D& out) const noexcept
{
    return physics_ != nullptr && physics_->TryGetBodyState(enemy_, out);
}

physics::PhysicsMetrics2D CombatGame::PhysicsMetrics() const noexcept
{
    return physics_ == nullptr ? physics::PhysicsMetrics2D{} : physics_->Metrics();
}

audio::AudioMetrics2D CombatGame::AudioMetrics() const noexcept
{
    return audio_ == nullptr ? audio::AudioMetrics2D{} : audio_->Metrics();
}

assets::ResourceRegistryStats CombatGame::ResourceStats() const noexcept
{
    return resources_.Stats();
}

audio::AudioSystem2D& CombatGame::Audio()
{
    if (audio_ == nullptr) throw std::runtime_error{"P2 audio requested before startup."};
    return *audio_;
}

const audio::AudioSystem2D& CombatGame::Audio() const
{
    if (audio_ == nullptr) throw std::runtime_error{"P2 audio requested before startup."};
    return *audio_;
}

assets::ResourceRegistry& CombatGame::Resources() noexcept { return resources_; }
const assets::ResourceRegistry& CombatGame::Resources() const noexcept { return resources_; }
} // namespace trace2d::examples
