#ifndef RELINKER_ELFREADER_HPP
#define RELINKER_ELFREADER_HPP

#include <relinker/IElfReader.hpp>
#include <fstream>
#include <vector>

namespace Relinker {

class ElfReader : public IElfReader {
public:
    explicit ElfReader(const std::string& FilePath);
    
    ElfHeader ReadHeader() const override;
    std::vector<ProgramHeader> ReadProgramHeaders() const override;
    std::vector<SectionHeader> ReadSectionHeaders() const override;
    std::vector<std::uint8_t> ReadSection(const SectionHeader& Header) const override;
    std::vector<std::uint8_t> ReadSegment(const ProgramHeader& Header) const override;
    std::uint64_t GetFileSize() const override;

private:
    std::string _filePath;
    mutable std::vector<std::uint8_t> _fileBuffer;
    
    void _loadFileIntoBuffer() const;
    std::uint64_t _readU64At(Offset Offset) const;
    std::uint32_t _readU32At(Offset Offset) const;
    std::uint16_t _readU16At(Offset Offset) const;
    std::uint8_t _readU8At(Offset Offset) const;
    std::string _resolveShdrName(std::uint32_t NameOffset, const ElfHeader& Header) const;
};

}

#endif // RELINKER_ELFREADER_HPP
