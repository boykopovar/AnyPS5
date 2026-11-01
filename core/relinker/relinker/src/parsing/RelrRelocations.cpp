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
    const auto loadAt = [&](const std::uint64_t target, const std::uint64_t extent, const bool file) -> const Domain::ProgramHeader& {
        const Domain::ProgramHeader* found = nullptr;
        for (const auto& header : headers) {
            const auto limit = file ? header.FileSize : header.MemorySize;
            if (header.Type != PT_LOAD || target < header.MappedAddress || target - header.MappedAddress > limit
                || extent > limit - (target - header.MappedAddress)) continue;
            if (found) throw Domain::RelinkerException("Ambiguous RELR load mapping", target);
            found = &header;
        }
        if (!found) throw Domain::RelinkerException(file ? "RELR table exceeds file-backed load segment" : "RELR target exceeds load segment", target);
        return *found;
    };
    const auto& table = loadAt(*address, *size, true);
    const auto tableOffset = table.Offset + (*address - table.MappedAddress);
    const auto readWord = [&](const std::uint64_t offset) {
        std::uint64_t result = 0;
        for (std::size_t index = 0; index < 8; ++index)
            result |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
        return result;
    };

    std::map<Domain::VirtualAddress, std::uint64_t> result;
    const auto append = [&](const std::uint64_t target) {
        const auto& load = loadAt(target, 8, false);
        const auto next = result.lower_bound(target);
        if ((next != result.end() && next->first - target < 8)
            || (next != result.begin() && target - std::prev(next)->first < 8))
            throw Domain::RelinkerException("Overlapping RELR targets", target);
        const auto relative = target - load.MappedAddress;
        std::uint64_t addend = 0;
        for (std::size_t index = 0; index < 8 && relative + index < load.FileSize; ++index)
            addend |= static_cast<std::uint64_t>(bytes[load.Offset + relative + index]) << (index * 8);
        result.emplace_hint(next, target, addend);
    };
    std::optional<std::uint64_t> cursor;
    for (std::uint64_t offset = 0; offset < *size; offset += 8) {
        const auto entry = readWord(tableOffset + offset);
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
