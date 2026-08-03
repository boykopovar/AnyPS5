#ifndef RELINKER_PARSING_SCEDYNLIBPARSER_HPP
#define RELINKER_PARSING_SCEDYNLIBPARSER_HPP

#include <relinker/domain/ISceDynlibParser.hpp>
#include <relinker/domain/ISdkRevisionProfile.hpp>
#include <memory>

namespace Relinker {

class SceDynlibParser : public ISceDynlibParser {
public:
    explicit SceDynlibParser(std::shared_ptr<ISdkRevisionProfile> sdkProfile);

    std::vector<LibraryImport> ParseLibraryImports(const std::vector<std::uint8_t>& sceDynlibData) override;
    std::vector<RelocationEntry> ParseRelocationTable(const std::vector<std::uint8_t>& sceDynlibData) override;
    std::vector<NidReference> ExtractNidReferences(
        const std::vector<RelocationEntry>& relocations,
        const std::vector<LibraryImport>& imports
    ) override;

private:
    std::shared_ptr<ISdkRevisionProfile> _sdkProfile;

    [[nodiscard]] std::uint32_t _readU32At(const std::vector<std::uint8_t>& data, FileByteOffset offset) const;
    [[nodiscard]] std::uint16_t _readU16At(const std::vector<std::uint8_t>& data, FileByteOffset offset) const;
    [[nodiscard]] std::uint8_t _readU8At(const std::vector<std::uint8_t>& data, FileByteOffset offset) const;
    [[nodiscard]] std::uint64_t _readU64At(const std::vector<std::uint8_t>& data, FileByteOffset offset) const;
    [[nodiscard]] std::string _extractNidFromRelocationInfo(std::uint64_t info) const;
    [[nodiscard]] std::string _getLibraryNameForRelocation(const std::vector<LibraryImport>& imports, std::uint64_t info) const;
};

}

#endif
