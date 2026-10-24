#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/RelocationRuns.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::size_t MaxAliases = 16384;
constexpr std::size_t MaxContentWords = 64;

std::uint64_t AlignedWord(std::uint32_t word) {
    return word & ~std::uint32_t {3};
}

}

bool Driver::drawRelocationEnabled() {
    static const bool enabled = std::getenv("APS5_NO_DRAW_RELOCATION") == nullptr;
    return enabled;
}

std::uint64_t Driver::relocatedContentKey(const RelocatableUserWord& word, std::uint32_t value, bool& readable) {
    std::uint64_t hash = 0x9e3779b97f4a7c15ull;
    const auto mix = [&](std::uint64_t mixed) {
        hash ^= mixed;
        hash *= 0x100000001b3ull;
    };
    mix(value & 3u);
    if (value == 0) {
        readable = false;
        return hash;
    }
    std::array<std::uint32_t, MaxContentWords> content;
    for (const auto& [offset, words] : word.reads) {
        mix(static_cast<std::uint64_t>(offset));
        mix(words);
        for (std::size_t done = 0; done < words; done += MaxContentWords) {
            const auto count = std::min<std::size_t>(words - done, MaxContentWords);
            const auto address = AlignedWord(value) + static_cast<std::uint64_t>(offset) + done * sizeof(std::uint32_t);
            if (GuestMemory::CopyMapped(address, std::as_writable_bytes(std::span(content).first(count))) != GuestMemory::Compare::Equal) {
                readable = false;
                return hash;
            }
            for (std::size_t i = 0; i < count; ++i) mix(content[i]);
        }
    }
    return hash;
}

void Driver::tagVariantRuns(DispatchVariant& variant, std::span<const ShaderRecompiler::MemoryRegion> regions, const ShaderRecompiler::SrtUserDataUse& use, bool eligible, std::uint32_t hiddenWords, std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> supplied) {
    std::uint64_t relocatable = eligible && drawRelocationEnabled() ? use.asBase & ~use.asValue : 0;
    if (hiddenWords != 0) relocatable &= hiddenWords >= 64 ? 0 : ~((std::uint64_t {1} << hiddenWords) - 1);
    std::vector<std::pair<std::uint64_t, std::int32_t>> reads;
    const auto covered = [&](std::uint64_t address) {
        return std::any_of(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& region) { return address >= region.guestAddress && address - region.guestAddress + sizeof(std::uint32_t) <= region.bytes.size(); });
    };
    if (relocatable != 0) {
        reads = use.reads;
        std::sort(reads.begin(), reads.end());
        reads.erase(std::unique(reads.begin(), reads.end()), reads.end());
        const auto drop = [&](std::int32_t origin) {
            if (origin >= 0) relocatable &= ~(std::uint64_t {1} << origin);
        };
        for (std::size_t i = 0; i < reads.size(); ++i) {
            if (i + 1 < reads.size() && reads[i].first == reads[i + 1].first) {
                drop(reads[i].second);
                drop(reads[i + 1].second);
            }
            if (reads[i].second >= 0 && ((reads[i].first & 3u) != 0 || !covered(reads[i].first))) drop(reads[i].second);
        }
    }
    variant.relocatableWords = relocatable;
    for (std::size_t index = 0; index < regions.size(); ++index) {
        const auto& region = regions[index];
        const auto count = region.bytes.size() / sizeof(std::uint32_t);
        const auto offset = variant.words.size();
        variant.words.resize(offset + count);
        std::memcpy(variant.words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
        if (relocatable == 0) {
            AppendSharedWords(variant.shared, variant.runs.size(), offset, region.bytes, supplied);
            variant.runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
            continue;
        }
        ForEachRelocationRun(region.guestAddress, count, reads, relocatable, [&](std::size_t first, std::size_t last, std::int8_t origin) {
            AppendSharedWords(variant.shared, variant.runs.size(), offset + first, region.bytes.subspan(first * sizeof(std::uint32_t), (last - first) * sizeof(std::uint32_t)), supplied);
            variant.runs.emplace_back(region.guestAddress + first * sizeof(std::uint32_t), region.guestAddress + last * sizeof(std::uint32_t));
            variant.runBases.push_back(origin);
        });
    }
}

