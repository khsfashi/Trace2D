#include <trace2d/persistence/SaveDocument.hpp>

#include <trace2d/core/FileTransaction.hpp>
#include <trace2d/core/Version.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::persistence
{
namespace
{
using Json = nlohmann::json;
using OrderedJson = nlohmann::ordered_json;

void AddDiagnostic(
    std::vector<SaveDiagnostic>& diagnostics,
    const std::string_view source,
    std::string path,
    std::string message,
    const std::size_t line = 0U,
    const std::size_t column = 0U)
{
    diagnostics.push_back(SaveDiagnostic{
        .source = std::string{source},
        .path = std::move(path),
        .message = std::move(message),
        .line = line,
        .column = column,
    });
}

std::pair<std::size_t, std::size_t> PositionForByte(
    const std::string_view text,
    const std::size_t oneBasedByte) noexcept
{
    std::size_t line = 1U;
    std::size_t column = 1U;
    const std::size_t limit = oneBasedByte == 0U
        ? 0U
        : std::min(text.size(), oneBasedByte - 1U);
    for (std::size_t index = 0U; index < limit; ++index)
    {
        if (text[index] == '\n')
        {
            ++line;
            column = 1U;
        }
        else
        {
            ++column;
        }
    }
    return {line, column};
}

bool IsKnownKey(
    const std::string_view key,
    const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const Json& object,
    const std::string_view source,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<SaveDiagnostic>& diagnostics)
{
    if (!object.is_object())
    {
        return;
    }
    for (const auto& [key, unused] : object.items())
    {
        static_cast<void>(unused);
        if (IsKnownKey(key, knownKeys))
        {
            continue;
        }
        AddDiagnostic(
            diagnostics,
            source,
            std::string{path} + "." + key,
            "Unknown field.");
    }
}

std::optional<std::string> ReadRequiredString(
    const Json& object,
    const std::string_view key,
    const std::string_view source,
    const std::string_view path,
    std::vector<SaveDiagnostic>& diagnostics,
    const bool requireNonEmpty = true)
{
    const auto iterator = object.find(key);
    if (iterator == object.end())
    {
        AddDiagnostic(diagnostics, source, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    if (!iterator->is_string())
    {
        AddDiagnostic(diagnostics, source, std::string{path}, "Expected a string.");
        return std::nullopt;
    }
    const std::string value = iterator->get<std::string>();
    if (requireNonEmpty && value.empty())
    {
        AddDiagnostic(diagnostics, source, std::string{path}, "Value must not be empty.");
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint32_t> ReadPositiveUint32(
    const Json& object,
    const std::string_view key,
    const std::string_view source,
    const std::string_view path,
    std::vector<SaveDiagnostic>& diagnostics)
{
    const auto iterator = object.find(key);
    if (iterator == object.end())
    {
        AddDiagnostic(diagnostics, source, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    std::uint64_t value = 0U;
    if (iterator->is_number_unsigned())
    {
        value = iterator->get<std::uint64_t>();
    }
    else if (iterator->is_number_integer())
    {
        const std::int64_t signedValue = iterator->get<std::int64_t>();
        if (signedValue <= 0)
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Version must be a positive uint32 value.");
            return std::nullopt;
        }
        value = static_cast<std::uint64_t>(signedValue);
    }
    else
    {
        AddDiagnostic(diagnostics, source, std::string{path}, "Expected an integer version.");
        return std::nullopt;
    }

    if (value == 0U || value > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        AddDiagnostic(diagnostics, source, std::string{path}, "Version must be a positive uint32 value.");
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::string_view SemanticKindName(const scene::SemanticValueKind kind) noexcept
{
    switch (kind)
    {
    case scene::SemanticValueKind::Boolean: return "boolean";
    case scene::SemanticValueKind::SignedInteger: return "signed_integer";
    case scene::SemanticValueKind::UnsignedInteger: return "unsigned_integer";
    case scene::SemanticValueKind::Float: return "float";
    case scene::SemanticValueKind::Text: return "text";
    case scene::SemanticValueKind::Float2: return "float2";
    case scene::SemanticValueKind::Float4: return "float4";
    case scene::SemanticValueKind::EntityReference: return "entity_reference";
    case scene::SemanticValueKind::ResourceReference: return "resource_reference";
    case scene::SemanticValueKind::EnumName: return "enum_name";
    }
    return {};
}

std::optional<scene::SemanticValueKind> ParseSemanticKind(const std::string_view name) noexcept
{
    if (name == "boolean") return scene::SemanticValueKind::Boolean;
    if (name == "signed_integer") return scene::SemanticValueKind::SignedInteger;
    if (name == "unsigned_integer") return scene::SemanticValueKind::UnsignedInteger;
    if (name == "float") return scene::SemanticValueKind::Float;
    if (name == "text") return scene::SemanticValueKind::Text;
    if (name == "float2") return scene::SemanticValueKind::Float2;
    if (name == "float4") return scene::SemanticValueKind::Float4;
    if (name == "entity_reference") return scene::SemanticValueKind::EntityReference;
    if (name == "resource_reference") return scene::SemanticValueKind::ResourceReference;
    if (name == "enum_name") return scene::SemanticValueKind::EnumName;
    return std::nullopt;
}

std::optional<OrderedJson> SerializeSemanticValue(
    const scene::SemanticValue& value,
    const std::string_view path,
    std::vector<SaveDiagnostic>& diagnostics)
{
    switch (value.kind)
    {
    case scene::SemanticValueKind::Boolean:
        return OrderedJson{value.booleanValue};
    case scene::SemanticValueKind::SignedInteger:
        return OrderedJson{value.signedIntegerValue};
    case scene::SemanticValueKind::UnsignedInteger:
        return OrderedJson{value.unsignedIntegerValue};
    case scene::SemanticValueKind::Float:
        if (!std::isfinite(value.floatValue))
        {
            AddDiagnostic(diagnostics, {}, std::string{path}, "Floating value must be finite.");
            return std::nullopt;
        }
        return OrderedJson{value.floatValue};
    case scene::SemanticValueKind::Text:
        return OrderedJson{value.textValue};
    case scene::SemanticValueKind::EntityReference:
    case scene::SemanticValueKind::ResourceReference:
    case scene::SemanticValueKind::EnumName:
        if (value.textValue.empty())
        {
            AddDiagnostic(diagnostics, {}, std::string{path}, "Semantic reference/name value must not be empty.");
            return std::nullopt;
        }
        return OrderedJson{value.textValue};
    case scene::SemanticValueKind::Float2:
    {
        if (!std::isfinite(value.vectorValue[0]) || !std::isfinite(value.vectorValue[1]))
        {
            AddDiagnostic(diagnostics, {}, std::string{path}, "float2 values must be finite.");
            return std::nullopt;
        }
        OrderedJson result = OrderedJson::array();
        result.push_back(value.vectorValue[0]);
        result.push_back(value.vectorValue[1]);
        return result;
    }
    case scene::SemanticValueKind::Float4:
    {
        OrderedJson result = OrderedJson::array();
        for (const double element : value.vectorValue)
        {
            if (!std::isfinite(element))
            {
                AddDiagnostic(diagnostics, {}, std::string{path}, "float4 values must be finite.");
                return std::nullopt;
            }
            result.push_back(element);
        }
        return result;
    }
    }

    AddDiagnostic(diagnostics, {}, std::string{path}, "Unsupported semantic value kind.");
    return std::nullopt;
}

std::optional<scene::SemanticValue> ParseSemanticValue(
    const scene::SemanticValueKind kind,
    const Json& node,
    const std::string_view source,
    const std::string_view path,
    std::vector<SaveDiagnostic>& diagnostics)
{
    scene::SemanticValue value{};
    value.kind = kind;
    switch (kind)
    {
    case scene::SemanticValueKind::Boolean:
        if (!node.is_boolean())
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Expected a boolean value.");
            return std::nullopt;
        }
        value.booleanValue = node.get<bool>();
        return value;
    case scene::SemanticValueKind::SignedInteger:
        if (node.is_number_unsigned())
        {
            const std::uint64_t raw = node.get<std::uint64_t>();
            if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            {
                AddDiagnostic(diagnostics, source, std::string{path}, "Signed integer exceeds int64 range.");
                return std::nullopt;
            }
            value.signedIntegerValue = static_cast<std::int64_t>(raw);
            return value;
        }
        if (!node.is_number_integer())
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Expected an integer value.");
            return std::nullopt;
        }
        value.signedIntegerValue = node.get<std::int64_t>();
        return value;
    case scene::SemanticValueKind::UnsignedInteger:
        if (node.is_number_unsigned())
        {
            value.unsignedIntegerValue = node.get<std::uint64_t>();
            return value;
        }
        if (node.is_number_integer())
        {
            const std::int64_t raw = node.get<std::int64_t>();
            if (raw < 0)
            {
                AddDiagnostic(diagnostics, source, std::string{path}, "Unsigned integer must not be negative.");
                return std::nullopt;
            }
            value.unsignedIntegerValue = static_cast<std::uint64_t>(raw);
            return value;
        }
        AddDiagnostic(diagnostics, source, std::string{path}, "Expected an unsigned integer value.");
        return std::nullopt;
    case scene::SemanticValueKind::Float:
        if (!node.is_number())
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Expected a numeric value.");
            return std::nullopt;
        }
        value.floatValue = node.get<double>();
        if (!std::isfinite(value.floatValue))
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Floating value must be finite.");
            return std::nullopt;
        }
        return value;
    case scene::SemanticValueKind::Text:
    case scene::SemanticValueKind::EntityReference:
    case scene::SemanticValueKind::ResourceReference:
    case scene::SemanticValueKind::EnumName:
        if (!node.is_string())
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Expected a string value.");
            return std::nullopt;
        }
        value.textValue = node.get<std::string>();
        if (kind != scene::SemanticValueKind::Text && value.textValue.empty())
        {
            AddDiagnostic(diagnostics, source, std::string{path}, "Semantic reference/name value must not be empty.");
            return std::nullopt;
        }
        return value;
    case scene::SemanticValueKind::Float2:
    case scene::SemanticValueKind::Float4:
    {
        const std::size_t expected = kind == scene::SemanticValueKind::Float2 ? 2U : 4U;
        if (!node.is_array() || node.size() != expected)
        {
            AddDiagnostic(
                diagnostics,
                source,
                std::string{path},
                "Expected an array with exactly " + std::to_string(expected) + " numeric values.");
            return std::nullopt;
        }
        for (std::size_t index = 0U; index < expected; ++index)
        {
            const Json& element = node[index];
            if (!element.is_number())
            {
                AddDiagnostic(
                    diagnostics,
                    source,
                    std::string{path} + "[" + std::to_string(index) + "]",
                    "Expected a numeric value.");
                return std::nullopt;
            }
            value.vectorValue[index] = element.get<double>();
            if (!std::isfinite(value.vectorValue[index]))
            {
                AddDiagnostic(
                    diagnostics,
                    source,
                    std::string{path} + "[" + std::to_string(index) + "]",
                    "Numeric value must be finite.");
                return std::nullopt;
            }
        }
        return value;
    }
    }
    AddDiagnostic(diagnostics, source, std::string{path}, "Unsupported semantic value kind.");
    return std::nullopt;
}

void Canonicalize(SaveDocument& document)
{
    std::sort(document.records.begin(), document.records.end(), [](const SaveRecord& left, const SaveRecord& right)
    {
        return left.id < right.id;
    });
    for (SaveRecord& record : document.records)
    {
        std::sort(record.data.fields.begin(), record.data.fields.end(), [](const auto& left, const auto& right)
        {
            return left.name < right.name;
        });
    }
}

std::optional<std::string> ReadTextFile(
    const std::filesystem::path& path,
    std::string& errorMessage)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        errorMessage = "Unable to open save file for reading.";
        return std::nullopt;
    }
    std::string text{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (input.bad())
    {
        errorMessage = "Unable to read save file completely.";
        return std::nullopt;
    }
    return text;
}
} // namespace

SaveParseResult ParseSaveDocumentJson(
    const std::string_view text,
    const std::string_view sourceName)
{
    SaveParseResult result{};
    Json root{};
    try
    {
        root = Json::parse(text.begin(), text.end());
    }
    catch (const Json::parse_error& error)
    {
        const auto [line, column] = PositionForByte(text, error.byte);
        AddDiagnostic(result.diagnostics, sourceName, "$", error.what(), line, column);
        return result;
    }

    if (!root.is_object())
    {
        AddDiagnostic(result.diagnostics, sourceName, "$", "Save document root must be an object.");
        return result;
    }
    ValidateKnownKeys(
        root,
        sourceName,
        "$",
        {"schema", "format_version", "producer_version", "save_id", "records"},
        result.diagnostics);

    const std::optional<std::string> schema =
        ReadRequiredString(root, "schema", sourceName, "$.schema", result.diagnostics);
    if (schema.has_value() && *schema != SaveSchemaId)
    {
        AddDiagnostic(
            result.diagnostics,
            sourceName,
            "$.schema",
            "Unsupported save schema '" + *schema + "'. Expected '" + std::string{SaveSchemaId} + "'.");
    }

    const std::optional<std::uint32_t> formatVersion =
        ReadPositiveUint32(root, "format_version", sourceName, "$.format_version", result.diagnostics);
    if (formatVersion.has_value() && *formatVersion != CurrentSaveFormatVersion)
    {
        AddDiagnostic(
            result.diagnostics,
            sourceName,
            "$.format_version",
            "Unsupported save format version " + std::to_string(*formatVersion) + ". Supported version is 1.");
    }

    SaveDocument document{};
    const std::optional<std::string> producerVersion =
        ReadRequiredString(root, "producer_version", sourceName, "$.producer_version", result.diagnostics);
    const std::optional<std::string> saveId =
        ReadRequiredString(root, "save_id", sourceName, "$.save_id", result.diagnostics);
    if (producerVersion.has_value()) document.producerVersion = *producerVersion;
    if (saveId.has_value()) document.saveId = *saveId;

    const auto recordsIterator = root.find("records");
    if (recordsIterator == root.end())
    {
        AddDiagnostic(result.diagnostics, sourceName, "$.records", "Required field is missing.");
    }
    else if (!recordsIterator->is_array())
    {
        AddDiagnostic(result.diagnostics, sourceName, "$.records", "Expected an array of save records.");
    }
    else
    {
        document.records.reserve(recordsIterator->size());
        for (std::size_t recordIndex = 0U; recordIndex < recordsIterator->size(); ++recordIndex)
        {
            const Json& recordNode = (*recordsIterator)[recordIndex];
            const std::string recordPath = "$.records[" + std::to_string(recordIndex) + "]";
            if (!recordNode.is_object())
            {
                AddDiagnostic(result.diagnostics, sourceName, recordPath, "Expected a save record object.");
                continue;
            }
            ValidateKnownKeys(
                recordNode,
                sourceName,
                recordPath,
                {"id", "type", "version", "fields"},
                result.diagnostics);

            SaveRecord record{};
            const std::optional<std::string> recordId =
                ReadRequiredString(recordNode, "id", sourceName, recordPath + ".id", result.diagnostics);
            const std::optional<std::string> typeId =
                ReadRequiredString(recordNode, "type", sourceName, recordPath + ".type", result.diagnostics);
            const std::optional<std::uint32_t> schemaVersion =
                ReadPositiveUint32(recordNode, "version", sourceName, recordPath + ".version", result.diagnostics);
            if (recordId.has_value()) record.id = *recordId;
            if (typeId.has_value()) record.typeId = *typeId;
            if (schemaVersion.has_value()) record.schemaVersion = *schemaVersion;

            const auto fieldsIterator = recordNode.find("fields");
            if (fieldsIterator == recordNode.end())
            {
                AddDiagnostic(result.diagnostics, sourceName, recordPath + ".fields", "Required field is missing.");
            }
            else if (!fieldsIterator->is_array())
            {
                AddDiagnostic(result.diagnostics, sourceName, recordPath + ".fields", "Expected an array of semantic fields.");
            }
            else
            {
                record.data.fields.reserve(fieldsIterator->size());
                for (std::size_t fieldIndex = 0U; fieldIndex < fieldsIterator->size(); ++fieldIndex)
                {
                    const Json& fieldNode = (*fieldsIterator)[fieldIndex];
                    const std::string fieldPath = recordPath + ".fields[" + std::to_string(fieldIndex) + "]";
                    if (!fieldNode.is_object())
                    {
                        AddDiagnostic(result.diagnostics, sourceName, fieldPath, "Expected a semantic field object.");
                        continue;
                    }
                    ValidateKnownKeys(
                        fieldNode,
                        sourceName,
                        fieldPath,
                        {"name", "kind", "value"},
                        result.diagnostics);
                    const std::optional<std::string> fieldName =
                        ReadRequiredString(fieldNode, "name", sourceName, fieldPath + ".name", result.diagnostics);
                    const std::optional<std::string> kindName =
                        ReadRequiredString(fieldNode, "kind", sourceName, fieldPath + ".kind", result.diagnostics);
                    const auto valueIterator = fieldNode.find("value");
                    if (valueIterator == fieldNode.end())
                    {
                        AddDiagnostic(result.diagnostics, sourceName, fieldPath + ".value", "Required field is missing.");
                        continue;
                    }
                    if (!fieldName.has_value() || !kindName.has_value())
                    {
                        continue;
                    }
                    const std::optional<scene::SemanticValueKind> kind = ParseSemanticKind(*kindName);
                    if (!kind.has_value())
                    {
                        AddDiagnostic(
                            result.diagnostics,
                            sourceName,
                            fieldPath + ".kind",
                            "Unsupported semantic value kind '" + *kindName + "'.");
                        continue;
                    }
                    const std::optional<scene::SemanticValue> value = ParseSemanticValue(
                        *kind,
                        *valueIterator,
                        sourceName,
                        fieldPath + ".value",
                        result.diagnostics);
                    if (value.has_value())
                    {
                        record.data.fields.push_back(scene::ComponentAuthoringField{*fieldName, *value});
                    }
                }
            }
            document.records.push_back(std::move(record));
        }
    }

    Canonicalize(document);
    for (std::size_t recordIndex = 0U; recordIndex < document.records.size(); ++recordIndex)
    {
        const SaveRecord& record = document.records[recordIndex];
        if (recordIndex > 0U && !record.id.empty() && record.id == document.records[recordIndex - 1U].id)
        {
            AddDiagnostic(
                result.diagnostics,
                sourceName,
                "$.records",
                "Duplicate save record ID '" + record.id + "'.");
        }
        for (std::size_t fieldIndex = 1U; fieldIndex < record.data.fields.size(); ++fieldIndex)
        {
            if (record.data.fields[fieldIndex - 1U].name == record.data.fields[fieldIndex].name)
            {
                AddDiagnostic(
                    result.diagnostics,
                    sourceName,
                    "$.records[" + std::to_string(recordIndex) + "].fields",
                    "Duplicate semantic field name '" + record.data.fields[fieldIndex].name + "'.");
            }
        }
    }

    if (result.diagnostics.empty())
    {
        result.document = std::move(document);
    }
    return result;
}

SaveSerializeResult SerializeSaveDocumentJson(const SaveDocument& document)
{
    SaveSerializeResult result{};
    if (document.saveId.empty())
    {
        AddDiagnostic(result.diagnostics, {}, "$.save_id", "Save semantic ID must not be empty.");
    }

    const std::string producerVersion = document.producerVersion.empty()
        ? std::string{core::Version()}
        : document.producerVersion;
    if (producerVersion.empty())
    {
        AddDiagnostic(result.diagnostics, {}, "$.producer_version", "Producer version must not be empty.");
    }

    std::vector<const SaveRecord*> records{};
    records.reserve(document.records.size());
    for (const SaveRecord& record : document.records)
    {
        records.push_back(&record);
    }
    std::sort(records.begin(), records.end(), [](const SaveRecord* left, const SaveRecord* right)
    {
        return left->id < right->id;
    });

    for (std::size_t index = 0U; index < records.size(); ++index)
    {
        const SaveRecord& record = *records[index];
        const std::string recordPath = "$.records[" + std::to_string(index) + "]";
        if (record.id.empty())
        {
            AddDiagnostic(result.diagnostics, {}, recordPath + ".id", "Save record ID must not be empty.");
        }
        if (record.typeId.empty())
        {
            AddDiagnostic(result.diagnostics, {}, recordPath + ".type", "Save record type ID must not be empty.");
        }
        if (record.schemaVersion == 0U)
        {
            AddDiagnostic(result.diagnostics, {}, recordPath + ".version", "Save record schema version must be positive.");
        }
        if (index > 0U && !record.id.empty() && record.id == records[index - 1U]->id)
        {
            AddDiagnostic(result.diagnostics, {}, "$.records", "Duplicate save record ID '" + record.id + "'.");
        }

        std::vector<const scene::ComponentAuthoringField*> fields{};
        fields.reserve(record.data.fields.size());
        for (const scene::ComponentAuthoringField& field : record.data.fields)
        {
            fields.push_back(&field);
        }
        std::sort(fields.begin(), fields.end(), [](const auto* left, const auto* right)
        {
            return left->name < right->name;
        });
        for (std::size_t fieldIndex = 0U; fieldIndex < fields.size(); ++fieldIndex)
        {
            const auto& field = *fields[fieldIndex];
            const std::string fieldPath = recordPath + ".fields[" + std::to_string(fieldIndex) + "]";
            if (field.name.empty())
            {
                AddDiagnostic(result.diagnostics, {}, fieldPath + ".name", "Semantic field name must not be empty.");
            }
            if (fieldIndex > 0U && field.name == fields[fieldIndex - 1U]->name)
            {
                AddDiagnostic(
                    result.diagnostics,
                    {},
                    recordPath + ".fields",
                    "Duplicate semantic field name '" + field.name + "'.");
            }
            if (SemanticKindName(field.value.kind).empty())
            {
                AddDiagnostic(result.diagnostics, {}, fieldPath + ".kind", "Unsupported semantic value kind.");
                continue;
            }
            static_cast<void>(SerializeSemanticValue(field.value, fieldPath + ".value", result.diagnostics));
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    OrderedJson root = OrderedJson::object();
    root["schema"] = SaveSchemaId;
    root["format_version"] = CurrentSaveFormatVersion;
    root["producer_version"] = producerVersion;
    root["save_id"] = document.saveId;
    root["records"] = OrderedJson::array();

    for (const SaveRecord* record : records)
    {
        OrderedJson recordNode = OrderedJson::object();
        recordNode["id"] = record->id;
        recordNode["type"] = record->typeId;
        recordNode["version"] = record->schemaVersion;
        recordNode["fields"] = OrderedJson::array();

        std::vector<const scene::ComponentAuthoringField*> fields{};
        fields.reserve(record->data.fields.size());
        for (const scene::ComponentAuthoringField& field : record->data.fields)
        {
            fields.push_back(&field);
        }
        std::sort(fields.begin(), fields.end(), [](const auto* left, const auto* right)
        {
            return left->name < right->name;
        });
        for (const scene::ComponentAuthoringField* field : fields)
        {
            OrderedJson fieldNode = OrderedJson::object();
            fieldNode["name"] = field->name;
            fieldNode["kind"] = SemanticKindName(field->value.kind);
            const std::optional<OrderedJson> value = SerializeSemanticValue(
                field->value,
                "$.records.fields.value",
                result.diagnostics);
            if (!value.has_value())
            {
                return result;
            }
            fieldNode["value"] = *value;
            recordNode["fields"].push_back(std::move(fieldNode));
        }
        root["records"].push_back(std::move(recordNode));
    }

    result.text = root.dump(2);
    result.text.push_back('\n');
    return result;
}

SaveParseResult ReadSaveDocumentFile(const std::filesystem::path& path)
{
    std::string errorMessage{};
    const std::optional<std::string> text = ReadTextFile(path, errorMessage);
    if (!text.has_value())
    {
        SaveParseResult result{};
        AddDiagnostic(result.diagnostics, path.string(), "$", std::move(errorMessage));
        return result;
    }
    return ParseSaveDocumentJson(*text, path.string());
}

SaveWriteResult WriteSaveDocumentFile(
    const std::filesystem::path& path,
    const SaveDocument& document)
{
    SaveWriteResult result{};
    SaveSerializeResult serialized = SerializeSaveDocumentJson(document);
    if (!serialized.Succeeded())
    {
        for (SaveDiagnostic& diagnostic : serialized.diagnostics)
        {
            if (diagnostic.source.empty()) diagnostic.source = path.string();
            result.diagnostics.push_back(std::move(diagnostic));
        }
        return result;
    }

    std::vector<SaveDiagnostic> validationDiagnostics{};
    std::string errorMessage{};
    const bool committed = core::CommitTextFileAtomically(
        path,
        serialized.text,
        "save",
        errorMessage,
        [&](const std::filesystem::path& temporary, std::string& validationError)
        {
            SaveParseResult parsed = ReadSaveDocumentFile(temporary);
            if (!parsed.Succeeded())
            {
                validationDiagnostics = std::move(parsed.diagnostics);
                for (SaveDiagnostic& diagnostic : validationDiagnostics)
                {
                    diagnostic.source = path.string();
                    diagnostic.message = "Temporary save validation failed: " + diagnostic.message;
                }
                validationError = "Temporary save payload did not parse as a valid save document.";
                return false;
            }

            SaveSerializeResult canonical = SerializeSaveDocumentJson(*parsed.document);
            if (!canonical.Succeeded())
            {
                validationDiagnostics = std::move(canonical.diagnostics);
                for (SaveDiagnostic& diagnostic : validationDiagnostics)
                {
                    diagnostic.source = path.string();
                    diagnostic.message = "Temporary save canonical validation failed: " + diagnostic.message;
                }
                validationError = "Temporary save payload could not be canonicalized.";
                return false;
            }
            if (canonical.text != serialized.text)
            {
                AddDiagnostic(
                    validationDiagnostics,
                    path.string(),
                    "$",
                    "Temporary save payload changed during canonical roundtrip validation.");
                validationError = "Temporary save payload was not canonical after disk write.";
                return false;
            }
            return true;
        });

    if (!committed)
    {
        if (!validationDiagnostics.empty())
        {
            result.diagnostics = std::move(validationDiagnostics);
        }
        else
        {
            AddDiagnostic(result.diagnostics, path.string(), "$", std::move(errorMessage));
        }
    }
    return result;
}
} // namespace trace2d::persistence
