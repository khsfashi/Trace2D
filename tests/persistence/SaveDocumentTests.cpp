#include <trace2d/persistence/SaveDocument.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace
{
trace2d::scene::SemanticValue Signed(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

trace2d::scene::SemanticValue Float(const double value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::Float;
    result.floatValue = value;
    return result;
}

trace2d::scene::SemanticValue Reference(
    const trace2d::scene::SemanticValueKind kind,
    std::string text)
{
    trace2d::scene::SemanticValue result{};
    result.kind = kind;
    result.textValue = std::move(text);
    return result;
}

trace2d::persistence::SaveDocument MakeDocument()
{
    trace2d::persistence::SaveDocument document{};
    document.saveId = "slot.main";
    document.producerVersion = "test-producer-1";

    trace2d::persistence::SaveRecord player{};
    player.id = "player.progress";
    player.typeId = "game.player-progress";
    player.schemaVersion = 3U;
    player.data.fields.push_back({
        .name = "spawn",
        .value = Reference(trace2d::scene::SemanticValueKind::EntityReference, "world.main/player"),
    });
    player.data.fields.push_back({
        .name = "health",
        .value = Signed(7),
    });

    trace2d::persistence::SaveRecord world{};
    world.id = "world.state";
    world.typeId = "game.world-state";
    world.schemaVersion = 1U;
    world.data.fields.push_back({
        .name = "theme",
        .value = Reference(trace2d::scene::SemanticValueKind::ResourceReference, "assets/themes/forest.theme"),
    });
    world.data.fields.push_back({
        .name = "time",
        .value = Float(12.5),
    });

    // Deliberately reverse canonical record order.
    document.records.push_back(std::move(world));
    document.records.push_back(std::move(player));
    return document;
}

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
    {
        const auto serial = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("trace2d-persistence-tests-" + std::to_string(serial));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored{};
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_{};
};

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    EXPECT_TRUE(input);
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::size_t FileCount(const std::filesystem::path& directory)
{
    std::size_t count = 0U;
    for (const auto& entry : std::filesystem::directory_iterator{directory})
    {
        if (entry.is_regular_file()) ++count;
    }
    return count;
}
} // namespace

TEST(SaveDocumentTests, CanonicalRoundtripIsByteIdenticalAndPreservesSemanticReferenceKinds)
{
    const trace2d::persistence::SaveSerializeResult serialized =
        trace2d::persistence::SerializeSaveDocumentJson(MakeDocument());
    ASSERT_TRUE(serialized.Succeeded());

    const std::size_t playerPosition = serialized.text.find("\"id\": \"player.progress\"");
    const std::size_t worldPosition = serialized.text.find("\"id\": \"world.state\"");
    ASSERT_NE(playerPosition, std::string::npos);
    ASSERT_NE(worldPosition, std::string::npos);
    EXPECT_LT(playerPosition, worldPosition);

    const std::size_t healthPosition = serialized.text.find("\"name\": \"health\"");
    const std::size_t spawnPosition = serialized.text.find("\"name\": \"spawn\"");
    ASSERT_NE(healthPosition, std::string::npos);
    ASSERT_NE(spawnPosition, std::string::npos);
    EXPECT_LT(healthPosition, spawnPosition);
    EXPECT_NE(serialized.text.find("\"kind\": \"entity_reference\""), std::string::npos);
    EXPECT_NE(serialized.text.find("\"kind\": \"resource_reference\""), std::string::npos);

    const trace2d::persistence::SaveParseResult parsed =
        trace2d::persistence::ParseSaveDocumentJson(serialized.text, "roundtrip.save");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_EQ(parsed.document->records.size(), 2U);
    EXPECT_EQ(parsed.document->records[0].id, "player.progress");
    ASSERT_EQ(parsed.document->records[0].data.fields.size(), 2U);
    EXPECT_EQ(parsed.document->records[0].data.fields[1].name, "spawn");
    EXPECT_EQ(
        parsed.document->records[0].data.fields[1].value.kind,
        trace2d::scene::SemanticValueKind::EntityReference);

    const trace2d::persistence::SaveSerializeResult reserialized =
        trace2d::persistence::SerializeSaveDocumentJson(*parsed.document);
    ASSERT_TRUE(reserialized.Succeeded());
    EXPECT_EQ(reserialized.text, serialized.text);
}

TEST(SaveDocumentTests, ParserFailsClosedOnUnknownStructureAndUnsupportedVersion)
{
    constexpr std::string_view text = R"json({
  "schema": "trace2d.save.v1",
  "format_version": 2,
  "producer_version": "test",
  "save_id": "slot.main",
  "records": [],
  "unexpected": true
})json";

    const trace2d::persistence::SaveParseResult parsed =
        trace2d::persistence::ParseSaveDocumentJson(text, "unsupported.save");
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_FALSE(parsed.document.has_value());
    ASSERT_GE(parsed.diagnostics.size(), 2U);

    bool sawUnknown = false;
    bool sawVersion = false;
    for (const auto& diagnostic : parsed.diagnostics)
    {
        if (diagnostic.path == "$.unexpected") sawUnknown = true;
        if (diagnostic.path == "$.format_version") sawVersion = true;
        EXPECT_EQ(diagnostic.source, "unsupported.save");
    }
    EXPECT_TRUE(sawUnknown);
    EXPECT_TRUE(sawVersion);
}

