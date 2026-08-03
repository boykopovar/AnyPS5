#ifndef RELINKER_TYPES_HPP
#define RELINKER_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

namespace Relinker {

using FileByteOffset = std::uint64_t;
using VirtualAddress = std::uint64_t;
using ByteCount = std::uint64_t;

struct ElfHeader {
    std::uint16_t Machine;
    std::uint16_t Type;
    std::uint8_t OsAbi;
    std::uint8_t AbiVersion;
    std::uint64_t EntryPoint;
    std::uint64_t ProgramHeaderOffset;
    std::uint64_t SectionHeaderOffset;
    std::uint32_t ProgramHeaderEntrySize;
    std::uint16_t ProgramHeaderCount;
    std::uint32_t SectionHeaderEntrySize;
    std::uint16_t SectionHeaderCount;
    std::uint16_t SectionHeaderStringIndex;
};

struct ProgramHeader {
    std::uint32_t Type;
    std::uint32_t Flags;
    FileByteOffset Offset;
    VirtualAddress MappedAddress;
    VirtualAddress PhysicalAddress;
    ByteCount FileSize;
    ByteCount MemorySize;
    std::uint64_t Alignment;
};

struct SectionHeader {
    std::string Name;
    std::uint32_t Type;
    std::uint64_t Flags;
    VirtualAddress MappedAddress;
    FileByteOffset Offset;
    ByteCount SectionSize;
    std::uint32_t Link;
    std::uint32_t Info;
    std::uint64_t EntrySize;
};

struct NidReference {
    std::string Nid;
    std::string Library;
    std::uint32_t RelocationTypeValue;
    FileByteOffset RelocationTableOffset;
    VirtualAddress RelocationAddress;
};

struct CallSiteInfo {
    std::vector<FileByteOffset> Sites;
    bool Resolved;
};

struct SymbolExport {
    std::string Nid;
    std::string Library;
    FileByteOffset GotOffset;
    FileByteOffset PltOffset;
    CallSiteInfo CallSites;
};

struct DynamicTag {
    std::int64_t Tag;
    std::uint64_t Value;
};

struct RelinkerException : std::runtime_error {
    FileByteOffset FailureOffset;

    explicit RelinkerException(const std::string& Message, const FileByteOffset failureOffset = 0)
        : std::runtime_error(Message), FailureOffset(failureOffset) {}
};

}

#endif // RELINKER_TYPES_HPP
