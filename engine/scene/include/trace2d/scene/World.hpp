#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/scene/SceneText.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace trace2d::scene
{
enum class WorldOperationCode : std::uint8_t
{
    Success = 0,
    InvalidArgument,
    DuplicateWorld,
    WorldNotFound,
    WorldOwnershipMismatch,
    DuplicateInstance,
    InstanceNotFound,
    TemplateStale,
    TemplateInvalid,
    SemanticConflict,
    ParentNotFound,
    InvalidOverride,
    ComponentFailure,
    HierarchyFailure,
    ResourceFailure,
};

[[nodiscard]] std::string_view ToString(WorldOperationCode code) noexcept;

struct WorldOperationResult final
{
    WorldOperationCode code{WorldOperationCode::Success};
    std::string message{};

    [[nodiscard]] bool Succeeded() const noexcept { return code == WorldOperationCode::Success; }
};

struct WorldDescriptor final
{
    std::string semanticId{};
    std::string name{};
    std::int32_t orderKey{0};
};

struct TemplateComponentOverride final
{
    std::string localEntityId{};
    AuthoredComponentSnapshot component{};
};

struct TemplateInstantiationRequest final
{
    std::string worldId{};
    assets::ResourceHandle<assets::SceneTemplateResource> templateResource{};
    std::string instanceId{};
    Transform2D rootTransform{};
    std::optional<EntityId> parent{};
    std::vector<TemplateComponentOverride> componentOverrides{};
};

struct TemplateEntityInstanceSnapshot final
{
    std::string localEntityId{};
    std::string semanticId{};
    EntityId entity{};
};

struct TemplateInstanceSnapshot final
{
    std::string worldId{};
    std::string instanceId{};
    std::string templateReference{};
    EntityId rootEntity{};
    std::vector<TemplateEntityInstanceSnapshot> entities{};
};

struct WorldSnapshot final
{
    std::string semanticId{};
    std::string name{};
    std::int32_t orderKey{0};
    std::size_t entityCount{0};
    std::size_t instanceCount{0};
};

enum class StructuralCommandKind : std::uint8_t
{
    CreateWorld = 0,
    Instantiate,
    Despawn,
    Reparent,
    UnloadWorld,
};

[[nodiscard]] std::string_view ToString(StructuralCommandKind kind) noexcept;

struct StructuralCommandResult final
{
    std::uint64_t sequence{0};
    StructuralCommandKind kind{StructuralCommandKind::CreateWorld};
    WorldOperationResult result{};
};

struct StructuralCommitReport final
{
    std::vector<StructuralCommandResult> results{};

    [[nodiscard]] bool Succeeded() const noexcept;
};

struct WorldLifecycleStats final
{
    std::uint64_t templateCompiles{0};
    std::uint64_t templateCacheHits{0};
    std::uint64_t queuedCommands{0};
    std::uint64_t committedCommands{0};
    std::size_t pendingCommands{0};
    std::size_t retainedCommandCapacity{0};
    std::size_t compiledTemplateCount{0};
    std::size_t loadedWorldCount{0};
};

// Owns only world/template structural lifecycle. Entity/component truth remains in the existing
// Scene type from E2. Structural queueing is explicit and committed at one caller-selected safe
// point; ordinary fixed-step code performs no template parsing, path normalization, or file I/O.
class WorldLifecycle final
{
public:
    explicit WorldLifecycle(const ComponentRegistry& componentRegistry, assets::ResourceRegistry& resources);
    ~WorldLifecycle();

    WorldLifecycle(const WorldLifecycle&) = delete;
    WorldLifecycle& operator=(const WorldLifecycle&) = delete;
    WorldLifecycle(WorldLifecycle&&) = delete;
    WorldLifecycle& operator=(WorldLifecycle&&) = delete;

    [[nodiscard]] WorldOperationResult CreateWorld(WorldDescriptor descriptor);
    [[nodiscard]] WorldOperationResult AttachWorld(WorldDescriptor descriptor, Scene& scene);
    [[nodiscard]] WorldOperationResult UnloadWorld(std::string_view worldId);

    [[nodiscard]] WorldOperationResult Instantiate(const TemplateInstantiationRequest& request);
    [[nodiscard]] WorldOperationResult Despawn(std::string_view worldId, std::string_view instanceId);
    [[nodiscard]] WorldOperationResult Reparent(
        std::string_view worldId,
        EntityId child,
        std::optional<EntityId> parent,
        ReparentMode mode = ReparentMode::KeepLocal);

    [[nodiscard]] std::uint64_t QueueCreateWorld(WorldDescriptor descriptor);
    [[nodiscard]] std::uint64_t QueueInstantiate(TemplateInstantiationRequest request);
    [[nodiscard]] std::uint64_t QueueDespawn(std::string worldId, std::string instanceId);
    [[nodiscard]] std::uint64_t QueueReparent(
        std::string worldId,
        EntityId child,
        std::optional<EntityId> parent,
        ReparentMode mode = ReparentMode::KeepLocal);
    [[nodiscard]] std::uint64_t QueueUnloadWorld(std::string worldId);
    [[nodiscard]] const StructuralCommitReport& CommitStructuralChanges();
    [[nodiscard]] const StructuralCommitReport& LastCommitReport() const noexcept;

    [[nodiscard]] Scene* TryGetScene(std::string_view worldId) noexcept;
    [[nodiscard]] const Scene* TryGetScene(std::string_view worldId) const noexcept;
    [[nodiscard]] std::optional<EntityId> FindInstanceRoot(
        std::string_view worldId,
        std::string_view instanceId) const noexcept;
    [[nodiscard]] std::optional<EntityId> FindInstanceEntity(
        std::string_view worldId,
        std::string_view instanceId,
        std::string_view localEntityId) const noexcept;

    [[nodiscard]] std::vector<WorldSnapshot> InspectWorlds() const;
    [[nodiscard]] std::vector<TemplateInstanceSnapshot> InspectInstances(std::string_view worldId) const;

    // Allocation-free deterministic traversal surface once structural setup is complete.
    [[nodiscard]] std::size_t OrderedWorldCount() const noexcept;
    [[nodiscard]] std::string_view OrderedWorldId(std::size_t orderedIndex) const noexcept;
    [[nodiscard]] Scene* OrderedWorld(std::size_t orderedIndex) noexcept;
    [[nodiscard]] const Scene* OrderedWorld(std::size_t orderedIndex) const noexcept;

    [[nodiscard]] std::size_t PendingStructuralCommandCount() const noexcept;
    [[nodiscard]] WorldLifecycleStats Stats() const noexcept;

private:
    struct CompiledEntity final
    {
        EntityDescriptor descriptor{};
        std::optional<std::string> parentLocalId{};
        std::vector<AuthoredComponentSnapshot> components{};
    };

    struct CompiledTemplate final
    {
        assets::ResourceHandle<assets::SceneTemplateResource> handle{};
        std::string canonicalReference{};
        std::vector<CompiledEntity> entities{};
    };

    struct InstanceEntity final
    {
        std::string localEntityId{};
        EntityId entity{};
    };

    struct InstanceRecord final
    {
        std::string instanceId{};
        assets::ResourceHandle<assets::SceneTemplateResource> templateResource{};
        std::string templateReference{};
        EntityId rootEntity{};
        std::vector<InstanceEntity> entities{};
    };

    struct WorldRecord final
    {
        WorldDescriptor descriptor{};
        std::unique_ptr<Scene> ownedScene{};
        Scene* scene{nullptr};
        bool loaded{false};
        bool external{false};
        std::vector<InstanceRecord> instances{};
    };

    struct CreateWorldCommand final { WorldDescriptor descriptor{}; };
    struct InstantiateCommand final { TemplateInstantiationRequest request{}; };
    struct DespawnCommand final { std::string worldId{}; std::string instanceId{}; };
    struct ReparentCommand final
    {
        std::string worldId{};
        EntityId child{};
        std::optional<EntityId> parent{};
        ReparentMode mode{ReparentMode::KeepLocal};
    };
    struct UnloadWorldCommand final { std::string worldId{}; };

    using StructuralCommand = std::variant<
        CreateWorldCommand,
        InstantiateCommand,
        DespawnCommand,
        ReparentCommand,
        UnloadWorldCommand>;

    // Pending commands are only created with both fields supplied by QueueCommand. Avoid requiring
    // a semantically meaningless default StructuralCommand (and therefore a default variant state).
    struct PendingCommand final
    {
        std::uint64_t sequence;
        StructuralCommand command;
    };

    [[nodiscard]] WorldRecord* FindWorld(std::string_view worldId) noexcept;
    [[nodiscard]] const WorldRecord* FindWorld(std::string_view worldId) const noexcept;
    [[nodiscard]] InstanceRecord* FindInstance(WorldRecord& world, std::string_view instanceId) noexcept;
    [[nodiscard]] const InstanceRecord* FindInstance(const WorldRecord& world, std::string_view instanceId) const noexcept;
    [[nodiscard]] CompiledTemplate* ResolveCompiledTemplate(
        assets::ResourceHandle<assets::SceneTemplateResource> handle,
        WorldOperationResult& result);
    [[nodiscard]] const InstanceEntity* FindLocalEntity(
        const InstanceRecord& instance,
        std::string_view localEntityId) const noexcept;
    [[nodiscard]] WorldOperationResult DespawnRecord(WorldRecord& world, std::size_t instanceIndex);
    void DestroyAllEntities(WorldRecord& world) noexcept;
    void RebuildWorldOrder();
    [[nodiscard]] std::uint64_t QueueCommand(StructuralCommand command);
    [[nodiscard]] StructuralCommandKind KindOf(const StructuralCommand& command) const noexcept;
    [[nodiscard]] WorldOperationResult Execute(const StructuralCommand& command);

    const ComponentRegistry& componentRegistry_;
    assets::ResourceRegistry& resources_;
    std::vector<std::unique_ptr<WorldRecord>> worlds_{};
    std::vector<WorldRecord*> worldOrder_{};
    std::vector<CompiledTemplate> compiledTemplates_{};
    std::vector<PendingCommand> pendingCommands_{};
    StructuralCommitReport lastCommit_{};
    std::uint64_t nextCommandSequence_{1};
    std::uint64_t templateCompiles_{0};
    std::uint64_t templateCacheHits_{0};
    std::uint64_t queuedCommands_{0};
    std::uint64_t committedCommands_{0};
};
} // namespace trace2d::scene
