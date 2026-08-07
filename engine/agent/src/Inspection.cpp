#include <trace2d/agent/Inspection.hpp>

#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/Scene.hpp>

#include <utility>

namespace trace2d::agent
{
namespace
{
InspectionResult MakeError(const InspectionErrorCode code, std::string message)
{
    InspectionResult result{};
    result.error = InspectionError{
        .code = code,
        .message = std::move(message),
    };
    return result;
}

ComponentFieldSnapshot MakeFloatField(std::string name, const float value)
{
    FieldValue fieldValue{};
    fieldValue.kind = FieldValueKind::Float;
    fieldValue.floatValue = value;

    return ComponentFieldSnapshot{
        .name = std::move(name),
        .value = std::move(fieldValue),
    };
}

Transform2DSnapshot MakeTransformSnapshot(const scene::Transform2D& transform) noexcept
{
    return Transform2DSnapshot{
        .position = Vector2Snapshot{
            .x = transform.position.x,
            .y = transform.position.y,
        },
        .rotationRadians = transform.rotationRadians,
        .scale = Vector2Snapshot{
            .x = transform.scale.x,
            .y = transform.scale.y,
        },
    };
}

ComponentSnapshot MakeTransformComponent(const scene::Transform2D& transform)
{
    ComponentSnapshot component{};
    component.type = "Transform2D";
    component.fields.reserve(5);
    component.fields.push_back(MakeFloatField("position.x", transform.position.x));
    component.fields.push_back(MakeFloatField("position.y", transform.position.y));
    component.fields.push_back(MakeFloatField("rotation_radians", transform.rotationRadians));
    component.fields.push_back(MakeFloatField("scale.x", transform.scale.x));
    component.fields.push_back(MakeFloatField("scale.y", transform.scale.y));
    return component;
}
} // namespace

std::string_view ToString(const InspectionErrorCode code) noexcept
{
    switch (code)
    {
    case InspectionErrorCode::RuntimeUnavailable:
        return "runtime_unavailable";
    case InspectionErrorCode::SceneUnavailable:
        return "scene_unavailable";
    }

    return "unknown_inspection_error";
}

std::string_view ToString(const FieldValueKind kind) noexcept
{
    switch (kind)
    {
    case FieldValueKind::Boolean:
        return "bool";
    case FieldValueKind::SignedInteger:
        return "int64";
    case FieldValueKind::UnsignedInteger:
        return "uint64";
    case FieldValueKind::Float:
        return "float";
    case FieldValueKind::String:
        return "string";
    }

    return "unknown";
}

AgentFacade::AgentFacade(
    const runtime::FixedStepRuntime* runtime,
    const scene::Scene* scene) noexcept
    : runtime_{runtime}
    , scene_{scene}
{
}

void AgentFacade::BindRuntime(const runtime::FixedStepRuntime* runtime) noexcept
{
    runtime_ = runtime;
}

void AgentFacade::BindScene(const scene::Scene* scene) noexcept
{
    scene_ = scene;
}

InspectionResult AgentFacade::Inspect() const
{
    if (runtime_ == nullptr)
    {
        return MakeError(
            InspectionErrorCode::RuntimeUnavailable,
            "No active runtime is bound to the agent facade.");
    }

    if (scene_ == nullptr)
    {
        return MakeError(
            InspectionErrorCode::SceneUnavailable,
            "No active scene is bound to the agent facade.");
    }

    const runtime::RuntimeState runtimeState = runtime_->State();

    InspectionSnapshot snapshot{};
    snapshot.runtime.frame = runtimeState.frame;
    snapshot.runtime.seed = runtimeState.seed;
    snapshot.runtime.fixedStepNanoseconds = runtime_->Config().fixedTimestep.count();
    snapshot.runtime.simulationTimeNanoseconds = runtimeState.simulationTime.count();

    const scene::SceneMetadata& metadata = scene_->Metadata();
    snapshot.scene.semanticId = metadata.semanticId;
    snapshot.scene.name = metadata.name;
    snapshot.scene.entities.reserve(scene_->EntityCount());

    scene_->ForEachEntity(
        [&snapshot](const scene::EntityId id, const scene::Entity& entity)
        {
            EntitySnapshot entitySnapshot{};
            entitySnapshot.handle = EntityHandleSnapshot{
                .index = id.index,
                .generation = id.generation,
            };
            entitySnapshot.semanticId = entity.SemanticId();
            entitySnapshot.name = entity.Name();
            entitySnapshot.tags = entity.Tags();

            const scene::Transform2D& transform = entity.Transform();
            entitySnapshot.transform = MakeTransformSnapshot(transform);

            // Bounds are deliberately nullable until a renderer/physics component owns
            // authoritative bounds. Explicit absence is more reliable than deriving
            // gameplay state from guessed sprite or coordinate data.
            entitySnapshot.bounds = std::nullopt;

            entitySnapshot.components.reserve(1);
            entitySnapshot.components.push_back(MakeTransformComponent(transform));

            snapshot.scene.entities.push_back(std::move(entitySnapshot));
        });

    InspectionResult result{};
    result.snapshot = std::move(snapshot);
    return result;
}
} // namespace trace2d::agent
