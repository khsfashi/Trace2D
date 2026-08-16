#include <trace2d/application/Application.hpp>

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/scene/Components.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
struct HealthState final
{
    std::int64_t current{0};
};

trace2d::scene::ComponentTypeHandle<HealthState> RegisterHealthState(
    trace2d::scene::ComponentRegistry& registry)
{
    trace2d::scene::ComponentRegistration<HealthState> registration{};
    registration.typeId = "test.health";
    registration.schemaVersion = 1;
    registration.componentClass = trace2d::scene::ComponentClass::RuntimeOnly;
    registration.inspect = [](const HealthState& health)
    {
        trace2d::scene::SemanticValue value{};
        value.kind = trace2d::scene::SemanticValueKind::SignedInteger;
        value.signedIntegerValue = health.current;
        return std::vector<trace2d::scene::ComponentInspectionField>{
            trace2d::scene::ComponentInspectionField{
                .name = "current",
                .value = std::move(value),
            },
        };
    };
    return registry.Register<HealthState>(std::move(registration));
}

class RegistryGame final : public trace2d::application::Game
{
public:
    explicit RegistryGame(trace2d::scene::ComponentTypeHandle<HealthState> healthType) noexcept
        : healthType_{healthType}
    {
    }

    void OnStart(trace2d::application::GameContext& context) override
    {
        trace2d::scene::EntityDescriptor player{};
        player.semanticId = "player";
        const trace2d::scene::EntityId playerId = context.Scene().CreateEntity(std::move(player));
        context.Scene().AddComponent(playerId, healthType_, HealthState{.current = 3});
    }

    void OnFixedUpdate(
        trace2d::application::GameContext&,
        const trace2d::application::FixedUpdate&) override
    {
    }

private:
    trace2d::scene::ComponentTypeHandle<HealthState> healthType_{};
};

TEST(ApplicationRegistryTests, CallerOwnedRegistryFeedsCanonicalAgentInspection)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto healthType = RegisterHealthState(registry);
    registry.Freeze();

    RegistryGame game{healthType};
    trace2d::application::ApplicationConfig config{};
    config.scene.semanticId = "b2.runtime-verifier-boundary";
    trace2d::application::Application application{game, registry, config};

    EXPECT_EQ(application.Scene().Registry(), &registry);
    application.Start();

    trace2d::agent::AgentFacade facade{
        &application.Runtime(),
        &application.Scene(),
        &application.Ui()};
    const trace2d::agent::QueryOneResult query = facade.QueryOne("#player");
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());

    const auto component = std::find_if(
        query.match->components.begin(),
        query.match->components.end(),
        [](const trace2d::agent::ComponentSnapshot& value)
        {
            return value.type == "test.health";
        });
    ASSERT_NE(component, query.match->components.end());
    ASSERT_EQ(component->fields.size(), 1U);
    EXPECT_EQ(component->fields.front().name, "current");
    EXPECT_EQ(component->fields.front().value.kind, trace2d::agent::FieldValueKind::SignedInteger);
    EXPECT_EQ(component->fields.front().value.signedIntegerValue, 3);
}

TEST(ApplicationRegistryTests, RegistryMustBeFrozenBeforeApplicationConstruction)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto healthType = RegisterHealthState(registry);
    RegistryGame game{healthType};

    EXPECT_THROW(
        static_cast<void>(trace2d::application::Application{game, registry}),
        std::invalid_argument);
}
} // namespace
