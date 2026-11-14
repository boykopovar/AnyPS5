#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
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
#include <optional>
#include <span>

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
    if (word.value) return hash;
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
            const auto address = (static_cast<std::uint64_t>(word.high) << 32u) + AlignedWord(value) + static_cast<std::uint64_t>(offset) + done * sizeof(std::uint32_t);
            if (GuestMemory::CopyMapped(address, std::as_writable_bytes(std::span(content).first(count))) != GuestMemory::Compare::Equal) {
                readable = false;
                return hash;
            }
            for (std::size_t i = 0; i < count; ++i) mix(std::find(word.dataOffsets.begin(), word.dataOffsets.end(), offset + static_cast<std::int64_t>(done + i) * 4) != word.dataOffsets.end() ? 0u : content[i]);
        }
    }
    return hash;
}

void Driver::tagUserValueWords(DispatchVariant& variant, const DrawProgram& program) {
    if (!drawRelocationEnabled() || variant.compiled == nullptr) return;
    const auto& compiled = *variant.compiled;
    const auto& forwarded = compiled.shaderDataUserWords;
    const auto users = std::min<std::size_t>(program.userData.size(), 64);
    const auto bound = [&](std::uint32_t value) {
        std::size_t count = 0;
        for (const auto& binding : compiled.bindings) count += static_cast<std::size_t>(std::count(binding.guestDescriptor.begin(), binding.guestDescriptor.end(), value));
        return count;
    };
    const auto elsewhere = [&](std::uint32_t index, std::uint32_t value) {
        for (std::size_t slot = 0; (slot + 1) * sizeof(std::uint32_t) <= compiled.pushConstants.size(); ++slot) {
            std::uint32_t word = 0;
            std::memcpy(&word, compiled.pushConstants.data() + slot * sizeof(std::uint32_t), sizeof(word));
            if (word == value && (slot >= forwarded.size() || forwarded[slot] != index)) return true;
        }
        if (std::any_of(compiled.specialization.begin(), compiled.specialization.end(), [&](const auto& constant) { return constant.value == value; })) return true;
        return std::find(variant.words.begin(), variant.words.end(), value) != variant.words.end();
    };
    const auto readThrough = [&](const std::uint32_t* descriptor) {
        const auto base = static_cast<std::uint64_t>(descriptor[0]) | static_cast<std::uint64_t>(descriptor[1] & 0xffffu) << 32u;
        const auto stride = (descriptor[1] >> 16u) & 0x3fffu;
        const auto bytes = std::max<std::uint64_t>(stride != 0 ? static_cast<std::uint64_t>(stride) * descriptor[2] : descriptor[2], 16u);
        return std::any_of(variant.runs.begin(), variant.runs.end(), [&](const auto& run) { return run.first < base + bytes && run.second > base; });
    };
    const auto taken = [&](std::uint32_t index) { return ((variant.relocatableWords | variant.valueWords | variant.calleeWords) >> index & 1u) != 0; };
    if (ShaderRecompiler::GcnGuestShaders() && program.binary.stage == ShaderRecompiler::ShaderStage::Vertex) {
        for (const auto instruction : program.binary.code) {
            if ((instruction & 0xff80ff00u) != 0xbe802100u) continue;
            const auto target = instruction & 0xffu;
            if (target < program.firstUserSgpr) continue;
            const auto index = program.hiddenUserWords + target - program.firstUserSgpr;
            if (index + 1 >= users || taken(index) || taken(index + 1)) continue;
            const auto low = program.userData[index];
            const auto address = static_cast<std::uint64_t>(low) | static_cast<std::uint64_t>(program.userData[index + 1]) << 32u;
            if (address == 0 || address % 4u != 0 || bound(low) != 0 || elsewhere(index, low)) continue;
            std::array<std::uint32_t, 1024> code;
            if (GuestMemory::CopyMapped(address, std::as_writable_bytes(std::span(code))) != GuestMemory::Compare::Equal) continue;
            std::optional<std::uint32_t> length;
            try {
                length = ShaderRecompiler::UserDataCalleeLength(code, (instruction >> 16u) & 0x7fu);
            } catch (const std::exception&) {
                continue;
            }
            if (!length || *length == 0) continue;
            variant.calleeWords |= std::uint64_t {1} << index;
            variant.calleeReads.emplace_back(index, *length);
        }
    }
    for (std::uint32_t index = program.hiddenUserWords; index + 2 < users; ++index) {
        const auto base = program.userData[index];
        if (base == 0 || taken(index) || bound(base) != 1 || elsewhere(index, base)) continue;
        std::uint32_t binding = 0, word = 0;
        for (std::size_t b = 0; b < compiled.bindings.size(); ++b) {
            const auto& descriptor = compiled.bindings[b].guestDescriptor;
            const auto found = std::find(descriptor.begin(), descriptor.end(), base);
            if (found == descriptor.end()) continue;
            binding = static_cast<std::uint32_t>(b);
            word = static_cast<std::uint32_t>(found - descriptor.begin());
        }
        const auto& target = compiled.bindings[binding];
        const auto element = word / 4u;
        const bool readOnly = target.role == ShaderRecompiler::DescriptorRole::GuestBuffers && target.guestDescriptor.size() == static_cast<std::size_t>(target.count) * 4u && word % 4u == 0 && element < target.bufferWritten.size() && !target.bufferWritten[element] && (element >= target.bufferAtomic.size() || !target.bufferAtomic[element]);
        if (!readOnly || target.guestDescriptor[word + 1] != program.userData[index + 1] || target.guestDescriptor[word + 2] != program.userData[index + 2] || readThrough(target.guestDescriptor.data() + word)) continue;
        variant.valueWords |= std::uint64_t {1} << index;
        variant.valueSlots.push_back({index, binding, word});
        const auto records = program.userData[index + 2];
        if (records != 0 && !taken(index + 2) && bound(records) == 1 && !elsewhere(index + 2, records)) {
            variant.valueWords |= std::uint64_t {1} << (index + 2);
            variant.valueSlots.push_back({index + 2, binding, word + 2});
        }
    }
}

