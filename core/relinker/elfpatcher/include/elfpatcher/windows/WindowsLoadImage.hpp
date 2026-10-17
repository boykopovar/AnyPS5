#ifndef ELFPATCHER_WINDOWS_LOADIMAGE_HPP
#define ELFPATCHER_WINDOWS_LOADIMAGE_HPP

#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <domain/Types.hpp>

namespace Elfpatcher::Windows {

class WindowsLoadImage {
public:
    WindowsLoadImage(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, bool requireEntry = true, std::uint64_t imageBase = ImageBase);
    std::uint32_t GetRva(std::uint64_t address, std::uint64_t size = 1) const;
    std::uint32_t GetEndRva() const;
    std::uint32_t GetEntryRva() const;
    std::uint64_t GetRelocatedAddress(std::uint64_t address) const;
    std::uint64_t GetImageBase() const;
    void WritePointer(std::uint64_t address, std::uint64_t value);
    void RequireWritable(std::uint64_t address, std::uint64_t size) const;
    std::vector<PeSection> BuildSections() const;

private:
    struct Block {
        std::uint64_t Offset;
        std::vector<std::uint8_t> Bytes;
    };

    Block& findBlock(std::uint64_t offset);

    std::vector<Domain::ProgramHeader> segments;
    std::vector<Block> blocks;
    std::uint64_t size;
    std::vector<std::uint32_t> pageFlags;
    std::uint64_t firstAddress;
    std::uint64_t imageBase;
    std::uint32_t entryRva = 0;
};

}

#endif
