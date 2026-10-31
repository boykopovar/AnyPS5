#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <relinker/domain/Types.hpp>
#include <algorithm>
#include <bit>
#include <cstring>
#include <utility>

namespace Relinker {

using namespace Elfpatcher;

namespace {

bool _rangeFits(const std::uint64_t offset, const std::uint64_t length, const std::size_t size) {
    return offset <= size && length <= size - offset;
}

}

ElfReader::ElfReader(std::vector<std::uint8_t> fileBuffer)
    : _fileBuffer(std::move(fileBuffer))
{
}

const std::vector<std::uint8_t>& ElfReader::GetRawBytes() const {
    return _fileBuffer;
}

std::vector<std::uint8_t> ElfReader::TakeBytes() {
    return std::move(_fileBuffer);
}

std::string ElfReader::formatMagic() const {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t index = 0; index < 4; ++index) {
        if (index != 0) text += ' ';
        text += digits[_fileBuffer[index] >> 4];
        text += digits[_fileBuffer[index] & 0x0f];
    }
    return text;
}

std::uint8_t ElfReader::_readU8At(FileByteOffset fileByteOffset) const {
    if (fileByteOffset >= _fileBuffer.size()) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    return _fileBuffer[fileByteOffset];
}

std::uint16_t ElfReader::_readU16At(FileByteOffset fileByteOffset) const {
    if (!_rangeFits(fileByteOffset, 2, _fileBuffer.size())) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint16_t value;
    std::memcpy(&value, _fileBuffer.data() + fileByteOffset, 2);
    return value;
}

std::uint32_t ElfReader::_readU32At(FileByteOffset fileByteOffset) const {
    if (!_rangeFits(fileByteOffset, 4, _fileBuffer.size())) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint32_t value;
    std::memcpy(&value, _fileBuffer.data() + fileByteOffset, 4);
    return value;
}

std::uint64_t ElfReader::_readU64At(FileByteOffset fileByteOffset) const {
    if (!_rangeFits(fileByteOffset, 8, _fileBuffer.size())) {
        throw RelinkerException("FileByteOffset out of bounds", fileByteOffset);
    }
    std::uint64_t value;
    std::memcpy(&value, _fileBuffer.data() + fileByteOffset, 8);
    return value;
}

void ElfReader::ValidateMagic() const {
    if (_fileBuffer.size() < 0x40) {
        throw RelinkerException("File too small for ELF header");
    }

    const std::uint32_t magic = _readU32At(0);
    if (magic == 0x1d3d154f || magic == 0xeef51454) {
        throw RelinkerException("The input is a SELF container, not an ELF");
    }

    if (_fileBuffer[0] != 0x7f || _fileBuffer[1] != 'E' ||
        _fileBuffer[2] != 'L' || _fileBuffer[3] != 'F') {
        throw RelinkerException("Invalid ELF magic number: " + formatMagic());
    }
}

