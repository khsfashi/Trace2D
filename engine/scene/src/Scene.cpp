#include <trace2d/scene/Scene.hpp>

#include <stdexcept>
#include <utility>

namespace trace2d::scene
{
Entity::Entity(EntityDescriptor descriptor)
    : semanticId_{std::move(descriptor.semanticId)}
    , name_{std::move(descriptor.name)}
    , tags_{std::move(descriptor.tags)}
    , transform_{descriptor.transform}
{
    std::sort(tags_.begin(), tags_.end());
    tags_.erase(std::unique(tags_.begin(), tags_.end()), tags_.end());
}

std::string_view Entity::SemanticId() const noexcept
{
    return semanticId_;
}

std::string_view Entity::Name() const noexcept
{
    return name_;
}

const std::vector<std::string>& Entity::Tags() const noexcept
{
    return tags_;
}

bool Entity::HasTag(const std::string_view tag) const noexcept
{
    return std::binary_search(
        tags_.begin(),
        tags_.end(),
        tag,
        [](const std::string& left, const std::string_view right)
        {
            return std::string_view{left} < right;
        });
}

Transform2D& Entity::Transform() noexcept
{
    return transform_;
}

const Transform2D& Entity::Transform() const noexcept
{
    return transform_;
}

EntityId Scene::CreateEntity(EntityDescriptor descriptor)
{
    if (!descriptor.semanticId.empty() && FindBySemanticId(descriptor.semanticId).has_value())
    {
        throw std::invalid_argument{"Scene semantic entity ID must be unique."};
    }

    std::uint32_t index = EntityId::InvalidIndex;
    if (!freeSlots_.empty())
    {
        index = freeSlots_.back();
        freeSlots_.pop_back();
    }
    else
    {
        if (slots_.size() >= static_cast<std::size_t>(EntityId::InvalidIndex))
        {
            throw std::length_error{"Scene exhausted the EntityId index space."};
        }

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
    if (!Contains(id))
    {
        return false;
    }

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
    if (!id.IsValid() || id.index >= slots_.size())
    {
        return false;
    }

    const Slot& slot = slots_[id.index];
    return slot.alive && slot.generation == id.generation && slot.entity.has_value();
}

Entity* Scene::TryGet(const EntityId id) noexcept
{
    if (!Contains(id))
    {
        return nullptr;
    }

    return &*slots_[id.index].entity;
}

const Entity* Scene::TryGet(const EntityId id) const noexcept
{
    if (!Contains(id))
    {
        return nullptr;
    }

    return &*slots_[id.index].entity;
}

std::optional<EntityId> Scene::FindBySemanticId(const std::string_view semanticId) const noexcept
{
    if (semanticId.empty())
    {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < slots_.size(); ++index)
    {
        const Slot& slot = slots_[index];
        if (!slot.alive || !slot.entity.has_value())
        {
            continue;
        }

        if (slot.entity->SemanticId() == semanticId)
        {
            return EntityId{static_cast<std::uint32_t>(index), slot.generation};
        }
    }

    return std::nullopt;
}

std::size_t Scene::EntityCount() const noexcept
{
    return entityCount_;
}
} // namespace trace2d::scene
