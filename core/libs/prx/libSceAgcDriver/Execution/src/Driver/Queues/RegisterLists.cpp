#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/RegisterLists.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanTails.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::uint32_t LeanReadLimit = std::uint32_t{1} << 16u;

struct ListShape {
    std::uint64_t address = 0;
    std::uint32_t mode = 0;
    std::uint32_t count = 0;

    bool Pairs() const { return mode == RegisterListCache::PairsMode; }
    std::size_t Values() const { return Pairs() ? std::size_t{count} * 2 : count; }
    std::size_t Bytes() const { return Values() * sizeof(std::uint32_t); }
    std::size_t Words() const { return std::size_t{count} * 2; }
};

std::optional<ListShape> leanShape(std::span<const std::uint32_t> packet) {
    if (packet.size() != 5 || packet[4] > LeanReadLimit) return std::nullopt;
    return ListShape{packet[1] | (static_cast<std::uint64_t>(packet[2]) << 32u), packet[3], packet[4]};
}

bool countedReads() {
    static const bool counted = std::getenv("APS5_PROFILE_DRAW") != nullptr || std::getenv("APS5_TRACE_UNREADABLE") != nullptr;
    return counted;
}

void expand(const ListShape& list, std::uint32_t* out) {
    if (list.Pairs()) return;
    const auto* values = out + list.count;
    for (std::uint32_t index = 0; index < list.count; ++index) {
        const auto value = values[index];
        out[2 * std::size_t{index}] = list.mode + index;
        out[2 * std::size_t{index} + 1] = value;
    }
}

std::uint32_t* valuesOf(const ListShape& list, std::uint32_t* out) {
    return list.Pairs() ? out : out + list.count;
}

void copyList(const ListShape& list, std::uint32_t* out) {
    if (list.count == 0) return;
    const auto* source = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(list.address));
    GuestMemory::CheckRange(source, list.Bytes(), alignof(std::uint32_t));
    GuestMemory::CopyGuestBytes(valuesOf(list, out), source, list.Bytes());
    expand(list, out);
}

void readCounted(const ListShape& list, std::uint32_t* out) {
    if (list.count == 0) return;
    GuestMemory::Read(list.address, std::as_writable_bytes(std::span(valuesOf(list, out), list.Values())), alignof(std::uint32_t));
    expand(list, out);
}

void hookList(const ListShape& list) {
    if (list.count == 0) return;
    GuestMemory::GuardRead(list.address, list.Bytes());
    GuestMemory::FlushGpuWrites(list.address, list.Bytes());
}

std::string listError(std::uint32_t header, std::size_t cursor, const std::exception& error) {
    return "AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": " + error.what();
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

void store(LeanTails& tails, const ListShape& list, const std::uint32_t* pairs, std::uint64_t tracked, std::uint64_t epoch) {
    auto* entry = tails.cache.Find(list.address, list.mode, list.count);
    if (entry == nullptr) {
        bool evicted = false;
        entry = &tails.cache.Place(list.address, list.mode, list.count, evicted);
        entry->admitted = false;
        if (evicted) LeanFront::AddOnWorker(LeanFront::Counter::ListCacheEvictions);
    }
    entry->pairs.assign(pairs, pairs + list.Words());
    entry->tracked = tracked;
    entry->epoch = epoch;
}

void checkAnswer(const ListShape& list, std::span<const std::uint32_t> today, std::span<const std::uint32_t> lean, RegisterListCache::Entry* cached, std::uint32_t header, std::size_t cursor, std::uint64_t serial) {
    LeanFront::NoteChecked(LeanFront::Item::RegisterList, cached != nullptr);
    if (today.size() == lean.size() && std::equal(today.begin(), today.end(), lean.begin())) return;
    std::size_t at = 0;
    while (at < today.size() && at < lean.size() && today[at] == lean[at]) ++at;
    if (cached != nullptr) {
        const auto tracked = cached->tracked;
        cached->live = false;
        if (GuestMemory::CollectWritesUncached(list.address, list.Bytes()) != 0 && !GuestMemory::UnchangedSince(list.address, list.Bytes(), tracked)) {
            LeanFront::NoteAllowed(LeanFront::Allowed::ListsRewrittenInEpoch);
            return;
        }
    }
    auto detail = "serial " + std::to_string(serial) + " " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ", list " + hex(list.address) + " mode " + hex(list.mode) + " count " + std::to_string(list.count) + (cached != nullptr ? " (cache hit)" : " (read)");
    if (today.size() != lean.size()) detail += ": " + std::to_string(lean.size()) + " lean words, " + std::to_string(today.size()) + " today";
    else detail += ": first difference at word " + std::to_string(at) + ": lean " + hex(lean[at]) + ", today " + hex(today[at]);
    LeanFront::NoteDifference(cached != nullptr ? LeanFront::Field::ListCache : LeanFront::Field::RegisterLists, detail);
}

}

