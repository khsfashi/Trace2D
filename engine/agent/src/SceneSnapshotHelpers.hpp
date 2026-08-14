#pragma once

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/scene/Scene.hpp>

#include <algorithm>
#include <cstddef>
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

inline Transform2DSnapshot MakeTransformSnapshot(const scene::Transform2D& transform) noexcept
{
    return Transform2DSnapshot{
        .position = Vector2Snapshot{.x = transform.position.x, .y = transform.position.y},
        .rotationRadians = transform.rotationRadians,
        .scale = Vector2Snapshot{.x = transform.scale.x, .y = transform.scale.y},
    };
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
        result.kind = FieldValueKind::String;
        result.stringValue = value.textValue;
        break;
    case scene::SemanticValueKind::Float2:
        result.kind = FieldValueKind::Float2;
        result.vectorValue[0] = static_cast<float>(value.vectorValue[0]);
        result.vectorValue[1] = static_cast<float>(value.vectorValue[1]);
        break;
    case scene::SemanticValueKind::Float4:
        result.kind = FieldValueKind::Float4;
        for (std::size_t index = 0; index < result.vectorValue.size(); ++index)
            result.vectorValue[index] = static_cast<float>(value.vectorValue[index]);
        break;
    case scene::SemanticValueKind::EntityReference:
        result.kind = FieldValueKind::EntityReference;
        result.stringValue = value.textValue;
        break;
    case scene::SemanticValueKind::ResourceReference:
        result.kind = FieldValueKind::ResourceReference;
        result.stringValue = value.textValue;
        break;
    case scene::SemanticValueKind::EnumName:
        result.kind = FieldValueKind::EnumName;
        result.stringValue = value.textValue;
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
        snapshot.parentHandle = EntityHandleSnapshot{
            .index = entity.Parent()->index,
            .generation = entity.Parent()->generation,
        };
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
    snapshot.worldTransform = sceneValue.TryGetWorldTransform(id, world)
        ? MakeTransformSnapshot(world)
        : snapshot.transform;
    snapshot.bounds = std::nullopt;

    const std::vector<scene::ComponentInspectionSnapshot> inspected = sceneValue.InspectComponents(id);
    snapshot.components.reserve(inspected.size() + 1U);
    snapshot.components.push_back(MakeTransformComponent(entity.LocalTransform()));
    for (const scene::ComponentInspectionSnapshot& component : inspected)
        snapshot.components.push_back(MakeComponentSnapshot(component));
    std::sort(snapshot.components.begin(), snapshot.components.end(), [](const ComponentSnapshot& left, const ComponentSnapshot& right)
    {
        return left.type < right.type;
    });
    return snapshot;
}
} // namespace trace2d::agent::detail
