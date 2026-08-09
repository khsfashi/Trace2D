#pragma once

#include <trace2d/agent/UiAutomation.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::runtime
{
class FixedStepRuntime;
}

namespace trace2d::scene
{
class Scene;
}

namespace trace2d::ui
{
class UiDocument;
}

namespace trace2d::agent
{
enum class InspectionErrorCode
{
    RuntimeUnavailable,
    SceneUnavailable,
};

[[nodiscard]] std::string_view ToString(InspectionErrorCode code) noexcept;

enum class FieldValueKind
{
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Float,
    String,
};

[[nodiscard]] std::string_view ToString(FieldValueKind kind) noexcept;

struct FieldValue final
{
    FieldValueKind kind{FieldValueKind::String};
    bool booleanValue{false};
    std::int64_t signedIntegerValue{0};
    std::uint64_t unsignedIntegerValue{0};
    float floatValue{0.0F};
    std::string stringValue{};

    [[nodiscard]] bool operator==(const FieldValue&) const noexcept = default;
};

struct ComponentFieldSnapshot final
{
    std::string name{};
    FieldValue value{};

    [[nodiscard]] bool operator==(const ComponentFieldSnapshot&) const noexcept = default;
};

struct ComponentSnapshot final
{
    std::string type{};
    std::vector<ComponentFieldSnapshot> fields{};

    [[nodiscard]] bool operator==(const ComponentSnapshot&) const noexcept = default;
};

struct Vector2Snapshot final
{
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] bool operator==(const Vector2Snapshot&) const noexcept = default;
};

struct Transform2DSnapshot final
{
    Vector2Snapshot position{};
    float rotationRadians{0.0F};
    Vector2Snapshot scale{1.0F, 1.0F};

    [[nodiscard]] bool operator==(const Transform2DSnapshot&) const noexcept = default;
};

struct Bounds2DSnapshot final
{
    Vector2Snapshot center{};
    Vector2Snapshot extents{};

    [[nodiscard]] bool operator==(const Bounds2DSnapshot&) const noexcept = default;
};

struct EntityHandleSnapshot final
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    [[nodiscard]] bool operator==(const EntityHandleSnapshot&) const noexcept = default;
};

struct EntitySnapshot final
{
    EntityHandleSnapshot handle{};
    std::string semanticId{};
    std::string name{};
    std::vector<std::string> tags{};
    Transform2DSnapshot transform{};
    std::optional<Bounds2DSnapshot> bounds{};
    std::vector<ComponentSnapshot> components{};

    [[nodiscard]] bool operator==(const EntitySnapshot&) const noexcept = default;
};

struct SceneSnapshot final
{
    std::string semanticId{};
    std::string name{};
    std::vector<EntitySnapshot> entities{};

    [[nodiscard]] bool operator==(const SceneSnapshot&) const noexcept = default;
};

struct RuntimeSnapshot final
{
    std::uint64_t frame{0};
    std::uint64_t seed{0};
    std::int64_t fixedStepNanoseconds{0};
    std::int64_t simulationTimeNanoseconds{0};

    [[nodiscard]] bool operator==(const RuntimeSnapshot&) const noexcept = default;
};

struct InspectionSnapshot final
{
    RuntimeSnapshot runtime{};
    SceneSnapshot scene{};

    [[nodiscard]] bool operator==(const InspectionSnapshot&) const noexcept = default;
};

struct InspectionError final
{
    InspectionErrorCode code{InspectionErrorCode::RuntimeUnavailable};
    std::string message{};

    [[nodiscard]] bool operator==(const InspectionError&) const noexcept = default;
};

struct InspectionResult final
{
    std::optional<InspectionSnapshot> snapshot{};
    std::optional<InspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return snapshot.has_value() && !error.has_value();
    }
};

enum class SelectorKind
{
    SemanticId,
    Name,
    Tag,
    Type,
};

[[nodiscard]] std::string_view ToString(SelectorKind kind) noexcept;

struct SemanticSelector final
{
    SelectorKind kind{SelectorKind::SemanticId};
    std::string value{};

    [[nodiscard]] bool operator==(const SemanticSelector&) const noexcept = default;
};

enum class QueryErrorCode
{
    SceneUnavailable,
    InvalidSelector,
    NoMatch,
    AmbiguousMatch,
};

[[nodiscard]] std::string_view ToString(QueryErrorCode code) noexcept;

struct QueryError final
{
    QueryErrorCode code{QueryErrorCode::InvalidSelector};
    std::string message{};

    [[nodiscard]] bool operator==(const QueryError&) const noexcept = default;
};

struct QueryResult final
{
    std::optional<SemanticSelector> selector{};
    std::vector<EntitySnapshot> matches{};
    std::optional<QueryError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return selector.has_value() && !error.has_value();
    }
};

struct QueryOneResult final
{
    std::optional<SemanticSelector> selector{};
    std::optional<EntitySnapshot> match{};
    std::optional<QueryError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return selector.has_value() && match.has_value() && !error.has_value();
    }
};

class AgentFacade final
{
public:
    AgentFacade() = default;
    AgentFacade(
        const runtime::FixedStepRuntime* runtime,
        const scene::Scene* scene,
        ui::UiDocument* uiDocument = nullptr) noexcept;

    void BindRuntime(const runtime::FixedStepRuntime* runtime) noexcept;
    void BindScene(const scene::Scene* scene) noexcept;
    void BindUi(ui::UiDocument* uiDocument) noexcept;

    [[nodiscard]] InspectionResult Inspect() const;
    [[nodiscard]] QueryResult Query(std::string_view selector) const;
    [[nodiscard]] QueryOneResult QueryOne(std::string_view selector) const;

    [[nodiscard]] UiTreeResult InspectUi() const;
    [[nodiscard]] UiQueryResult QueryUi(const UiSelector& selector) const;
    [[nodiscard]] UiQueryOneResult QueryOneUi(const UiSelector& selector) const;
    [[nodiscard]] UiActionResponse FocusUi(const UiSelector& selector);
    [[nodiscard]] UiActionResponse ActivateUi(const UiSelector& selector);
    [[nodiscard]] UiActionResponse InputUiText(
        const UiSelector& selector,
        std::string_view text);
    [[nodiscard]] UiAssertionResult AssertUi(
        const UiSelector& selector,
        const UiExpectedState& expected) const;

private:
    const runtime::FixedStepRuntime* runtime_{nullptr};
    const scene::Scene* scene_{nullptr};
    ui::UiDocument* ui_{nullptr};
};
} // namespace trace2d::agent
