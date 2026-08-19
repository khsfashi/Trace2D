#include <trace2d/physics/PhysicsComponents2D.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::physics
{
namespace
{
[[nodiscard]] scene::SemanticValue BooleanValue(const bool value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Boolean;
    semantic.booleanValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue FloatValue(const double value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Float;
    semantic.floatValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue SignedValue(const std::int64_t value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::SignedInteger;
    semantic.signedIntegerValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue TextValue(std::string value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Text;
    semantic.textValue = std::move(value);
    return semantic;
}

[[nodiscard]] scene::SemanticValue Float2Value(const scene::Vector2 value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Float2;
    semantic.vectorValue[0] = value.x;
    semantic.vectorValue[1] = value.y;
    return semantic;
}

[[nodiscard]] bool IsFiniteVector(const scene::Vector2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] std::string_view BodyTypeName(const RigidBodyType2D type) noexcept
{
    switch (type)
    {
    case RigidBodyType2D::Static: return "static";
    case RigidBodyType2D::Kinematic: return "kinematic";
    case RigidBodyType2D::Dynamic: return "dynamic";
    }
    return {};
}

[[nodiscard]] bool ParseBodyType(const std::string_view text, RigidBodyType2D& outType) noexcept
{
    if (text == "static") outType = RigidBodyType2D::Static;
    else if (text == "kinematic") outType = RigidBodyType2D::Kinematic;
    else if (text == "dynamic") outType = RigidBodyType2D::Dynamic;
    else return false;
    return true;
}

[[nodiscard]] std::string_view ColliderShapeName(const ColliderShape2D shape) noexcept
{
    switch (shape)
    {
    case ColliderShape2D::Box: return "box";
    case ColliderShape2D::Circle: return "circle";
    }
    return {};
}

[[nodiscard]] bool ParseColliderShape(const std::string_view text, ColliderShape2D& outShape) noexcept
{
    if (text == "box") outShape = ColliderShape2D::Box;
    else if (text == "circle") outShape = ColliderShape2D::Circle;
    else return false;
    return true;
}

[[nodiscard]] bool ValidateRigidBody(const RigidBody2D& body, std::string& error)
{
    if (BodyTypeName(body.type).empty())
    {
        error = "trace2d.rigidbody2d.body_type is invalid.";
        return false;
    }
    if (!IsFiniteVector(body.linearVelocity) || !std::isfinite(body.angularVelocity))
    {
        error = "trace2d.rigidbody2d velocities must be finite.";
        return false;
    }
    if (!std::isfinite(body.linearDamping) || body.linearDamping < 0.0F ||
        !std::isfinite(body.angularDamping) || body.angularDamping < 0.0F)
    {
        error = "trace2d.rigidbody2d damping values must be finite and non-negative.";
        return false;
    }
    if (!std::isfinite(body.gravityScale))
    {
        error = "trace2d.rigidbody2d.gravity_scale must be finite.";
        return false;
    }
    return true;
}

[[nodiscard]] bool ValidateCollider(const Collider2D& collider, std::string& error)
{
    if (collider.semanticId.empty() || collider.semanticId.size() >= PhysicsSemanticIdCapacity2D)
    {
        error = "trace2d.collider2d.semantic_id must contain 1..63 bytes.";
        return false;
    }
    if (ColliderShapeName(collider.shape).empty())
    {
        error = "trace2d.collider2d.shape is invalid.";
        return false;
    }
    if (!IsFiniteVector(collider.localOffset) || !IsFiniteVector(collider.halfExtents) ||
        collider.halfExtents.x <= 0.0F || collider.halfExtents.y <= 0.0F)
    {
        error = "trace2d.collider2d offsets/extents must be finite and half_extents must be positive.";
        return false;
    }
    if (!std::isfinite(collider.radius) || collider.radius <= 0.0F)
    {
        error = "trace2d.collider2d.radius must be finite and positive.";
        return false;
    }
    if (collider.layerBits == 0U)
    {
        error = "trace2d.collider2d.layer_bits must contain at least one bit.";
        return false;
    }
    if (!std::isfinite(collider.density) || collider.density < 0.0F ||
        !std::isfinite(collider.friction) || collider.friction < 0.0F ||
        !std::isfinite(collider.restitution) || collider.restitution < 0.0F || collider.restitution > 1.0F)
    {
        error = "trace2d.collider2d material values require density/friction >= 0 and restitution in [0,1].";
        return false;
    }
    return true;
}

[[nodiscard]] bool ReadFloat2(
    const scene::SemanticValue* value,
    scene::Vector2& destination,
    const std::string_view field,
    std::string& error)
{
    if (value == nullptr || value->kind != scene::SemanticValueKind::Float2)
    {
        error = std::string{field} + " must be float2.";
        return false;
    }
    destination = scene::Vector2{
        static_cast<float>(value->vectorValue[0]),
        static_cast<float>(value->vectorValue[1]),
    };
    return true;
}

[[nodiscard]] bool ReadFloat(
    const scene::SemanticValue* value,
    float& destination,
    const std::string_view field,
    std::string& error)
{
    if (value == nullptr || value->kind != scene::SemanticValueKind::Float)
    {
        error = std::string{field} + " must be float.";
        return false;
    }
    destination = static_cast<float>(value->floatValue);
    return true;
}

[[nodiscard]] bool ReadBoolean(
    const scene::SemanticValue* value,
    bool& destination,
    const std::string_view field,
    std::string& error)
{
    if (value == nullptr || value->kind != scene::SemanticValueKind::Boolean)
    {
        error = std::string{field} + " must be bool.";
        return false;
    }
    destination = value->booleanValue;
    return true;
}

[[nodiscard]] bool ReadUInt32(
    const scene::SemanticValue* value,
    std::uint32_t& destination,
    const std::string_view field,
    std::string& error)
{
    if (value == nullptr)
    {
        error = std::string{field} + " is missing.";
        return false;
    }
    std::uint64_t integer = 0U;
    if (value->kind == scene::SemanticValueKind::SignedInteger)
    {
        if (value->signedIntegerValue < 0)
        {
            error = std::string{field} + " must be non-negative.";
            return false;
        }
        integer = static_cast<std::uint64_t>(value->signedIntegerValue);
    }
    else if (value->kind == scene::SemanticValueKind::UnsignedInteger)
    {
        integer = value->unsignedIntegerValue;
    }
    else
    {
        error = std::string{field} + " must be an integer bitset.";
        return false;
    }
    if (integer > std::numeric_limits<std::uint32_t>::max())
    {
        error = std::string{field} + " exceeds the PHYS1 32-bit filter vocabulary.";
        return false;
    }
    destination = static_cast<std::uint32_t>(integer);
    return true;
}

[[nodiscard]] bool ParseRigidBody(
    const scene::ComponentAuthoringObject& authored,
    RigidBody2D& body,
    std::string& error)
{
    if (authored.fields.size() != 8U)
    {
        error = "trace2d.rigidbody2d expects exactly 8 authored fields.";
        return false;
    }
    const scene::SemanticValue* const bodyType = authored.Find("body_type");
    if (bodyType == nullptr || bodyType->kind != scene::SemanticValueKind::Text ||
        !ParseBodyType(bodyType->textValue, body.type))
    {
        error = "trace2d.rigidbody2d.body_type must be static, kinematic, or dynamic.";
        return false;
    }
    if (!ReadFloat2(authored.Find("linear_velocity"), body.linearVelocity, "trace2d.rigidbody2d.linear_velocity", error) ||
        !ReadFloat(authored.Find("angular_velocity"), body.angularVelocity, "trace2d.rigidbody2d.angular_velocity", error) ||
        !ReadFloat(authored.Find("linear_damping"), body.linearDamping, "trace2d.rigidbody2d.linear_damping", error) ||
        !ReadFloat(authored.Find("angular_damping"), body.angularDamping, "trace2d.rigidbody2d.angular_damping", error) ||
        !ReadFloat(authored.Find("gravity_scale"), body.gravityScale, "trace2d.rigidbody2d.gravity_scale", error) ||
        !ReadBoolean(authored.Find("fixed_rotation"), body.fixedRotation, "trace2d.rigidbody2d.fixed_rotation", error) ||
        !ReadBoolean(authored.Find("bullet"), body.bullet, "trace2d.rigidbody2d.bullet", error))
    {
        return false;
    }
    return ValidateRigidBody(body, error);
}

[[nodiscard]] scene::ComponentAuthoringObject SerializeRigidBody(const RigidBody2D& body)
{
    scene::ComponentAuthoringObject authored{};
    authored.fields.reserve(8U);
    authored.fields.push_back({"body_type", TextValue(std::string{BodyTypeName(body.type)})});
    authored.fields.push_back({"linear_velocity", Float2Value(body.linearVelocity)});
    authored.fields.push_back({"angular_velocity", FloatValue(body.angularVelocity)});
    authored.fields.push_back({"linear_damping", FloatValue(body.linearDamping)});
    authored.fields.push_back({"angular_damping", FloatValue(body.angularDamping)});
    authored.fields.push_back({"gravity_scale", FloatValue(body.gravityScale)});
    authored.fields.push_back({"fixed_rotation", BooleanValue(body.fixedRotation)});
    authored.fields.push_back({"bullet", BooleanValue(body.bullet)});
    return authored;
}

[[nodiscard]] bool ParseCollider(
    const scene::ComponentAuthoringObject& authored,
    Collider2D& collider,
    std::string& error)
{
    if (authored.fields.size() != 11U)
    {
        error = "trace2d.collider2d expects exactly 11 authored fields.";
        return false;
    }
    const scene::SemanticValue* const semanticId = authored.Find("semantic_id");
    const scene::SemanticValue* const shape = authored.Find("shape");
    if (semanticId == nullptr || semanticId->kind != scene::SemanticValueKind::Text)
    {
        error = "trace2d.collider2d.semantic_id must be text.";
        return false;
    }
    if (shape == nullptr || shape->kind != scene::SemanticValueKind::Text ||
        !ParseColliderShape(shape->textValue, collider.shape))
    {
        error = "trace2d.collider2d.shape must be box or circle.";
        return false;
    }
    collider.semanticId = semanticId->textValue;
    if (!ReadFloat2(authored.Find("local_offset"), collider.localOffset, "trace2d.collider2d.local_offset", error) ||
        !ReadFloat2(authored.Find("half_extents"), collider.halfExtents, "trace2d.collider2d.half_extents", error) ||
        !ReadFloat(authored.Find("radius"), collider.radius, "trace2d.collider2d.radius", error) ||
        !ReadUInt32(authored.Find("layer_bits"), collider.layerBits, "trace2d.collider2d.layer_bits", error) ||
        !ReadUInt32(authored.Find("mask_bits"), collider.maskBits, "trace2d.collider2d.mask_bits", error) ||
        !ReadBoolean(authored.Find("sensor"), collider.sensor, "trace2d.collider2d.sensor", error) ||
        !ReadFloat(authored.Find("density"), collider.density, "trace2d.collider2d.density", error) ||
        !ReadFloat(authored.Find("friction"), collider.friction, "trace2d.collider2d.friction", error) ||
        !ReadFloat(authored.Find("restitution"), collider.restitution, "trace2d.collider2d.restitution", error))
    {
        return false;
    }
    return ValidateCollider(collider, error);
}

[[nodiscard]] scene::ComponentAuthoringObject SerializeCollider(const Collider2D& collider)
{
    scene::ComponentAuthoringObject authored{};
    authored.fields.reserve(11U);
    authored.fields.push_back({"semantic_id", TextValue(collider.semanticId)});
    authored.fields.push_back({"shape", TextValue(std::string{ColliderShapeName(collider.shape)})});
    authored.fields.push_back({"local_offset", Float2Value(collider.localOffset)});
    authored.fields.push_back({"half_extents", Float2Value(collider.halfExtents)});
    authored.fields.push_back({"radius", FloatValue(collider.radius)});
    authored.fields.push_back({"layer_bits", SignedValue(collider.layerBits)});
    authored.fields.push_back({"mask_bits", SignedValue(collider.maskBits)});
    authored.fields.push_back({"sensor", BooleanValue(collider.sensor)});
    authored.fields.push_back({"density", FloatValue(collider.density)});
    authored.fields.push_back({"friction", FloatValue(collider.friction)});
    authored.fields.push_back({"restitution", FloatValue(collider.restitution)});
    return authored;
}

[[nodiscard]] std::vector<scene::ComponentInspectionField> Inspect(
    const scene::ComponentAuthoringObject& authored)
{
    std::vector<scene::ComponentInspectionField> fields{};
    fields.reserve(authored.fields.size());
    for (const auto& field : authored.fields)
    {
        fields.push_back({field.name, field.value});
    }
    return fields;
}
} // namespace

PhysicsComponentTypes2D RegisterPhysics2DComponents(scene::ComponentRegistry& registry)
{
    scene::ComponentRegistration<RigidBody2D> bodyRegistration{};
    bodyRegistration.typeId = "trace2d.rigidbody2d";
    bodyRegistration.schemaVersion = 1U;
    bodyRegistration.componentClass = scene::ComponentClass::Authored;
    bodyRegistration.parseAuthored = ParseRigidBody;
    bodyRegistration.validate = ValidateRigidBody;
    bodyRegistration.serializeAuthored = SerializeRigidBody;
    bodyRegistration.inspect = [](const RigidBody2D& body)
    {
        return Inspect(SerializeRigidBody(body));
    };
    const auto bodyType = registry.Register(std::move(bodyRegistration));

    scene::ComponentRegistration<Collider2D> colliderRegistration{};
    colliderRegistration.typeId = "trace2d.collider2d";
    colliderRegistration.schemaVersion = 1U;
    colliderRegistration.componentClass = scene::ComponentClass::Authored;
    colliderRegistration.parseAuthored = ParseCollider;
    colliderRegistration.validate = ValidateCollider;
    colliderRegistration.serializeAuthored = SerializeCollider;
    colliderRegistration.inspect = [](const Collider2D& collider)
    {
        return Inspect(SerializeCollider(collider));
    };
    const auto colliderType = registry.Register(std::move(colliderRegistration));

    return PhysicsComponentTypes2D{bodyType, colliderType};
}
} // namespace trace2d::physics