const RegisterListRef* RegisterLists::Find(std::size_t cursor) const {
    const auto found = std::lower_bound(refs.begin(), refs.end(), cursor, [](const RegisterListRef& ref, std::size_t at) { return ref.cursor < at; });
    return found != refs.end() && found->cursor == cursor ? &*found : nullptr;
}

const RegisterListRef* RegisterLists::Next(std::size_t cursor, std::size_t& hint) const {
    if (hint > refs.size() || (hint > 0 && refs[hint - 1].cursor >= cursor)) {
        hint = static_cast<std::size_t>(std::lower_bound(refs.begin(), refs.end(), cursor, [](const RegisterListRef& ref, std::size_t at) { return ref.cursor < at; }) - refs.begin());
    }
    while (hint < refs.size() && refs[hint].cursor < cursor) ++hint;
    return hint < refs.size() && refs[hint].cursor == cursor ? &refs[hint] : nullptr;
}

RegisterListView RegisterListAt(const Submission& submission, std::size_t cursor, std::size_t& hint) {
    if (!submission.leanLists) {
        const auto& listed = submission.registerLists.at(cursor);
        return {listed, Pm4::IndirectRegistersValid(listed)};
    }
    const auto* ref = submission.lists.Next(cursor, hint);
    if (ref == nullptr) throw std::out_of_range("AGC driver: no register list was read for the packet at DWORD " + std::to_string(cursor));
    return submission.lists.View(*ref);
}

std::optional<RegisterListView> FindRegisterList(const Submission& submission, std::size_t cursor) {
    if (!submission.leanLists) {
        const auto found = submission.registerLists.find(cursor);
        if (found == submission.registerLists.end()) return std::nullopt;
        return RegisterListView{found->second, Pm4::IndirectRegistersValid(found->second)};
    }
    const auto* ref = submission.lists.Find(cursor);
    if (ref == nullptr) return std::nullopt;
    return submission.lists.View(*ref);
}

std::size_t RegisterListCache::setOf(std::uint64_t address, std::uint32_t mode, std::uint32_t count) {
    auto mixed = (address >> 2u) ^ (static_cast<std::uint64_t>(mode) << 21u) ^ (static_cast<std::uint64_t>(count) << 47u);
    mixed *= 0x9e3779b97f4a7c15ull;
    return static_cast<std::size_t>(mixed >> 53u) % Sets;
}

RegisterListCache::Entry* RegisterListCache::Find(std::uint64_t address, std::uint32_t mode, std::uint32_t count) {
    if (entries.empty()) return nullptr;
    auto* set = entries.data() + setOf(address, mode, count) * Ways;
    for (std::size_t way = 0; way < Ways; ++way) {
        if (set[way].Is(address, mode, count)) {
            set[way].used = clock;
            return &set[way];
        }
    }
    return nullptr;
}

RegisterListCache::Entry& RegisterListCache::Place(std::uint64_t address, std::uint32_t mode, std::uint32_t count, bool& evicted) {
    if (entries.empty()) entries.resize(Sets * Ways);
    auto* set = entries.data() + setOf(address, mode, count) * Ways;
    Entry* chosen = nullptr;
    for (std::size_t way = 0; way < Ways && chosen == nullptr; ++way) {
        if (set[way].Is(address, mode, count)) chosen = &set[way];
    }
    for (std::size_t way = 0; way < Ways && chosen == nullptr; ++way) {
        if (!set[way].live) chosen = &set[way];
    }
    evicted = chosen == nullptr;
    if (chosen == nullptr) {
        chosen = set;
        for (std::size_t way = 1; way < Ways; ++way) {
            if (set[way].used < chosen->used) chosen = &set[way];
        }
    }
    chosen->address = address;
    chosen->mode = mode;
    chosen->count = count;
    chosen->used = clock;
    chosen->live = true;
    return *chosen;
}