ElfHeader ElfReader::ReadHeader() const {
    ValidateMagic();
    if (_fileBuffer[4] != 2 || _fileBuffer[5] != 1 || _fileBuffer[6] != 1)
        throw RelinkerException("Expected little-endian ELF64 version 1");
    if (_readU16At(0x12) != 62)
        throw RelinkerException("Expected an x86-64 ELF");
    if (_readU32At(0x14) != 1)
        throw RelinkerException("Unsupported ELF object version: expected 1", 0x14);
    if (_readU16At(0x34) != 64)
        throw RelinkerException("Invalid ELF header size: expected 64 bytes", 0x34);

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
    if (header.ProgramHeaderCount != 0 && header.ProgramHeaderEntrySize != 56) {
        throw RelinkerException("Invalid ELF program header entry size: expected 56 bytes", 0x36);
    }

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

        if (ph.Type == PT_LOAD && ph.FileSize > ph.MemorySize)
            throw RelinkerException("PT_LOAD file size exceeds memory size", ph.Offset);
        if (ph.Type == PT_LOAD && !_rangeFits(ph.Offset, ph.FileSize, _fileBuffer.size()))
            throw RelinkerException("Segment offset out of bounds", ph.Offset);

        headers.push_back(ph);
        offset += header.ProgramHeaderEntrySize;
    }

    for (auto& relro : headers) {
        if (relro.Type != PT_OS_RELRO || relro.MemorySize == 0) continue;
        bool covered = false;
        for (const auto& load : headers) {
            if (load.Type != PT_LOAD || load.MemorySize == 0) continue;
            const bool overlaps = relro.MappedAddress < load.MappedAddress ? load.MappedAddress - relro.MappedAddress < relro.MemorySize : relro.MappedAddress - load.MappedAddress < load.MemorySize;
            if (!overlaps) continue;
            if (relro.MappedAddress < load.MappedAddress || relro.MemorySize > load.MemorySize - (relro.MappedAddress - load.MappedAddress))
                throw RelinkerException("PT_SCE_RELRO partially overlaps a PT_LOAD segment", relro.MappedAddress);
            covered = true;
        }
        if (!covered) {
            if (relro.FileSize > relro.MemorySize)
                throw RelinkerException("PT_SCE_RELRO file size exceeds memory size", relro.Offset);
            if (!_rangeFits(relro.Offset, relro.FileSize, _fileBuffer.size()))
                throw RelinkerException("Segment offset out of bounds", relro.Offset);
            relro.Type = PT_LOAD;
            relro.Flags = PF_R | PF_W;
        }
    }

    bool hasTls = false;
    for (const auto& tls : headers) {
        if (tls.Type != PT_TLS) continue;
        if (hasTls)
            throw RelinkerException("Multiple ELF TLS segments", tls.Offset);
        hasTls = true;
        if (tls.FileSize > tls.MemorySize || (tls.Alignment > 1 && !std::has_single_bit(tls.Alignment)))
            throw RelinkerException("Invalid or unsupported ELF TLS layout", tls.Offset);
        if (tls.FileSize == 0) continue;
        if (!_rangeFits(tls.Offset, tls.FileSize, _fileBuffer.size()))
            throw RelinkerException("PT_TLS initialization image is outside the file", tls.Offset);
        const bool covered = std::any_of(headers.begin(), headers.end(), [&](const ProgramHeader& load) {
            return load.Type == PT_LOAD && tls.MappedAddress >= load.MappedAddress && tls.MappedAddress - load.MappedAddress <= load.MemorySize
                && tls.FileSize <= load.MemorySize - (tls.MappedAddress - load.MappedAddress);
        });
        if (!covered)
            throw RelinkerException("PT_TLS initialization image is outside PT_LOAD memory", tls.Offset);
        const bool matches = std::any_of(headers.begin(), headers.end(), [&](const ProgramHeader& load) {
            if (load.Type != PT_LOAD || tls.MappedAddress < load.MappedAddress) return false;
            const auto relative = tls.MappedAddress - load.MappedAddress;
            if (relative > load.MemorySize || tls.FileSize > load.MemorySize - relative) return false;
            const auto fileBytes = relative < load.FileSize ? std::min(tls.FileSize, load.FileSize - relative) : 0;
            const auto tlsBegin = _fileBuffer.begin() + tls.Offset;
            if (fileBytes != 0 && !std::equal(tlsBegin, tlsBegin + fileBytes, _fileBuffer.begin() + load.Offset + relative)) return false;
            return std::all_of(tlsBegin + fileBytes, tlsBegin + tls.FileSize, [](std::uint8_t value) { return value == 0; });
        });
        if (!matches)
            throw RelinkerException("PT_TLS initialization image differs from PT_LOAD memory", tls.Offset);
    }

    return headers;
}