namespace {

template <typename TMap>
std::size_t dropEveryOther(TMap& map) {
    std::size_t dropped = 0;
    bool drop = false;
    for (auto it = map.begin(); it != map.end();) {
        drop = !drop;
        if (!drop) {
            ++it;
            continue;
        }
        it = map.erase(it);
        ++dropped;
    }
    return dropped;
}

}

void Driver::learnPendingRelocation() {
    const auto pending = drawRelocationPending;
    drawRelocationPending = {};
    if (pending.key == 0) return;
    const auto inserted = drawCache.find(pending.key);
    if (inserted == drawCache.end() || inserted->second->decode == nullptr) return;
    const auto& entry = *inserted->second;
    const auto& programs = entry.decode->programs;
    if (entry.stages.size() != programs.size()) return;
    const auto known = drawRelocations.find(pending.identity);
    auto shape = std::make_shared<DrawRelocation>();
    if (known != drawRelocations.end()) *shape = *known->second;
    bool changed = known == drawRelocations.end();
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (entry.stages[i].empty()) continue;
        const auto& variant = *entry.stages[i].front();
        const auto& program = programs[i];
        for (std::uint32_t index = program.hiddenUserWords; index < program.userData.size() && index < 64; ++index) {
            const auto reg = program.userDataBase + index - program.hiddenUserWords;
            const auto held = std::find_if(shape->words.begin(), shape->words.end(), [&](const RelocatableUserWord& word) { return word.reg == reg; });
            if ((variant.relocatableWords >> index & 1u) == 0) {
                if (held == shape->words.end()) continue;
                shape->words.erase(held);
                shape->banned.push_back(reg);
                changed = true;
                continue;
            }
            if (held != shape->words.end() || std::find(shape->banned.begin(), shape->banned.end(), reg) != shape->banned.end()) continue;
            RelocatableUserWord word{reg, {}};
            const auto base = AlignedWord(program.userData[index]);
            for (std::size_t run = 0; run < variant.runs.size() && run < variant.runBases.size(); ++run) {
                if (variant.runBases[run] != static_cast<std::int8_t>(index)) continue;
                word.reads.emplace_back(static_cast<std::int64_t>(variant.runs[run].first - base), static_cast<std::uint32_t>((variant.runs[run].second - variant.runs[run].first) / sizeof(std::uint32_t)));
            }
            shape->words.push_back(std::move(word));
            changed = true;
        }
    }
    if (changed) {
        std::sort(shape->words.begin(), shape->words.end(), [](const RelocatableUserWord& a, const RelocatableUserWord& b) { return a.reg < b.reg; });
        drawRelocations[pending.identity] = std::move(shape);
    }
    if (pending.alias == 0) return;
    if (drawAliases.size() >= MaxAliases) drawRelocationCounters.evicted += dropEveryOther(drawAliases);
    drawAliases[pending.alias] = inserted->second;
}

