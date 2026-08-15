#include <trace2d/input/Input.hpp>
#include <trace2d/input/InputMap.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

namespace
{
using trace2d::input::ActionMap;
using trace2d::input::InputAxis;
using trace2d::input::InputControl;
using trace2d::input::InputEvent;
using trace2d::input::InputEventType;
using trace2d::input::InputMapDocument;
using trace2d::input::InputMapErrorCode;
using trace2d::input::InputMapStore;
using trace2d::input::InputSystem;

constexpr std::string_view AuthoredMap = R"toml(format_version = 1

[[buttons]]
id = "jump"
controls = ["GamepadSouth", "Space"]

[[axes1d]]
id = "move_x"
negative = "KeyA"
positive = "KeyD"
analog = [
  { axis = "GamepadLeftX", deadzone = 0.2, scale = 1.0 },
]
)toml";

TEST(InputMapTests, AuthoredDefinitionsBuildTheSameRuntimeSemanticsAsProgrammaticBindings)
{
    auto load = trace2d::input::ParseInputMapToml(AuthoredMap, "input.trace2d.input.toml");
    ASSERT_TRUE(load.Succeeded());

    auto build = trace2d::input::BuildActionMap(*load.document);
    ASSERT_TRUE(build.Succeeded());
    ActionMap authored = std::move(*build.actionMap);

    ActionMap programmatic{};
    const auto jump = programmatic.AddButtonAction("jump");
    programmatic.BindButton(jump, InputControl::Space);
    programmatic.BindButton(jump, InputControl::GamepadSouth);
    const auto moveX = programmatic.AddAxis1DAction("move_x", InputControl::KeyA, InputControl::KeyD);
    programmatic.BindAxis1DAnalog(moveX, InputAxis::GamepadLeftX, 0.2F, 1.0F);
    programmatic.Finalize();

    InputSystem input{};
    input.ApplyEvent(InputEvent{.type = InputEventType::DeviceConnected, .device = 7U});
    input.ApplyEvent(InputEvent{
        .type = InputEventType::AxisMotion,
        .axis = InputAxis::GamepadLeftX,
        .device = 7U,
        .value = 0.6F,
    });
    input.ApplyEvent(InputEvent{.control = InputControl::Space, .type = InputEventType::Press});

    authored.Resolve(input);
    programmatic.Resolve(input);

    const auto authoredJump = authored.FindButtonAction("jump");
    const auto authoredMove = authored.FindAxis1DAction("move_x");
    ASSERT_TRUE(authoredJump.has_value());
    ASSERT_TRUE(authoredMove.has_value());

    EXPECT_EQ(authored.ButtonState(*authoredJump), programmatic.ButtonState(jump));
    EXPECT_FLOAT_EQ(authored.Axis1D(*authoredMove), programmatic.Axis1D(moveX));
    EXPECT_NEAR(authored.Axis1D(*authoredMove), 0.5F, 0.00001F);
}

TEST(InputMapTests, CanonicalSerializationIsStableAcrossSaveLoadSave)
{
    constexpr std::string_view nonCanonical = R"toml(format_version = 1

[[axes1d]]
id = "move_x"
positive = "KeyD"
negative = "KeyA"
analog = [
  { axis = "GamepadRightX", scale = -1, deadzone = 0.1 },
  { axis = "GamepadLeftX", scale = 1, deadzone = 0.2 },
]

[[buttons]]
id = "jump"
controls = ["Space", "Enter"]

[[buttons]]
id = "attack"
controls = ["MouseLeft"]
)toml";

    auto firstLoad = trace2d::input::ParseInputMapToml(nonCanonical);
    ASSERT_TRUE(firstLoad.Succeeded());
    const std::string canonical = trace2d::input::SaveInputMapToml(*firstLoad.document);

    auto secondLoad = trace2d::input::ParseInputMapToml(canonical);
    ASSERT_TRUE(secondLoad.Succeeded());
    EXPECT_EQ(trace2d::input::SaveInputMapToml(*secondLoad.document), canonical);
}

