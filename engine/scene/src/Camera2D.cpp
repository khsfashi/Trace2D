#include <trace2d/scene/Camera2D.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] SemanticValue BooleanValue(const bool value)
{
    SemanticValue semantic{};
    semantic.kind = SemanticValueKind::Boolean;
    semantic.booleanValue = value;
    return semantic;
}

[[nodiscard]] SemanticValue SignedValue(const std::int64_t value)
{
    SemanticValue semantic{};
    semantic.kind = SemanticValueKind::SignedInteger;
    semantic.signedIntegerValue = value;
    return semantic;
}

[[nodiscard]] SemanticValue FloatValue(const double value)
{
    SemanticValue semantic{};
    semantic.kind = SemanticValueKind::Float;
    semantic.floatValue = value;
    return semantic;
}

[[nodiscard]] SemanticValue TextValue(std::string value)
{
    SemanticValue semantic{};
    semantic.kind = SemanticValueKind::Text;
    semantic.textValue = std::move(value);
    return semantic;
}

[[nodiscard]] bool ValidateCamera(const Camera2D& camera, std::string& error)
{
    if (!std::isfinite(camera.verticalSize) || camera.verticalSize <= 0.0F)
    {
        error = "trace2d.camera2d.vertical_size must be finite and greater than zero.";
        return false;
    }
    if (camera.targetViewport.empty())
    {
        error = "trace2d.camera2d.target_viewport must not be empty.";
        return false;
    }
    return true;
}

[[nodiscard]] bool ParseCamera(
    const ComponentAuthoringObject& authored,
    Camera2D& camera,
    std::string& error)
{
    if (authored.fields.size() != 4U)
    {
        error = "trace2d.camera2d expects exactly enabled, priority, vertical_size, and target_viewport.";
        return false;
    }

    const SemanticValue* const enabled = authored.Find("enabled");
    const SemanticValue* const priority = authored.Find("priority");
    const SemanticValue* const verticalSize = authored.Find("vertical_size");
    const SemanticValue* const targetViewport = authored.Find("target_viewport");
    if (enabled == nullptr || priority == nullptr || verticalSize == nullptr || targetViewport == nullptr)
    {
        error = "trace2d.camera2d has an unknown, duplicate, or missing authored field.";
        return false;
    }
    if (enabled->kind != SemanticValueKind::Boolean ||
        priority->kind != SemanticValueKind::SignedInteger ||
        verticalSize->kind != SemanticValueKind::Float ||
        targetViewport->kind != SemanticValueKind::Text)
    {
        error = "trace2d.camera2d field types must be bool, signed integer, float, and text respectively.";
        return false;
    }
    if (priority->signedIntegerValue < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        priority->signedIntegerValue > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        error = "trace2d.camera2d.priority is outside the int32 range.";
        return false;
    }

    camera.enabled = enabled->booleanValue;
    camera.priority = static_cast<std::int32_t>(priority->signedIntegerValue);
    camera.verticalSize = static_cast<float>(verticalSize->floatValue);
    camera.targetViewport = targetViewport->textValue;
    return true;
}

[[nodiscard]] ComponentAuthoringObject SerializeCamera(const Camera2D& camera)
{
    ComponentAuthoringObject authored{};
    authored.fields.reserve(4U);
    authored.fields.push_back({"enabled", BooleanValue(camera.enabled)});
    authored.fields.push_back({"priority", SignedValue(camera.priority)});
    authored.fields.push_back({"vertical_size", FloatValue(camera.verticalSize)});
    authored.fields.push_back({"target_viewport", TextValue(camera.targetViewport)});
    return authored;
}

[[nodiscard]] std::vector<ComponentInspectionField> InspectCamera(const Camera2D& camera)
{
    std::vector<ComponentInspectionField> fields{};
    fields.reserve(4U);
    fields.push_back({"enabled", BooleanValue(camera.enabled)});
    fields.push_back({"priority", SignedValue(camera.priority)});
    fields.push_back({"vertical_size", FloatValue(camera.verticalSize)});
    fields.push_back({"target_viewport", TextValue(camera.targetViewport)});
    return fields;
}
} // namespace

ComponentTypeHandle<Camera2D> RegisterCamera2DComponent(ComponentRegistry& registry)
{
    ComponentRegistration<Camera2D> registration{};
    registration.typeId = "trace2d.camera2d";
    registration.schemaVersion = 1U;
    registration.componentClass = ComponentClass::Authored;
    registration.parseAuthored = ParseCamera;
    registration.validate = ValidateCamera;
    registration.serializeAuthored = SerializeCamera;
    registration.inspect = InspectCamera;
    return registry.Register(std::move(registration));
}
} // namespace trace2d::scene
