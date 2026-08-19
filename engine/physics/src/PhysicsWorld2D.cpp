#include <trace2d/physics/PhysicsWorld2D.hpp>

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::physics
{
namespace
{
[[nodiscard]] bool IsFiniteVector(const scene::Vector2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool IsUnitScale(const scene::Transform2D& transform) noexcept
{
    return transform.scale.x == 1.0F && transform.scale.y == 1.0F;
}

[[nodiscard]] bool IsFiniteTransform(const scene::Transform2D& transform) noexcept
{
    return IsFiniteVector(transform.position) && std::isfinite(transform.rotationRadians) &&
        std::isfinite(transform.scale.x) && std::isfinite(transform.scale.y);
}

[[nodiscard]] bool IsValidBody(const RigidBody2D& body) noexcept
{
    const bool typeValid = body.type == RigidBodyType2D::Static ||
        body.type == RigidBodyType2D::Kinematic || body.type == RigidBodyType2D::Dynamic;
    return typeValid && IsFiniteVector(body.linearVelocity) && std::isfinite(body.angularVelocity) &&
        std::isfinite(body.linearDamping) && body.linearDamping >= 0.0F &&
        std::isfinite(body.angularDamping) && body.angularDamping >= 0.0F &&
        std::isfinite(body.gravityScale);
}

[[nodiscard]] bool IsValidCollider(const Collider2D& collider) noexcept
{
    const bool shapeValid = collider.shape == ColliderShape2D::Box || collider.shape == ColliderShape2D::Circle;
    return !collider.semanticId.empty() && collider.semanticId.size() < PhysicsSemanticIdCapacity2D && shapeValid &&
        IsFiniteVector(collider.localOffset) && IsFiniteVector(collider.halfExtents) &&
        collider.halfExtents.x > 0.0F && collider.halfExtents.y > 0.0F &&
        std::isfinite(collider.radius) && collider.radius > 0.0F && collider.layerBits != 0U &&
        std::isfinite(collider.density) && collider.density >= 0.0F &&
        std::isfinite(collider.friction) && collider.friction >= 0.0F &&
        std::isfinite(collider.restitution) && collider.restitution >= 0.0F && collider.restitution <= 1.0F;
}

[[nodiscard]] b2BodyType ToBox2DBodyType(const RigidBodyType2D type) noexcept
{
    switch (type)
    {
    case RigidBodyType2D::Static: return b2_staticBody;
    case RigidBodyType2D::Kinematic: return b2_kinematicBody;
    case RigidBodyType2D::Dynamic: return b2_dynamicBody;
    }
    return b2_staticBody;
}
} // namespace

class PhysicsWorld2D::Impl final
{
public:
    struct Binding final
    {
        scene::EntityId entity{};
        RigidBodyType2D bodyType{RigidBodyType2D::Static};
        b2BodyId bodyId{};
        b2ShapeId shapeId{};
        std::array<char, PhysicsSemanticIdCapacity2D> semanticId{};
        std::uint8_t semanticIdLength{0U};

        [[nodiscard]] std::string_view SemanticId() const noexcept
        {
            return {semanticId.data(), semanticIdLength};
        }
    };

    struct ScratchHit final
    {
        Binding* binding{nullptr};
        b2Vec2 point{};
        b2Vec2 normal{};
        float fraction{0.0F};
    };

    Impl(scene::Scene& scene, const PhysicsComponentTypes2D componentTypes, const PhysicsWorldConfig2D config)
        : scene_{scene}, componentTypes_{componentTypes}, subStepCount_{config.subStepCount}
    {
        if (!componentTypes_.rigidBody.IsValid() || !componentTypes_.collider.IsValid())
        {
            throw std::invalid_argument{"PhysicsWorld2D requires valid Physics2D component handles."};
        }
        if (!IsFiniteVector(config.gravity) || subStepCount_ <= 0)
        {
            throw std::invalid_argument{"PhysicsWorld2D requires finite gravity and a positive sub-step count."};
        }

        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = b2Vec2{config.gravity.x, config.gravity.y};
        worldDef.enableContinuous = true;
        worldId_ = b2CreateWorld(&worldDef);
        if (!b2World_IsValid(worldId_))
        {
            throw std::runtime_error{"Box2D failed to create the PhysicsWorld2D backend world."};
        }
    }

    ~Impl()
    {
        if (b2World_IsValid(worldId_))
        {
            b2DestroyWorld(worldId_);
            worldId_ = b2_nullWorldId;
        }
    }

    void Reserve(const std::size_t bodyCapacity, const std::size_t rayHitCapacity)
    {
        bindings_.reserve(bodyCapacity);
        if (rayHitCapacity > rayScratch_.size()) rayScratch_.resize(rayHitCapacity);
    }

    [[nodiscard]] Binding* FindBinding(const scene::EntityId entity) noexcept
    {
        const auto iterator = std::find_if(bindings_.begin(), bindings_.end(), [entity](const auto& binding)
        {
            return binding->entity == entity;
        });
        return iterator == bindings_.end() ? nullptr : iterator->get();
    }

    [[nodiscard]] const Binding* FindBinding(const scene::EntityId entity) const noexcept
    {
        const auto iterator = std::find_if(bindings_.begin(), bindings_.end(), [entity](const auto& binding)
        {
            return binding->entity == entity;
        });
        return iterator == bindings_.end() ? nullptr : iterator->get();
    }

    [[nodiscard]] bool Contains(const scene::EntityId entity) const noexcept
    {
        return scene_.Contains(entity) && FindBinding(entity) != nullptr;
    }

    [[nodiscard]] PhysicsAttachResult2D Attach(const scene::EntityId entity)
    {
        if (!scene_.Contains(entity)) return PhysicsAttachResult2D::EntityNotFound;
        if (FindBinding(entity) != nullptr) return PhysicsAttachResult2D::AlreadyAttached;

        scene::Entity* const sceneEntity = scene_.TryGet(entity);
        const RigidBody2D* const body = scene_.TryGetComponent(entity, componentTypes_.rigidBody);
        const Collider2D* const collider = scene_.TryGetComponent(entity, componentTypes_.collider);
        if (body == nullptr) return PhysicsAttachResult2D::MissingRigidBody;
        if (collider == nullptr) return PhysicsAttachResult2D::MissingCollider;
        if (sceneEntity == nullptr) return PhysicsAttachResult2D::EntityNotFound;
        if (sceneEntity->Parent().has_value()) return PhysicsAttachResult2D::ParentedEntityUnsupported;
        if (!IsUnitScale(sceneEntity->Transform())) return PhysicsAttachResult2D::NonUnitScaleUnsupported;
        if (!IsFiniteTransform(sceneEntity->Transform())) return PhysicsAttachResult2D::InvalidTransform;
        if (!IsValidBody(*body) || !IsValidCollider(*collider)) return PhysicsAttachResult2D::InvalidComponent;
        if (!b2World_IsValid(worldId_)) return PhysicsAttachResult2D::BackendFailure;

        // Finish all allocation-capable setup before creating backend objects. Once capacity is
        // available, moving a unique_ptr into the vector is non-throwing, so a failed allocation
        // cannot orphan a Box2D body that Trace2D no longer has a binding for.
        if (bindings_.size() == bindings_.capacity()) bindings_.reserve(bindings_.size() + 1U);
        auto binding = std::make_unique<Binding>();
        binding->entity = entity;
        binding->bodyType = body->type;
        binding->semanticIdLength = static_cast<std::uint8_t>(collider->semanticId.size());
        std::memcpy(binding->semanticId.data(), collider->semanticId.data(), collider->semanticId.size());

        const scene::Transform2D& transform = sceneEntity->Transform();
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = ToBox2DBodyType(body->type);
        bodyDef.position = b2Vec2{transform.position.x, transform.position.y};
        bodyDef.rotation = b2MakeRot(transform.rotationRadians);
        bodyDef.linearVelocity = b2Vec2{body->linearVelocity.x, body->linearVelocity.y};
        bodyDef.angularVelocity = body->angularVelocity;
        bodyDef.linearDamping = body->linearDamping;
        bodyDef.angularDamping = body->angularDamping;
        bodyDef.gravityScale = body->gravityScale;
        bodyDef.fixedRotation = body->fixedRotation;
        bodyDef.isBullet = body->bullet;
        bodyDef.userData = binding.get();
        binding->bodyId = b2CreateBody(worldId_, &bodyDef);
        if (!b2Body_IsValid(binding->bodyId)) return PhysicsAttachResult2D::BackendFailure;

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.userData = binding.get();
        shapeDef.density = collider->density;
        shapeDef.material.friction = collider->friction;
        shapeDef.material.restitution = collider->restitution;
        shapeDef.filter.categoryBits = static_cast<std::uint64_t>(collider->layerBits);
        shapeDef.filter.maskBits = static_cast<std::uint64_t>(collider->maskBits);
        shapeDef.isSensor = collider->sensor;
        shapeDef.enableSensorEvents = false;
        shapeDef.enableContactEvents = false;
        shapeDef.enableHitEvents = false;

        if (collider->shape == ColliderShape2D::Circle)
        {
            const b2Circle circle{
                b2Vec2{collider->localOffset.x, collider->localOffset.y},
                collider->radius,
            };
            binding->shapeId = b2CreateCircleShape(binding->bodyId, &shapeDef, &circle);
        }
        else
        {
            const b2Polygon polygon = b2MakeOffsetBox(
                collider->halfExtents.x,
                collider->halfExtents.y,
                b2Vec2{collider->localOffset.x, collider->localOffset.y},
                b2Rot_identity);
            binding->shapeId = b2CreatePolygonShape(binding->bodyId, &shapeDef, &polygon);
        }

        if (!b2Shape_IsValid(binding->shapeId))
        {
            b2DestroyBody(binding->bodyId);
            return PhysicsAttachResult2D::BackendFailure;
        }

        bindings_.push_back(std::move(binding));
        return PhysicsAttachResult2D::Success;
    }

    [[nodiscard]] bool Detach(const scene::EntityId entity) noexcept
    {
        const auto iterator = std::find_if(bindings_.begin(), bindings_.end(), [entity](const auto& binding)
        {
            return binding->entity == entity;
        });
        if (iterator == bindings_.end()) return false;
        if (b2Body_IsValid((*iterator)->bodyId)) b2DestroyBody((*iterator)->bodyId);
        bindings_.erase(iterator);
        return true;
    }

    void PruneInvalidBindings() noexcept
    {
        std::size_t index = 0U;
        while (index < bindings_.size())
        {
            Binding& binding = *bindings_[index];
            if (!scene_.Contains(binding.entity))
            {
                if (b2Body_IsValid(binding.bodyId)) b2DestroyBody(binding.bodyId);
                bindings_.erase(bindings_.begin() + static_cast<std::ptrdiff_t>(index));
                ++stalePruneCount_;
                continue;
            }

            const scene::Entity* const entity = scene_.TryGet(binding.entity);
            if (entity == nullptr || entity->Parent().has_value() || !IsUnitScale(entity->Transform()) ||
                !IsFiniteTransform(entity->Transform()))
            {
                if (b2Body_IsValid(binding.bodyId)) b2DestroyBody(binding.bodyId);
                bindings_.erase(bindings_.begin() + static_cast<std::ptrdiff_t>(index));
                ++unsupportedTransformPruneCount_;
                continue;
            }
            ++index;
        }
    }

    [[nodiscard]] PhysicsStepResult2D Step(const float fixedDeltaSeconds) noexcept
    {
        if (!std::isfinite(fixedDeltaSeconds) || fixedDeltaSeconds <= 0.0F)
            return PhysicsStepResult2D::InvalidDelta;
        if (!b2World_IsValid(worldId_)) return PhysicsStepResult2D::BackendInvalid;

        PruneInvalidBindings();
        b2World_Step(worldId_, fixedDeltaSeconds, subStepCount_);
        ++fixedStepCount_;

        for (const auto& ownedBinding : bindings_)
        {
            const Binding& binding = *ownedBinding;
            if (binding.bodyType == RigidBodyType2D::Static || !b2Body_IsValid(binding.bodyId)) continue;
            scene::Entity* const entity = scene_.TryGet(binding.entity);
            if (entity == nullptr) continue;
            const b2Vec2 position = b2Body_GetPosition(binding.bodyId);
            const b2Rot rotation = b2Body_GetRotation(binding.bodyId);
            entity->Transform().position = scene::Vector2{position.x, position.y};
            entity->Transform().rotationRadians = b2Atan2(rotation.s, rotation.c);
        }
        return PhysicsStepResult2D::Success;
    }

    [[nodiscard]] bool TryGetBodyState(const scene::EntityId entity, PhysicsBodyState2D& outState) const noexcept
    {
        if (!scene_.Contains(entity)) return false;
        const Binding* const binding = FindBinding(entity);
        if (binding == nullptr || !b2Body_IsValid(binding->bodyId)) return false;
        const b2Vec2 position = b2Body_GetPosition(binding->bodyId);
        const b2Rot rotation = b2Body_GetRotation(binding->bodyId);
        const b2Vec2 linearVelocity = b2Body_GetLinearVelocity(binding->bodyId);
        outState.position = scene::Vector2{position.x, position.y};
        outState.rotationRadians = b2Atan2(rotation.s, rotation.c);
        outState.linearVelocity = scene::Vector2{linearVelocity.x, linearVelocity.y};
        outState.angularVelocity = b2Body_GetAngularVelocity(binding->bodyId);
        outState.awake = b2Body_IsAwake(binding->bodyId);
        return true;
    }

    [[nodiscard]] PhysicsRaycastReport2D Raycast(
        const PhysicsRaycastQuery2D& query,
        const std::span<PhysicsRaycastHit2D> output) noexcept
    {
        ++rayQueryCount_;
        if (!b2World_IsValid(worldId_))
            return {PhysicsQueryResult2D::BackendInvalid, 0U, 0U};
        if (!IsFiniteVector(query.origin) || !IsFiniteVector(query.translation) || query.layerBits == 0U ||
            (query.translation.x == 0.0F && query.translation.y == 0.0F))
        {
            return {PhysicsQueryResult2D::InvalidInput, 0U, 0U};
        }

        // Queries are observable gameplay state too. Remove stale/unsupported bindings before
        // traversing the backend so a destroyed Scene slot or hierarchy mutation cannot leak an
        // old backend shape through a query performed before the next fixed simulation step.
        PruneInvalidBindings();

        struct RayContext final
        {
            std::vector<ScratchHit>& scratch;
            std::size_t totalHits{0U};
        } context{rayScratch_};

        const auto callback = [](const b2ShapeId shapeId, const b2Vec2 point, const b2Vec2 normal,
                                 const float fraction, void* rawContext) -> float
        {
            auto& rayContext = *static_cast<RayContext*>(rawContext);
            auto* const binding = static_cast<Binding*>(b2Shape_GetUserData(shapeId));
            if (binding == nullptr) return 1.0F;
            if (rayContext.totalHits < rayContext.scratch.size())
            {
                rayContext.scratch[rayContext.totalHits] = ScratchHit{binding, point, normal, fraction};
            }
            ++rayContext.totalHits;
            return 1.0F;
        };

        const b2QueryFilter filter{
            static_cast<std::uint64_t>(query.layerBits),
            static_cast<std::uint64_t>(query.maskBits),
        };
        (void)b2World_CastRay(
            worldId_,
            b2Vec2{query.origin.x, query.origin.y},
            b2Vec2{query.translation.x, query.translation.y},
            filter,
            callback,
            &context);

        if (context.totalHits > rayScratch_.size() || context.totalHits > output.size())
        {
            ++rayCapacityFailureCount_;
            return {PhysicsQueryResult2D::CapacityExceeded, 0U, context.totalHits};
        }

        const auto begin = rayScratch_.begin();
        const auto end = begin + static_cast<std::ptrdiff_t>(context.totalHits);
        std::sort(begin, end, [](const ScratchHit& left, const ScratchHit& right)
        {
            if (left.fraction != right.fraction) return left.fraction < right.fraction;
            const std::string_view leftSemantic = left.binding->SemanticId();
            const std::string_view rightSemantic = right.binding->SemanticId();
            if (leftSemantic != rightSemantic) return leftSemantic < rightSemantic;
            if (left.binding->entity.index != right.binding->entity.index)
                return left.binding->entity.index < right.binding->entity.index;
            return left.binding->entity.generation < right.binding->entity.generation;
        });

        for (std::size_t index = 0U; index < context.totalHits; ++index)
        {
            const ScratchHit& source = rayScratch_[index];
            PhysicsRaycastHit2D& destination = output[index];
            destination.entity = source.binding->entity;
            destination.point = scene::Vector2{source.point.x, source.point.y};
            destination.normal = scene::Vector2{source.normal.x, source.normal.y};
            destination.fraction = source.fraction;
            destination.colliderSemanticId.fill('\0');
            destination.colliderSemanticIdLength = source.binding->semanticIdLength;
            std::memcpy(
                destination.colliderSemanticId.data(),
                source.binding->semanticId.data(),
                source.binding->semanticIdLength);
        }
        return {PhysicsQueryResult2D::Success, context.totalHits, context.totalHits};
    }

    [[nodiscard]] PhysicsMetrics2D Metrics() const noexcept
    {
        return PhysicsMetrics2D{
            bindings_.size(),
            bindings_.capacity(),
            rayScratch_.size(),
            fixedStepCount_,
            stalePruneCount_,
            unsupportedTransformPruneCount_,
            rayQueryCount_,
            rayCapacityFailureCount_,
        };
    }

private:
    scene::Scene& scene_;
    PhysicsComponentTypes2D componentTypes_{};
    int subStepCount_{4};
    b2WorldId worldId_{};
    std::vector<std::unique_ptr<Binding>> bindings_{};
    std::vector<ScratchHit> rayScratch_{};
    std::uint64_t fixedStepCount_{0U};
    std::uint64_t stalePruneCount_{0U};
    std::uint64_t unsupportedTransformPruneCount_{0U};
    std::uint64_t rayQueryCount_{0U};
    std::uint64_t rayCapacityFailureCount_{0U};
};

std::string_view ToString(const PhysicsAttachResult2D result) noexcept
{
    switch (result)
    {
    case PhysicsAttachResult2D::Success: return "success";
    case PhysicsAttachResult2D::EntityNotFound: return "entity_not_found";
    case PhysicsAttachResult2D::MissingRigidBody: return "missing_rigidbody";
    case PhysicsAttachResult2D::MissingCollider: return "missing_collider";
    case PhysicsAttachResult2D::AlreadyAttached: return "already_attached";
    case PhysicsAttachResult2D::ParentedEntityUnsupported: return "parented_entity_unsupported";
    case PhysicsAttachResult2D::NonUnitScaleUnsupported: return "non_unit_scale_unsupported";
    case PhysicsAttachResult2D::InvalidTransform: return "invalid_transform";
    case PhysicsAttachResult2D::InvalidComponent: return "invalid_component";
    case PhysicsAttachResult2D::BackendFailure: return "backend_failure";
    }
    return "unknown";
}

std::string_view ToString(const PhysicsStepResult2D result) noexcept
{
    switch (result)
    {
    case PhysicsStepResult2D::Success: return "success";
    case PhysicsStepResult2D::InvalidDelta: return "invalid_delta";
    case PhysicsStepResult2D::BackendInvalid: return "backend_invalid";
    }
    return "unknown";
}

std::string_view ToString(const PhysicsQueryResult2D result) noexcept
{
    switch (result)
    {
    case PhysicsQueryResult2D::Success: return "success";
    case PhysicsQueryResult2D::InvalidInput: return "invalid_input";
    case PhysicsQueryResult2D::CapacityExceeded: return "capacity_exceeded";
    case PhysicsQueryResult2D::BackendInvalid: return "backend_invalid";
    }
    return "unknown";
}

PhysicsWorld2D::PhysicsWorld2D(
    scene::Scene& scene,
    const PhysicsComponentTypes2D componentTypes,
    const PhysicsWorldConfig2D config)
    : impl_{std::make_unique<Impl>(scene, componentTypes, config)}
{
}

PhysicsWorld2D::~PhysicsWorld2D() = default;

void PhysicsWorld2D::Reserve(const std::size_t bodyCapacity, const std::size_t rayHitCapacity)
{
    impl_->Reserve(bodyCapacity, rayHitCapacity);
}

PhysicsAttachResult2D PhysicsWorld2D::AttachEntity(const scene::EntityId entity)
{
    return impl_->Attach(entity);
}

bool PhysicsWorld2D::DetachEntity(const scene::EntityId entity) noexcept
{
    return impl_->Detach(entity);
}

bool PhysicsWorld2D::Contains(const scene::EntityId entity) const noexcept
{
    return impl_->Contains(entity);
}

PhysicsStepResult2D PhysicsWorld2D::Step(const float fixedDeltaSeconds) noexcept
{
    return impl_->Step(fixedDeltaSeconds);
}

bool PhysicsWorld2D::TryGetBodyState(const scene::EntityId entity, PhysicsBodyState2D& outState) const noexcept
{
    return impl_->TryGetBodyState(entity, outState);
}

PhysicsRaycastReport2D PhysicsWorld2D::Raycast(
    const PhysicsRaycastQuery2D& query,
    const std::span<PhysicsRaycastHit2D> output) noexcept
{
    return impl_->Raycast(query, output);
}

PhysicsMetrics2D PhysicsWorld2D::Metrics() const noexcept
{
    return impl_->Metrics();
}
} // namespace trace2d::physics
