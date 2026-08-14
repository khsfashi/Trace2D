#include <trace2d/agent/Inspection.hpp>

#include "SceneSnapshotHelpers.hpp"

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
    result.error = InspectionError{.code = code, .message = std::move(message)};
    return result;
}
} // namespace

std::string_view ToString(const InspectionErrorCode code) noexcept
{
    switch (code)
    {
    case InspectionErrorCode::RuntimeUnavailable: return "runtime_unavailable";
    case InspectionErrorCode::SceneUnavailable: return "scene_unavailable";
    }
    return "unknown_inspection_error";
}

std::string_view ToString(const FieldValueKind kind) noexcept
{
    switch (kind)
    {
    case FieldValueKind::Boolean: return "bool";
    case FieldValueKind::SignedInteger: return "int64";
    case FieldValueKind::UnsignedInteger: return "uint64";
    case FieldValueKind::Float: return "float";
    case FieldValueKind::String: return "string";
    case FieldValueKind::Float2: return "float2";
    case FieldValueKind::Float4: return "float4";
    case FieldValueKind::EntityReference: return "entity_ref";
    case FieldValueKind::ResourceReference: return "resource_ref";
    case FieldValueKind::EnumName: return "enum";
    }
    return "unknown";
}

AgentFacade::AgentFacade(
    const runtime::FixedStepRuntime* runtime,
    const scene::Scene* scene,
    ui::UiDocument* uiDocument) noexcept
    : runtime_{runtime}, scene_{scene}, ui_{uiDocument}
{
}

void AgentFacade::BindRuntime(const runtime::FixedStepRuntime* runtime) noexcept { runtime_ = runtime; }
void AgentFacade::BindScene(const scene::Scene* scene) noexcept { scene_ = scene; }
void AgentFacade::BindUi(ui::UiDocument* uiDocument) noexcept { ui_ = uiDocument; }

InspectionResult AgentFacade::Inspect() const
{
    if (runtime_ == nullptr)
        return MakeError(InspectionErrorCode::RuntimeUnavailable, "No active runtime is bound to the agent facade.");
    if (scene_ == nullptr)
        return MakeError(InspectionErrorCode::SceneUnavailable, "No active scene is bound to the agent facade.");

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
    scene_->ForEachEntity([this, &snapshot](const scene::EntityId id, const scene::Entity& entity)
    {
        snapshot.scene.entities.push_back(detail::MakeEntitySnapshot(*scene_, id, entity));
    });

    InspectionResult result{};
    result.snapshot = std::move(snapshot);
    return result;
}
} // namespace trace2d::agent
