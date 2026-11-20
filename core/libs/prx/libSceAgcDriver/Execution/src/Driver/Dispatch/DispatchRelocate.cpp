#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Recipe.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::size_t MaxDonorsPerProgram = 8;
constexpr std::size_t MaxDonorTries = 2;
constexpr std::size_t MaxDonorPrograms = 4096;
constexpr std::size_t DescriptorWords = 4;
constexpr std::uint32_t DescriptorBaseHighMask = 0xffffu;

using Descriptor = std::array<std::uint32_t, DescriptorWords>;

std::uint32_t AlignedBase(std::uint32_t word) {
    return word & ~std::uint32_t{3};
}

bool UsesAddressTables(const ShaderRecompiler::RecompileResult& compiled) {
    return std::any_of(compiled.bindings.begin(), compiled.bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; });
}

bool BoundBuffer(const ShaderRecompiler::RecompileResult& compiled, const Descriptor& words) {
    for (const auto& binding : compiled.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        const auto& held = binding.guestDescriptor;
        for (std::size_t at = 0; at + DescriptorWords <= held.size(); at += DescriptorWords) {
            if (held[at] == words[0] && held[at + 1] == words[1] && held[at + 2] == words[2]) return true;
        }
    }
    return false;
}

void ReplaceDescriptor(ShaderRecompiler::RecompileResult& compiled, const Descriptor& from, const Descriptor& to, bool& inBuffers) {
    for (auto& binding : compiled.bindings) {
        auto& held = binding.guestDescriptor;
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
            for (std::size_t at = 0; at + DescriptorWords <= held.size(); at += DescriptorWords) {
                if (held[at] != from[0] || held[at + 1] != from[1] || held[at + 2] != from[2]) continue;
                held[at] = to[0];
                held[at + 1] = to[1];
                inBuffers = true;
            }
            continue;
        }
        if (binding.role != ShaderRecompiler::DescriptorRole::FlattenedSrt && binding.role != ShaderRecompiler::DescriptorRole::ShaderData) continue;
        for (std::size_t at = 0; at + DescriptorWords <= held.size();) {
            if (!std::equal(from.begin(), from.end(), held.begin() + static_cast<std::ptrdiff_t>(at))) {
                ++at;
                continue;
            }
            std::copy(to.begin(), to.end(), held.begin() + static_cast<std::ptrdiff_t>(at));
            at += DescriptorWords;
        }
    }
}

bool HoldsWord(const ShaderRecompiler::RecompileResult& compiled, std::uint32_t value) {
    for (std::size_t at = 0; at + sizeof(std::uint32_t) <= compiled.pushConstants.size(); at += sizeof(std::uint32_t)) {
        std::uint32_t word = 0;
        std::memcpy(&word, compiled.pushConstants.data() + at, sizeof(word));
        if (word == value) return true;
    }
    for (const auto& binding : compiled.bindings) {
        if (std::find(binding.guestDescriptor.begin(), binding.guestDescriptor.end(), value) != binding.guestDescriptor.end()) return true;
    }
    return std::any_of(compiled.specialization.begin(), compiled.specialization.end(), [&](const auto& constant) { return constant.value == value; });
}

}

namespace {

std::atomic<std::uint64_t> relocationsMade{0};

}

std::uint64_t DispatchRelocationsMade() {
    return relocationsMade.load(std::memory_order_relaxed);
}

bool Driver::dispatchRelocationEnabled() {
    return !Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::NoDispatchRelocation);
}

void Driver::learnDispatchRelocation(std::uint64_t identity, std::uint64_t key, const DispatchVariant& variant, std::span<const std::uint32_t> userData) {
    if (variant.compiled == nullptr || variant.linking != nullptr || variant.memory != nullptr) return;
    if (dispatchDonors.size() >= MaxDonorPrograms && !dispatchDonors.contains(identity)) {
        dispatchRelocationCounters.evicted += dispatchDonors.size();
        dispatchDonors.clear();
    }
    auto& donors = dispatchDonors[identity];
    std::erase_if(donors, [&](const DispatchDonor& donor) { return donor.key == key; });
    donors.insert(donors.begin(), {key, std::vector<std::uint32_t>(userData.begin(), userData.end())});
    if (donors.size() > MaxDonorsPerProgram) donors.pop_back();
    ++dispatchRelocationCounters.learned;
}

