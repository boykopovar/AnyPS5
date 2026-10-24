#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>

namespace Elfpatcher::Windows {

WindowsLoadImage::WindowsLoadImage(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, const bool requireEntry, const std::uint64_t imageBase) : imageBase(imageBase) {
    if (source.size() < 64 || Io::ReadU32(source, 0) != 0x464c457f || source[4] != 2 || source[5] != 1 || source[6] != 1 || Io::ReadU16(source, 18) != 62)
        throw Domain::RelinkerException("Windows output requires a little-endian ELF64 x86-64 image");
    const auto type = Io::ReadU16(source, 16);
    if (type != 3 && type != 0xfe10 && type != 0xfe18)
        throw Domain::RelinkerException("Windows output requires a position-independent ELF image");
    for (const auto& header : headers) {
        if (header.Type != 1)
            continue;
        if (header.FileSize > header.MemorySize || header.Offset > source.size() || header.FileSize > source.size() - header.Offset)
            throw Domain::RelinkerException("Invalid PT_LOAD file range", header.Offset);
        if (header.MemorySize == 0)
            throw Domain::RelinkerException("Empty PT_LOAD segment", header.Offset);
        if (header.MappedAddress > std::numeric_limits<std::uint64_t>::max() - header.MemorySize)
            throw Domain::RelinkerException("PT_LOAD address overflow", header.MappedAddress);
        if ((header.Flags & ~7u) != 0)
            throw Domain::RelinkerException("Unsupported PT_LOAD permission flags", header.Offset);
        segments.push_back(header);
    }
    if (segments.empty())
        throw Domain::RelinkerException("No PT_LOAD segments found");
    std::sort(segments.begin(), segments.end(), [](const auto& left, const auto& right) { return left.MappedAddress < right.MappedAddress; });
    firstAddress = segments.front().MappedAddress & ~static_cast<std::uint64_t>(SectionAlignment - 1);
    std::uint64_t end = firstAddress;
    for (const auto& segment : segments) {
        if (segment.MappedAddress < end)
            throw Domain::RelinkerException("Overlapping PT_LOAD memory ranges", segment.MappedAddress);
        end = segment.MappedAddress + segment.MemorySize;
    }
    size = AlignRva(end - firstAddress);
    CheckedRva(LoadRva + size);
    pageFlags.resize(static_cast<std::size_t>(size / SectionAlignment));
    for (const auto& segment : segments) {
        const auto offset = static_cast<std::size_t>(segment.MappedAddress - firstAddress);
        if (segment.FileSize != 0) {
            auto& block = backBlock(offset, segment.FileSize);
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(segment.Offset), static_cast<std::size_t>(segment.FileSize), block.Bytes.begin() + static_cast<std::ptrdiff_t>(offset - block.Offset));
        }
        std::uint32_t flags = 0;
        if ((segment.Flags & 1) != 0)
            flags |= SectionExecute | SectionRead;
        if ((segment.Flags & 2) != 0)
            flags |= SectionWrite;
        if ((segment.Flags & 4) != 0)
            flags |= SectionRead;
        for (auto page = offset / SectionAlignment; page <= (offset + segment.MemorySize - 1) / SectionAlignment; ++page)
            pageFlags[page] |= flags;
    }
    const auto entry = Io::ReadU64(source, 24);
    if (!requireEntry && entry == 0) return;
    entryRva = GetRva(entry);
    if ((pageFlags[(entryRva - LoadRva) / SectionAlignment] & SectionExecute) == 0)
        throw Domain::RelinkerException("ELF entry point is not executable", entry);
}

std::uint32_t WindowsLoadImage::GetRva(const std::uint64_t address, const std::uint64_t size) const {
    if (size == 0)
        throw Domain::RelinkerException("Cannot map an empty ELF address range", address);
    for (const auto& segment : segments) {
        if (address >= segment.MappedAddress && address - segment.MappedAddress < segment.MemorySize && size <= segment.MemorySize - (address - segment.MappedAddress))
            return CheckedRva(LoadRva + address - firstAddress);
    }
    throw Domain::RelinkerException("ELF address is not contained in PT_LOAD memory", address);
}

