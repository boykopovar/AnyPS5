#include <relinker/parsing/ElfReader.hpp>
#include <relinker/domain/Types.hpp>
#include <cstring>

namespace Relinker {

ElfReader::ElfReader(std::vector<std::uint8_t> fileBuffer)
    : _fileBuffer(std::move(fileBuffer))
{
}

const std::vector<std::uint8_t>& ElfReader::GetRawBytes() const {
    return _fileBuffer;
}

std::uint8_t ElfReader::_readU8At(FileByteOffset fileByteOffset) const {
    if (fileByteOffset >= _fileBuffer.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    return _fileBuffer[fileByteOffset];
}

std::uint16_t ElfReader::_readU16At(FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 2 > _fileBuffer.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint16_t value;
    std::memcpy(&value, _fileBuffer.data() + fileByteOffset, 2);
    return value;
}

std::uint32_t ElfReader::_readU32At(FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 4 > _fileBuffer.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint32_t value;
    std::memcpy(&value, _fileBuffer.data() + fileByteOffset, 4);
    return value;
}

std::uint64_t ElfReader::_readU64At(FileByteOffset fileByteOffset) const {
    if (fileByteOffset + 8 > _fileBuffer.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint64_t value;
    std::memcpy(&value, _fileBuffer.data() + fileByteOffset, 8);
    return value;
}

ElfHeader ElfReader::ReadHeader() const {
    if (_fileBuffer.size() < 20) {
        throw RelinkerException("File too small for ELF header");
    }

    if (_fileBuffer[0] != 0x7f || _fileBuffer[1] != 'E' ||
        _fileBuffer[2] != 'L' || _fileBuffer[3] != 'F') {
        throw RelinkerException("Invalid ELF magic number");
    }

    ElfHeader header{};
    header.Machine = _readU16At(0x12);
    header.Type = _readU16At(0x10);
    header.OsAbi = _readU8At(0x07);
    header.AbiVersion = _readU8At(0x08);
    header.EntryPoint = _readU64At(0x18);
    header.ProgramHeaderOffset = _readU64At(0x20);
    header.SectionHeaderOffset = _readU64At(0x28);
    header.ProgramHeaderEntrySize = _readU16At(0x36);
    header.ProgramHeaderCount = _readU16At(0x38);
    header.SectionHeaderEntrySize = _readU16At(0x3a);
    header.SectionHeaderCount = _readU16At(0x3c);
    header.SectionHeaderStringIndex = _readU16At(0x3e);

    return header;
}

std::vector<ProgramHeader> ElfReader::ReadProgramHeaders() const {
    const ElfHeader header = ReadHeader();

    std::vector<ProgramHeader> headers;
    FileByteOffset offset = header.ProgramHeaderOffset;

    for (std::uint16_t i = 0; i < header.ProgramHeaderCount; ++i) {
        ProgramHeader ph{};
        ph.Type = _readU32At(offset);
        ph.Flags = _readU32At(offset + 0x04);
        ph.Offset = _readU64At(offset + 0x08);
        ph.MappedAddress = _readU64At(offset + 0x10);
        ph.PhysicalAddress = _readU64At(offset + 0x18);
        ph.FileSize = _readU64At(offset + 0x20);
        ph.MemorySize = _readU64At(offset + 0x28);
        ph.Alignment = _readU64At(offset + 0x30);

        headers.push_back(ph);
        offset += header.ProgramHeaderEntrySize;
    }

    return headers;
}

std::vector<SectionHeader> ElfReader::ReadSectionHeaders() const {
    const ElfHeader header = ReadHeader();

    std::vector<SectionHeader> headers;
    FileByteOffset offset = header.SectionHeaderOffset;

    for (std::uint16_t i = 0; i < header.SectionHeaderCount; ++i) {
        SectionHeader sh;
        const std::uint32_t nameOffset = _readU32At(offset);
        sh.Type = _readU32At(offset + 0x04);
        sh.Flags = _readU64At(offset + 0x08);
        sh.MappedAddress = _readU64At(offset + 0x10);
        sh.Offset = _readU64At(offset + 0x18);
        sh.SectionSize = _readU64At(offset + 0x20);
        sh.Link = _readU32At(offset + 0x28);
        sh.Info = _readU32At(offset + 0x2c);
        sh.EntrySize = _readU64At(offset + 0x30);

        sh.Name = _resolveShdrName(nameOffset, header);

        headers.push_back(sh);
        offset += header.SectionHeaderEntrySize;
    }

    return headers;
}

std::string ElfReader::_resolveShdrName(std::uint32_t nameOffset, const ElfHeader& header) const {
    if (header.SectionHeaderStringIndex == 0) {
        return "";
    }

    FileByteOffset shstrOffset = header.SectionHeaderOffset +
                        (header.SectionHeaderStringIndex * header.SectionHeaderEntrySize);

    const FileByteOffset strTableOffset = _readU64At(shstrOffset + 0x18);

    std::string name;
    FileByteOffset currentPos = strTableOffset + nameOffset;

    while (currentPos < _fileBuffer.size() && _fileBuffer[currentPos] != '\0') {
        name += static_cast<char>(_fileBuffer[currentPos]);
        currentPos++;
    }

    return name;
}

std::vector<DynamicTag> ElfReader::ReadDynamicTags(const ProgramHeader& dynamicHeader) const {
    std::vector<DynamicTag> tags;
    FileByteOffset offset = dynamicHeader.Offset;
    const FileByteOffset end = dynamicHeader.Offset + dynamicHeader.FileSize;

    while (offset + 16 <= end && offset + 16 <= _fileBuffer.size()) {
        DynamicTag tag;
        tag.Tag = static_cast<std::int64_t>(_readU64At(offset));
        tag.Value = _readU64At(offset + 0x08);

        if (tag.Tag == 0) {
            break;
        }

        tags.push_back(tag);
        offset += 16;
    }

    return tags;
}

FileByteOffset ElfReader::TranslateVirtualAddress(VirtualAddress address) const {
    const ElfHeader header = ReadHeader();

    FileByteOffset offset = header.ProgramHeaderOffset;

    for (std::uint16_t i = 0; i < header.ProgramHeaderCount; ++i) {
        const std::uint32_t type = _readU32At(offset);
        const FileByteOffset segOffset = _readU64At(offset + 0x08);
        const VirtualAddress segVAddr = _readU64At(offset + 0x10);
        const ByteCount segFileSize = _readU64At(offset + 0x20);

        static constexpr std::uint32_t PT_LOAD = 1;

        if (type == PT_LOAD && address >= segVAddr && address < segVAddr + segFileSize) {
            return segOffset + (address - segVAddr);
        }

        offset += header.ProgramHeaderEntrySize;
    }

    throw RelinkerException("Virtual address not mapped by any PT_LOAD segment", address);
}

std::vector<std::uint8_t> ElfReader::ReadSection(const SectionHeader& header) const {
    if (header.Offset + header.SectionSize > _fileBuffer.size()) {
        throw RelinkerException("Section offset out of bounds", header.Offset);
    }

    return std::vector<std::uint8_t>(
        _fileBuffer.begin() + header.Offset,
        _fileBuffer.begin() + header.Offset + header.SectionSize);
}

std::vector<std::uint8_t> ElfReader::ReadSegment(const ProgramHeader& header) const {
    if (header.Offset + header.FileSize > _fileBuffer.size()) {
        throw RelinkerException("Segment offset out of bounds", header.Offset);
    }

    return std::vector<std::uint8_t>(
        _fileBuffer.begin() + header.Offset,
        _fileBuffer.begin() + header.Offset + header.FileSize);
}

std::vector<ProgramHeader> ElfReader::ReadCodeSegments() const {
    static constexpr std::uint32_t kPtLoad = 1;
    static constexpr std::uint32_t kPfX = 0x1;
    std::vector<ProgramHeader> result;
    for (const auto& ph : ReadProgramHeaders()) {
        if (ph.Type == kPtLoad && (ph.Flags & kPfX)) {
            result.push_back(ph);
        }
    }
    return result;
}

std::uint64_t ElfReader::GetFileSize() const {
    return _fileBuffer.size();
}

}
