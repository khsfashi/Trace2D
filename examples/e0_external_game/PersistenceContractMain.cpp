#include <trace2d/persistence/SaveDocument.hpp>

#include <iostream>

int main()
{
    trace2d::persistence::SaveDocument document{};
    document.saveId = "external-consumer-contract";

    const auto serialized = trace2d::persistence::SerializeSaveDocumentJson(document);
    if (!serialized.Succeeded() || serialized.text.empty())
    {
        std::cerr << "Trace2D::Persistence installed-package contract failed to serialize a minimal save document.\n";
        return 1;
    }

    const auto parsed = trace2d::persistence::ParseSaveDocumentJson(
        serialized.text,
        "external-consumer-contract");
    if (!parsed.Succeeded() || parsed.document->saveId != document.saveId)
    {
        std::cerr << "Trace2D::Persistence installed-package contract failed canonical roundtrip.\n";
        return 2;
    }

    return 0;
}