std::uint32_t WindowsLoadImage::GetEndRva() const {
    return CheckedRva(LoadRva + size);
}

std::uint32_t WindowsLoadImage::GetEntryRva() const {
    return entryRva;
}

std::uint64_t WindowsLoadImage::GetRelocatedAddress(const std::uint64_t address) const {
    if (address < firstAddress || address - firstAddress > size)
        throw Domain::RelinkerException("Relative relocation addend is outside the ELF image", address);
    return imageBase + LoadRva + address - firstAddress;
}

std::uint64_t WindowsLoadImage::GetImageBase() const {
    return imageBase;
}

WindowsLoadImage::Block& WindowsLoadImage::backBlock(const std::uint64_t offset, const std::uint64_t length) {
    const auto start = offset / SectionAlignment * SectionAlignment;
    const auto end = AlignRva(offset + length);
    auto index = static_cast<std::size_t>(std::upper_bound(blocks.begin(), blocks.end(), offset, [](const std::uint64_t value, const Block& block) { return value < block.Offset; }) - blocks.begin());
    if (index == 0 || blocks[index - 1].Offset + blocks[index - 1].Bytes.size() < start)
        blocks.insert(blocks.begin() + static_cast<std::ptrdiff_t>(index), Block{start, {}});
    else
        --index;
    auto& block = blocks[index];
    if (block.Offset + block.Bytes.size() < end)
        block.Bytes.resize(end - block.Offset);
    while (index + 1 < blocks.size() && blocks[index + 1].Offset <= block.Offset + block.Bytes.size()) {
        const auto& next = blocks[index + 1];
        const auto nextOffset = static_cast<std::size_t>(next.Offset - block.Offset);
        block.Bytes.resize(std::max(block.Bytes.size(), nextOffset + next.Bytes.size()));
        std::copy(next.Bytes.begin(), next.Bytes.end(), block.Bytes.begin() + static_cast<std::ptrdiff_t>(nextOffset));
        blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(index + 1));
    }
    return block;
}

void WindowsLoadImage::WritePointer(const std::uint64_t address, const std::uint64_t value) {
    const auto offset = GetRva(address, 8) - LoadRva;
    auto& block = backBlock(offset, 8);
    Io::WriteU64(block.Bytes, offset - block.Offset, value);
}

void WindowsLoadImage::RequireWritable(const std::uint64_t address, const std::uint64_t size) const {
    const auto offset = GetRva(address, size) - LoadRva;
    for (auto page = offset / SectionAlignment; page <= (offset + size - 1) / SectionAlignment; ++page) {
        if ((pageFlags[page] & SectionWrite) == 0)
            throw Domain::RelinkerException("Runtime import target is not writable", address);
    }
}

std::vector<PeSection> WindowsLoadImage::BuildSections() const {
    std::vector<PeSection> sections;
    auto block = blocks.begin();
    for (std::size_t first = 0; first < pageFlags.size();) {
        while (block != blocks.end() && block->Offset + block->Bytes.size() <= first * SectionAlignment)
            ++block;
        const auto backed = block != blocks.end() && block->Offset <= first * SectionAlignment;
        const auto dataEnd = backed ? static_cast<std::size_t>((block->Offset + block->Bytes.size()) / SectionAlignment) : first;
        const auto following = backed ? std::next(block) : block;
        const auto stop = following != blocks.end() ? following->Offset / SectionAlignment : pageFlags.size();
        auto last = first + 1;
        while (last < stop && pageFlags[last] == pageFlags[first])
            ++last;
        const auto characteristics = pageFlags[first] | ((pageFlags[first] & SectionExecute) != 0 ? 0x20u : 0x40u);
        PeSection section{".elf" + std::to_string(sections.size()), CheckedRva(LoadRva + first * SectionAlignment), characteristics, {}, CheckedRva((last - first) * SectionAlignment)};
        if (backed) {
            const auto begin = block->Bytes.begin() + static_cast<std::ptrdiff_t>(first * SectionAlignment - block->Offset);
            section.Data.assign(begin, begin + static_cast<std::ptrdiff_t>((std::min(last, dataEnd) - first) * SectionAlignment));
        }
        sections.push_back(std::move(section));
        first = last;
    }
    return sections;
}
}
