#include <trace2d/scene/Scene.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] bool IsFiniteTransform(const Transform2D& transform) noexcept
{
    return std::isfinite(transform.position.x) && std::isfinite(transform.position.y) &&
           std::isfinite(transform.rotationRadians) && std::isfinite(transform.scale.x) &&
           std::isfinite(transform.scale.y);
}

[[nodiscard]] bool ComposeTransforms(
    const Transform2D& parent,
    const Transform2D& local,
    Transform2D& outTransform) noexcept
{
    const double cosine = std::cos(static_cast<double>(parent.rotationRadians));
    const double sine = std::sin(static_cast<double>(parent.rotationRadians));
    const double scaledX = static_cast<double>(local.position.x) * static_cast<double>(parent.scale.x);
    const double scaledY = static_cast<double>(local.position.y) * static_cast<double>(parent.scale.y);

    Transform2D result{};
    result.position.x = static_cast<float>(static_cast<double>(parent.position.x) + cosine * scaledX - sine * scaledY);
    result.position.y = static_cast<float>(static_cast<double>(parent.position.y) + sine * scaledX + cosine * scaledY);
    result.rotationRadians = parent.rotationRadians + local.rotationRadians;
    result.scale.x = parent.scale.x * local.scale.x;
    result.scale.y = parent.scale.y * local.scale.y;
    if (!IsFiniteTransform(result)) return false;
    outTransform = result;
    return true;
}

[[nodiscard]] bool MakeLocalFromWorld(
    const Transform2D& parentWorld,
    const Transform2D& world,
    Transform2D& outLocal) noexcept
{
    if (parentWorld.scale.x == 0.0F || parentWorld.scale.y == 0.0F) return false;

    const double dx = static_cast<double>(world.position.x) - static_cast<double>(parentWorld.position.x);
    const double dy = static_cast<double>(world.position.y) - static_cast<double>(parentWorld.position.y);
    const double cosine = std::cos(static_cast<double>(parentWorld.rotationRadians));
    const double sine = std::sin(static_cast<double>(parentWorld.rotationRadians));

    Transform2D local{};
    local.position.x = static_cast<float>((cosine * dx + sine * dy) / static_cast<double>(parentWorld.scale.x));
    local.position.y = static_cast<float>((-sine * dx + cosine * dy) / static_cast<double>(parentWorld.scale.y));
    local.rotationRadians = world.rotationRadians - parentWorld.rotationRadians;
    local.scale.x = world.scale.x / parentWorld.scale.x;
    local.scale.y = world.scale.y / parentWorld.scale.y;
    if (!IsFiniteTransform(local)) return false;
    outLocal = local;
    return true;
}
} // namespace

std::string_view ToString(const HierarchyResult result) noexcept
{
    switch (result)
    {
    case HierarchyResult::Success: return "success";
    case HierarchyResult::EntityNotFound: return "entity_not_found";
    case HierarchyResult::ParentNotFound: return "parent_not_found";
    case HierarchyResult::SelfParent: return "self_parent";
    case HierarchyResult::Cycle: return "cycle";
    case HierarchyResult::NonInvertibleParentTransform: return "non_invertible_parent_transform";
    case HierarchyResult::InvalidWorldTransform: return "invalid_world_transform";
    }
    return "unknown_hierarchy_result";
}

std::string_view ToString(const ComponentAttachResult result) noexcept
{
    switch (result)
    {
    case ComponentAttachResult::Success: return "success";
    case ComponentAttachResult::EntityNotFound: return "entity_not_found";
    case ComponentAttachResult::RegistryUnavailable: return "registry_unavailable";
    case ComponentAttachResult::RegistryNotFrozen: return "registry_not_frozen";
    case ComponentAttachResult::UnknownType: return "unknown_type";
    case ComponentAttachResult::NotAuthored: return "not_authored";
    case ComponentAttachResult::SchemaMismatch: return "schema_mismatch";
    case ComponentAttachResult::DuplicateComponent: return "duplicate_component";
    case ComponentAttachResult::ParseFailed: return "parse_failed";
    case ComponentAttachResult::ValidationFailed: return "validation_failed";
    }
    return "unknown_component_attach_result";
}

Entity::Entity(EntityDescriptor descriptor)
    : semanticId_{std::move(descriptor.semanticId)}
    , name_{std::move(descriptor.name)}
    , tags_{std::move(descriptor.tags)}
    , transform_{descriptor.transform}
{
    std::sort(tags_.begin(), tags_.end());
    tags_.erase(std::unique(tags_.begin(), tags_.end()), tags_.end());
}