bool RegisterListCache::Volatile(std::uint64_t address) const {
    if (blocks.empty()) return false;
    const auto block = address >> 16u;
    const auto& slot = blocks[static_cast<std::size_t>((block * 0x9e3779b97f4a7c15ull) >> 52u) % BlockSlots];
    return slot.block == block && slot.declines >= VolatileDeclines;
}

void RegisterListCache::Decline(std::uint64_t address, std::uint32_t weight) {
    if (blocks.empty()) blocks.resize(BlockSlots);
    const auto block = address >> 16u;
    auto& slot = blocks[static_cast<std::size_t>((block * 0x9e3779b97f4a7c15ull) >> 52u) % BlockSlots];
    if (slot.block != block) slot = Block{block, 0};
    slot.declines = std::min(VolatileDeclines, slot.declines + weight);
}

void RegisterListCache::Tick() {
    ++clock;
    if (++declinesSince < DeclineWindow) return;
    declinesSince = 0;
    std::fill(blocks.begin(), blocks.end(), Block{});
}

void RegisterListCache::Clear() {
    for (auto& entry : entries) {
        entry.live = false;
        entry.admitted = false;
    }
    std::fill(blocks.begin(), blocks.end(), Block{});
    declinesSince = 0;
}

void Driver::resolveLeanLists(Submission& submission, bool cacheable) {
    using LeanFront::Counter;
    if (cacheable && LeanFront::SwitchSet(frontLatch, Graphics::RuntimeSwitch::LeanListDirect)) {
        cacheable = false;
        LeanFront::AddOnWorker(Counter::DirectListTails);
    }
    auto& lists = submission.lists;
    lists = leanTails.TakeLists();
    submission.leanLists = true;
    auto& cache = leanTails.cache;
    auto& hits = leanTails.hits;
    hits.clear();
    if (cacheable) cache.Tick();
    const auto epoch = cacheable ? GuestMemory::ThreadCollectEpoch() : 0;
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Registers);
    const auto& commands = submission.commands;
    std::uint64_t resolved = 0;
    for (std::size_t cursor = 0; cursor < commands.size(); cursor += Pm4::PacketWords(commands[cursor])) {
        const auto header = commands[cursor];
        if ((header >> 30u) != 3u || !Pm4::IndirectRegisterOpcode((header >> 8u) & 0xffu)) continue;
        const auto packet = std::span<const std::uint32_t>(commands).subspan(cursor, Pm4::PacketWords(header));
        ++resolved;
        try {
            const auto first = lists.words.size();
            const auto shape = leanShape(packet);
            if (!shape) {
                const auto pairs = Pm4::ReadIndirectRegisters(packet);
                lists.words.insert(lists.words.end(), pairs.begin(), pairs.end());
                lists.refs.push_back({static_cast<std::uint32_t>(cursor), static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(pairs.size() / 2), Pm4::IndirectRegistersValid(pairs) ? RegisterLists::Valid : 0u});
                continue;
            }
            const bool candidate = cacheable && RegisterListCache::Cacheable(shape->count);
            const bool bypass = candidate && cache.Volatile(shape->address);
            auto* entry = candidate && !bypass ? cache.Find(shape->address, shape->mode, shape->count) : nullptr;
            if (LeanFront::Due(frontLatch, LeanFront::Item::RegisterList)) {
                const auto tracked = candidate && !bypass ? GuestMemory::CollectWrites(shape->address, shape->Bytes()) : 0;
                const auto pairs = Pm4::ReadIndirectRegisters(packet);
                const bool valid = Pm4::IndirectRegistersValid(pairs);
                lists.words.insert(lists.words.end(), pairs.begin(), pairs.end());
                lists.refs.push_back({static_cast<std::uint32_t>(cursor), static_cast<std::uint32_t>(first), shape->count, valid ? RegisterLists::Valid : 0u});
                auto& lean = leanTails.scratch;
                lean.assign(shape->Words(), 0);
                RegisterListCache::Entry* cached = nullptr;
                if (entry != nullptr && tracked == 0) entry->live = false;
                if (entry != nullptr && tracked != 0) {
                    entry->epoch = epoch;
                    if (GuestMemory::UnchangedSince(shape->address, shape->Bytes(), entry->tracked)) {
                        std::copy(entry->pairs.begin(), entry->pairs.end(), lean.begin());
                        cached = entry;
                    }
                }
                bool answered = true;
                try {
                    if (cached == nullptr) copyList(*shape, lean.data());
                } catch (const std::exception& error) {
                    answered = false;
                    LeanFront::NoteChecked(LeanFront::Item::RegisterList);
                    LeanFront::NoteDifference(LeanFront::Field::RegisterLists, "serial " + std::to_string(submission.serial) + " " + listError(header, cursor, error) + " in the lean read after today's read succeeded");
                }
                if (answered) checkAnswer(*shape, pairs, lean, cached, header, cursor, submission.serial);
                if (tracked != 0 && valid) store(leanTails, *shape, pairs.data(), tracked, epoch);
                else if (candidate && !bypass && tracked == 0) cache.Decline(shape->address, RegisterListCache::VolatileDeclines);
                continue;
            }
            lists.words.resize(first + shape->Words());
            auto* out = lists.words.data() + first;
            std::uint32_t flags = 0;
            if (shape->count == 0) {
                flags = RegisterLists::Valid;
            } else if (entry != nullptr) {
                hookList(*shape);
                if (entry->epoch != epoch) {
                    if (GuestMemory::CollectWrites(shape->address, shape->Bytes()) == 0) entry->live = false;
                    else entry->epoch = epoch;
                }
                if (entry->live) {
                    std::copy(entry->pairs.begin(), entry->pairs.end(), out);
                    hits.push_back({static_cast<std::uint32_t>(lists.refs.size()), static_cast<std::uint32_t>(cursor), shape->address, shape->mode, shape->count, entry->tracked, entry});
                    flags = RegisterLists::Valid | RegisterLists::Cached;
                } else {
                    cache.Decline(shape->address, RegisterListCache::VolatileDeclines);
                    copyList(*shape, out);
                    flags = Pm4::IndirectRegistersValid(std::span<const std::uint32_t>(out, shape->Words())) ? RegisterLists::Valid : 0u;
                    LeanFront::AddOnWorker(Counter::ListCacheMisses);
                }
            } else {
                std::uint64_t tracked = 0;
                if (countedReads()) {
                    if (candidate && !bypass) tracked = GuestMemory::CollectWrites(shape->address, shape->Bytes());
                    readCounted(*shape, out);
                } else {
                    hookList(*shape);
                    if (candidate && !bypass) tracked = GuestMemory::CollectWrites(shape->address, shape->Bytes());
                    copyList(*shape, out);
                }
                const bool valid = Pm4::IndirectRegistersValid(std::span<const std::uint32_t>(out, shape->Words()));
                flags = valid ? RegisterLists::Valid : 0u;
                if (bypass) {
                    LeanFront::AddOnWorker(Counter::ListCacheBypassed);
                } else if (candidate) {
                    LeanFront::AddOnWorker(Counter::ListCacheMisses);
                    if (tracked != 0 && valid) store(leanTails, *shape, out, tracked, epoch);
                    else if (tracked == 0) cache.Decline(shape->address, RegisterListCache::VolatileDeclines);
                }
            }
            lists.refs.push_back({static_cast<std::uint32_t>(cursor), static_cast<std::uint32_t>(first), shape->count, flags});
        } catch (const std::exception& error) {
            throw std::runtime_error(listError(header, cursor, error));
        }
    }
    LeanFront::AddOnWorker(Counter::LeanLists, resolved);
    if (!hits.empty()) settleListHits(submission);
}

