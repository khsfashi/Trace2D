#include <trace2d/agent/Inspection.hpp>

#include "SceneSnapshotHelpers.hpp"

#include <trace2d/scene/Scene.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace trace2d::agent
{
namespace
{
struct SelectorParseResult final
{
    std::optional<SemanticSelector> selector{};
    std::optional<QueryError> error{};
};

SelectorParseResult ParseSelector(const std::string_view text)
{
    SelectorParseResult result{};
    if (text.empty())
    {
        result.error = QueryError{.code = QueryErrorCode::InvalidSelector, .message = "Selector must not be empty. Expected #id, name:value, tag:value, or type:value."};
        return result;
    }

    SelectorKind kind = SelectorKind::SemanticId;
    std::string_view value{};
    if (text.front() == '#') { kind = SelectorKind::SemanticId; value = text.substr(1); }
    else if (text.starts_with("name:")) { kind = SelectorKind::Name; value = text.substr(5); }
    else if (text.starts_with("tag:")) { kind = SelectorKind::Tag; value = text.substr(4); }
    else if (text.starts_with("type:")) { kind = SelectorKind::Type; value = text.substr(5); }
    else
    {
        result.error = QueryError{.code = QueryErrorCode::InvalidSelector, .message = "Unsupported selector syntax. Expected #id, name:value, tag:value, or type:value."};
        return result;
    }
    if (value.empty())
    {
        result.error = QueryError{.code = QueryErrorCode::InvalidSelector, .message = "Selector value must not be empty."};
        return result;
    }
    result.selector = SemanticSelector{.kind = kind, .value = std::string{value}};
    return result;
}

bool MatchesSelector(
    const scene::Scene& sceneValue,
    const scene::EntityId id,
    const scene::Entity& entity,
    const SemanticSelector& selector) noexcept
{
    switch (selector.kind)
    {
    case SelectorKind::SemanticId: return entity.SemanticId() == selector.value;
    case SelectorKind::Name: return entity.Name() == selector.value;
    case SelectorKind::Tag: return entity.HasTag(selector.value);
    case SelectorKind::Type: return sceneValue.HasComponentType(id, selector.value);
    }
    return false;
}

std::optional<EntitySnapshot> SnapshotBySemanticId(
    const scene::Scene& sceneValue,
    const std::string_view semanticId)
{
    const std::optional<scene::EntityId> id = sceneValue.FindBySemanticId(semanticId);
    if (!id.has_value()) return std::nullopt;
    const scene::Entity* entity = sceneValue.TryGet(*id);
    if (entity == nullptr) return std::nullopt;
    return detail::MakeEntitySnapshot(sceneValue, *id, *entity);
}
} // namespace

std::string_view ToString(const SelectorKind kind) noexcept
{
    switch (kind)
    {
    case SelectorKind::SemanticId: return "id";
    case SelectorKind::Name: return "name";
    case SelectorKind::Tag: return "tag";
    case SelectorKind::Type: return "type";
    }
    return "unknown";
}

std::string_view ToString(const QueryErrorCode code) noexcept
{
    switch (code)
    {
    case QueryErrorCode::SceneUnavailable: return "scene_unavailable";
    case QueryErrorCode::InvalidSelector: return "invalid_selector";
    case QueryErrorCode::NoMatch: return "no_match";
    case QueryErrorCode::AmbiguousMatch: return "ambiguous_match";
    }
    return "unknown_query_error";
}

QueryResult AgentFacade::Query(const std::string_view selectorText) const
{
    QueryResult result{};
    if (scene_ == nullptr)
    {
        result.error = QueryError{.code = QueryErrorCode::SceneUnavailable, .message = "No active scene is bound to the agent facade."};
        return result;
    }
    SelectorParseResult parsed = ParseSelector(selectorText);
    if (!parsed.selector.has_value())
    {
        result.error = std::move(parsed.error);
        return result;
    }
    result.selector = std::move(parsed.selector);

    if (result.selector->kind == SelectorKind::SemanticId)
    {
        std::optional<EntitySnapshot> snapshot = SnapshotBySemanticId(*scene_, result.selector->value);
        if (snapshot.has_value()) result.matches.push_back(std::move(*snapshot));
        return result;
    }

    scene_->ForEachEntity([this, &result](const scene::EntityId id, const scene::Entity& entity)
    {
        if (MatchesSelector(*scene_, id, entity, *result.selector))
            result.matches.push_back(detail::MakeEntitySnapshot(*scene_, id, entity));
    });
    return result;
}

QueryOneResult AgentFacade::QueryOne(const std::string_view selectorText) const
{
    QueryOneResult result{};
    if (scene_ == nullptr)
    {
        result.error = QueryError{.code = QueryErrorCode::SceneUnavailable, .message = "No active scene is bound to the agent facade."};
        return result;
    }
    SelectorParseResult parsed = ParseSelector(selectorText);
    if (!parsed.selector.has_value())
    {
        result.error = std::move(parsed.error);
        return result;
    }
    result.selector = std::move(parsed.selector);

    if (result.selector->kind == SelectorKind::SemanticId)
    {
        result.match = SnapshotBySemanticId(*scene_, result.selector->value);
        if (!result.match.has_value())
            result.error = QueryError{.code = QueryErrorCode::NoMatch, .message = "Selector matched no entities."};
        return result;
    }

    std::size_t matchCount = 0;
    scene_->ForEachEntity([this, &result, &matchCount](const scene::EntityId id, const scene::Entity& entity)
    {
        if (!MatchesSelector(*scene_, id, entity, *result.selector)) return;
        ++matchCount;
        if (matchCount == 1U) result.match = detail::MakeEntitySnapshot(*scene_, id, entity);
    });
    if (matchCount == 0U)
    {
        result.match.reset();
        result.error = QueryError{.code = QueryErrorCode::NoMatch, .message = "Selector matched no entities."};
        return result;
    }
    if (matchCount != 1U)
    {
        result.match.reset();
        result.error = QueryError{.code = QueryErrorCode::AmbiguousMatch, .message = "Selector matched " + std::to_string(matchCount) + " entities; single-result query requires exactly one match."};
        return result;
    }
    return result;
}
} // namespace trace2d::agent
