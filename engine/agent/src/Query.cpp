#include <trace2d/agent/Inspection.hpp>

#include <trace2d/scene/Scene.hpp>

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
        result.error = QueryError{
            .code = QueryErrorCode::InvalidSelector,
            .message = "Selector must not be empty. Expected #id, name:value, tag:value, or type:value.",
        };
        return result;
    }

    SelectorKind kind = SelectorKind::SemanticId;
    std::string_view value{};

    if (text.front() == '#')
    {
        kind = SelectorKind::SemanticId;
        value = text.substr(1);
    }
    else if (text.starts_with("name:"))
    {
        kind = SelectorKind::Name;
        value = text.substr(5);
    }
    else if (text.starts_with("tag:"))
    {
        kind = SelectorKind::Tag;
        value = text.substr(4);
    }
    else if (text.starts_with("type:"))
    {
        kind = SelectorKind::Type;
        value = text.substr(5);
    }
    else
    {
        result.error = QueryError{
            .code = QueryErrorCode::InvalidSelector,
            .message = "Unsupported selector syntax. Expected #id, name:value, tag:value, or type:value.",
        };
        return result;
    }

    if (value.empty())
    {
        result.error = QueryError{
            .code = QueryErrorCode::InvalidSelector,
            .message = "Selector value must not be empty.",
        };
        return result;
    }

    result.selector = SemanticSelector{
        .kind = kind,
        .value = std::string{value},
    };
    return result;
}

ComponentFieldSnapshot MakeFloatField(std::string name, const float value)
{
    FieldValue fieldValue{};
    fieldValue.kind = FieldValueKind::Float;
    fieldValue.floatValue = value;

    return ComponentFieldSnapshot{
        .name = std::move(name),
        .value = std::move(fieldValue),
    };
}

Transform2DSnapshot MakeTransformSnapshot(const scene::Transform2D& transform) noexcept
{
    return Transform2DSnapshot{
        .position = Vector2Snapshot{
            .x = transform.position.x,
            .y = transform.position.y,
        },
        .rotationRadians = transform.rotationRadians,
        .scale = Vector2Snapshot{
            .x = transform.scale.x,
            .y = transform.scale.y,
        },
    };
}

ComponentSnapshot MakeTransformComponent(const scene::Transform2D& transform)
{
    ComponentSnapshot component{};
    component.type = "Transform2D";
    component.fields.reserve(5);
    component.fields.push_back(MakeFloatField("position.x", transform.position.x));
    component.fields.push_back(MakeFloatField("position.y", transform.position.y));
    component.fields.push_back(MakeFloatField("rotation_radians", transform.rotationRadians));
    component.fields.push_back(MakeFloatField("scale.x", transform.scale.x));
    component.fields.push_back(MakeFloatField("scale.y", transform.scale.y));
    return component;
}

EntitySnapshot MakeEntitySnapshot(const scene::EntityId id, const scene::Entity& entity)
{
    EntitySnapshot snapshot{};
    snapshot.handle = EntityHandleSnapshot{
        .index = id.index,
        .generation = id.generation,
    };
    snapshot.semanticId = entity.SemanticId();
    snapshot.name = entity.Name();
    snapshot.tags = entity.Tags();

    const scene::Transform2D& transform = entity.Transform();
    snapshot.transform = MakeTransformSnapshot(transform);
    snapshot.bounds = std::nullopt;
    snapshot.components.reserve(1);
    snapshot.components.push_back(MakeTransformComponent(transform));
    return snapshot;
}

bool MatchesSelector(const scene::Entity& entity, const SemanticSelector& selector) noexcept
{
    switch (selector.kind)
    {
    case SelectorKind::SemanticId:
        return entity.SemanticId() == selector.value;
    case SelectorKind::Name:
        return entity.Name() == selector.value;
    case SelectorKind::Tag:
        return entity.HasTag(selector.value);
    case SelectorKind::Type:
        // P3 currently exposes Transform2D as the only authoritative component type.
        return selector.value == "Transform2D";
    }

    return false;
}
} // namespace

std::string_view ToString(const SelectorKind kind) noexcept
{
    switch (kind)
    {
    case SelectorKind::SemanticId:
        return "id";
    case SelectorKind::Name:
        return "name";
    case SelectorKind::Tag:
        return "tag";
    case SelectorKind::Type:
        return "type";
    }

    return "unknown";
}

std::string_view ToString(const QueryErrorCode code) noexcept
{
    switch (code)
    {
    case QueryErrorCode::SceneUnavailable:
        return "scene_unavailable";
    case QueryErrorCode::InvalidSelector:
        return "invalid_selector";
    case QueryErrorCode::NoMatch:
        return "no_match";
    case QueryErrorCode::AmbiguousMatch:
        return "ambiguous_match";
    }

    return "unknown_query_error";
}

QueryResult AgentFacade::Query(const std::string_view selectorText) const
{
    QueryResult result{};

    if (scene_ == nullptr)
    {
        result.error = QueryError{
            .code = QueryErrorCode::SceneUnavailable,
            .message = "No active scene is bound to the agent facade.",
        };
        return result;
    }

    SelectorParseResult parsed = ParseSelector(selectorText);
    if (!parsed.selector.has_value())
    {
        result.error = std::move(parsed.error);
        return result;
    }

    result.selector = std::move(parsed.selector);
    scene_->ForEachEntity(
        [&result](const scene::EntityId id, const scene::Entity& entity)
        {
            if (MatchesSelector(entity, *result.selector))
            {
                result.matches.push_back(MakeEntitySnapshot(id, entity));
            }
        });

    return result;
}

QueryOneResult AgentFacade::QueryOne(const std::string_view selectorText) const
{
    QueryResult many = Query(selectorText);

    QueryOneResult result{};
    result.selector = std::move(many.selector);

    if (many.error.has_value())
    {
        result.error = std::move(many.error);
        return result;
    }

    if (many.matches.empty())
    {
        result.error = QueryError{
            .code = QueryErrorCode::NoMatch,
            .message = "Selector matched no entities.",
        };
        return result;
    }

    if (many.matches.size() != 1U)
    {
        result.error = QueryError{
            .code = QueryErrorCode::AmbiguousMatch,
            .message = "Selector matched " + std::to_string(many.matches.size()) +
                       " entities; single-result query requires exactly one match.",
        };
        return result;
    }

    result.match = std::move(many.matches.front());
    return result;
}
} // namespace trace2d::agent
