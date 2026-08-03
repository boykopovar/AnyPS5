#ifndef RELINKER_TYPES_HPP
#define RELINKER_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

namespace Relinker {

using Offset = std::uint64_t;
using Address = std::uint64_t;
using Size = std::uint64_t;

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
    Offset Offset;
    Address VirtualAddress;
    Address PhysicalAddress;
    Size FileSize;
    Size MemorySize;
    std::uint64_t Alignment;
};

struct SectionHeader {
    std::string Name;
    std::uint32_t Type;
    std::uint64_t Flags;
    Address VirtualAddress;
    Offset Offset;
    Size Size;
    std::uint32_t Link;
    std::uint32_t Info;
    std::uint64_t EntrySize;
};

struct NidReference {
    std::string Nid;
    std::string Library;
    std::uint32_t RelocationTypeValue;
    Offset RelocationTableOffset;
    Address RelocationAddress;
};

struct CallSiteInfo {
    std::vector<Offset> Sites;
    bool Resolved;
};

struct SymbolExport {
    std::string Nid;
    std::string Library;
    Offset GotOffset;
    Offset PltOffset;
    CallSiteInfo CallSites;
};

struct RelinkerException : std::runtime_error {
    Offset FailureOffset;
    
    explicit RelinkerException(const std::string& Message, Offset Offset = 0)
        : std::runtime_error(Message), FailureOffset(Offset) {}
};

}

#endif // RELINKER_TYPES_HPP