std::string_view Entity::SemanticId() const noexcept { return semanticId_; }
std::string_view Entity::Name() const noexcept { return name_; }
const std::vector<std::string>& Entity::Tags() const noexcept { return tags_; }
bool Entity::HasTag(const std::string_view tag) const noexcept
{
    const auto iterator = std::lower_bound(tags_.begin(), tags_.end(), tag,
        [](const std::string& left, const std::string_view right) { return std::string_view{left} < right; });
    return iterator != tags_.end() && std::string_view{*iterator} == tag;
}
Transform2D& Entity::Transform() noexcept { return transform_; }
const Transform2D& Entity::Transform() const noexcept { return transform_; }
Transform2D& Entity::LocalTransform() noexcept { return transform_; }
const Transform2D& Entity::LocalTransform() const noexcept { return transform_; }
std::optional<EntityId> Entity::Parent() const noexcept { return parent_; }
const std::vector<EntityId>& Entity::Children() const noexcept { return children_; }
std::size_t Entity::ComponentCount() const noexcept { return components_.size(); }

Scene::Scene(SceneMetadata metadata) : metadata_{std::move(metadata)} {}
Scene::Scene(const ComponentRegistry& registry, SceneMetadata metadata)
    : registry_{&registry}, metadata_{std::move(metadata)}
{
}
SceneMetadata& Scene::Metadata() noexcept { return metadata_; }
const SceneMetadata& Scene::Metadata() const noexcept { return metadata_; }
const ComponentRegistry* Scene::Registry() const noexcept { return registry_; }

EntityId Scene::CreateEntity(EntityDescriptor descriptor)
{
    if (!descriptor.semanticId.empty() && FindBySemanticId(descriptor.semanticId).has_value())
        throw std::invalid_argument{"Scene semantic entity ID must be unique."};

    std::uint32_t index = EntityId::InvalidIndex;
    if (!freeSlots_.empty())
    {
        index = freeSlots_.back();
        freeSlots_.pop_back();
    }
    else
    {
        if (slots_.size() >= static_cast<std::size_t>(EntityId::InvalidIndex))
            throw std::length_error{"Scene exhausted the EntityId index space."};
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
    }

    Slot& slot = slots_[index];
    slot.entity = Entity{std::move(descriptor)};
    slot.alive = true;
    ++entityCount_;
    return EntityId{index, slot.generation};
}

bool Scene::DestroyEntity(const EntityId id) noexcept
{
    if (!Contains(id)) return false;
    return DestroyEntityRecursive(id);
}

bool Scene::DestroyEntityRecursive(const EntityId id) noexcept
{
    if (!Contains(id)) return false;
    Entity& entity = *slots_[id.index].entity;
    while (!entity.children_.empty())
    {
        const EntityId child = entity.children_.back();
        if (!DestroyEntityRecursive(child)) entity.children_.pop_back();
    }
    if (entity.parent_.has_value()) RemoveChild(*entity.parent_, id);

    Slot& slot = slots_[id.index];
    slot.entity.reset();
    slot.alive = false;
    --entityCount_;
    if (slot.generation != std::numeric_limits<std::uint32_t>::max())
    {
        ++slot.generation;
        freeSlots_.push_back(id.index);
    }
    return true;
}

bool Scene::Contains(const EntityId id) const noexcept
{
    if (!id.IsValid() || static_cast<std::size_t>(id.index) >= slots_.size()) return false;
    const Slot& slot = slots_[id.index];
    return slot.alive && slot.generation == id.generation && slot.entity.has_value();
}
Entity* Scene::TryGet(const EntityId id) noexcept { return Contains(id) ? &*slots_[id.index].entity : nullptr; }
const Entity* Scene::TryGet(const EntityId id) const noexcept { return Contains(id) ? &*slots_[id.index].entity : nullptr; }

std::optional<EntityId> Scene::FindBySemanticId(const std::string_view semanticId) const noexcept
{
    if (semanticId.empty()) return std::nullopt;
    for (std::size_t index = 0; index < slots_.size(); ++index)
    {
        const Slot& slot = slots_[index];
        if (!slot.alive || !slot.entity.has_value()) continue;
        if (slot.entity->SemanticId() == semanticId)
            return EntityId{static_cast<std::uint32_t>(index), slot.generation};
    }
    return std::nullopt;
}