void Driver::tagVariantRuns(DispatchVariant& variant, std::span<const ShaderRecompiler::MemoryRegion> regions, const ShaderRecompiler::SrtUserDataUse& use, bool eligible, std::uint32_t hiddenWords, std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> supplied) {
    variant.userWordsWalked = use.asBase | use.asValue;
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
    const auto origin = [&](std::uint64_t address) -> std::int8_t {
        const auto found = std::lower_bound(reads.begin(), reads.end(), std::make_pair(address, std::numeric_limits<std::int32_t>::min()));
        if (found == reads.end() || found->first != address || found->second < 0) return -1;
        return (relocatable >> found->second & 1u) != 0 ? static_cast<std::int8_t>(found->second) : std::int8_t {-1};
    };
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
        std::size_t start = 0;
        auto current = count != 0 ? origin(region.guestAddress) : std::int8_t {-1};
        for (std::size_t word = 1; word <= count; ++word) {
            const auto next = word < count ? origin(region.guestAddress + word * sizeof(std::uint32_t)) : std::int8_t {-2};
            if (next == current) continue;
            AppendSharedWords(variant.shared, variant.runs.size(), offset + start, region.bytes.subspan(start * sizeof(std::uint32_t), (word - start) * sizeof(std::uint32_t)), supplied);
            variant.runs.emplace_back(region.guestAddress + start * sizeof(std::uint32_t), region.guestAddress + word * sizeof(std::uint32_t));
            variant.runBases.push_back(current);
            start = word;
            current = next;
        }
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
            const bool relocatable = (variant.relocatableWords >> index & 1u) != 0;
            const bool value = !relocatable && (variant.valueWords >> index & 1u) != 0;
            if (!relocatable && !value && (variant.calleeWords >> index & 1u) == 0) {
                if (held == shape->words.end()) continue;
                shape->words.erase(held);
                shape->banned.push_back(reg);
                changed = true;
                continue;
            }
            if (held != shape->words.end() || std::find(shape->banned.begin(), shape->banned.end(), reg) != shape->banned.end()) continue;
            RelocatableUserWord word{reg, {}, value};
            for (const auto& [callee, length] : variant.calleeReads) {
                if (callee != index) continue;
                word.reads.emplace_back(0, length);
                word.high = program.userData[index + 1];
            }
            const auto base = AlignedWord(program.userData[index]);
            std::uint32_t position = 0;
            for (std::size_t run = 0; run < variant.runs.size(); ++run) {
                const auto count = static_cast<std::uint32_t>((variant.runs[run].second - variant.runs[run].first) / sizeof(std::uint32_t));
                if (run < variant.runBases.size() && variant.runBases[run] == static_cast<std::int8_t>(index)) {
                    word.reads.emplace_back(static_cast<std::int64_t>(variant.runs[run].first - base), count);
                    if (!variant.bufferWordBuffers.empty()) {
                        for (const auto data : variant.dataPositions) {
                            if (data >= position && data < position + count) word.dataOffsets.push_back(static_cast<std::int64_t>(variant.runs[run].first - base) + static_cast<std::int64_t>(data - position) * 4);
                        }
                    }
                }
                position += count;
            }
            shape->words.push_back(std::move(word));
            changed = true;
        }
    }
    if (changed) {
        std::sort(shape->words.begin(), shape->words.end(), [](const RelocatableUserWord& a, const RelocatableUserWord& b) { return a.reg < b.reg; });
        drawRelocations[pending.identity] = std::move(shape);
    }
    if (pending.aliases.structure == 0) return;
    if (drawStageDonors.size() >= MaxAliases) drawRelocationCounters.evicted += dropEveryOther(drawStageDonors);
    if (drawDecodes.size() >= MaxAliases) drawRelocationCounters.evicted += dropEveryOther(drawDecodes);
    DrawDecodeDonor donor{entry.decode, std::vector<std::uint64_t>(programs.size(), 0)};
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (entry.stages[i].empty() || entry.stages[i].front() == nullptr || entry.stages[i].front()->prepared == nullptr) continue;
        const auto& variant = entry.stages[i].front();
        const auto& program = programs[i];
        std::uint64_t key = 0xcbf29ce484222325ull;
        const auto mix = [&](std::uint64_t value) {
            key ^= value;
            key *= 0x100000001b3ull;
        };
        mix(reinterpret_cast<std::uintptr_t>(variant->prepared.get()));
        mix(variant->pushOffset);
        mix(program.userData.size());
        for (std::uint32_t index = 0; index < program.userData.size(); ++index) {
            const bool relocatable = pending.relocation != nullptr && index >= program.hiddenUserWords && std::any_of(pending.relocation->words.begin(), pending.relocation->words.end(), [&](const RelocatableUserWord& word) { return word.reg == program.userDataBase + index - program.hiddenUserWords; });
            if (!relocatable && index >= program.hiddenUserWords) mix(program.userData[index]);
        }
        donor.stageKeys[i] = key == 0 ? 1 : key;
        drawStageDonors[pending.aliases.Key(donor.stageKeys[i], program.userDataBase)] = {variant, program.userData, pending.key, inserted->second};
    }
    const auto held = drawDecodes.find(pending.aliases.structure);
    if (held != drawDecodes.end() && held->second.stageKeys.size() == donor.stageKeys.size()) {
        for (std::size_t i = 0; i < donor.stageKeys.size(); ++i) {
            if (donor.stageKeys[i] == 0) donor.stageKeys[i] = held->second.stageKeys[i];
        }
    }
    drawDecodes[pending.aliases.structure] = std::move(donor);
}