std::shared_ptr<DispatchVariant> Driver::relocatedVariant(const DispatchVariant& donor, std::span<const DispatchUserMove> moves, std::span<const DispatchDescriptorMove> userDescriptors, DispatchRelocationCounters& counters) {
    if (donor.compiled == nullptr || donor.linking != nullptr || donor.memory != nullptr || donor.runBases.size() != donor.runs.size()) return nullptr;
    for (const auto& move : moves) {
        if (move.descriptor) continue;
        if (move.index >= 64 || (donor.relocatableWords >> move.index & 1u) == 0) {
            ++counters.refused;
            return nullptr;
        }
    }
    auto copy = std::make_shared<DispatchVariant>();
    copy->runs = donor.runs;
    copy->runBases = donor.runBases;
    copy->words = donor.words;
    copy->forgetSerial = donor.forgetSerial;
    copy->registeredViews = donor.registeredViews;
    copy->shader = donor.shader;
    copy->dataPositions = donor.dataPositions;
    copy->dataSlots = donor.dataSlots;
    copy->flatBinding = donor.flatBinding;
    copy->relocatableWords = donor.relocatableWords;
    copy->userWordsWalked = donor.userWordsWalked;
    std::vector<std::pair<Descriptor, Descriptor>> descriptors;
    struct Shift {
        std::uint64_t begin, end, delta;
    };
    std::vector<Shift> shifts;
    const auto addDescriptor = [&](const Descriptor& from, const Descriptor& to) {
        descriptors.emplace_back(from, to);
        const auto oldBase = static_cast<std::uint64_t>(from[0]) | static_cast<std::uint64_t>(from[1] & DescriptorBaseHighMask) << 32u;
        const auto newBase = static_cast<std::uint64_t>(to[0]) | static_cast<std::uint64_t>(to[1] & DescriptorBaseHighMask) << 32u;
        const auto stride = (from[1] >> 16u) & 0x3fffu;
        const auto bytes = std::max<std::uint64_t>(stride != 0 ? static_cast<std::uint64_t>(stride) * from[2] : from[2], 16u);
        shifts.push_back({oldBase, oldBase + bytes, newBase - oldBase});
    };
    for (const auto& move : userDescriptors) addDescriptor(move.from, move.to);
    std::vector<std::size_t> offsets(copy->runs.size());
    std::size_t total = 0;
    for (std::size_t run = 0; run < copy->runs.size(); ++run) {
        offsets[run] = total;
        total += static_cast<std::size_t>((copy->runs[run].second - copy->runs[run].first) / sizeof(std::uint32_t));
    }
    if (total > copy->words.size()) return nullptr;
    const auto compare = [&](std::size_t run, std::uint64_t delta, bool nested) {
        const auto count = static_cast<std::size_t>((copy->runs[run].second - copy->runs[run].first) / sizeof(std::uint32_t));
        copy->runs[run].first += delta;
        copy->runs[run].second += delta;
        const std::span<const std::uint32_t> was(donor.words.data() + offsets[run], count);
        const std::span<std::uint32_t> now(copy->words.data() + offsets[run], count);
        std::array<std::uint32_t, 64> content;
        for (std::size_t done = 0; done < count; done += content.size()) {
            const auto chunk = std::min<std::size_t>(count - done, content.size());
            if (GuestMemory::CopyMapped(copy->runs[run].first + done * sizeof(std::uint32_t), std::as_writable_bytes(std::span(content).first(chunk))) != GuestMemory::Compare::Equal) {
                ++counters.unreadable;
                return false;
            }
            std::copy_n(content.begin(), chunk, now.begin() + static_cast<std::ptrdiff_t>(done));
        }
        for (std::size_t at = 0; at < count;) {
            if (now[at] == was[at]) {
                ++at;
                continue;
            }
            if (nested || at + DescriptorWords > count || was[at + 2] != now[at + 2] || was[at + 3] != now[at + 3] || ((was[at + 1] ^ now[at + 1]) & ~DescriptorBaseHighMask) != 0) {
                ++counters.contentChanged;
                return false;
            }
            Descriptor from{}, to{};
            std::copy_n(was.begin() + static_cast<std::ptrdiff_t>(at), DescriptorWords, from.begin());
            std::copy_n(now.begin() + static_cast<std::ptrdiff_t>(at), DescriptorWords, to.begin());
            const auto known = std::find_if(descriptors.begin(), descriptors.end(), [&](const auto& pair) { return pair.first == from; });
            if (known != descriptors.end() && known->second != to) {
                ++counters.descriptorChanged;
                return false;
            }
            if (known == descriptors.end()) addDescriptor(from, to);
            at += DescriptorWords;
        }
        return true;
    };
    std::vector<bool> moved(copy->runs.size(), false);
    for (std::size_t run = 0; run < copy->runs.size(); ++run) {
        const auto origin = copy->runBases[run];
        const auto move = std::find_if(moves.begin(), moves.end(), [&](const DispatchUserMove& candidate) { return !candidate.descriptor && static_cast<std::int8_t>(candidate.index) == origin; });
        if (move == moves.end()) continue;
        moved[run] = true;
        if (!compare(run, static_cast<std::uint64_t>(AlignedBase(move->to)) - static_cast<std::uint64_t>(AlignedBase(move->from)), false)) return nullptr;
    }
    for (std::size_t run = 0; run < copy->runs.size(); ++run) {
        if (moved[run]) continue;
        const auto& [begin, end] = donor.runs[run];
        const auto shift = std::find_if(shifts.begin(), shifts.end(), [&](const Shift& candidate) { return begin >= candidate.begin && end <= candidate.end; });
        if (shift == shifts.end()) {
            if (std::any_of(shifts.begin(), shifts.end(), [&](const Shift& candidate) { return begin < candidate.end && end > candidate.begin; })) {
                ++counters.descriptorChanged;
                return nullptr;
            }
            continue;
        }
        if (!compare(run, shift->delta, true)) return nullptr;
    }
    if (!descriptors.empty() && UsesAddressTables(*donor.compiled)) {
        ++counters.descriptorChanged;
        return nullptr;
    }
    bool movesBuffers = false;
    const auto& forwarded = donor.compiled->shaderDataUserWords;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> patches;
    for (const auto& move : moves) {
        for (std::uint32_t slot = 0; slot < forwarded.size(); ++slot) {
            if (forwarded[slot] == move.index) patches.emplace_back(slot, move.to);
        }
    }
    if (patches.empty() && descriptors.empty()) {
        copy->compiled = donor.compiled;
    } else {
        auto rebased = std::make_shared<ShaderRecompiler::RecompileResult>(*donor.compiled);
        for (const auto& [slot, value] : patches) patchForwardedWord(*rebased, slot, value);
        for (const auto& [from, to] : descriptors) {
            bool inBuffers = false;
            ReplaceDescriptor(*rebased, from, to, inBuffers);
            movesBuffers = movesBuffers || inBuffers;
            if (HoldsWord(*rebased, from[0])) {
                ++counters.descriptorChanged;
                return nullptr;
            }
        }
        copy->compiled = std::move(rebased);
    }
    for (const auto& move : moves) {
        if (move.descriptor || !HoldsWord(*copy->compiled, move.from)) continue;
        ++counters.refused;
        return nullptr;
    }
    counters.movedDescriptors += descriptors.size();
    if (const auto recipe = donor.recipe.load(std::memory_order_acquire); recipe != nullptr) {
        auto own = std::make_shared<Recipe>(*recipe);
        bool kept = true;
        for (const auto& [slot, value] : patches) {
            const auto at = static_cast<std::size_t>(slot) * sizeof(std::uint32_t);
            const auto from = std::find_if(moves.begin(), moves.end(), [&](const DispatchUserMove& move) { return move.index == forwarded[slot]; });
            std::uint32_t held = 0;
            if (at + sizeof(std::uint32_t) <= own->pushBytes.size()) std::memcpy(&held, own->pushBytes.data() + at, sizeof(held));
            if (at + sizeof(std::uint32_t) > own->pushBytes.size() || from == moves.end() || held != from->from) {
                kept = false;
                break;
            }
            std::memcpy(own->pushBytes.data() + at, &value, sizeof(value));
        }
        if (kept) {
            if (!patches.empty() || !descriptors.empty()) own->dataWordsHash = Graphics::ShaderResources::DataWordsHash({ShaderRecompiler::ShaderStage::Compute, copy->compiled.get(), 0});
            own->movesBuffers = own->movesBuffers || movesBuffers;
            copy->recipe.store(std::move(own), std::memory_order_release);
        } else {
            ++counters.recipesDropped;
        }
    }
    return copy;
}

