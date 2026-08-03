#include <relinker/ElfReader.hpp>
#include <relinker/Types.hpp>
#include <fstream>
#include <cstring>

namespace Relinker {

ElfReader::ElfReader(const std::string& FilePath) : _filePath(FilePath) {
}

void ElfReader::_loadFileIntoBuffer() const {
    if (!_fileBuffer.empty()) {
        return;
    }
    
    std::ifstream file(_filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw RelinkerException("Cannot open file: " + _filePath);
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    _fileBuffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(_fileBuffer.data()), size)) {
        throw RelinkerException("Cannot read file: " + _filePath);
    }
}

std::uint8_t ElfReader::_readU8At(Offset Offset) const {
    _loadFileIntoBuffer();
    if (Offset >= _fileBuffer.size()) {
        throw RelinkerException("Offset out of bounds", Offset);
    }
    return _fileBuffer[Offset];
}

std::uint16_t ElfReader::_readU16At(Offset Offset) const {
    _loadFileIntoBuffer();
    if (Offset + 2 > _fileBuffer.size()) {
        throw RelinkerException("Offset out of bounds", Offset);
    }
    std::uint16_t value;
    std::memcpy(&value, _fileBuffer.data() + Offset, 2);
    return value;
}

std::uint32_t ElfReader::_readU32At(Offset Offset) const {
    _loadFileIntoBuffer();
    if (Offset + 4 > _fileBuffer.size()) {
        throw RelinkerException("Offset out of bounds", Offset);
    }
    std::uint32_t value;
    std::memcpy(&value, _fileBuffer.data() + Offset, 4);
    return value;
}

std::uint64_t ElfReader::_readU64At(Offset Offset) const {
    _loadFileIntoBuffer();
    if (Offset + 8 > _fileBuffer.size()) {
        throw RelinkerException("Offset out of bounds", Offset);
    }
    std::uint64_t value;
    std::memcpy(&value, _fileBuffer.data() + Offset, 8);
    return value;
}

ElfHeader ElfReader::ReadHeader() const {
    _loadFileIntoBuffer();
    
    if (_fileBuffer.size() < 20) {
        throw RelinkerException("File too small for ELF header");
    }
    
    if (_fileBuffer[0] != 0x7f || _fileBuffer[1] != 'E' || 
        _fileBuffer[2] != 'L' || _fileBuffer[3] != 'F') {
        throw RelinkerException("Invalid ELF magic number");
    }
    
    ElfHeader header;
    header.Machine = _readU16At(0x12);
    header.Type = _readU16At(0x10);
    header.OsAbi = _readU8At(0x07);
    header.AbiVersion = _readU8At(0x08);
    header.EntryPoint = _readU64At(0x18);
    header.ProgramHeaderOffset = _readU64At(0x20);
    header.SectionHeaderOffset = _readU64At(0x28);
    header.ProgramHeaderEntrySize = _readU16At(0x32);
    header.ProgramHeaderCount = _readU16At(0x38);
    header.SectionHeaderEntrySize = _readU16At(0x3a);
    header.SectionHeaderCount = _readU16At(0x3c);
    header.SectionHeaderStringIndex = _readU16At(0x3e);
    
    return header;
}

std::vector<ProgramHeader> ElfReader::ReadProgramHeaders() const {
    _loadFileIntoBuffer();
    ElfHeader header = ReadHeader();
    
    std::vector<ProgramHeader> headers;
    Offset offset = header.ProgramHeaderOffset;
    
    for (std::uint16_t i = 0; i < header.ProgramHeaderCount; ++i) {
        ProgramHeader ph;
        ph.Type = _readU32At(offset);
        ph.Flags = _readU32At(offset + 0x04);
        ph.Offset = _readU64At(offset + 0x08);
        ph.VirtualAddress = _readU64At(offset + 0x10);
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
    _loadFileIntoBuffer();
    ElfHeader header = ReadHeader();
    
    std::vector<SectionHeader> headers;
    Offset offset = header.SectionHeaderOffset;
    
    for (std::uint16_t i = 0; i < header.SectionHeaderCount; ++i) {
        SectionHeader sh;
        std::uint32_t nameOffset = _readU32At(offset);
        sh.Type = _readU32At(offset + 0x04);
        sh.Flags = _readU64At(offset + 0x08);
        sh.VirtualAddress = _readU64At(offset + 0x10);
        sh.Offset = _readU64At(offset + 0x18);
        sh.Size = _readU64At(offset + 0x20);
        sh.Link = _readU32At(offset + 0x28);
        sh.Info = _readU32At(offset + 0x2c);
        sh.EntrySize = _readU64At(offset + 0x30);
        
        sh.Name = _resolveShdrName(nameOffset, header);
        
        headers.push_back(sh);
        offset += header.SectionHeaderEntrySize;
    }
    
    return headers;
}

std::string ElfReader::_resolveShdrName(std::uint32_t NameOffset, const ElfHeader& Header) const {
    if (Header.SectionHeaderStringIndex == 0) {
        return "";
    }
    
    Offset shstrOffset = Header.SectionHeaderOffset + 
                        (Header.SectionHeaderStringIndex * Header.SectionHeaderEntrySize);
    
    Offset strTableOffset = _readU64At(shstrOffset + 0x18);
    
    std::string name;
    Offset currentPos = strTableOffset + NameOffset;
    
    while (currentPos < _fileBuffer.size() && _fileBuffer[currentPos] != '\0') {
        name += static_cast<char>(_fileBuffer[currentPos]);
        currentPos++;
    }
    
    return name;
}

std::vector<std::uint8_t> ElfReader::ReadSection(const SectionHeader& Header) const {
    _loadFileIntoBuffer();
    
    if (Header.Offset + Header.Size > _fileBuffer.size()) {
        throw RelinkerException("Section offset out of bounds", Header.Offset);
    }
    
    std::vector<std::uint8_t> data(_fileBuffer.begin() + Header.Offset, _fileBuffer.begin() + Header.Offset + Header.Size);
    return data;
}

std::vector<std::uint8_t> ElfReader::ReadSegment(const ProgramHeader& Header) const {
    _loadFileIntoBuffer();
    
    if (Header.Offset + Header.FileSize > _fileBuffer.size()) {
        throw RelinkerException("Segment offset out of bounds", Header.Offset);
    }
    
    std::vector<std::uint8_t> data(_fileBuffer.begin() + Header.Offset, _fileBuffer.begin() + Header.Offset + Header.FileSize);
    return data;
}

std::uint64_t ElfReader::GetFileSize() const {
    _loadFileIntoBuffer();
    return _fileBuffer.size();
}

}
