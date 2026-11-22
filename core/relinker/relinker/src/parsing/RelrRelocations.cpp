#include <relinker/parsing/RelrRelocations.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <iterator>
#include <limits>
#include <optional>

namespace Relinker {

std::map<Domain::VirtualAddress, std::uint64_t> ReadRelrRelocations(
    const std::span<const std::uint8_t> bytes,
    const std::span<const Domain::ProgramHeader> headers,
    const std::span<const Domain::DynamicTag> tags) {
    using namespace Elfpatcher;
    const auto value = [&](const std::int64_t kind, const char* name) {
        std::optional<std::uint64_t> result;
        for (const auto& tag : tags) {
            if (tag.Tag != kind) continue;
            if (result && *result != tag.Value)
                throw Domain::RelinkerException(std::string("Conflicting duplicate ") + name + " tags");
            result = tag.Value;
        }
        return result;
    };
    const auto address = value(DT_RELR, "DT_RELR");
    const auto size = value(DT_RELRSZ, "DT_RELRSZ");
    const auto stride = value(DT_RELRENT, "DT_RELRENT");
    if (!address && !size && !stride) return {};
    if (!address || !size || !stride)
        throw Domain::RelinkerException("Incomplete DT_RELR metadata");
    if (*stride != 8)
        throw Domain::RelinkerException("Invalid DT_RELRENT: expected 8 bytes");
    if (*size % 8 != 0)
        throw Domain::RelinkerException("Invalid DT_RELRSZ: expected a multiple of 8 bytes");
    if (*size == 0) return {};

    for (const auto& header : headers) {
        if (header.Type != PT_LOAD) continue;
        if (header.FileSize > header.MemorySize || header.Offset > bytes.size() || header.FileSize > bytes.size() - header.Offset
            || header.MemorySize > std::numeric_limits<std::uint64_t>::max() - header.MappedAddress)
            throw Domain::RelinkerException("Invalid RELR load segment", header.Offset);
    }
    const auto memorySegmentAt = [&](const std::uint64_t vaddr) -> const Domain::ProgramHeader* {
        const Domain::ProgramHeader* found = nullptr;
        for (const auto& header : headers) {
            if (header.Type != PT_LOAD || vaddr < header.MappedAddress || vaddr - header.MappedAddress >= header.MemorySize) continue;
            if (found) throw Domain::RelinkerException("Ambiguous RELR load mapping", vaddr);
            found = &header;
        }
        return found;
    };
    const auto fileSegmentAt = [&](const std::uint64_t vaddr) -> const Domain::ProgramHeader* {
        const Domain::ProgramHeader* found = nullptr;
        for (const auto& header : headers) {
            if (header.Type != PT_LOAD || vaddr < header.MappedAddress || vaddr - header.MappedAddress >= header.FileSize) continue;
            if (found) throw Domain::RelinkerException("Ambiguous RELR load mapping", vaddr);
            found = &header;
        }
        return found;
    };
    if (*address > std::numeric_limits<std::uint64_t>::max() - *size)
        throw Domain::RelinkerException("RELR table exceeds file-backed load segment", *address);
    std::uint64_t covered = *address;
    const std::uint64_t tableEnd = *address + *size;
    bool progress = true;
    while (progress && covered < tableEnd) {
        progress = false;
        for (const auto& header : headers) {
            if (header.Type != PT_LOAD) continue;
            if (header.MappedAddress <= covered && covered < header.MappedAddress + header.FileSize) {
                covered = header.MappedAddress + header.FileSize;
                progress = true;
            }
        }
    }
    if (covered < tableEnd)
        throw Domain::RelinkerException("RELR table exceeds file-backed load segment", covered);
    const auto readWord = [&](const std::uint64_t vaddr) {
        std::uint64_t result = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            const auto* segment = fileSegmentAt(vaddr + index);
            if (!segment) throw Domain::RelinkerException("RELR table exceeds file-backed load segment", vaddr + index);
            result |= static_cast<std::uint64_t>(bytes[segment->Offset + (vaddr + index - segment->MappedAddress)]) << (index * 8);
        }
        return result;
    };

    std::map<Domain::VirtualAddress, std::uint64_t> result;
    const auto append = [&](const std::uint64_t target) {
        const auto next = result.lower_bound(target);
        if ((next != result.end() && next->first - target < 8)
            || (next != result.begin() && target - std::prev(next)->first < 8))
            throw Domain::RelinkerException("Overlapping RELR targets", target);
        if (target > std::numeric_limits<std::uint64_t>::max() - 7)
            throw Domain::RelinkerException("RELR target exceeds load segment", target);
        std::uint64_t addend = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            const auto* segment = memorySegmentAt(target + index);
            if (!segment) throw Domain::RelinkerException("RELR target exceeds load segment", target + index);
            const auto relative = target + index - segment->MappedAddress;
            if (relative < segment->FileSize)
                addend |= static_cast<std::uint64_t>(bytes[segment->Offset + relative]) << (index * 8);
        }
        result.emplace_hint(next, target, addend);
    };
    std::optional<std::uint64_t> cursor;
    for (std::uint64_t offset = 0; offset < *size; offset += 8) {
        const auto entry = readWord(*address + offset);
        if ((entry & 1) == 0) {
            append(entry);
            cursor = entry + 8;
        } else {
            if (!cursor) throw Domain::RelinkerException("RELR bitmap precedes address entry", *address + offset);
            if (*cursor > std::numeric_limits<std::uint64_t>::max() - 63 * 8)
                throw Domain::RelinkerException("RELR bitmap address overflow", *cursor);
            for (std::size_t bit = 1; bit < 64; ++bit)
                if ((entry >> bit) & 1) append(*cursor + (bit - 1) * 8);
            *cursor += 63 * 8;
        }
    }
    return result;
}

}
