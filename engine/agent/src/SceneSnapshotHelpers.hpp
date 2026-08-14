#pragma once

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/scene/Scene.hpp>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <utility>

namespace trace2d::agent::detail
{
inline ComponentFieldSnapshot MakeFloatField(std::string name, const float value)
{
    FieldValue fieldValue{};
    fieldValue.kind = FieldValueKind::Float;
    fieldValue.floatValue = value;
    return ComponentFieldSnapshot{.name = std::move(name), .value = std::move(fieldValue)};
}

inline ComponentFieldSnapshot MakeUnsignedField(std::string name, const std::uint64_t value)
{
    FieldValue fieldValue{};
    fieldValue.kind = FieldValueKind::UnsignedInteger;
    fieldValue.unsignedIntegerValue = value;
    return ComponentFieldSnapshot{.name = std::move(name), .value = std::move(fieldValue)};
}

inline ComponentFieldSnapshot MakeStringField(std::string name, std::string value)
{
    FieldValue fieldValue{};
    fieldValue.kind = FieldValueKind::String;
    fieldValue.stringValue = std::move(value);
    return ComponentFieldSnapshot{.name = std::move(name), .value = std::move(fieldValue)};
}

inline Transform2DSnapshot MakeTransformSnapshot(const scene::Transform2D& transform) noexcept
{
    return Transform2DSnapshot{
        .position = Vector2Snapshot{.x = transform.position.x, .y = transform.position.y},
        .rotationRadians = transform.rotationRadians,
        .scale = Vector2Snapshot{.x = transform.scale.x, .y = transform.scale.y},
    };
}

inline std::string FormatSemanticVector(const scene::SemanticValue& value, const std::size_t count)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << '[' << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index != 0U) stream << ',';
        stream << value.vectorValue[index];
    }
    stream << ']';
    return stream.str();
}

inline FieldValue MakeFieldValue(const scene::SemanticValue& value)
{
    FieldValue result{};
    switch (value.kind)
    {
    case scene::SemanticValueKind::Boolean:
        result.kind = FieldValueKind::Boolean;
        result.booleanValue = value.booleanValue;
        break;
    case scene::SemanticValueKind::SignedInteger:
        result.kind = FieldValueKind::SignedInteger;
        result.signedIntegerValue = value.signedIntegerValue;
        break;
    case scene::SemanticValueKind::UnsignedInteger:
        result.kind = FieldValueKind::UnsignedInteger;
        result.unsignedIntegerValue = value.unsignedIntegerValue;
        break;
    case scene::SemanticValueKind::Float:
        result.kind = FieldValueKind::Float;
        result.floatValue = static_cast<float>(value.floatValue);
        break;
    case scene::SemanticValueKind::Text:
    case scene::SemanticValueKind::EntityReference:
    case scene::SemanticValueKind::ResourceReference:
    case scene::SemanticValueKind::EnumName:
        result.kind = FieldValueKind::String;
        result.stringValue = value.textValue;
        break;
    case scene::SemanticValueKind::Float2:
        result.kind = FieldValueKind::String;
        result.stringValue = FormatSemanticVector(value, 2U);
        break;
    case scene::SemanticValueKind::Float4:
        result.kind = FieldValueKind::String;
        result.stringValue = FormatSemanticVector(value, 4U);
        break;
    }
    return result;
}

inline ComponentSnapshot MakeTransformComponent(const scene::Transform2D& transform)
{
    ComponentSnapshot component{};
    component.type = "Transform2D";
    component.schemaVersion = 1;
    component.authored = true;
    component.fields.reserve(5);
    component.fields.push_back(MakeFloatField("position.x", transform.position.x));
    component.fields.push_back(MakeFloatField("position.y", transform.position.y));
    component.fields.push_back(MakeFloatField("rotation_radians", transform.rotationRadians));
    component.fields.push_back(MakeFloatField("scale.x", transform.scale.x));
    component.fields.push_back(MakeFloatField("scale.y", transform.scale.y));
    return component;
}