TEST(InputMapTests, InvalidVersionsControlsAndAnalogParametersProduceStructuredDiagnostics)
{
    constexpr std::string_view invalid = R"toml(format_version = 2

[[buttons]]
id = "jump"
controls = ["NotAControl"]

[[axes1d]]
id = "move_x"
analog = [
  { axis = "GamepadLeftX", deadzone = 1.0, scale = 0.0 },
]
)toml";

    const auto result = trace2d::input::ParseInputMapToml(invalid, "broken.input.toml");
    EXPECT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());

    bool unsupportedFormat = false;
    bool schemaError = false;
    for (const auto& diagnostic : result.diagnostics)
    {
        unsupportedFormat = unsupportedFormat || diagnostic.code == InputMapErrorCode::UnsupportedFormat;
        schemaError = schemaError || diagnostic.code == InputMapErrorCode::SchemaError;
        EXPECT_FALSE(diagnostic.path.empty());
        EXPECT_FALSE(diagnostic.message.empty());
    }
    EXPECT_TRUE(unsupportedFormat);
    EXPECT_TRUE(schemaError);
}

TEST(InputMapTests, RebindingUsesExpectedCurrentBindingAsAStaleWritePrecondition)
{
    auto load = trace2d::input::ParseInputMapToml(AuthoredMap);
    ASSERT_TRUE(load.Succeeded());
    InputMapDocument document = *load.document;

    const InputMapDocument beforeStale = document;
    const auto stale = trace2d::input::RebindControl(
        document,
        "jump",
        InputControl::Enter,
        InputControl::KeyJ);
    EXPECT_FALSE(stale.Succeeded());
    ASSERT_EQ(stale.diagnostics.size(), 1U);
    EXPECT_EQ(stale.diagnostics[0].code, InputMapErrorCode::StaleBinding);
    EXPECT_EQ(document, beforeStale);

    const auto buttonRebind = trace2d::input::RebindControl(
        document,
        "jump",
        InputControl::Space,
        InputControl::KeyJ);
    EXPECT_TRUE(buttonRebind.Succeeded());
    EXPECT_TRUE(buttonRebind.changed);

    const auto axisRebind = trace2d::input::RebindAnalogAxis(
        document,
        "move_x",
        InputAxis::GamepadLeftX,
        InputAxis::GamepadRightX);
    EXPECT_TRUE(axisRebind.Succeeded());
    EXPECT_TRUE(axisRebind.changed);

    const auto rebuilt = trace2d::input::BuildActionMap(document);
    EXPECT_TRUE(rebuilt.Succeeded());
}

TEST(InputMapTests, ProjectStorePersistsAndReloadsCanonicalRebindingsWithoutDiscovery)
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("trace2d-input-map-" + std::to_string(nonce));
    std::filesystem::remove_all(root);

    auto load = trace2d::input::ParseInputMapToml(AuthoredMap);
    ASSERT_TRUE(load.Succeeded());
    InputMapDocument document = *load.document;
    ASSERT_TRUE(trace2d::input::RebindControl(
                    document,
                    "jump",
                    InputControl::Space,
                    InputControl::KeyJ)
                    .Succeeded());

    const InputMapStore store{root};
    const auto saveDiagnostics = store.Save("config/gameplay.input.toml", document);
    EXPECT_TRUE(saveDiagnostics.empty());

    const auto persisted = store.Load("config/gameplay.input.toml");
    ASSERT_TRUE(persisted.Succeeded());
    EXPECT_EQ(
        trace2d::input::SaveInputMapToml(*persisted.document),
        trace2d::input::SaveInputMapToml(document));

    const auto traversal = store.Load("../outside.input.toml");
    EXPECT_FALSE(traversal.Succeeded());
    ASSERT_EQ(traversal.diagnostics.size(), 1U);
    EXPECT_EQ(traversal.diagnostics[0].code, InputMapErrorCode::InvalidReference);

    std::filesystem::remove_all(root);
}

TEST(InputMapTests, EmptyHandBuiltDocumentIsRejectedBeforePersistence)
{
    const auto result = trace2d::input::BuildActionMap(InputMapDocument{});
    EXPECT_FALSE(result.Succeeded());
}
} // namespace
