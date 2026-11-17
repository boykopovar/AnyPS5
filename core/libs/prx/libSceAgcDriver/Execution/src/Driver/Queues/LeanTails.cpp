#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanTails.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/WaitMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <string>
#include <utility>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::size_t SpareLimit = 512;
constexpr std::size_t SpareWordsLimit = std::size_t{1} << 20u;
constexpr std::size_t SpareBytesLimit = std::size_t{64} << 20u;

bool listCacheEnabled() {
    static const bool enabled = std::getenv("APS5_LEAN_NO_LIST_CACHE") == nullptr && std::getenv("APS5_SUBMISSION_EPOCH_EACH") == nullptr && !APS5_ENABLE_TIMING_LOG && GuestMemory::WriteWatched();
    return enabled;
}

std::uint32_t controlWord(const std::uint32_t* tail) {
    return std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(tail - 1)).load(std::memory_order_relaxed);
}

std::size_t commandBytes(const std::vector<std::uint32_t>& commands) {
    return commands.capacity() * sizeof(std::uint32_t);
}

std::size_t listBytes(const RegisterLists& lists) {
    return lists.words.capacity() * sizeof(std::uint32_t) + lists.refs.capacity() * sizeof(RegisterListRef);
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::string conditionalText(const std::map<std::size_t, std::size_t>& ends) {
    std::string text;
    for (const auto& [from, to] : ends) {
        if (text.size() > 96) {
            text += " ...";
            break;
        }
        text += (text.empty() ? "" : " ") + std::to_string(from) + "->" + std::to_string(to);
    }
    return text.empty() ? std::string("none") : text;
}

void compareKeptTail(const Submission& kept, const Submission& fresh, const Submission& stalled) {
    const auto where = "serial " + std::to_string(stalled.serial) + " tail " + hex(reinterpret_cast<std::uintptr_t>(stalled.rewindTail));
    const auto& left = kept.commands;
    const auto& right = fresh.commands;
    const auto masked = [&](std::size_t index, std::uint32_t value) { return fresh.rewindTail != nullptr && index + 1 == right.size() ? value & ~Pm4::RewindValid : value; };
    std::size_t differs = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < std::min(left.size(), right.size()); ++index) {
        if (masked(index, left[index]) != masked(index, right[index])) {
            differs = index;
            break;
        }
    }
    if (left.size() != right.size() || differs != left.size()) {
        auto detail = where + ": the copy kept at the header has " + std::to_string(left.size()) + " dwords, today's copy " + std::to_string(right.size());
        if (differs < left.size() && differs < right.size()) detail += "; first difference at dword " + std::to_string(differs) + ": kept " + hex(left[differs]) + ", today " + hex(right[differs]);
        LeanFront::NoteDifference(LeanFront::Field::SegmentWords, detail);
    }
    if (kept.conditionalEnds != fresh.conditionalEnds) LeanFront::NoteDifference(LeanFront::Field::ConditionalEnds, where + ": kept " + conditionalText(kept.conditionalEnds) + ", today " + conditionalText(fresh.conditionalEnds));
    const auto sameSegments = std::equal(kept.indirectSegments.begin(), kept.indirectSegments.end(), fresh.indirectSegments.begin(), fresh.indirectSegments.end(), [](const Submission::IndirectSegment& a, const Submission::IndirectSegment& b) { return a.start == b.start && a.target == b.target && a.words == b.words; });
    if (!sameSegments) LeanFront::NoteDifference(LeanFront::Field::SegmentWords, where + ": kept " + std::to_string(kept.indirectSegments.size()) + " indirect segments, today " + std::to_string(fresh.indirectSegments.size()));
    if (kept.rewindTail != fresh.rewindTail || kept.rewindWords != fresh.rewindWords) {
        LeanFront::NoteDifference(LeanFront::Field::RewindTail, where + ": kept next tail " + hex(reinterpret_cast<std::uintptr_t>(kept.rewindTail)) + " with " + std::to_string(kept.rewindWords) + " dwords, today " + hex(reinterpret_cast<std::uintptr_t>(fresh.rewindTail)) + " with " + std::to_string(fresh.rewindWords));
    }
}

}

