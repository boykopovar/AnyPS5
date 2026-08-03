#ifndef RELINKER_SCEDYNLIBPARSER_HPP
#define RELINKER_SCEDYNLIBPARSER_HPP

#include <relinker/ISceDynlibParser.hpp>
#include <memory>

namespace Relinker {

class ISdkRevisionProfile;

class SceDynlibParser : public ISceDynlibParser {
public:
    explicit SceDynlibParser(std::shared_ptr<ISdkRevisionProfile> SdkProfile);
    
    std::vector<LibraryImport> ParseLibraryImports(const std::vector<std::uint8_t>& SceDynlibData) override;
    std::vector<RelocationEntry> ParseRelocationTable(const std::vector<std::uint8_t>& SceDynlibData) override;
    std::vector<NidReference> ExtractNidReferences(
        const std::vector<RelocationEntry>& Relocations,
        const std::vector<LibraryImport>& Imports
    ) override;

private:
    std::shared_ptr<ISdkRevisionProfile> _sdkProfile;
    
    std::uint32_t _readU32At(const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const;
    std::uint16_t _readU16At(const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const;
    std::uint8_t _readU8At(const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const;
    std::uint64_t _readU64At(const std::vector<std::uint8_t>& Data, FileByteOffset FileByteOffset) const;
    std::string _extractNidFromRelocationInfo(std::uint64_t Info) const;
    std::string _getLibraryNameForRelocation(const std::vector<LibraryImport>& Imports, std::uint64_t Info) const;
};

}

#endif // RELINKER_SCEDYNLIBPARSER_HPP