void Driver::relocateDispatchEntry(std::uint64_t identity, std::uint64_t key, std::span<const std::uint32_t> userData) {
    struct Candidate {
        DispatchDonor donor;
        std::shared_ptr<DispatchEntry> entry;
        bool recipe = false;
    };
    std::vector<Candidate> candidates;
    {
        std::lock_guard lock(dispatchCacheMutex);
        if (dispatchCache.contains(key)) return;
        const auto known = dispatchDonors.find(identity);
        if (known == dispatchDonors.end() || known->second.empty()) return;
        ++dispatchRelocationCounters.lookups;
        for (const auto& donor : known->second) {
            if (donor.userData.size() != userData.size()) continue;
            const auto found = dispatchCache.find(donor.key);
            if (found == dispatchCache.end() || found->second->variants.empty()) continue;
            candidates.push_back({donor, found->second, found->second->variants.front()->recipe.load(std::memory_order_acquire) != nullptr});
        }
    }
    std::stable_partition(candidates.begin(), candidates.end(), [](const Candidate& candidate) { return candidate.recipe; });
    DispatchRelocationCounters counters;
    std::shared_ptr<DispatchEntry> relocated;
    for (const auto& candidate : candidates) {
        const auto& front = *candidate.entry->variants.front();
        const auto relocatable = front.relocatableWords;
        const auto& was = candidate.donor.userData;
        std::vector<DispatchUserMove> moves;
        std::vector<DispatchDescriptorMove> userDescriptors;
        bool fits = true;
        for (std::uint32_t index = 0; index < userData.size() && fits; ++index) {
            const auto from = was[index];
            const auto to = userData[index];
            if (from == to) continue;
            if (index < 64 && (relocatable >> index & 1u) != 0 && from != 0 && to != 0 && ((from ^ to) & 3u) == 0) {
                moves.push_back({index, from, to});
                continue;
            }
            fits = false;
            if (front.compiled == nullptr) break;
            for (std::uint32_t start = index >= 1 ? index - 1 : 0; start <= index && !fits; ++start) {
                if (start + DescriptorWords > userData.size()) continue;
                const DispatchDescriptorMove move{{was[start], was[start + 1], was[start + 2], was[start + 3]}, {userData[start], userData[start + 1], userData[start + 2], userData[start + 3]}};
                if (move.from[2] != move.to[2] || move.from[3] != move.to[3] || ((move.from[1] ^ move.to[1]) & ~DescriptorBaseHighMask) != 0) continue;
                if (start + 1 == index && move.from[0] != move.to[0] && std::none_of(userDescriptors.begin(), userDescriptors.end(), [&](const DispatchDescriptorMove& held) { return held.from == move.from; })) continue;
                if (!BoundBuffer(*front.compiled, move.from)) continue;
                fits = true;
                if (std::none_of(userDescriptors.begin(), userDescriptors.end(), [&](const DispatchDescriptorMove& held) { return held.from == move.from; })) userDescriptors.push_back(move);
                moves.push_back({index, from, to, true});
            }
        }
        if (!fits || moves.empty()) continue;
        if (counters.donors == MaxDonorTries) break;
        ++counters.donors;
        auto entry = std::make_shared<DispatchEntry>();
        for (const auto& variant : candidate.entry->variants) {
            if (variant == nullptr) continue;
            if (auto copy = relocatedVariant(*variant, moves, userDescriptors, counters); copy != nullptr) {
                entry->variants.push_back(std::move(copy));
                break;
            }
        }
        if (entry->variants.empty()) continue;
        relocated = std::move(entry);
        break;
    }
    std::lock_guard lock(dispatchCacheMutex);
    auto& total = dispatchRelocationCounters;
    total.donors += counters.donors;
    total.refused += counters.refused;
    total.contentChanged += counters.contentChanged;
    total.descriptorChanged += counters.descriptorChanged;
    total.unreadable += counters.unreadable;
    total.movedDescriptors += counters.movedDescriptors;
    total.recipesDropped += counters.recipesDropped;
    if (relocated == nullptr) {
        ++total.noDonor;
        return;
    }
    if (dispatchCache.contains(key)) return;
    ++total.relocated;
    relocationsMade.fetch_add(1, std::memory_order_relaxed);
    relocated->touched = dispatchCacheHits;
    for (const auto& variant : relocated->variants) accountVariant(*variant, true);
    dispatchOrder.push_front(key);
    relocated->order = dispatchOrder.begin();
    dispatchCache.emplace(key, std::move(relocated));
    auto& donors = dispatchDonors[identity];
    donors.insert(donors.begin(), {key, std::vector<std::uint32_t>(userData.begin(), userData.end())});
    if (donors.size() > MaxDonorsPerProgram) donors.pop_back();
    while (dispatchCache.size() > dispatchCacheEntries() || (dispatchCache.size() > 1 && dispatchCacheVariantBytes > DispatchCacheBytes())) {
        eraseDispatchEntry(dispatchCache.find(dispatchOrder.back()));
        ++dispatchCacheEvictions;
    }
}

void Driver::reportDispatchRelocation() {
    auto& counters = dispatchRelocationCounters;
    AgcDriver::ProfilePrint_nid_no_patch("[dispatch-relocate] (10 s) %llu misses with known donors: %llu relocated (%llu buffer descriptors moved), %llu with no fitting donor; donor tries %llu, refused user words %llu, content changed %llu, descriptor refused %llu, unreadable %llu; %llu recipes dropped; %llu donors learned, %zu programs held (%llu dropped at the limit)\n", static_cast<unsigned long long>(counters.lookups), static_cast<unsigned long long>(counters.relocated), static_cast<unsigned long long>(counters.movedDescriptors), static_cast<unsigned long long>(counters.noDonor), static_cast<unsigned long long>(counters.donors), static_cast<unsigned long long>(counters.refused), static_cast<unsigned long long>(counters.contentChanged), static_cast<unsigned long long>(counters.descriptorChanged), static_cast<unsigned long long>(counters.unreadable), static_cast<unsigned long long>(counters.recipesDropped), static_cast<unsigned long long>(counters.learned), dispatchDonors.size(), static_cast<unsigned long long>(counters.evicted));
    counters = {};
}

}
