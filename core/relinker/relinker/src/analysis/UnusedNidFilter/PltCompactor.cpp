#include <relinker/analysis/UnusedNidFilter/PltCompactor.hpp>
#include <cstring>
#include <limits>
#include <map>
#include <set>

namespace Relinker::UnusedNidFilter {

CompactedPlt CompactPlt(const std::vector<NidReference>& originalReferences, const std::vector<NidReference>& keptReferences, const std::vector<std::uint8_t>& text, VirtualAddress textVaddr, FileByteOffset textOffset, FileByteOffset tableOffset) {
    CompactedPlt result;
    result.References = keptReferences;
    std::map<VirtualAddress, std::uint32_t> originalSlots;
    for (const auto& reference : originalReferences) {
        if (reference.RelocationTypeValue != 7) continue;
        if (reference.RelocationTableOffset < tableOffset || (reference.RelocationTableOffset - tableOffset) % 24 != 0 || (reference.RelocationTableOffset - tableOffset) / 24 > std::numeric_limits<std::uint32_t>::max())
            throw RelinkerException("Strict filter: invalid original PLT relocation index", reference.RelocationTableOffset);
        if (!originalSlots.emplace(reference.RelocationAddress, static_cast<std::uint32_t>((reference.RelocationTableOffset - tableOffset) / 24)).second)
            throw RelinkerException("Strict filter: duplicate original PLT slot", reference.RelocationAddress);
    }
    std::map<VirtualAddress, std::uint32_t> newSlots;
    for (auto& reference : result.References) {
        if (reference.RelocationTypeValue != 7) continue;
        if (!originalSlots.contains(reference.RelocationAddress) || !newSlots.emplace(reference.RelocationAddress, result.SlotCount).second)
            throw RelinkerException("Strict filter: invalid retained PLT slot", reference.RelocationAddress);
        reference.RelocationTableOffset = tableOffset + static_cast<std::uint64_t>(result.SlotCount++) * 24;
    }
    std::set<VirtualAddress> found;
    for (std::size_t offset = 0; offset + 16 <= text.size(); ++offset) {
        if (text[offset] != 0xFF || text[offset + 1] != 0x25) continue;
        std::int32_t displacement;
        std::memcpy(&displacement, text.data() + offset + 2, sizeof(displacement));
        const auto slot = textVaddr + offset + 6 + static_cast<std::uint64_t>(static_cast<std::int64_t>(displacement));
        const auto original = originalSlots.find(slot);
        if (original == originalSlots.end()) continue;
        if (text[offset + 6] != 0x68 || text[offset + 11] != 0xE9) continue;
        std::uint32_t index;
        std::memcpy(&index, text.data() + offset + 7, sizeof(index));
        if (index != original->second) throw RelinkerException("Strict filter: PLT thunk index disagrees with its relocation", textVaddr + offset);
        found.insert(slot);
        if (const auto kept = newSlots.find(slot); kept != newSlots.end()) {
            std::vector<std::uint8_t> bytes(sizeof(std::uint32_t));
            std::memcpy(bytes.data(), &kept->second, bytes.size());
            result.Patches.push_back({textOffset + offset + 7, std::move(bytes)});
        } else {
            result.Patches.push_back({textOffset + offset, {0x0F, 0x0B}});
            result.Patches.push_back({textOffset + offset + 6, {0x0F, 0x0B}});
        }
        offset += 15;
    }
    for (const auto& [slot, index] : originalSlots)
        if (!found.contains(slot)) throw RelinkerException("Strict filter: canonical PLT thunk was not found", slot);
    return result;
}

}