void Driver::relocateDrawEntry(const QueueState& queue, std::uint64_t alias, std::shared_ptr<DrawEntry>& entry, std::shared_ptr<const DrawDecode>& decode, std::vector<DrawUserPatch>& patches) {
    auto& counters = drawRelocationCounters;
    ++counters.lookups;
    const auto found = drawAliases.find(alias);
    if (found == drawAliases.end() || found->second->decode == nullptr) return;
    const auto donor = found->second;
    const auto& programs = donor->decode->programs;
    if (donor->stages.size() != programs.size()) return;
    struct Move {
        std::size_t program;
        std::uint32_t index, from, to;
    };
    std::vector<Move> moves;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& program = programs[i];
        for (std::uint32_t index = program.hiddenUserWords; index < program.userData.size(); ++index) {
            const auto current = readUserData(queue.shader, program.userDataBase + index - program.hiddenUserWords);
            if (current != program.userData[index]) moves.push_back({i, index, program.userData[index], current});
        }
    }
    if (moves.empty()) return;
    ++counters.donors;
    auto relocated = std::make_shared<DrawEntry>();
    relocated->decode = donor->decode;
    relocated->stages.resize(donor->stages.size());
    for (std::size_t i = 0; i < donor->stages.size(); ++i) {
        const bool moved = std::any_of(moves.begin(), moves.end(), [&](const Move& move) { return move.program == i; });
        if (!moved) {
            relocated->stages[i] = donor->stages[i];
            continue;
        }
        for (const auto& variant : donor->stages[i]) {
            if (variant == nullptr || variant->compiled == nullptr) continue;
            const bool allowed = std::all_of(moves.begin(), moves.end(), [&](const Move& move) { return move.program != i || (move.index < 64 && (variant->relocatableWords >> move.index & 1u) != 0 && move.from != 0 && move.to != 0 && ((move.from ^ move.to) & 3u) == 0); });
            if (!allowed) {
                ++counters.refused;
                continue;
            }
            auto copy = std::make_shared<DispatchVariant>();
            copy->runs = variant->runs;
            copy->runBases = variant->runBases;
            copy->relocatableWords = variant->relocatableWords;
            copy->words = variant->words;
            copy->forgetSerial = variant->forgetSerial;
            copy->compiled = variant->compiled;
            copy->meshIndexDescriptor = variant->meshIndexDescriptor;
            copy->shader = variant->shader;
            copy->dataPositions = variant->dataPositions;
            copy->dataSlots = variant->dataSlots;
            copy->flatBinding = variant->flatBinding;
            copy->pushOffset = variant->pushOffset;
            copy->vertexInfo = variant->vertexInfo;
            for (const auto& move : moves) {
                if (move.program != i) continue;
                const auto delta = AlignedWord(move.to) - AlignedWord(move.from);
                for (std::size_t run = 0; run < copy->runs.size() && run < copy->runBases.size(); ++run) {
                    if (copy->runBases[run] != static_cast<std::int8_t>(move.index)) continue;
                    copy->runs[run].first += delta;
                    copy->runs[run].second += delta;
                }
                const auto& forwarded = variant->compiled->shaderDataUserWords;
                for (std::uint32_t slot = 0; slot < forwarded.size(); ++slot) {
                    if (forwarded[slot] == move.index) copy->shaderDataPatches.emplace_back(slot, move.to);
                }
            }
            relocated->stages[i].push_back(std::move(copy));
        }
    }
    patches.clear();
    for (const auto& move : moves) patches.push_back({move.program, move.index, move.to});
    entry = std::move(relocated);
    decode = donor->decode;
}

void Driver::applyShaderDataPatches(DispatchVariant& variant) {
    if (variant.shaderDataPatches.empty()) return;
    auto rebased = std::make_shared<ShaderRecompiler::RecompileResult>(*variant.compiled);
    for (const auto& [slot, value] : variant.shaderDataPatches) {
        const auto at = static_cast<std::size_t>(slot) * sizeof(std::uint32_t);
        if (at + sizeof(std::uint32_t) <= rebased->pushConstants.size()) std::memcpy(rebased->pushConstants.data() + at, &value, sizeof(value));
        for (auto& binding : rebased->bindings) {
            if (binding.role == ShaderRecompiler::DescriptorRole::ShaderData && slot < binding.guestDescriptor.size()) binding.guestDescriptor[slot] = value;
        }
    }
    variant.compiled = std::move(rebased);
    variant.shaderDataPatches.clear();
}

void Driver::reportDrawRelocation() {
    auto& counters = drawRelocationCounters;
    AgcDriver::ProfilePrint_nid_no_patch("[draw-relocate] %llu alias lookups: %llu donors with moved user words, %llu refusals, %llu hits through a relocated entry; %zu shader sets learned, %zu aliases held (%llu dropped at the limit)\n", static_cast<unsigned long long>(counters.lookups), static_cast<unsigned long long>(counters.donors), static_cast<unsigned long long>(counters.refused), static_cast<unsigned long long>(counters.hits), drawRelocations.size(), drawAliases.size(), static_cast<unsigned long long>(counters.evicted));
    counters = {};
}

}