bool Scene::WouldCreateCycle(const EntityId child, const EntityId parent) const noexcept
{
    EntityId current = parent;
    while (Contains(current))
    {
        if (current == child) return true;
        const Entity* entity = TryGet(current);
        if (entity == nullptr || !entity->parent_.has_value()) break;
        current = *entity->parent_;
    }
    return false;
}

bool Scene::EntityOrderLess(const EntityId left, const EntityId right) const noexcept
{
    const Entity* leftEntity = TryGet(left);
    const Entity* rightEntity = TryGet(right);
    if (leftEntity == nullptr || rightEntity == nullptr) return left.index < right.index;
    const std::string_view leftId = leftEntity->SemanticId();
    const std::string_view rightId = rightEntity->SemanticId();
    if (!leftId.empty() && !rightId.empty() && leftId != rightId) return leftId < rightId;
    if (leftId.empty() != rightId.empty()) return !leftId.empty();
    if (left.index != right.index) return left.index < right.index;
    return left.generation < right.generation;
}

void Scene::InsertChildOrdered(const EntityId parent, const EntityId child)
{
    Entity& parentEntity = *TryGet(parent);
    const auto iterator = std::lower_bound(parentEntity.children_.begin(), parentEntity.children_.end(), child,
        [this](const EntityId left, const EntityId right) { return EntityOrderLess(left, right); });
    parentEntity.children_.insert(iterator, child);
}

void Scene::RemoveChild(const EntityId parent, const EntityId child) noexcept
{
    Entity* parentEntity = TryGet(parent);
    if (parentEntity == nullptr) return;
    const auto iterator = std::find(parentEntity->children_.begin(), parentEntity->children_.end(), child);
    if (iterator != parentEntity->children_.end()) parentEntity->children_.erase(iterator);
}

HierarchyResult Scene::SetParent(
    const EntityId child,
    const std::optional<EntityId> parent,
    const ReparentMode mode) noexcept
{
    if (!Contains(child)) return HierarchyResult::EntityNotFound;
    if (parent.has_value() && !Contains(*parent)) return HierarchyResult::ParentNotFound;
    if (parent.has_value() && child == *parent) return HierarchyResult::SelfParent;
    if (parent.has_value() && WouldCreateCycle(child, *parent)) return HierarchyResult::Cycle;

    Entity& childEntity = *TryGet(child);
    if (childEntity.parent_ == parent) return HierarchyResult::Success;

    Transform2D preservedWorld{};
    if (mode == ReparentMode::KeepWorld && !TryGetWorldTransform(child, preservedWorld))
        return HierarchyResult::InvalidWorldTransform;

    Transform2D newLocal = childEntity.transform_;
    if (mode == ReparentMode::KeepWorld && parent.has_value())
    {
        Transform2D parentWorld{};
        if (!TryGetWorldTransform(*parent, parentWorld)) return HierarchyResult::InvalidWorldTransform;
        if (!MakeLocalFromWorld(parentWorld, preservedWorld, newLocal))
            return HierarchyResult::NonInvertibleParentTransform;
    }
    else if (mode == ReparentMode::KeepWorld)
    {
        newLocal = preservedWorld;
    }

    if (childEntity.parent_.has_value()) RemoveChild(*childEntity.parent_, child);
    childEntity.parent_ = parent;
    childEntity.transform_ = newLocal;
    if (parent.has_value()) InsertChildOrdered(*parent, child);
    return HierarchyResult::Success;
}

bool Scene::TryGetWorldTransform(const EntityId id, Transform2D& outTransform) const noexcept
{
    const Entity* entity = TryGet(id);
    if (entity == nullptr || !IsFiniteTransform(entity->transform_)) return false;
    Transform2D world = entity->transform_;
    std::optional<EntityId> parent = entity->parent_;
    std::size_t depth = 0;
    while (parent.has_value())
    {
        if (++depth > slots_.size()) return false;
        const Entity* parentEntity = TryGet(*parent);
        if (parentEntity == nullptr || !IsFiniteTransform(parentEntity->transform_)) return false;
        Transform2D composed{};
        if (!ComposeTransforms(parentEntity->transform_, world, composed)) return false;
        world = composed;
        parent = parentEntity->parent_;
    }
    outTransform = world;
    return true;
}