void Driver::settleListHits(Submission& submission) {
    using LeanFront::Counter;
    auto& hits = leanTails.hits;
    auto& queries = leanTails.queries;
    queries.clear();
    for (const auto& hit : hits) queries.push_back({hit.address, ListShape{hit.address, hit.mode, hit.count}.Bytes(), hit.tracked});
    const bool all = GuestMemory::UnchangedSinceAll(queries);
    const auto epoch = GuestMemory::ThreadCollectEpoch();
    auto& lists = submission.lists;
    for (const auto& hit : hits) {
        const ListShape shape{hit.address, hit.mode, hit.count};
        auto* entry = hit.entry != nullptr && hit.entry->Is(hit.address, hit.mode, hit.count) && hit.entry->tracked == hit.tracked ? hit.entry : nullptr;
        if (all || GuestMemory::UnchangedSince(hit.address, shape.Bytes(), hit.tracked)) {
            LeanFront::AddOnWorker(Counter::ListCacheHits);
            if (entry != nullptr && !entry->admitted) {
                entry->admitted = true;
                LeanFront::AddOnWorker(Counter::ListCacheAdmits);
            }
            continue;
        }
        LeanFront::AddOnWorker(Counter::ListCacheInvalidations);
        leanTails.cache.Decline(hit.address);
        auto& ref = lists.refs[hit.ref];
        auto* out = lists.words.data() + ref.first;
        std::uint64_t tracked = 0;
        try {
            tracked = GuestMemory::CollectWrites(hit.address, shape.Bytes());
            copyList(shape, out);
        } catch (const std::exception& error) {
            throw std::runtime_error(listError(submission.commands[hit.cursor], hit.cursor, error));
        }
        const bool valid = Pm4::IndirectRegistersValid(std::span<const std::uint32_t>(out, shape.Words()));
        ref.flags = valid ? RegisterLists::Valid : 0u;
        if (entry == nullptr) continue;
        if (tracked == 0 || !valid) {
            entry->live = false;
            continue;
        }
        entry->pairs.assign(out, out + shape.Words());
        entry->tracked = tracked;
        entry->epoch = epoch;
        entry->admitted = false;
    }
    hits.clear();
}

