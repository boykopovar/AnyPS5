#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>

namespace Elfpatcher::Windows {

WindowsLoadImage::WindowsLoadImage(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers) {
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
    const auto end = segments.back().MappedAddress + segments.back().MemorySize;
    data.resize(AlignRva(end - firstAddress));
    CheckedRva(LoadRva + static_cast<std::uint64_t>(data.size()));
    pageFlags.resize(data.size() / SectionAlignment);
    std::uint64_t previousEnd = firstAddress;
    for (const auto& segment : segments) {
        if (segment.MappedAddress < previousEnd)
            throw Domain::RelinkerException("Overlapping PT_LOAD memory ranges", segment.MappedAddress);
        previousEnd = segment.MappedAddress + segment.MemorySize;
        const auto offset = static_cast<std::size_t>(segment.MappedAddress - firstAddress);
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(segment.Offset), static_cast<std::size_t>(segment.FileSize), data.begin() + static_cast<std::ptrdiff_t>(offset));
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
    return CheckedRva(LoadRva + data.size());
}

std::uint32_t WindowsLoadImage::GetEntryRva() const {
    return entryRva;
}

std::uint64_t WindowsLoadImage::GetRelocatedAddress(const std::uint64_t address) const {
    if (address < firstAddress || address - firstAddress > data.size())
        throw Domain::RelinkerException("Relative relocation addend is outside the ELF image", address);
    return ImageBase + LoadRva + address - firstAddress;
}

void WindowsLoadImage::WritePointer(const std::uint64_t address, const std::uint64_t value) {
    Io::WriteU64(data, GetRva(address, 8) - LoadRva, value);
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
    for (std::size_t first = 0; first < pageFlags.size();) {
        auto last = first + 1;
        while (last < pageFlags.size() && pageFlags[last] == pageFlags[first])
            ++last;
        const auto characteristics = pageFlags[first] | ((pageFlags[first] & SectionExecute) != 0 ? 0x20u : 0x40u);
        sections.push_back({".elf" + std::to_string(sections.size()), CheckedRva(LoadRva + first * SectionAlignment), characteristics, {data.begin() + static_cast<std::ptrdiff_t>(first * SectionAlignment), data.begin() + static_cast<std::ptrdiff_t>(last * SectionAlignment)}});
        first = last;
    }
    return sections;
}

}