ComponentAttachResult Scene::AddAuthoredComponent(
    const EntityId entityId,
    const std::string_view typeId,
    const std::uint32_t schemaVersion,
    const ComponentAuthoringObject& authored,
    std::string& error)
{
    error.clear();
    if (!Contains(entityId)) return ComponentAttachResult::EntityNotFound;
    if (registry_ == nullptr) return ComponentAttachResult::RegistryUnavailable;
    if (!registry_->IsFrozen()) return ComponentAttachResult::RegistryNotFrozen;
    const std::optional<ComponentTypeIndex> index = registry_->FindIndexById(typeId);
    if (!index.has_value()) return ComponentAttachResult::UnknownType;
    const ComponentTypeDescriptor* descriptor = registry_->Descriptor(*index);
    if (descriptor == nullptr) return ComponentAttachResult::UnknownType;
    if (descriptor->componentClass != ComponentClass::Authored) return ComponentAttachResult::NotAuthored;
    if (descriptor->schemaVersion != schemaVersion) return ComponentAttachResult::SchemaMismatch;

    Entity& entity = *TryGet(entityId);
    const auto iterator = std::lower_bound(
        entity.components_.begin(), entity.components_.end(), *index,
        [](const ComponentInstance& instance, const ComponentTypeIndex value) { return instance.index_ < value; });
    if (iterator != entity.components_.end() && iterator->index_ == *index)
        return ComponentAttachResult::DuplicateComponent;

    void* object = descriptor->CreateDefault();
    if (!descriptor->ParseAuthored(authored, object, error))
    {
        descriptor->Destroy(object);
        return ComponentAttachResult::ParseFailed;
    }
    if (!descriptor->Validate(object, error))
    {
        descriptor->Destroy(object);
        return ComponentAttachResult::ValidationFailed;
    }

    entity.components_.insert(iterator, ComponentInstance{*index, descriptor, object});
    return ComponentAttachResult::Success;
}

bool Scene::HasComponentType(const EntityId entityId, const std::string_view typeId) const noexcept
{
    if (!Contains(entityId)) return false;
    if (typeId == "Transform2D") return true;
    if (registry_ == nullptr) return false;
    const std::optional<ComponentTypeIndex> index = registry_->FindIndexById(typeId);
    if (!index.has_value()) return false;
    const Entity& entity = *TryGet(entityId);
    const auto iterator = std::lower_bound(
        entity.components_.begin(), entity.components_.end(), *index,
        [](const ComponentInstance& instance, const ComponentTypeIndex value) { return instance.index_ < value; });
    return iterator != entity.components_.end() && iterator->index_ == *index;
}

std::vector<ComponentInspectionSnapshot> Scene::InspectComponents(const EntityId entityId) const
{
    std::vector<ComponentInspectionSnapshot> snapshots{};
    const Entity* entity = TryGet(entityId);
    if (entity == nullptr) return snapshots;
    snapshots.reserve(entity->components_.size());
    for (const ComponentInstance& instance : entity->components_)
    {
        ComponentInspectionSnapshot snapshot{};
        snapshot.typeId = instance.descriptor_->typeId;
        snapshot.schemaVersion = instance.descriptor_->schemaVersion;
        snapshot.componentClass = instance.descriptor_->componentClass;
        snapshot.fields = instance.descriptor_->Inspect(instance.data_);
        std::sort(snapshot.fields.begin(), snapshot.fields.end(), [](const auto& left, const auto& right) { return left.name < right.name; });
        snapshots.push_back(std::move(snapshot));
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const auto& left, const auto& right) { return left.typeId < right.typeId; });
    return snapshots;
}

std::vector<AuthoredComponentSnapshot> Scene::SerializeAuthoredComponents(
    const EntityId entityId,
    std::string& error) const
{
    error.clear();
    std::vector<AuthoredComponentSnapshot> snapshots{};
    const Entity* entity = TryGet(entityId);
    if (entity == nullptr)
    {
        error = "Entity is missing.";
        return snapshots;
    }
    for (const ComponentInstance& instance : entity->components_)
    {
        if (instance.descriptor_->componentClass != ComponentClass::Authored) continue;
        if (!instance.descriptor_->Validate(instance.data_, error))
        {
            snapshots.clear();
            return snapshots;
        }
        AuthoredComponentSnapshot snapshot{};
        snapshot.typeId = instance.descriptor_->typeId;
        snapshot.schemaVersion = instance.descriptor_->schemaVersion;
        snapshot.data = instance.descriptor_->SerializeAuthored(instance.data_);
        std::sort(snapshot.data.fields.begin(), snapshot.data.fields.end(), [](const auto& left, const auto& right) { return left.name < right.name; });
        snapshots.push_back(std::move(snapshot));
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const auto& left, const auto& right) { return left.typeId < right.typeId; });
    return snapshots;
}

std::size_t Scene::EntityCount() const noexcept { return entityCount_; }
} // namespace trace2d::scene