void Driver::relocateDrawEntry(const QueueState& queue, const DrawAliasKeys& aliases, std::shared_ptr<DrawEntry>& entry, std::shared_ptr<const DrawDecode>& decode, std::vector<DrawUserPatch>& patches) {
    auto& counters = drawRelocationCounters;
    ++counters.lookups;
    const auto known = drawDecodes.find(aliases.structure);
    if (known == drawDecodes.end() || known->second.decode == nullptr) return;
    const auto shared = known->second.decode;
    const auto& stageKeys = known->second.stageKeys;
    const auto& programs = shared->programs;
    auto relocated = std::make_shared<DrawEntry>();
    relocated->decode = shared;
    relocated->stages.resize(programs.size());
    std::vector<DrawUserPatch> current;
    bool complete = true;
    bool mixedOwners = false;
    drawStageDonorKeys.assign(programs.size(), 0);
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& program = programs[i];
        if (i < shared->roles.size() && shared->roles[i] == ShaderRecompiler::ProgramRole::GeometryBack) continue;
        const auto found = i < stageKeys.size() && stageKeys[i] != 0 ? drawStageDonors.find(aliases.Key(stageKeys[i], program.userDataBase)) : drawStageDonors.end();
        if (found != drawStageDonors.end()) drawStageDonorKeys[i] = found->first;
        const auto owner = found != drawStageDonors.end() ? found->second.owner.lock() : nullptr;
        const auto cachedOwner = found != drawStageDonors.end() ? drawCache.find(found->second.ownerKey) : drawCache.end();
        const bool liveOwner = owner != nullptr && cachedOwner != drawCache.end() && cachedOwner->second.get() == owner.get();
        const auto variant = liveOwner ? found->second.variant.lock() : nullptr;
        const auto* donor = variant != nullptr && variant->compiled != nullptr && found->second.userData.size() == program.userData.size() && (variant->registeredViews == nullptr || CaptureRegisteredViewsCurrent(*variant->registeredViews)) ? &found->second : nullptr;
        struct Move {
            std::uint32_t index, from, to;
        };
        std::vector<Move> moves;
        bool allowed = donor != nullptr;
        for (std::uint32_t index = program.hiddenUserWords; index < program.userData.size(); ++index) {
            const auto now = readUserData(queue.shader, program.userDataBase + index - program.hiddenUserWords);
            if (now != program.userData[index]) current.push_back({i, index, now});
            if (donor == nullptr || now == donor->userData[index]) continue;
            const auto from = donor->userData[index];
            if (index >= 64 || from == 0 || now == 0 || !(((((variant->relocatableWords | variant->calleeWords) >> index) & 1u) != 0 && ((from ^ now) & 3u) == 0) || ((variant->valueWords >> index) & 1u) != 0)) allowed = false;
            moves.push_back({index, from, now});
        }
        if (!allowed) {
            if (donor != nullptr) ++counters.refused;
            complete = false;
            continue;
        }
        const auto recipeOwner = owner->donor != nullptr ? owner->donor : owner;
        if (!mixedOwners) {
            if (relocated->donor == nullptr) relocated->donor = recipeOwner;
            else if (relocated->donor != recipeOwner) {
                relocated->donor = nullptr;
                mixedOwners = true;
            }
        }
        if (moves.empty()) {
            relocated->stages[i].push_back(variant);
            continue;
        }
        auto copy = cloneVariant(*variant);
        copy->donor = variant->donor != nullptr ? variant->donor : variant;
        for (const auto& move : moves) {
            const auto delta = AlignedWord(move.to) - AlignedWord(move.from);
            for (std::size_t run = 0; run < copy->runs.size() && run < copy->runBases.size(); ++run) {
                if (copy->runBases[run] != static_cast<std::int8_t>(move.index)) continue;
                copy->runs[run].first += delta;
                copy->runs[run].second += delta;
            }
            for (const auto& [index, binding, word] : variant->valueSlots) {
                if (index == move.index) copy->descriptorPatches.push_back({binding, word, move.to});
            }
            const auto& forwarded = variant->compiled->shaderDataUserWords;
            for (std::uint32_t slot = 0; slot < forwarded.size(); ++slot) {
                if (forwarded[slot] == move.index) copy->shaderDataPatches.emplace_back(slot, move.to);
            }
        }
        relocated->stages[i].push_back(std::move(copy));
    }
    if (complete) ++counters.donors;
    patches = std::move(current);
    entry = std::move(relocated);
    decode = shared;
}