KeptTail& LeanTails::Keep() {
    if (keptFront == keptEnd) keptFront = keptEnd = 0;
    if (keptEnd == kept.size()) kept.emplace_back();
    auto& slot = kept[keptEnd++];
    if (slot.copy.commands.capacity() == 0 || slot.copy.commands.capacity() > SpareWordsLimit) slot.copy.commands = TakeCommands();
    slot.copy.rewindTail = nullptr;
    slot.copy.rewindWords = 0;
    slot.sources.clear();
    return slot;
}

void LeanTails::Unkeep() {
    if (keptEnd != keptFront) --keptEnd;
}

KeptTail* LeanTails::Front(const std::uint32_t* control) {
    if (keptFront == keptEnd || kept[keptFront].control != control) return nullptr;
    return &kept[keptFront];
}

void LeanTails::Pop() {
    if (keptFront == keptEnd) return;
    if (++keptFront == keptEnd) keptFront = keptEnd = 0;
}

void LeanTails::ClearKept() {
    keptFront = keptEnd = 0;
}

void LeanTails::TrimKept() {
    for (std::size_t index = 0; index < kept.size(); ++index) {
        auto& commands = kept[index].copy.commands;
        if ((index >= keptFront && index < keptEnd) || commands.capacity() == 0) continue;
        GiveCommands(std::move(commands));
        std::vector<std::uint32_t>().swap(commands);
    }
}

std::vector<std::uint32_t> LeanTails::TakeCommands() {
    if (spareCommands.empty()) return {};
    auto commands = std::move(spareCommands.back());
    spareCommands.pop_back();
    spareBytes -= commandBytes(commands);
    return commands;
}

void LeanTails::GiveCommands(std::vector<std::uint32_t>&& commands) {
    const auto bytes = commandBytes(commands);
    if (commands.capacity() == 0 || commands.capacity() > SpareWordsLimit || spareCommands.size() >= SpareLimit || spareBytes + bytes > SpareBytesLimit) return;
    commands.clear();
    spareBytes += bytes;
    spareCommands.push_back(std::move(commands));
}

RegisterLists LeanTails::TakeLists() {
    if (spareLists.empty()) return {};
    auto lists = std::move(spareLists.back());
    spareLists.pop_back();
    spareBytes -= listBytes(lists);
    return lists;
}

void LeanTails::GiveLists(RegisterLists&& lists) {
    const auto bytes = listBytes(lists);
    if (lists.words.capacity() > SpareWordsLimit || spareLists.size() >= SpareLimit || spareBytes + bytes > SpareBytesLimit) return;
    lists.Clear();
    spareBytes += bytes;
    spareLists.push_back(std::move(lists));
}

void Driver::keepReleasedTails(const Submission& submission) {
    auto& released = releasedTails();
    const auto tracked = GuestMemory::TrackerGeneration();
    const auto forgets = GuestMemory::ForgetSerial();
    const std::uint32_t* tail = submission.rewindTail;
    std::size_t words = submission.rewindWords;
    for (std::size_t chunk = 0; chunk < 256 && tail != nullptr; ++chunk) {
        if ((std::atomic_ref<std::uint32_t>(*const_cast<std::uint32_t*>(tail - 1)).load(std::memory_order_acquire) & 0x80000000u) == 0) break;
        released.push_back(tail - 1);
        auto& kept = leanTails.Keep();
        kept.control = tail - 1;
        kept.words = words;
        kept.tracked = tracked;
        kept.forgets = forgets;
        kept.stamp = frontLatch.generation;
        try {
            copyCommands(kept.copy, tail, words, &kept.sources);
        } catch (const std::exception&) {
            leanTails.Unkeep();
            break;
        }
        tail = kept.copy.rewindTail;
        words = kept.copy.rewindWords;
    }
    leanTails.TrimKept();
}

