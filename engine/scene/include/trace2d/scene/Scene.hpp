#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::scene
{
struct EntityId final
{
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{InvalidIndex};
    std::uint32_t generation{0};

    [[nodiscard]] bool IsValid() const noexcept
    {
        return index != InvalidIndex && generation != 0;
    }

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

private:
    explicit Entity(EntityDescriptor descriptor);

    std::string semanticId_{};
    std::string name_{};
    std::vector<std::string> tags_{};
    Transform2D transform_{};

    friend class Scene;
};

class Scene final
{
public:
    Scene() = default;
    explicit Scene(SceneMetadata metadata);

    [[nodiscard]] SceneMetadata& Metadata() noexcept;
    [[nodiscard]] const SceneMetadata& Metadata() const noexcept;

    [[nodiscard]] EntityId CreateEntity(EntityDescriptor descriptor);
    [[nodiscard]] bool DestroyEntity(EntityId id) noexcept;

    [[nodiscard]] bool Contains(EntityId id) const noexcept;
    [[nodiscard]] Entity* TryGet(EntityId id) noexcept;
    [[nodiscard]] const Entity* TryGet(EntityId id) const noexcept;
    [[nodiscard]] std::optional<EntityId> FindBySemanticId(std::string_view semanticId) const noexcept;

    [[nodiscard]] std::size_t EntityCount() const noexcept;

    template <typename Visitor>
    void ForEachEntity(Visitor&& visitor)
    {
        auto&& callable = visitor;
        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            Slot& slot = slots_[index];
            if (!slot.alive)
            {
                continue;
            }

            callable(
                EntityId{static_cast<std::uint32_t>(index), slot.generation},
                *slot.entity);
        }
    }

    template <typename Visitor>
    void ForEachEntity(Visitor&& visitor) const
    {
        auto&& callable = visitor;
        for (std::size_t index = 0; index < slots_.size(); ++index)
        {
            const Slot& slot = slots_[index];
            if (!slot.alive)
            {
                continue;
            }

            callable(
                EntityId{static_cast<std::uint32_t>(index), slot.generation},
                *slot.entity);
        }
    }

private:
    struct Slot final
    {
        std::uint32_t generation{1};
        bool alive{false};
        std::optional<Entity> entity{};
    };

    SceneMetadata metadata_{};
    std::vector<Slot> slots_{};
    std::vector<std::uint32_t> freeSlots_{};
    std::size_t entityCount_{0};
};
} // namespace trace2d::scene