TEST(SaveDocumentTests, ParserRejectsDuplicateRecordAndFieldIdentity)
{
    constexpr std::string_view text = R"json({
  "schema": "trace2d.save.v1",
  "format_version": 1,
  "producer_version": "test",
  "save_id": "slot.main",
  "records": [
    {
      "id": "same",
      "type": "game.state",
      "version": 1,
      "fields": [
        {"name": "health", "kind": "signed_integer", "value": 1},
        {"name": "health", "kind": "signed_integer", "value": 2}
      ]
    },
    {
      "id": "same",
      "type": "game.other",
      "version": 1,
      "fields": []
    }
  ]
})json";

    const trace2d::persistence::SaveParseResult parsed =
        trace2d::persistence::ParseSaveDocumentJson(text, "duplicates.save");
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_FALSE(parsed.document.has_value());

    bool sawRecordDuplicate = false;
    bool sawFieldDuplicate = false;
    for (const auto& diagnostic : parsed.diagnostics)
    {
        if (diagnostic.message.find("Duplicate save record ID") != std::string::npos)
            sawRecordDuplicate = true;
        if (diagnostic.message.find("Duplicate semantic field name") != std::string::npos)
            sawFieldDuplicate = true;
    }
    EXPECT_TRUE(sawRecordDuplicate);
    EXPECT_TRUE(sawFieldDuplicate);
}

TEST(SaveDocumentTests, SerializerRejectsNonFiniteSemanticValues)
{
    trace2d::persistence::SaveDocument document = MakeDocument();
    document.records[0].data.fields.push_back({
        .name = "invalid",
        .value = Float(std::numeric_limits<double>::quiet_NaN()),
    });

    const trace2d::persistence::SaveSerializeResult serialized =
        trace2d::persistence::SerializeSaveDocumentJson(document);
    EXPECT_FALSE(serialized.Succeeded());
    EXPECT_TRUE(serialized.text.empty());
}

TEST(SaveDocumentTests, FileWriteValidatesCanonicalPayloadAndLeavesOnlyAuthoritativeTarget)
{
    TemporaryDirectory directory{};
    const std::filesystem::path target = directory.Path() / "slot.save.json";
    const trace2d::persistence::SaveDocument document = MakeDocument();

    const trace2d::persistence::SaveWriteResult write =
        trace2d::persistence::WriteSaveDocumentFile(target, document);
    ASSERT_TRUE(write.Succeeded());
    EXPECT_EQ(FileCount(directory.Path()), 1U);

    const trace2d::persistence::SaveParseResult loaded =
        trace2d::persistence::ReadSaveDocumentFile(target);
    ASSERT_TRUE(loaded.Succeeded());
    const trace2d::persistence::SaveSerializeResult canonical =
        trace2d::persistence::SerializeSaveDocumentJson(*loaded.document);
    ASSERT_TRUE(canonical.Succeeded());
    EXPECT_EQ(ReadText(target), canonical.text);
}

TEST(SaveDocumentTests, InvalidReplacementNeverChangesExistingAuthoritativeSave)
{
    TemporaryDirectory directory{};
    const std::filesystem::path target = directory.Path() / "slot.save.json";
    const trace2d::persistence::SaveDocument valid = MakeDocument();
    ASSERT_TRUE(trace2d::persistence::WriteSaveDocumentFile(target, valid).Succeeded());
    const std::string before = ReadText(target);

    trace2d::persistence::SaveDocument invalid = valid;
    invalid.records[0].data.fields.push_back({
        .name = "bad-number",
        .value = Float(std::numeric_limits<double>::infinity()),
    });
    const trace2d::persistence::SaveWriteResult write =
        trace2d::persistence::WriteSaveDocumentFile(target, invalid);

    EXPECT_FALSE(write.Succeeded());
    EXPECT_EQ(ReadText(target), before);
    EXPECT_EQ(FileCount(directory.Path()), 1U);
}