bool Driver::keptTailCurrent(const KeptTail& kept, const Submission& stalled) const {
    if (kept.stamp != frontLatch.generation || kept.words != stalled.rewindWords) return false;
    for (const auto& [address, bytes] : kept.sources) {
        if (GuestMemory::ForgetStamp(address, static_cast<std::size_t>(bytes)) > kept.forgets) return false;
    }
    return !GuestMemory::DriverStoredOver(kept.sources, kept.tracked);
}

void Driver::recycleLeanTail(Submission& tail) {
    if (tail.queue != 0) return;
    leanTails.GiveCommands(std::move(tail.commands));
    if (tail.leanLists) leanTails.GiveLists(std::move(tail.lists));
}

void Driver::prepareLeanTail(const Submission& stalled, Submission& tail, bool popped, bool verify, PacketTable* table) {
    using LeanFront::Counter;
    const auto control = stalled.rewindTail - 1;
    if (frontLatch.Renew(leanTails.cache.stamp)) leanTails.cache.Clear();
    KeptTail* kept = nullptr;
    if (leanTails.Kept()) {
        kept = leanTails.Front(control);
        if (verify && (popped ? kept == nullptr : kept != nullptr)) {
            LeanFront::NoteDifference(LeanFront::Field::ReleasedTails, "serial " + std::to_string(stalled.serial) + " tail " + hex(reinterpret_cast<std::uintptr_t>(stalled.rewindTail)) + ": the released tails " + (popped ? "list it next" : "do not list it next") + ", the kept copies " + (kept != nullptr ? "do" : "do not"));
        }
        if (!popped || kept == nullptr) {
            leanTails.ClearKept();
            kept = nullptr;
        }
    }
    const bool current = kept != nullptr && keptTailCurrent(*kept, stalled);
    if (kept == nullptr) LeanFront::AddOnWorker(Counter::PreparedCopiesMissed);
    else if (!current) LeanFront::AddOnWorker(Counter::PreparedCopiesRefused);
    if (current && !verify) {
        std::swap(tail.commands, kept->copy.commands);
        tail.conditionalEnds = std::move(kept->copy.conditionalEnds);
        kept->copy.conditionalEnds.clear();
        tail.indirectSegments = std::move(kept->copy.indirectSegments);
        kept->copy.indirectSegments.clear();
        tail.rewindTail = kept->copy.rewindTail;
        tail.rewindWords = kept->copy.rewindWords;
        if (tail.rewindTail != nullptr && !tail.commands.empty()) tail.commands.back() = controlWord(tail.rewindTail);
        LeanFront::AddOnWorker(Counter::PreparedCopiesUsed);
    } else {
        tail.commands = leanTails.TakeCommands();
        copyCommands(tail, stalled.rewindTail, stalled.rewindWords);
        if (verify) {
            LeanFront::NoteChecked(LeanFront::Item::Segment);
            if (current) compareKeptTail(kept->copy, tail, stalled);
        }
    }
    if (kept != nullptr) leanTails.Pop();
    if (APS5_ENABLE_TIMING_LOG) tail.copiedAt = std::chrono::steady_clock::now();
    if (table != nullptr && !verify) {
        scanSegment(tail, *table, true, stalled.rewindTail);
        LeanFront::AddOnWorker(Counter::PacketTables);
    } else {
        validate(tail, stalled.rewindTail);
    }
    const bool cacheable = stalled.queue == 0 && listCacheEnabled();
    if (cacheable && !popped) {
        noteReleasedTails(tail, FrontMode::Lean);
        bumpEpoch(&EpochBumps::submissions);
        tail.enqueuedAt = lastEpochBump();
        LeanFront::AddOnWorker(Counter::LateTailHoists);
    }
    const bool cache = cacheable && GuestMemory::ThreadCollectEpoch() != 0;
    if (verify) {
        readRegisterLists(tail);
        verifyLeanLists(tail, cache);
    } else {
        resolveLeanLists(tail, cache);
    }
}

}