std::shared_ptr<DispatchVariant> Driver::cloneVariant(const DispatchVariant& variant) {
    auto copy = std::make_shared<DispatchVariant>();
    copy->runs = variant.runs;
    copy->registeredViews = variant.registeredViews;
    copy->userDataMask = variant.userDataMask;
    copy->userWordsWalked = variant.userWordsWalked;
    copy->valueWords = variant.valueWords;
    copy->valueSlots = variant.valueSlots;
    copy->descriptorPatches = variant.descriptorPatches;
    copy->calleeWords = variant.calleeWords;
    copy->calleeReads = variant.calleeReads;
    copy->meshIndexDescriptor = variant.meshIndexDescriptor;
    copy->donor = variant.donor;

    copy->runBases = variant.runBases;
    copy->relocatableWords = variant.relocatableWords;
    copy->words = variant.words;
    copy->forgetSerial = variant.forgetSerial;
    copy->compiled = variant.compiled;
    copy->shader = variant.shader;
    copy->dataPositions = variant.dataPositions;
    copy->dataSlots = variant.dataSlots;
    copy->flatBinding = variant.flatBinding;
    copy->pushOffset = variant.pushOffset;
    copy->prepared = variant.prepared;
    copy->bufferWordBuffers = variant.bufferWordBuffers;
    copy->patchedPushConstants = variant.patchedPushConstants;
    copy->shaderDataPatches = variant.shaderDataPatches;
    copy->vertexInfo = variant.vertexInfo;
    return copy;
}

