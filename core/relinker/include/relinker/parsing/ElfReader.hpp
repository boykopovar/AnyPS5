#ifndef RELINKER_PARSING_ELFREADER_HPP
#define RELINKER_PARSING_ELFREADER_HPP

#include <relinker/domain/IElfReader.hpp>
#include <vector>

namespace Relinker {

class ElfReader : public IElfReader {
public:
    explicit ElfReader(const std::string& filePath);

    ElfHeader ReadHeader() const override;
    std::vector<ProgramHeader> ReadProgramHeaders() const override;
    std::vector<SectionHeader> ReadSectionHeaders() const override;
    std::vector<DynamicTag> ReadDynamicTags(const ProgramHeader& dynamicHeader) const override;
    FileByteOffset TranslateVirtualAddress(VirtualAddress address) const override;
    std::vector<std::uint8_t> ReadSection(const SectionHeader& header) const override;
    std::vector<std::uint8_t> ReadSegment(const ProgramHeader& header) const override;
    std::uint64_t GetFileSize() const override;

private:
    std::string _filePath;
    mutable std::vector<std::uint8_t> _fileBuffer;

    void _loadFileIntoBuffer() const;
    std::uint64_t _readU64At(FileByteOffset offset) const;
    std::uint32_t _readU32At(FileByteOffset offset) const;
    std::uint16_t _readU16At(FileByteOffset offset) const;
    std::uint8_t _readU8At(FileByteOffset offset) const;
    std::string _resolveShdrName(std::uint32_t nameOffset, const ElfHeader& header) const;
};

}

#endif
