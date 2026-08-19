#pragma once

#include <trace2d/scene/Components.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::scene
{
struct EntityId final
{
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index{InvalidIndex};
    std::uint32_t generation{0};
    [[nodiscard]] bool IsValid() const noexcept { return index != InvalidIndex && generation != 0; }
    [[nodiscard]] bool operator==(const EntityId&) const noexcept = default;
};

struct Vector2 final
{
    float x{0.0F};
    float y{0.0F};
    [[nodiscard]] bool operator==(const Vector2&) const noexcept = default;
};

struct Transform2D final
{
    Vector2 position{};
    float rotationRadians{0.0F};
    Vector2 scale{1.0F, 1.0F};
    [[nodiscard]] bool operator==(const Transform2D&) const noexcept = default;
};

struct EntityDescriptor final
{
    std::string semanticId{};
    std::string name{};
    std::vector<std::string> tags{};
    Transform2D transform{};
};

struct SceneMetadata final
{
    std::string semanticId{};
    std::string name{};
    [[nodiscard]] bool operator==(const SceneMetadata&) const noexcept = default;
};

enum class ReparentMode : std::uint8_t { KeepLocal = 0, KeepWorld = 1 };
enum class HierarchyResult : std::uint8_t
{
    Success = 0,
    EntityNotFound,
    ParentNotFound,
    SelfParent,
    Cycle,
    NonInvertibleParentTransform,
    InvalidWorldTransform,
};
[[nodiscard]] std::string_view ToString(HierarchyResult result) noexcept;

enum class ComponentAttachResult : std::uint8_t
{
    Success = 0,
    EntityNotFound,
    RegistryUnavailable,
    RegistryNotFrozen,
    UnknownType,
    NotAuthored,
    SchemaMismatch,
    DuplicateComponent,
    ParseFailed,
    ValidationFailed,
};
[[nodiscard]] std::string_view ToString(ComponentAttachResult result) noexcept;

struct AuthoredComponentSnapshot final
{
    std::string typeId{};
    std::uint32_t schemaVersion{0};
    ComponentAuthoringObject data{};
};

template <typename T>
struct ComponentHandle final
{
    EntityId entity{};
    ComponentTypeHandle<T> type{};
    [[nodiscard]] bool IsValid() const noexcept { return entity.IsValid() && type.IsValid(); }
    [[nodiscard]] bool operator==(const ComponentHandle&) const noexcept = default;
};

class Entity final
{
public:
    Entity(const Entity&) = default;
    Entity& operator=(const Entity&) = default;
    Entity(Entity&&) noexcept = default;
    Entity& operator=(Entity&&) noexcept = default;
    ~Entity() = default;

    [[nodiscard]] std::string_view SemanticId() const noexcept;
    [[nodiscard]] std::string_view Name() const noexcept;
    [[nodiscard]] const std::vector<std::string>& Tags() const noexcept;
    [[nodiscard]] bool HasTag(std::string_view tag) const noexcept;

    [[nodiscard]] Transform2D& Transform() noexcept;
    [[nodiscard]] const Transform2D& Transform() const noexcept;
    [[nodiscard]] Transform2D& LocalTransform() noexcept;
    [[nodiscard]] const Transform2D& LocalTransform() const noexcept;

    [[nodiscard]] std::optional<EntityId> Parent() const noexcept;
    [[nodiscard]] const std::vector<EntityId>& Children() const noexcept;
    [[nodiscard]] std::size_t ComponentCount() const noexcept;

private:
    explicit Entity(EntityDescriptor descriptor);

    std::string semanticId_{};
    std::string name_{};
    std::vector<std::string> tags_{};
    Transform2D transform_{};
    std::optional<EntityId> parent_{};
    std::vector<EntityId> children_{};
    std::vector<ComponentInstance> components_{};

    friend class Scene;
    friend class TweenBindingSystem2D;
};

class Scene final
{
public:
    Scene() = default;
    explicit Scene(SceneMetadata metadata);
    explicit Scene(const ComponentRegistry& registry, SceneMetadata metadata = {});

    [[nodiscard]] SceneMetadata& Metadata() noexcept;
    [[nodiscard]] const SceneMetadata& Metadata() const noexcept;
    [[nodiscard]] const ComponentRegistry* Registry() const noexcept;

    [[nodiscard]] EntityId CreateEntity(EntityDescriptor descriptor);
    [[nodiscard]] bool DestroyEntity(EntityId id) noexcept;

    [[nodiscard]] bool Contains(EntityId id) const noexcept;
    [[nodiscard]] Entity* TryGet(EntityId id) noexcept;
    [[nodiscard]] const Entity* TryGet(EntityId id) const noexcept;
    [[nodiscard]] std::optional<EntityId> FindBySemanticId(std::string_view semanticId) const noexcept;

    [[nodiscard]] HierarchyResult SetParent(
        EntityId child,
        std::optional<EntityId> parent,
        ReparentMode mode = ReparentMode::KeepLocal) noexcept;
    [[nodiscard]] bool TryGetWorldTransform(EntityId id, Transform2D& outTransform) const noexcept;

    [[nodiscard]] ComponentAttachResult AddAuthoredComponent(
        EntityId entity,
        std::string_view typeId,
        std::uint32_t schemaVersion,
        const ComponentAuthoringObject& authored,
        std::string& error);

    template <typename T>
    [[nodiscard]] T& AddComponent(EntityId entityId, ComponentTypeHandle<T> type, T value = {})
    {
        if (!Contains(entityId)) throw std::invalid_argument{"Cannot add a component to a stale or missing entity."};
        if (registry_ == nullptr || type.owner_ != registry_) throw std::invalid_argument{"Component handle belongs to a different registry."};
        const ComponentTypeDescriptor* descriptor = registry_->Descriptor(type.index_);
        if (descriptor == nullptr) throw std::invalid_argument{"Component type handle is invalid."};

        Entity& entity = *TryGet(entityId);
        const auto iterator = std::lower_bound(
            entity.components_.begin(), entity.components_.end(), type.index_,
            [](const ComponentInstance& instance, const ComponentTypeIndex index) { return instance.index_ < index; });
        if (iterator != entity.components_.end() && iterator->index_ == type.index_)
            throw std::invalid_argument{"Entity already has this component type."};

        std::string error{};
        if (!descriptor->Validate(&value, error))
            throw std::invalid_argument{error.empty() ? "Component validation failed." : error};

        auto inserted = entity.components_.insert(
            iterator,
            ComponentInstance{type.index_, descriptor, new T{std::move(value)}});
        return *static_cast<T*>(inserted->data_);
    }

    template <typename T>
    [[nodiscard]] T* TryGetComponent(EntityId entityId, ComponentTypeHandle<T> type) noexcept
    {
        if (!Contains(entityId) || registry_ == nullptr || type.owner_ != registry_) return nullptr;
        Entity& entity = *TryGet(entityId);
        const auto iterator = std::lower_bound(
            entity.components_.begin(), entity.components_.end(), type.index_,
            [](const ComponentInstance& instance, const ComponentTypeIndex index) { return instance.index_ < index; });
        if (iterator == entity.components_.end() || iterator->index_ != type.index_) return nullptr;
        return static_cast<T*>(iterator->data_);
    }

    template <typename T>
    [[nodiscard]] const T* TryGetComponent(EntityId entityId, ComponentTypeHandle<T> type) const noexcept
    {
        if (!Contains(entityId) || registry_ == nullptr || type.owner_ != registry_) return nullptr;
        const Entity& entity = *TryGet(entityId);
        const auto iterator = std::lower_bound(
            entity.components_.begin(), entity.components_.end(), type.index_,
            [](const ComponentInstance& instance, const ComponentTypeIndex index) { return instance.index_ < index; });
        if (iterator == entity.components_.end() || iterator->index_ != type.index_) return nullptr;
        return static_cast<const T*>(iterator->data_);
    }

    template <typename T>
    [[nodiscard]] ComponentHandle<T> MakeComponentHandle(EntityId entityId, ComponentTypeHandle<T> type) const noexcept
    {
        return TryGetComponent(entityId, type) == nullptr ? ComponentHandle<T>{} : ComponentHandle<T>{entityId, type};
    }

    template <typename T>
    [[nodiscard]] T* Resolve(ComponentHandle<T> handle) noexcept
    {
        return TryGetComponent(handle.entity, handle.type);
    }

    template <typename T>
    [[nodiscard]] const T* Resolve(ComponentHandle<T> handle) const noexcept
    {
        return TryGetComponent(handle.entity, handle.type);
    }

    [[nodiscard]] bool HasComponentType(EntityId entity, std::string_view typeId) const noexcept;
    [[nodiscard]] std::vector<ComponentInspectionSnapshot> InspectComponents(EntityId entity) const;
    [[nodiscard]] std::vector<AuthoredComponentSnapshot> SerializeAuthoredComponents(
        EntityId entity,
        std::string& error) const;

    [[nodiscard]] std::size_t EntityCount() const noexcept;

    template <typename Visitor>
    void ForEachEntity(Visitor&& visitor)
    {
        auto&& callable = visitor;
        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            Slot& slot = slots_[index];
            if (!slot.alive) continue;
            callable(EntityId{static_cast<std::uint32_t>(index), slot.generation}, *slot.entity);
        }
    }

    template <typename Visitor>
    void ForEachEntity(Visitor&& visitor) const
    {
        auto&& callable = visitor;
        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            const Slot& slot = slots_[index];
            if (!slot.alive) continue;
            callable(EntityId{static_cast<std::uint32_t>(index), slot.generation}, *slot.entity);
        }
    }

private:
    struct Slot final
    {
        std::uint32_t generation{1};
        bool alive{false};
        std::optional<Entity> entity{};
    };

    [[nodiscard]] bool WouldCreateCycle(EntityId child, EntityId parent) const noexcept;
    void InsertChildOrdered(EntityId parent, EntityId child);
    void RemoveChild(EntityId parent, EntityId child) noexcept;
    [[nodiscard]] bool DestroyEntityRecursive(EntityId id) noexcept;
    [[nodiscard]] bool EntityOrderLess(EntityId left, EntityId right) const noexcept;

    const ComponentRegistry* registry_{nullptr};
    SceneMetadata metadata_{};
    std::vector<Slot> slots_{};
    std::vector<std::uint32_t> freeSlots_{};
    std::size_t entityCount_{0};
};
} // namespace trace2d::scene