void Driver::verifyLeanLists(const Submission& submission, bool cacheable) {
    auto& cache = leanTails.cache;
    const auto epoch = cacheable ? GuestMemory::ThreadCollectEpoch() : 0;
    const auto& commands = submission.commands;
    for (std::size_t cursor = 0; cursor < commands.size(); cursor += Pm4::PacketWords(commands[cursor])) {
        const auto header = commands[cursor];
        if ((header >> 30u) != 3u || !Pm4::IndirectRegisterOpcode((header >> 8u) & 0xffu)) continue;
        const auto packet = std::span<const std::uint32_t>(commands).subspan(cursor, Pm4::PacketWords(header));
        const auto shape = leanShape(packet);
        const auto found = submission.registerLists.find(cursor);
        if (!shape || found == submission.registerLists.end()) continue;
        const bool candidate = cacheable && RegisterListCache::Cacheable(shape->count);
        auto* entry = candidate && !cache.Volatile(shape->address) ? cache.Find(shape->address, shape->mode, shape->count) : nullptr;
        auto& lean = leanTails.scratch;
        lean.assign(shape->Words(), 0);
        RegisterListCache::Entry* cached = nullptr;
        try {
            if (entry != nullptr && (entry->epoch == epoch || GuestMemory::CollectWrites(shape->address, shape->Bytes()) != 0)) {
                entry->epoch = epoch;
                if (GuestMemory::UnchangedSince(shape->address, shape->Bytes(), entry->tracked)) {
                    std::copy(entry->pairs.begin(), entry->pairs.end(), lean.begin());
                    cached = entry;
                }
            }
            if (cached == nullptr) copyList(*shape, lean.data());
        } catch (const std::exception& error) {
            LeanFront::NoteDifference(LeanFront::Field::RegisterLists, "serial " + std::to_string(submission.serial) + " " + listError(header, cursor, error) + " in the lean read after today's read succeeded");
            continue;
        }
        checkAnswer(*shape, found->second, lean, cached, header, cursor, submission.serial);
    }
}

}