inline ComponentSnapshot MakeHierarchyComponent(
    const scene::Scene& sceneValue,
    const scene::EntityId id,
    const scene::Entity& entity,
    const scene::Transform2D& world)
{
    ComponentSnapshot component{};
    component.type = "Hierarchy2D";
    component.schemaVersion = 1;
    component.authored = true;
    component.fields.reserve(7U + entity.Children().size());
    if (entity.Parent().has_value())
    {
        const scene::Entity* parent = sceneValue.TryGet(*entity.Parent());
        component.fields.push_back(MakeStringField(
            "parent",
            parent == nullptr ? std::string{} : std::string{parent->SemanticId()}));
    }
    component.fields.push_back(MakeUnsignedField("child_count", entity.Children().size()));
    for (std::size_t childIndex = 0; childIndex < entity.Children().size(); ++childIndex)
    {
        const scene::Entity* child = sceneValue.TryGet(entity.Children()[childIndex]);
        component.fields.push_back(MakeStringField(
            "children[" + std::to_string(childIndex) + "]",
            child == nullptr ? std::string{} : std::string{child->SemanticId()}));
    }
    component.fields.push_back(MakeFloatField("world.position.x", world.position.x));
    component.fields.push_back(MakeFloatField("world.position.y", world.position.y));
    component.fields.push_back(MakeFloatField("world.rotation_radians", world.rotationRadians));
    component.fields.push_back(MakeFloatField("world.scale.x", world.scale.x));
    component.fields.push_back(MakeFloatField("world.scale.y", world.scale.y));
    return component;
}

inline ComponentSnapshot MakeComponentSnapshot(const scene::ComponentInspectionSnapshot& source)
{
    ComponentSnapshot component{};
    component.type = source.typeId;
    component.schemaVersion = source.schemaVersion;
    component.authored = source.componentClass == scene::ComponentClass::Authored;
    component.fields.reserve(source.fields.size());
    for (const scene::ComponentInspectionField& field : source.fields)
    {
        component.fields.push_back(ComponentFieldSnapshot{
            .name = field.name,
            .value = MakeFieldValue(field.value),
        });
    }
    return component;
}

inline EntitySnapshot MakeEntitySnapshot(
    const scene::Scene& sceneValue,
    const scene::EntityId id,
    const scene::Entity& entity)
{
    EntitySnapshot snapshot{};
    snapshot.handle = EntityHandleSnapshot{.index = id.index, .generation = id.generation};
    snapshot.semanticId = entity.SemanticId();
    snapshot.name = entity.Name();
    snapshot.tags = entity.Tags();

    if (entity.Parent().has_value())
    {
        snapshot.parentHandle = EntityHandleSnapshot{.index = entity.Parent()->index, .generation = entity.Parent()->generation};
        if (const scene::Entity* parent = sceneValue.TryGet(*entity.Parent()); parent != nullptr && !parent->SemanticId().empty())
            snapshot.parentSemanticId = std::string{parent->SemanticId()};
    }
    snapshot.childHandles.reserve(entity.Children().size());
    snapshot.childSemanticIds.reserve(entity.Children().size());
    for (const scene::EntityId childId : entity.Children())
    {
        snapshot.childHandles.push_back(EntityHandleSnapshot{.index = childId.index, .generation = childId.generation});
        if (const scene::Entity* child = sceneValue.TryGet(childId); child != nullptr)
            snapshot.childSemanticIds.emplace_back(child->SemanticId());
        else
            snapshot.childSemanticIds.emplace_back();
    }

    snapshot.transform = MakeTransformSnapshot(entity.LocalTransform());
    scene::Transform2D world{};
    const bool hasWorld = sceneValue.TryGetWorldTransform(id, world);
    snapshot.worldTransform = hasWorld ? MakeTransformSnapshot(world) : snapshot.transform;
    snapshot.bounds = std::nullopt;

    const std::vector<scene::ComponentInspectionSnapshot> inspected = sceneValue.InspectComponents(id);
    const bool hasHierarchy = entity.Parent().has_value() || !entity.Children().empty();
    snapshot.components.reserve(inspected.size() + (hasHierarchy ? 2U : 1U));
    snapshot.components.push_back(MakeTransformComponent(entity.LocalTransform()));
    if (hasHierarchy)
        snapshot.components.push_back(MakeHierarchyComponent(sceneValue, id, entity, hasWorld ? world : entity.LocalTransform()));
    for (const scene::ComponentInspectionSnapshot& component : inspected)
        snapshot.components.push_back(MakeComponentSnapshot(component));
    std::sort(snapshot.components.begin(), snapshot.components.end(), [](const ComponentSnapshot& left, const ComponentSnapshot& right)
    {
        return left.type < right.type;
    });
    return snapshot;
}
} // namespace trace2d::agent::detail