void Driver::tagBufferWords(DispatchVariant& variant, std::span<const ShaderRecompiler::BufferBaseWord> bufferBaseWords) {
    TagDrawBufferBases_nid_no_patch(variant, bufferBaseWords, drawRelocationEnabled());
}

void TagDrawBufferBases_nid_no_patch(DispatchVariant& variant, std::span<const ShaderRecompiler::BufferBaseWord> bufferBaseWords, bool enabled) {
    if (!enabled || bufferBaseWords.empty() || variant.compiled == nullptr || !variant.dataPositions.empty()) return;
    const auto& compiled = *variant.compiled;
    using Patches = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
    std::vector<std::pair<std::uint32_t, Patches>> tagged;
    for (const auto& base : bufferBaseWords) {
        if (!base.bufferRangeKnown) continue;
        const bool capturedBuffer = base.bufferBytes != 0u && std::any_of(variant.runs.begin(), variant.runs.end(), [&](const auto& run) {
            const auto [begin, end] = run;
            return begin < end && (begin >= base.bufferAddress ? begin - base.bufferAddress < base.bufferBytes : base.bufferAddress < end);
        });
        if (capturedBuffer) continue;
        std::uint32_t position = 0;
        bool located = false;
        for (const auto& [begin, end] : variant.runs) {
            if (base.address >= begin && base.address < end && (base.address - begin) % sizeof(std::uint32_t) == 0) {
                position += static_cast<std::uint32_t>((base.address - begin) / sizeof(std::uint32_t));
                located = true;
                break;
            }
            position += static_cast<std::uint32_t>((end - begin) / sizeof(std::uint32_t));
        }
        if (!located || position >= variant.words.size() || variant.words[position] == 0) continue;
        const auto value = variant.words[position];
        Patches patches;
        std::size_t slots = 0, others = 0;
        for (std::size_t binding = 0; binding < compiled.bindings.size(); ++binding) {
            const auto& words = compiled.bindings[binding].guestDescriptor;
            for (std::size_t word = 0; word < words.size(); ++word) {
                if (words[word] != value) continue;
                const bool slot = std::any_of(compiled.bufferBaseSlots.begin(), compiled.bufferBaseSlots.end(), [&](const std::array<std::uint32_t, 3>& entry) { return entry[0] == base.buffer && entry[1] == binding && entry[2] == word; });
                const bool flat = compiled.bindings[binding].role == ShaderRecompiler::DescriptorRole::FlattenedSrt && base.flatOffset >= 0 && static_cast<std::int64_t>(word) == base.flatOffset;
                if (slot) ++slots;
                if (!slot && !flat) ++others;
                if (slot || flat) patches.emplace_back(static_cast<std::uint32_t>(binding), static_cast<std::uint32_t>(word));
            }
        }
        for (std::size_t at = 0; at + sizeof(std::uint32_t) <= compiled.pushConstants.size(); at += sizeof(std::uint32_t)) {
            std::uint32_t word = 0;
            std::memcpy(&word, compiled.pushConstants.data() + at, sizeof(word));
            if (word == value) ++others;
        }
        for (const auto& constant : compiled.specialization) {
            if (constant.value == value) ++others;
        }
        const auto expected = static_cast<std::size_t>(std::count_if(compiled.bufferBaseSlots.begin(), compiled.bufferBaseSlots.end(), [&](const std::array<std::uint32_t, 3>& entry) { return entry[0] == base.buffer; }));
        if (slots == 0 || slots != expected || others != 0) continue;
        if (std::any_of(tagged.begin(), tagged.end(), [&](const auto& entry) { return entry.first == position; })) continue;
        tagged.emplace_back(position, std::move(patches));
    }
    std::sort(tagged.begin(), tagged.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& [position, patches] : tagged) {
        variant.dataPositions.push_back(position);
        variant.bufferWordBuffers.push_back(std::move(patches));
    }
}

