#include <trace2d/scene/World.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace trace2d::scene
{
namespace
{
constexpr std::size_t InitialStructuralCommandCapacity = 16U;

[[nodiscard]] WorldOperationResult Failure(const WorldOperationCode code, std::string message)
{
    return WorldOperationResult{code, std::move(message)};
}

[[nodiscard]] bool IsValidSemanticSegment(const std::string_view value) noexcept
{
    if (value.empty()) return false;
    for (const char character : value)
    {
        if (character == '/' || character == '\\' || static_cast<unsigned char>(character) < 0x20U)
            return false;
    }
    return true;
}

[[nodiscard]] bool IsFiniteTransform(const Transform2D& transform) noexcept
{
    return std::isfinite(transform.position.x) && std::isfinite(transform.position.y) &&
           std::isfinite(transform.rotationRadians) && std::isfinite(transform.scale.x) &&
           std::isfinite(transform.scale.y);
}

[[nodiscard]] std::string InstanceRootSemanticId(
    const std::string_view worldId,
    const std::string_view instanceId)
{
    std::string result{};
    result.reserve(worldId.size() + instanceId.size() + 1U);
    result.append(worldId);
    result.push_back('/');
    result.append(instanceId);
    return result;
}

[[nodiscard]] std::string InstanceEntitySemanticId(
    const std::string_view worldId,
    const std::string_view instanceId,
    const std::string_view localEntityId)
{
    std::string result = InstanceRootSemanticId(worldId, instanceId);
    result.reserve(result.size() + localEntityId.size() + 1U);
    result.push_back('/');
    result.append(localEntityId);
    return result;
}

[[nodiscard]] std::string FirstTemplateDiagnostic(const SceneLoadResult& result)
{
    if (result.diagnostics.empty()) return "scene template could not be parsed";
    const SceneTextDiagnostic& diagnostic = result.diagnostics.front();
    std::string message = diagnostic.path.empty() ? "$" : diagnostic.path;
    message.append(": ");
    message.append(diagnostic.message);
    return message;
}
} // namespace

std::string_view ToString(const WorldOperationCode code) noexcept
{
    switch (code)
    {
    case WorldOperationCode::Success: return "success";
    case WorldOperationCode::InvalidArgument: return "invalid_argument";
    case WorldOperationCode::DuplicateWorld: return "duplicate_world";
    case WorldOperationCode::WorldNotFound: return "world_not_found";
    case WorldOperationCode::WorldOwnershipMismatch: return "world_ownership_mismatch";
    case WorldOperationCode::DuplicateInstance: return "duplicate_instance";
    case WorldOperationCode::InstanceNotFound: return "instance_not_found";
    case WorldOperationCode::TemplateStale: return "template_stale";
    case WorldOperationCode::TemplateInvalid: return "template_invalid";
    case WorldOperationCode::SemanticConflict: return "semantic_conflict";
    case WorldOperationCode::ParentNotFound: return "parent_not_found";
    case WorldOperationCode::InvalidOverride: return "invalid_override";
    case WorldOperationCode::ComponentFailure: return "component_failure";
    case WorldOperationCode::HierarchyFailure: return "hierarchy_failure";
    case WorldOperationCode::ResourceFailure: return "resource_failure";
    }
    return "unknown_world_operation";
}

std::string_view ToString(const StructuralCommandKind kind) noexcept
{
    switch (kind)
    {
    case StructuralCommandKind::CreateWorld: return "create_world";
    case StructuralCommandKind::Instantiate: return "instantiate";
    case StructuralCommandKind::Despawn: return "despawn";
    case StructuralCommandKind::Reparent: return "reparent";
    case StructuralCommandKind::UnloadWorld: return "unload_world";
    }
    return "unknown_structural_command";
}

bool StructuralCommitReport::Succeeded() const noexcept
{
    return std::all_of(results.begin(), results.end(), [](const StructuralCommandResult& result)
    {
        return result.result.Succeeded();
    });
}

WorldLifecycle::WorldLifecycle(
    const ComponentRegistry& componentRegistry,
    assets::ResourceRegistry& resources)
    : componentRegistry_{componentRegistry}
    , resources_{resources}
{
    if (!componentRegistry_.IsFrozen())
        throw std::invalid_argument{"WorldLifecycle requires a frozen ComponentRegistry."};
    pendingCommands_.reserve(InitialStructuralCommandCapacity);
    lastCommit_.results.reserve(InitialStructuralCommandCapacity);
}

WorldLifecycle::~WorldLifecycle()
{
    for (const auto& worldOwner : worlds_)
    {
        WorldRecord& world = *worldOwner;
        while (!world.instances.empty())
        {
            (void)DespawnRecord(world, world.instances.size() - 1U);
        }
        if (world.loaded) DestroyAllEntities(world);
    }
}

WorldOperationResult WorldLifecycle::CreateWorld(WorldDescriptor descriptor)
{
    if (!IsValidSemanticSegment(descriptor.semanticId))
        return Failure(WorldOperationCode::InvalidArgument, "world semantic ID must be non-empty and cannot contain '/' or control characters");

    if (WorldRecord* existing = FindWorld(descriptor.semanticId); existing != nullptr)
    {
        if (existing->loaded)
            return Failure(WorldOperationCode::DuplicateWorld, "world semantic ID is already loaded");
        if (existing->external)
            return Failure(WorldOperationCode::WorldOwnershipMismatch, "an externally attached world must be reattached, not recreated as owned");

        existing->descriptor = std::move(descriptor);
        existing->scene->Metadata() = SceneMetadata{existing->descriptor.semanticId, existing->descriptor.name};
        existing->loaded = true;
        RebuildWorldOrder();
        return {};
    }

    auto record = std::make_unique<WorldRecord>();
    record->descriptor = std::move(descriptor);
    record->ownedScene = std::make_unique<Scene>(
        componentRegistry_,
        SceneMetadata{record->descriptor.semanticId, record->descriptor.name});
    record->scene = record->ownedScene.get();
    record->loaded = true;
    record->external = false;
    worlds_.push_back(std::move(record));
    RebuildWorldOrder();
    return {};
}

WorldOperationResult WorldLifecycle::AttachWorld(WorldDescriptor descriptor, Scene& scene)
{
    if (!IsValidSemanticSegment(descriptor.semanticId))
        return Failure(WorldOperationCode::InvalidArgument, "world semantic ID must be non-empty and cannot contain '/' or control characters");
    if (scene.Registry() != &componentRegistry_)
        return Failure(WorldOperationCode::InvalidArgument, "attached Scene must use the same frozen ComponentRegistry as WorldLifecycle");

    if (WorldRecord* existing = FindWorld(descriptor.semanticId); existing != nullptr)
    {
        if (existing->loaded)
            return Failure(WorldOperationCode::DuplicateWorld, "world semantic ID is already loaded");
        if (!existing->external || existing->scene != &scene)
            return Failure(WorldOperationCode::WorldOwnershipMismatch, "world semantic ID belongs to a different retained Scene incarnation");

        existing->descriptor = std::move(descriptor);
        existing->scene->Metadata() = SceneMetadata{existing->descriptor.semanticId, existing->descriptor.name};
        existing->loaded = true;
        RebuildWorldOrder();
        return {};
    }

    auto record = std::make_unique<WorldRecord>();
    record->descriptor = std::move(descriptor);
    record->scene = &scene;
    record->loaded = true;
    record->external = true;
    scene.Metadata() = SceneMetadata{record->descriptor.semanticId, record->descriptor.name};
    worlds_.push_back(std::move(record));
    RebuildWorldOrder();
    return {};
}

WorldOperationResult WorldLifecycle::UnloadWorld(const std::string_view worldId)
{
    WorldRecord* world = FindWorld(worldId);
    if (world == nullptr || !world->loaded)
        return Failure(WorldOperationCode::WorldNotFound, "world is not loaded");

    WorldOperationResult firstFailure{};
    while (!world->instances.empty())
    {
        WorldOperationResult result = DespawnRecord(*world, world->instances.size() - 1U);
        if (!result.Succeeded() && firstFailure.Succeeded()) firstFailure = std::move(result);
    }
    DestroyAllEntities(*world);
    world->loaded = false;
    RebuildWorldOrder();
    return firstFailure;
}

WorldOperationResult WorldLifecycle::Instantiate(const TemplateInstantiationRequest& request)
{
    if (!IsValidSemanticSegment(request.worldId) || !IsValidSemanticSegment(request.instanceId))
        return Failure(WorldOperationCode::InvalidArgument, "world and instance IDs must be non-empty semantic segments without '/' or control characters");
    if (!IsFiniteTransform(request.rootTransform))
        return Failure(WorldOperationCode::InvalidArgument, "instance root transform must be finite");

    WorldRecord* world = FindWorld(request.worldId);
    if (world == nullptr || !world->loaded)
        return Failure(WorldOperationCode::WorldNotFound, "target world is not loaded");
    if (FindInstance(*world, request.instanceId) != nullptr)
        return Failure(WorldOperationCode::DuplicateInstance, "template instance ID already exists in the target world");
    if (request.parent.has_value() && !world->scene->Contains(*request.parent))
        return Failure(WorldOperationCode::ParentNotFound, "requested instance parent is stale or missing from the target world");

    WorldOperationResult compileResult{};
    CompiledTemplate* compiled = ResolveCompiledTemplate(request.templateResource, compileResult);
    if (compiled == nullptr) return compileResult;

    const std::string rootSemanticId = InstanceRootSemanticId(request.worldId, request.instanceId);
    if (world->scene->FindBySemanticId(rootSemanticId).has_value())
        return Failure(WorldOperationCode::SemanticConflict, "instance root semantic ID conflicts with an existing entity");

    std::vector<std::string> generatedSemanticIds{};
    generatedSemanticIds.reserve(compiled->entities.size());
    for (const CompiledEntity& entity : compiled->entities)
    {
        const std::string semanticId = InstanceEntitySemanticId(request.worldId, request.instanceId, entity.descriptor.semanticId);
        if (world->scene->FindBySemanticId(semanticId).has_value())
            return Failure(WorldOperationCode::SemanticConflict, "template child semantic ID conflicts with an existing entity: " + semanticId);
        generatedSemanticIds.push_back(semanticId);
    }

    for (std::size_t left = 0; left < request.componentOverrides.size(); ++left)
    {
        const TemplateComponentOverride& overrideValue = request.componentOverrides[left];
        const auto sourceEntity = std::find_if(compiled->entities.begin(), compiled->entities.end(), [&overrideValue](const CompiledEntity& entity)
        {
            return entity.descriptor.semanticId == overrideValue.localEntityId;
        });
        if (sourceEntity == compiled->entities.end())
            return Failure(WorldOperationCode::InvalidOverride, "component override references an unknown template-local entity");

        const auto sourceComponent = std::find_if(sourceEntity->components.begin(), sourceEntity->components.end(), [&overrideValue](const AuthoredComponentSnapshot& component)
        {
            return component.typeId == overrideValue.component.typeId;
        });
        if (sourceComponent == sourceEntity->components.end() || sourceComponent->schemaVersion != overrideValue.component.schemaVersion)
            return Failure(WorldOperationCode::InvalidOverride, "component override must match an authored template component type and schema version");

        for (std::size_t right = left + 1U; right < request.componentOverrides.size(); ++right)
        {
            if (overrideValue.localEntityId == request.componentOverrides[right].localEntityId &&
                overrideValue.component.typeId == request.componentOverrides[right].component.typeId)
                return Failure(WorldOperationCode::InvalidOverride, "duplicate component override for the same local entity and type");
        }
    }

    const assets::ResourceOperationResult retainResult = resources_.Retain(request.templateResource.Untyped());
    if (!retainResult.Succeeded())
        return Failure(WorldOperationCode::ResourceFailure, "scene template resource could not be retained for the live instance");

    std::vector<EntityId> created{};
    created.reserve(compiled->entities.size() + 1U);
    InstanceRecord instance{};
    instance.instanceId = request.instanceId;
    instance.templateResource = request.templateResource;
    instance.templateReference = compiled->canonicalReference;
    instance.entities.reserve(compiled->entities.size());
    world->instances.reserve(world->instances.size() + 1U);

    const auto rollback = [&]() noexcept
    {
        for (auto iterator = created.rbegin(); iterator != created.rend(); ++iterator)
            if (world->scene->Contains(*iterator)) (void)world->scene->DestroyEntity(*iterator);
        (void)resources_.Release(request.templateResource.Untyped());
    };

    try
    {
        instance.rootEntity = world->scene->CreateEntity(EntityDescriptor{
            .semanticId = rootSemanticId,
            .name = request.instanceId,
            .tags = {"trace2d.template-instance-root"},
            .transform = request.rootTransform,
        });
        created.push_back(instance.rootEntity);
        if (request.parent.has_value())
        {
            const HierarchyResult parentResult = world->scene->SetParent(instance.rootEntity, request.parent, ReparentMode::KeepLocal);
            if (parentResult != HierarchyResult::Success)
            {
                rollback();
                return Failure(WorldOperationCode::HierarchyFailure, "instance root parent failed: " + std::string{ToString(parentResult)});
            }
        }

        for (std::size_t index = 0; index < compiled->entities.size(); ++index)
        {
            EntityDescriptor descriptor = compiled->entities[index].descriptor;
            descriptor.semanticId = generatedSemanticIds[index];
            const EntityId entity = world->scene->CreateEntity(std::move(descriptor));
            created.push_back(entity);
            instance.entities.push_back(InstanceEntity{compiled->entities[index].descriptor.semanticId, entity});
        }

        for (std::size_t index = 0; index < compiled->entities.size(); ++index)
        {
            const CompiledEntity& source = compiled->entities[index];
            const EntityId entity = instance.entities[index].entity;
            for (const AuthoredComponentSnapshot& component : source.components)
            {
                const AuthoredComponentSnapshot* selected = &component;
                const auto overrideIterator = std::find_if(
                    request.componentOverrides.begin(), request.componentOverrides.end(),
                    [&source, &component](const TemplateComponentOverride& overrideValue)
                    {
                        return overrideValue.localEntityId == source.descriptor.semanticId &&
                               overrideValue.component.typeId == component.typeId;
                    });
                if (overrideIterator != request.componentOverrides.end()) selected = &overrideIterator->component;

                std::string error{};
                const ComponentAttachResult attach = world->scene->AddAuthoredComponent(
                    entity,
                    selected->typeId,
                    selected->schemaVersion,
                    selected->data,
                    error);
                if (attach != ComponentAttachResult::Success)
                {
                    rollback();
                    std::string message = "template component construction failed: ";
                    message.append(ToString(attach));
                    if (!error.empty())
                    {
                        message.append(" - ");
                        message.append(error);
                    }
                    return Failure(WorldOperationCode::ComponentFailure, std::move(message));
                }
            }
        }

        for (std::size_t index = 0; index < compiled->entities.size(); ++index)
        {
            const CompiledEntity& source = compiled->entities[index];
            EntityId parent = instance.rootEntity;
            if (source.parentLocalId.has_value())
            {
                const InstanceEntity* parentEntity = FindLocalEntity(instance, *source.parentLocalId);
                if (parentEntity == nullptr)
                {
                    rollback();
                    return Failure(WorldOperationCode::HierarchyFailure, "compiled template parent could not be resolved during instantiation");
                }
                parent = parentEntity->entity;
            }

            const HierarchyResult hierarchy = world->scene->SetParent(
                instance.entities[index].entity,
                parent,
                ReparentMode::KeepLocal);
            if (hierarchy != HierarchyResult::Success)
            {
                rollback();
                return Failure(WorldOperationCode::HierarchyFailure, "template hierarchy construction failed: " + std::string{ToString(hierarchy)});
            }
        }

        world->instances.push_back(std::move(instance));
        return {};
    }
    catch (const std::exception& error)
    {
        rollback();
        return Failure(WorldOperationCode::TemplateInvalid, std::string{"template instantiation threw before publish: "} + error.what());
    }
}

WorldOperationResult WorldLifecycle::Despawn(
    const std::string_view worldId,
    const std::string_view instanceId)
{
    WorldRecord* world = FindWorld(worldId);
    if (world == nullptr || !world->loaded)
        return Failure(WorldOperationCode::WorldNotFound, "world is not loaded");

    for (std::size_t index = 0; index < world->instances.size(); ++index)
    {
        if (world->instances[index].instanceId == instanceId)
            return DespawnRecord(*world, index);
    }
    return Failure(WorldOperationCode::InstanceNotFound, "template instance is not present in the target world");
}

WorldOperationResult WorldLifecycle::Reparent(
    const std::string_view worldId,
    const EntityId child,
    const std::optional<EntityId> parent,
    const ReparentMode mode)
{
    WorldRecord* world = FindWorld(worldId);
    if (world == nullptr || !world->loaded)
        return Failure(WorldOperationCode::WorldNotFound, "world is not loaded");
    if (!world->scene->Contains(child))
        return Failure(WorldOperationCode::ParentNotFound, "reparent child is stale or missing from the target world");
    if (parent.has_value() && !world->scene->Contains(*parent))
        return Failure(WorldOperationCode::ParentNotFound, "reparent target is stale or missing from the target world");

    const HierarchyResult result = world->scene->SetParent(child, parent, mode);
    if (result != HierarchyResult::Success)
        return Failure(WorldOperationCode::HierarchyFailure, "reparent failed: " + std::string{ToString(result)});
    return {};
}

std::uint64_t WorldLifecycle::QueueCreateWorld(WorldDescriptor descriptor)
{
    return QueueCommand(CreateWorldCommand{std::move(descriptor)});
}

std::uint64_t WorldLifecycle::QueueInstantiate(TemplateInstantiationRequest request)
{
    return QueueCommand(InstantiateCommand{std::move(request)});
}

std::uint64_t WorldLifecycle::QueueDespawn(std::string worldId, std::string instanceId)
{
    return QueueCommand(DespawnCommand{std::move(worldId), std::move(instanceId)});
}

std::uint64_t WorldLifecycle::QueueReparent(
    std::string worldId,
    const EntityId child,
    const std::optional<EntityId> parent,
    const ReparentMode mode)
{
    return QueueCommand(ReparentCommand{std::move(worldId), child, parent, mode});
}

std::uint64_t WorldLifecycle::QueueUnloadWorld(std::string worldId)
{
    return QueueCommand(UnloadWorldCommand{std::move(worldId)});
}

const StructuralCommitReport& WorldLifecycle::CommitStructuralChanges()
{
    lastCommit_.results.clear();
    if (lastCommit_.results.capacity() < pendingCommands_.size())
        lastCommit_.results.reserve(pendingCommands_.size());

    for (const PendingCommand& pending : pendingCommands_)
    {
        const StructuralCommandKind kind = KindOf(pending.command);
        WorldOperationResult result = Execute(pending.command);
        lastCommit_.results.push_back(StructuralCommandResult{pending.sequence, kind, std::move(result)});
        ++committedCommands_;
    }
    pendingCommands_.clear();
    return lastCommit_;
}

const StructuralCommitReport& WorldLifecycle::LastCommitReport() const noexcept
{
    return lastCommit_;
}

Scene* WorldLifecycle::TryGetScene(const std::string_view worldId) noexcept
{
    WorldRecord* world = FindWorld(worldId);
    return world != nullptr && world->loaded ? world->scene : nullptr;
}

const Scene* WorldLifecycle::TryGetScene(const std::string_view worldId) const noexcept
{
    const WorldRecord* world = FindWorld(worldId);
    return world != nullptr && world->loaded ? world->scene : nullptr;
}

std::optional<EntityId> WorldLifecycle::FindInstanceRoot(
    const std::string_view worldId,
    const std::string_view instanceId) const noexcept
{
    const WorldRecord* world = FindWorld(worldId);
    if (world == nullptr || !world->loaded) return std::nullopt;
    const InstanceRecord* instance = FindInstance(*world, instanceId);
    if (instance == nullptr || !world->scene->Contains(instance->rootEntity)) return std::nullopt;
    return instance->rootEntity;
}

std::optional<EntityId> WorldLifecycle::FindInstanceEntity(
    const std::string_view worldId,
    const std::string_view instanceId,
    const std::string_view localEntityId) const noexcept
{
    const WorldRecord* world = FindWorld(worldId);
    if (world == nullptr || !world->loaded) return std::nullopt;
    const InstanceRecord* instance = FindInstance(*world, instanceId);
    if (instance == nullptr) return std::nullopt;
    const InstanceEntity* entity = FindLocalEntity(*instance, localEntityId);
    if (entity == nullptr || !world->scene->Contains(entity->entity)) return std::nullopt;
    return entity->entity;
}

std::vector<WorldSnapshot> WorldLifecycle::InspectWorlds() const
{
    std::vector<WorldSnapshot> snapshots{};
    snapshots.reserve(worldOrder_.size());
    for (const WorldRecord* world : worldOrder_)
    {
        snapshots.push_back(WorldSnapshot{
            world->descriptor.semanticId,
            world->descriptor.name,
            world->descriptor.orderKey,
            world->scene->EntityCount(),
            world->instances.size(),
        });
    }
    return snapshots;
}

std::vector<TemplateInstanceSnapshot> WorldLifecycle::InspectInstances(const std::string_view worldId) const
{
    std::vector<TemplateInstanceSnapshot> snapshots{};
    const WorldRecord* world = FindWorld(worldId);
    if (world == nullptr || !world->loaded) return snapshots;
    snapshots.reserve(world->instances.size());
    for (const InstanceRecord& instance : world->instances)
    {
        TemplateInstanceSnapshot snapshot{};
        snapshot.worldId = world->descriptor.semanticId;
        snapshot.instanceId = instance.instanceId;
        snapshot.templateReference = instance.templateReference;
        snapshot.rootEntity = instance.rootEntity;
        snapshot.entities.reserve(instance.entities.size());
        for (const InstanceEntity& entity : instance.entities)
        {
            const Entity* live = world->scene->TryGet(entity.entity);
            snapshot.entities.push_back(TemplateEntityInstanceSnapshot{
                entity.localEntityId,
                live == nullptr ? std::string{} : std::string{live->SemanticId()},
                entity.entity,
            });
        }
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

std::size_t WorldLifecycle::OrderedWorldCount() const noexcept
{
    return worldOrder_.size();
}

std::string_view WorldLifecycle::OrderedWorldId(const std::size_t orderedIndex) const noexcept
{
    return orderedIndex < worldOrder_.size() ? std::string_view{worldOrder_[orderedIndex]->descriptor.semanticId} : std::string_view{};
}

Scene* WorldLifecycle::OrderedWorld(const std::size_t orderedIndex) noexcept
{
    return orderedIndex < worldOrder_.size() ? worldOrder_[orderedIndex]->scene : nullptr;
}

const Scene* WorldLifecycle::OrderedWorld(const std::size_t orderedIndex) const noexcept
{
    return orderedIndex < worldOrder_.size() ? worldOrder_[orderedIndex]->scene : nullptr;
}

std::size_t WorldLifecycle::PendingStructuralCommandCount() const noexcept
{
    return pendingCommands_.size();
}

WorldLifecycleStats WorldLifecycle::Stats() const noexcept
{
    return WorldLifecycleStats{
        templateCompiles_,
        templateCacheHits_,
        queuedCommands_,
        committedCommands_,
        pendingCommands_.size(),
        pendingCommands_.capacity(),
        compiledTemplates_.size(),
        worldOrder_.size(),
    };
}

WorldLifecycle::WorldRecord* WorldLifecycle::FindWorld(const std::string_view worldId) noexcept
{
    const auto iterator = std::find_if(worlds_.begin(), worlds_.end(), [worldId](const auto& world)
    {
        return std::string_view{world->descriptor.semanticId} == worldId;
    });
    return iterator == worlds_.end() ? nullptr : iterator->get();
}

const WorldLifecycle::WorldRecord* WorldLifecycle::FindWorld(const std::string_view worldId) const noexcept
{
    const auto iterator = std::find_if(worlds_.begin(), worlds_.end(), [worldId](const auto& world)
    {
        return std::string_view{world->descriptor.semanticId} == worldId;
    });
    return iterator == worlds_.end() ? nullptr : iterator->get();
}

WorldLifecycle::InstanceRecord* WorldLifecycle::FindInstance(
    WorldRecord& world,
    const std::string_view instanceId) noexcept
{
    const auto iterator = std::find_if(world.instances.begin(), world.instances.end(), [instanceId](const InstanceRecord& instance)
    {
        return std::string_view{instance.instanceId} == instanceId;
    });
    return iterator == world.instances.end() ? nullptr : &*iterator;
}

const WorldLifecycle::InstanceRecord* WorldLifecycle::FindInstance(
    const WorldRecord& world,
    const std::string_view instanceId) const noexcept
{
    const auto iterator = std::find_if(world.instances.begin(), world.instances.end(), [instanceId](const InstanceRecord& instance)
    {
        return std::string_view{instance.instanceId} == instanceId;
    });
    return iterator == world.instances.end() ? nullptr : &*iterator;
}

WorldLifecycle::CompiledTemplate* WorldLifecycle::ResolveCompiledTemplate(
    const assets::ResourceHandle<assets::SceneTemplateResource> handle,
    WorldOperationResult& result)
{
    const assets::SceneTemplateResource* resource = resources_.Resolve(handle);
    if (resource == nullptr)
    {
        result = Failure(WorldOperationCode::TemplateStale, "scene template handle is stale or not ready");
        return nullptr;
    }

    const std::optional<assets::ResourceSnapshot> resourceSnapshot = resources_.Inspect(handle.Untyped());
    if (!resourceSnapshot.has_value())
    {
        result = Failure(WorldOperationCode::ResourceFailure, "scene template resource identity could not be inspected");
        return nullptr;
    }

    const auto cached = std::find_if(compiledTemplates_.begin(), compiledTemplates_.end(), [handle](const CompiledTemplate& candidate)
    {
        return candidate.handle == handle;
    });
    if (cached != compiledTemplates_.end())
    {
        ++templateCacheHits_;
        result = {};
        return &*cached;
    }

    compiledTemplates_.erase(
        std::remove_if(compiledTemplates_.begin(), compiledTemplates_.end(), [handle](const CompiledTemplate& candidate)
        {
            return candidate.handle.slot == handle.slot && candidate.handle.generation != handle.generation;
        }),
        compiledTemplates_.end());

    SceneLoadResult load = LoadSceneToml(
        resource->canonicalToml,
        componentRegistry_,
        resourceSnapshot->identity.canonicalReference);
    if (!load.Succeeded())
    {
        result = Failure(WorldOperationCode::TemplateInvalid, FirstTemplateDiagnostic(load));
        return nullptr;
    }
    if (load.scene->EntityCount() == 0U)
    {
        result = Failure(WorldOperationCode::TemplateInvalid, "scene template must contain at least one authored entity");
        return nullptr;
    }

    CompiledTemplate compiled{};
    compiled.handle = handle;
    compiled.canonicalReference = resourceSnapshot->identity.canonicalReference;
    compiled.entities.reserve(load.scene->EntityCount());

    bool valid = true;
    std::string error{};
    load.scene->ForEachEntity([&](const EntityId entityId, const Entity& entity)
    {
        if (!valid) return;
        if (!IsValidSemanticSegment(entity.SemanticId()))
        {
            valid = false;
            error = "template-local entity IDs must be non-empty semantic segments without '/' or control characters";
            return;
        }

        CompiledEntity entry{};
        entry.descriptor.semanticId = std::string{entity.SemanticId()};
        entry.descriptor.name = std::string{entity.Name()};
        entry.descriptor.tags = entity.Tags();
        entry.descriptor.transform = entity.LocalTransform();
        if (entity.Parent().has_value())
        {
            const Entity* parent = load.scene->TryGet(*entity.Parent());
            if (parent == nullptr || parent->SemanticId().empty())
            {
                valid = false;
                error = "template hierarchy contains an unresolved semantic parent";
                return;
            }
            entry.parentLocalId = std::string{parent->SemanticId()};
        }

        std::string componentError{};
        entry.components = load.scene->SerializeAuthoredComponents(entityId, componentError);
        if (!componentError.empty())
        {
            valid = false;
            error = "template authored component serialization failed: " + componentError;
            return;
        }
        compiled.entities.push_back(std::move(entry));
    });

    if (!valid)
    {
        result = Failure(WorldOperationCode::TemplateInvalid, std::move(error));
        return nullptr;
    }

    compiledTemplates_.push_back(std::move(compiled));
    ++templateCompiles_;
    result = {};
    return &compiledTemplates_.back();
}

const WorldLifecycle::InstanceEntity* WorldLifecycle::FindLocalEntity(
    const InstanceRecord& instance,
    const std::string_view localEntityId) const noexcept
{
    const auto iterator = std::find_if(instance.entities.begin(), instance.entities.end(), [localEntityId](const InstanceEntity& entity)
    {
        return std::string_view{entity.localEntityId} == localEntityId;
    });
    return iterator == instance.entities.end() ? nullptr : &*iterator;
}

WorldOperationResult WorldLifecycle::DespawnRecord(WorldRecord& world, const std::size_t instanceIndex)
{
    if (instanceIndex >= world.instances.size())
        return Failure(WorldOperationCode::InstanceNotFound, "template instance index is out of range");

    InstanceRecord& instance = world.instances[instanceIndex];
    for (auto iterator = instance.entities.rbegin(); iterator != instance.entities.rend(); ++iterator)
        if (world.scene->Contains(iterator->entity)) (void)world.scene->DestroyEntity(iterator->entity);
    if (world.scene->Contains(instance.rootEntity)) (void)world.scene->DestroyEntity(instance.rootEntity);

    const assets::ResourceOperationResult release = resources_.Release(instance.templateResource.Untyped());
    world.instances.erase(world.instances.begin() + static_cast<std::ptrdiff_t>(instanceIndex));
    if (!release.Succeeded())
        return Failure(WorldOperationCode::ResourceFailure, "despawn completed but the retained scene-template resource could not be released");
    return {};
}

void WorldLifecycle::DestroyAllEntities(WorldRecord& world) noexcept
{
    std::vector<EntityId> entities{};
    entities.reserve(world.scene->EntityCount());
    world.scene->ForEachEntity([&entities](const EntityId id, const Entity&) { entities.push_back(id); });
    for (const EntityId id : entities)
        if (world.scene->Contains(id)) (void)world.scene->DestroyEntity(id);
}

void WorldLifecycle::RebuildWorldOrder()
{
    worldOrder_.clear();
    worldOrder_.reserve(worlds_.size());
    for (const auto& world : worlds_)
        if (world->loaded) worldOrder_.push_back(world.get());
    std::sort(worldOrder_.begin(), worldOrder_.end(), [](const WorldRecord* left, const WorldRecord* right)
    {
        if (left->descriptor.orderKey != right->descriptor.orderKey)
            return left->descriptor.orderKey < right->descriptor.orderKey;
        return left->descriptor.semanticId < right->descriptor.semanticId;
    });
}

std::uint64_t WorldLifecycle::QueueCommand(StructuralCommand command)
{
    if (nextCommandSequence_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error{"WorldLifecycle structural command sequence exhausted."};
    const std::uint64_t sequence = nextCommandSequence_++;
    pendingCommands_.push_back(PendingCommand{sequence, std::move(command)});
    ++queuedCommands_;
    return sequence;
}

StructuralCommandKind WorldLifecycle::KindOf(const StructuralCommand& command) const noexcept
{
    return static_cast<StructuralCommandKind>(command.index());
}

WorldOperationResult WorldLifecycle::Execute(const StructuralCommand& command)
{
    return std::visit([this](const auto& value) -> WorldOperationResult
    {
        using Command = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Command, CreateWorldCommand>)
            return CreateWorld(value.descriptor);
        else if constexpr (std::is_same_v<Command, InstantiateCommand>)
            return Instantiate(value.request);
        else if constexpr (std::is_same_v<Command, DespawnCommand>)
            return Despawn(value.worldId, value.instanceId);
        else if constexpr (std::is_same_v<Command, ReparentCommand>)
            return Reparent(value.worldId, value.child, value.parent, value.mode);
        else
            return UnloadWorld(value.worldId);
    }, command);
}
} // namespace trace2d::scene
