#ifndef RELINKER_ISCEDYNLIBPARSER_HPP
#define RELINKER_ISCEDYNLIBPARSER_HPP

#include <relinker/Types.hpp>
#include <vector>

namespace Relinker {

struct LibraryImport {
    std::string LibraryName;
    std::uint32_t LibraryId;
    std::uint32_t Version;
};

struct RelocationEntry {
    Offset Offset;
    std::uint64_t Info;
    std::int64_t Addend;
};

class ISceDynlibParser {
public:
    virtual ~ISceDynlibParser() = default;

    virtual std::vector<LibraryImport> ParseLibraryImports(const std::vector<std::uint8_t>& SceDynlibData) = 0;
    virtual std::vector<RelocationEntry> ParseRelocationTable(const std::vector<std::uint8_t>& SceDynlibData) = 0;
    virtual std::vector<NidReference> ExtractNidReferences(
        const std::vector<RelocationEntry>& Relocations,
        const std::vector<LibraryImport>& Imports
    ) = 0;
};

}

#endif // RELINKER_ISCEDYNLIBPARSER_HPP
