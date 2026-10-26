#ifndef ELFPATCHER_WINDOWS_PEFORMAT_HPP
#define ELFPATCHER_WINDOWS_PEFORMAT_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Elfpatcher::Windows {

inline constexpr std::uint32_t SectionAlignment = 0x1000;
inline constexpr std::uint32_t FileAlignment = 0x200;
inline constexpr std::uint32_t LoadRva = 0x10000;
inline constexpr std::uint64_t ImageBase = 0x140000000;
inline constexpr std::uint64_t ExecutableBase = 0x400000;
inline constexpr std::uint32_t SectionRead = 0x40000000;
inline constexpr std::uint32_t SectionWrite = 0x80000000;
inline constexpr std::uint32_t SectionExecute = 0x20000000;

struct PeSection {
    std::string Name;
    std::uint32_t Rva;
    std::uint32_t Characteristics;
    std::vector<std::uint8_t> Data;
    std::uint32_t VirtualSize = 0;

    std::uint64_t MemorySize() const {
        return std::max<std::uint64_t>(Data.size(), VirtualSize);
    }

    bool Writable(const std::uint32_t rva, const std::uint64_t size) {
        if (rva < Rva || rva - Rva > MemorySize() || size > MemorySize() - (rva - Rva))
            return false;
        if (rva - Rva + size > Data.size()) {
            VirtualSize = static_cast<std::uint32_t>(MemorySize());
            Data.resize(static_cast<std::size_t>(rva - Rva + size));
        }
        return true;
    }
};

struct PeDirectory {
    std::uint32_t Rva = 0;
    std::uint32_t Size = 0;
};

struct PeImport {
    std::string Name;
    std::uint32_t TargetRva;
    std::uint64_t Addend;
    std::int32_t TargetModule = -1;
    std::uint32_t RelocationType = 1;
    std::string Library;
    bool Weak = false;
    bool Function = false;
};

struct PeRelocations {
    std::vector<std::uint32_t> BaseRelocations;
    std::vector<PeImport> Imports;
};

std::uint32_t CheckedRva(std::uint64_t value);
std::uint32_t AlignRva(std::uint64_t value);
std::string ReadString(const std::vector<std::uint8_t>& bytes, std::size_t offset);

}

#endif
