#include <trace2d/scene/Components.hpp>
#include <trace2d/scene/Camera2D.hpp>

#include <string>
#include <utility>

namespace trace2d::scene
{
namespace
{
bool ParseVisibility(
    const ComponentAuthoringObject& authored,
    Visibility2D& visibility,
    std::string& error)
{
    if (authored.fields.size() != 1U || authored.fields.front().name != "visible")
    {
        error = "trace2d.visibility2d expects exactly the 'visible' field.";
        return false;
    }
    const SemanticValue& value = authored.fields.front().value;
    if (value.kind != SemanticValueKind::Boolean)
    {
        error = "trace2d.visibility2d.visible must be a boolean.";
        return false;
    }
    visibility.visible = value.booleanValue;
    return true;
}

bool ValidateVisibility(const Visibility2D&, std::string&) { return true; }

ComponentAuthoringObject SerializeVisibility(const Visibility2D& visibility)
{
    SemanticValue value{};
    value.kind = SemanticValueKind::Boolean;
    value.booleanValue = visibility.visible;
    ComponentAuthoringObject authored{};
    authored.fields.push_back(ComponentAuthoringField{.name = "visible", .value = std::move(value)});
    return authored;
}

std::vector<ComponentInspectionField> InspectVisibility(const Visibility2D& visibility)
{
    SemanticValue value{};
    value.kind = SemanticValueKind::Boolean;
    value.booleanValue = visibility.visible;
    return {ComponentInspectionField{.name = "visible", .value = std::move(value)}};
}
} // namespace

SceneComponentTypes RegisterSceneComponents(ComponentRegistry& registry)
{
    ComponentRegistration<Visibility2D> visibility{};
    visibility.typeId = "trace2d.visibility2d";
    visibility.schemaVersion = 1;
    visibility.componentClass = ComponentClass::Authored;
    visibility.parseAuthored = ParseVisibility;
    visibility.validate = ValidateVisibility;
    visibility.serializeAuthored = SerializeVisibility;
    visibility.inspect = InspectVisibility;

    SceneComponentTypes types{};
    types.visibility = registry.Register(std::move(visibility));
    types.camera = RegisterCamera2DComponent(registry);
    return types;
}
} // namespace trace2d::scene
