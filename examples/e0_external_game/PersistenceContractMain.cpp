#include <trace2d/persistence/SaveDocument.hpp>

int main()
{
    trace2d::persistence::SaveDocument document{};
    document.saveId = "e0.persistence-contract";

    const trace2d::persistence::SaveSerializeResult first =
        trace2d::persistence::SerializeSaveDocumentJson(document);
    if (!first.Succeeded() || first.text.empty())
    {
        return 1;
    }

    const trace2d::persistence::SaveParseResult parsed =
        trace2d::persistence::ParseSaveDocumentJson(first.text, "e0.persistence-contract");
    if (!parsed.Succeeded() || !parsed.document.has_value() ||
        parsed.document->saveId != document.saveId)
    {
        return 2;
    }

    const trace2d::persistence::SaveSerializeResult second =
        trace2d::persistence::SerializeSaveDocumentJson(*parsed.document);
    if (!second.Succeeded() || second.text != first.text)
    {
        return 3;
    }

    return 0;
}