std::vector<SectionHeader> ElfReader::ReadSectionHeaders() const {
    const ElfHeader header = ReadHeader();
    if (header.SectionHeaderCount != 0 && header.SectionHeaderEntrySize != kShdrEntrySize) {
        throw RelinkerException("Invalid ELF section header entry size: expected 64 bytes", 0x3a);
    }
    const ByteCount tableSize = static_cast<ByteCount>(header.SectionHeaderCount) * header.SectionHeaderEntrySize;
    if (!_rangeFits(header.SectionHeaderOffset, tableSize, _fileBuffer.size())) {
        throw RelinkerException("Section header table out of bounds", header.SectionHeaderOffset);
    }
    if (header.SectionHeaderStringIndex != 0 && header.SectionHeaderStringIndex >= header.SectionHeaderCount) {
        throw RelinkerException("Section header string table index out of bounds", 0x3e);
    }

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
        sh.EntrySize = _readU64At(offset + kShdrEntSizeOffset);

        if (sh.Type != SHT_NULL && sh.Type != SHT_NOBITS && sh.SectionSize != 0) {
            if (!_rangeFits(sh.Offset, sh.SectionSize, _fileBuffer.size())) {
                throw RelinkerException("Section offset out of bounds", sh.Offset);
            }
        }

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

    if (_readU32At(shstrOffset + kShdrTypeOffset) != SHT_STRTAB) {
        throw RelinkerException("Section names do not reference a string table", shstrOffset);
    }
    const FileByteOffset strTableOffset = _readU64At(shstrOffset + 0x18);
    const ByteCount strTableSize = _readU64At(shstrOffset + kShdrSizeOffset);
    if (!_rangeFits(strTableOffset, strTableSize, _fileBuffer.size())) {
        throw RelinkerException("Section name string table out of bounds", strTableOffset);
    }
    if (strTableSize == 0 && nameOffset == 0) {
        return "";
    }
    if (nameOffset >= strTableSize) {
        throw RelinkerException("Section name offset out of bounds", nameOffset);
    }
    const auto first = _fileBuffer.begin() + strTableOffset + nameOffset;
    const auto end = _fileBuffer.begin() + strTableOffset + strTableSize;
    const auto terminator = std::find(first, end, 0);
    if (terminator == end) {
        throw RelinkerException("Unterminated section name", strTableOffset + nameOffset);
    }
    return std::string(first, terminator);
}

std::vector<DynamicTag> ElfReader::ReadDynamicTags(const ProgramHeader& dynamicHeader) const {
    if (!_rangeFits(dynamicHeader.Offset, dynamicHeader.FileSize, _fileBuffer.size())) {
        throw RelinkerException("Dynamic segment out of bounds", dynamicHeader.Offset);
    }
    std::vector<DynamicTag> tags;
    FileByteOffset offset = dynamicHeader.Offset;
    const FileByteOffset end = dynamicHeader.Offset + dynamicHeader.FileSize;

    while (end - offset >= 16) {
        DynamicTag tag;
        tag.Tag = static_cast<std::int64_t>(_readU64At(offset));
        tag.Value = _readU64At(offset + 0x08);

        if (tag.Tag == 0) {
            return tags;
        }

        tags.push_back(tag);
        offset += 16;
    }

    throw RelinkerException("Unterminated dynamic segment", dynamicHeader.Offset);
}

FileByteOffset ElfReader::TranslateVirtualAddress(VirtualAddress address) const {
    for (const auto& segment : ReadProgramHeaders()) {
        if (segment.Type == PT_LOAD && address >= segment.MappedAddress &&
            address - segment.MappedAddress < segment.FileSize)
            return segment.Offset + (address - segment.MappedAddress);
    }

    throw RelinkerException("Virtual address not mapped by any PT_LOAD segment", address);
}

std::vector<std::uint8_t> ElfReader::ReadSegment(const ProgramHeader& header) const {
    if (!_rangeFits(header.Offset, header.FileSize, _fileBuffer.size())) {
        throw RelinkerException("Segment offset out of bounds", header.Offset);
    }

    return std::vector<std::uint8_t>(
        _fileBuffer.begin() + header.Offset,
        _fileBuffer.begin() + header.Offset + header.FileSize);
}

std::uint64_t ElfReader::GetFileSize() const {
    return _fileBuffer.size();
}

}
