#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <map>

#include <FCGlobal.h>
namespace fs = std::filesystem;

namespace Base
{

struct BaseExport XMLElement
{
    std::string tag;
    std::map<std::string, std::string> attrs;
    std::vector<std::unique_ptr<XMLElement>> children;
    std::string content;

    // Disable costly recursive copies
    XMLElement(const XMLElement& elem) = delete;
    XMLElement& operator=(XMLElement const&) = delete;

    // Accept move semantics
    XMLElement() = default;
    XMLElement(XMLElement&&) noexcept = default;
    XMLElement& operator=(XMLElement&&) noexcept = default;

    std::unique_ptr<XMLElement> clone() const;
};


BaseExport std::unique_ptr<XMLElement> parseXMLFile(const fs::path& path);
BaseExport void saveXMLFile(const fs::path& path, const XMLElement& xmlTree);
BaseExport std::optional<std::string> checkXMLDocument(
    const XMLElement& xmlTree,
    const std::string& xsdString
);

}  // namespace Base