std::shared_ptr<DispatchVariant> Driver::patchBufferWords(const DispatchVariant& variant, std::span<const std::pair<std::uint32_t, std::uint32_t>> live) {
    if (live.empty() || variant.compiled == nullptr || variant.dataPositions.size() != variant.bufferWordBuffers.size()) return nullptr;
    auto copy = cloneVariant(variant);
    auto rebased = std::make_shared<ShaderRecompiler::RecompileResult>(*variant.compiled);
    for (const auto& [position, fresh] : live) {
        const auto found = std::lower_bound(variant.dataPositions.begin(), variant.dataPositions.end(), position);
        if (found == variant.dataPositions.end() || *found != position || position >= copy->words.size()) return nullptr;
        const auto stored = copy->words[position];
        if (stored == 0 || fresh == 0 || ((stored ^ fresh) & 3u) != 0) return nullptr;
        for (const auto& [binding, word] : variant.bufferWordBuffers[static_cast<std::size_t>(found - variant.dataPositions.begin())]) {
            if (binding >= rebased->bindings.size() || word >= rebased->bindings[binding].guestDescriptor.size() || rebased->bindings[binding].guestDescriptor[word] != stored) return nullptr;
            rebased->bindings[binding].guestDescriptor[word] = fresh;
        }
        copy->words[position] = fresh;
    }
    copy->compiled = std::move(rebased);
    copy->dataPatched = true;
    return copy;
}

std::shared_ptr<DispatchVariant> Driver::patchStageDataWords(const DispatchVariant& variant, std::span<const std::pair<std::uint32_t, std::uint32_t>> live) {
    if (live.empty() || variant.compiled == nullptr) return nullptr;
    if (!variant.bufferWordBuffers.empty()) return patchBufferWords(variant, live);
    if (variant.dataPositions.empty() || variant.flatBinding == DispatchVariant::NoFlatBinding || variant.flatBinding >= variant.compiled->bindings.size()) return nullptr;
    auto copy = cloneVariant(variant);
    auto words = copy->words;
    for (const auto& [position, fresh] : live) {
        if (position >= words.size()) return nullptr;
        words[position] = fresh;
    }
    copy->compiled = patchDataWords(*copy, words);
    copy->words = std::move(words);
    copy->dataPatched = true;
    return copy;
}

void Driver::applyShaderDataPatches(DispatchVariant& variant) {
    if (variant.shaderDataPatches.empty() && variant.descriptorPatches.empty()) return;
    const bool stored = std::any_of(variant.compiled->bindings.begin(), variant.compiled->bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::ShaderData; });
    if (!stored && variant.descriptorPatches.empty()) {
        if (variant.patchedPushConstants.empty()) variant.patchedPushConstants = variant.compiled->pushConstants;
        for (const auto& [slot, value] : variant.shaderDataPatches) {
            const auto at = static_cast<std::size_t>(slot) * sizeof(std::uint32_t);
            if (at + sizeof(std::uint32_t) <= variant.patchedPushConstants.size()) std::memcpy(variant.patchedPushConstants.data() + at, &value, sizeof(value));
        }
    } else {
        auto rebased = std::make_shared<ShaderRecompiler::RecompileResult>(*variant.compiled);
        if (!variant.patchedPushConstants.empty()) rebased->pushConstants = variant.patchedPushConstants;
        for (const auto& [slot, value] : variant.shaderDataPatches) patchForwardedWord(*rebased, slot, value);
        for (const auto& [binding, word, value] : variant.descriptorPatches) {
            if (binding < rebased->bindings.size() && word < rebased->bindings[binding].guestDescriptor.size()) rebased->bindings[binding].guestDescriptor[word] = value;
        }
        variant.compiled = std::move(rebased);
        variant.patchedPushConstants.clear();
    }
    variant.shaderDataPatches.clear();
    variant.descriptorPatches.clear();
}

void Driver::patchForwardedWord(ShaderRecompiler::RecompileResult& result, std::uint32_t slot, std::uint32_t value) {
    const auto at = static_cast<std::size_t>(slot) * sizeof(std::uint32_t);
    if (at + sizeof(std::uint32_t) <= result.pushConstants.size()) std::memcpy(result.pushConstants.data() + at, &value, sizeof(value));
    for (auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::ShaderData && slot < binding.guestDescriptor.size()) binding.guestDescriptor[slot] = value;
    }
}

void Driver::reportDrawRelocation() {
    auto& counters = drawRelocationCounters;
    AgcDriver::ProfilePrint_nid_no_patch("[draw-relocate] %llu alias lookups: %llu donors with moved user words, %llu refusals, %llu hits through a relocated entry; %zu shader sets learned, %zu stage donors held (%llu dropped at the limit)\n", static_cast<unsigned long long>(counters.lookups), static_cast<unsigned long long>(counters.donors), static_cast<unsigned long long>(counters.refused), static_cast<unsigned long long>(counters.hits), drawRelocations.size(), drawStageDonors.size(), static_cast<unsigned long long>(counters.evicted));
    counters = {};
}

}
