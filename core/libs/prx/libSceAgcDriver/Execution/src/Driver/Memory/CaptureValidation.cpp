#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

bool Driver::captureStable(std::span<const ShaderRecompiler::MemoryRegion> captured) {
    using Policy = ShaderMemory::PendingWrite;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::uint64_t ValidateCounters::*reason = &ValidateCounters::syncedOff;
    bool stable = true;
    std::uint64_t rawRegions = 0, mismatches = 0;
    double waited = 0;
    PendingView pending;
    pending.Load();
    thread_local std::vector<std::byte>* knownSlot = nullptr;
    auto& known = ShaderRecompiler::ThreadOwned(knownSlot);
    for (const auto& region : captured) {
        if (!stable) break;
        known.resize(region.bytes.size());
        const auto policy = validateSkipEnabled() ? classifyPendingWrite(region.guestAddress, region.bytes.size(), reason, pending, known) : Policy::Sync;
        if (policy == Policy::KnownValue || policy == Policy::VerifyKnownValue) {
            stable = std::memcmp(known.data(), region.bytes.data(), known.size()) == 0;
            if (policy != Policy::VerifyKnownValue) continue;
            const bool synced = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
            knownValueVerified.fetch_add(1, std::memory_order_relaxed);
            if (synced != stable) knownValueMismatches.fetch_add(1, std::memory_order_relaxed);
            stable = synced;
            continue;
        }
        if (policy != Policy::Raw && policy != Policy::VerifyRaw) {
            stable = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
            continue;
        }
        ++rawRegions;
        stable = GuestMemory::EqualsCommittedUnsynced(region.guestAddress, region.bytes);
        if (policy != Policy::VerifyRaw) continue;
        const auto start = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
        const bool synced = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
        if (profile) waited += Graphics::Recorder::ThreadWaitedMs() - start;
        if (synced != stable) ++mismatches;
        stable = synced;
    }
    if (!profile || rawRegions == 0) return stable;
    std::lock_guard lock(validateMutex);
    auto& counters = validateCounters;
    ++counters.pending;
    if (validateSkipVerify()) {
        ++counters.verified;
        counters.verifiedWaitMs += waited;
        counters.mismatches += mismatches;
    } else {
        ++counters.skipped;
    }
    return stable;
}

bool Driver::validateCaptured(std::uint64_t program, std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> captured, const ShaderRecompiler::RecompileResult& compiled, bool inPlace, const PendingView& view, bool* unmapped, std::optional<SampledReadScope>* sampling, const DataMask* data) {
    using Policy = ShaderMemory::PendingWrite;

    thread_local std::vector<std::size_t>* pendingSlot = nullptr;
    auto& pending = ShaderRecompiler::ThreadOwned(pendingSlot);
    pending.clear();
    for (std::size_t i = 0; i < captured.size(); ++i) {
        const auto& region = captured[i];
        if (view.Overlaps(region.guestAddress, region.bytes.size())) pending.push_back(i);
    }

    const auto compareMasked = [&](const ShaderRecompiler::MemoryRegion& region, std::size_t first) {
        thread_local std::vector<std::byte>* liveSlot = nullptr;
        auto& live = ShaderRecompiler::ThreadOwned(liveSlot);
        live.resize(region.bytes.size());
        const auto copied = GuestMemory::CopyMapped(region.guestAddress, live);
        if (copied != GuestMemory::Compare::Equal) return copied;
        const auto count = region.bytes.size() / sizeof(std::uint32_t);
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t stored = 0, fresh = 0;
            std::memcpy(&stored, region.bytes.data() + i * sizeof(std::uint32_t), sizeof(stored));
            std::memcpy(&fresh, live.data() + i * sizeof(std::uint32_t), sizeof(fresh));
            if (stored == fresh) continue;
            const auto position = static_cast<std::uint32_t>(first + i);
            if (!std::binary_search(data->positions.begin(), data->positions.end(), position)) return GuestMemory::Compare::Differs;
            data->live->emplace_back(position, fresh);
        }
        return GuestMemory::Compare::Equal;
    };

    static constexpr std::size_t NoKnownValue = std::numeric_limits<std::size_t>::max();
    thread_local std::vector<Policy>* policiesSlot = nullptr;
    thread_local std::vector<std::size_t>* knownOffsetsSlot = nullptr;
    thread_local std::vector<std::byte>* knownBytesSlot = nullptr;
    auto& policies = ShaderRecompiler::ThreadOwned(policiesSlot);
    auto& knownOffsets = ShaderRecompiler::ThreadOwned(knownOffsetsSlot);
    auto& knownBytes = ShaderRecompiler::ThreadOwned(knownBytesSlot);
    policies.assign(pending.size(), Policy::None);
    knownOffsets.assign(pending.size(), NoKnownValue);
    knownBytes.clear();
    std::uint64_t knownServed = 0, knownMismatches = 0;
    std::vector<std::size_t> observable;
    std::size_t reachedPending = 0;
    std::uint64_t hookWaitsAtCopy = 0;
    const auto compare = [&](bool rawPending) {
        std::size_t next = 0;
        std::size_t first = 0;
        observable.clear();
        reachedPending = 0;
        if (data != nullptr) data->live->clear();
        for (std::size_t i = 0; i < captured.size(); ++i) {
            const auto& region = captured[i];
            const bool isPending = next < pending.size() && pending[next] == i;
            if (isPending) ++next;
            const bool unsynced = isPending ? rawPending : inPlace;
            const bool known = isPending && knownOffsets[next - 1] != NoKnownValue;
            bool same = false;
            bool masked = false;
            if (known) {
                same = std::memcmp(knownBytes.data() + knownOffsets[next - 1], region.bytes.data(), region.bytes.size()) == 0;
                if (!rawPending && policies[next - 1] == Policy::VerifyKnownValue) {
                    const bool synced = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                    if (synced != same) ++knownMismatches;
                    same = synced;
                }
            } else if (!unsynced) same = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
            else if (validateLegacy()) same = GuestMemory::EqualsCommittedUnsynced(region.guestAddress, region.bytes);
            else {
                auto outcome = GuestMemory::CompareMapped(region.guestAddress, region.bytes);
                masked = !isPending && data != nullptr;
                if (outcome == GuestMemory::Compare::Differs && masked) outcome = compareMasked(region, first);
                if (outcome == GuestMemory::Compare::Unmapped && unmapped != nullptr) *unmapped = true;
                same = outcome == GuestMemory::Compare::Equal;
            }
            first += region.bytes.size() / sizeof(std::uint32_t);
            if (isPending && !rawPending && !known) {
                ++reachedPending;
                if (Graphics::Recorder::ThreadHookWaits() != hookWaitsAtCopy) observable.push_back(next - 1);
            }
            if (!same) {
                if (data != nullptr && !masked) dataPendingMisses.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }
        return true;
    };
    if (pending.empty()) return compare(false);
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const bool verify = validateSkipVerify();
    std::optional<SampledReadScope> ownSampling;
    auto& sample = sampling != nullptr ? *sampling : ownSampling;
    if (!sample) sample.emplace(siteSampling() ? evidenceValidations : evidenceReads);
    std::uint64_t ValidateCounters::*reason = &ValidateCounters::syncedOff;
    bool skip = validateSkipEnabled();

    for (std::size_t i = 0; validateSkipEnabled() && i < pending.size(); ++i) {
        const auto& region = captured[pending[i]];
        std::uint64_t ValidateCounters::*regionReason = &ValidateCounters::syncedOff;
        const auto offset = knownBytes.size();
        knownBytes.resize(offset + region.bytes.size());
        auto policy = classifyPendingWrite(region.guestAddress, region.bytes.size(), regionReason, view, std::span(knownBytes).subspan(offset));
        const bool known = policy == Policy::KnownValue || policy == Policy::VerifyKnownValue;
        if (!known) knownBytes.resize(offset);
        if (policy != Policy::Sync && policy != Policy::None) {

            bool published = false;
            const bool stored = Graphics::StorageTexture::FlushPending(region.guestAddress, region.bytes.size(), nullptr, "memory access", Graphics::PublishScope::Whole, &published);
            if (stored || published) {
                regionReason = &ValidateCounters::syncedImage;
                policy = Policy::Sync;
                if (known) knownBytes.resize(offset);
            } else if (known) {
                knownOffsets[i] = offset;
                ++knownServed;
            }
        }
        policies[i] = policy;
        if (policy == Policy::Sync && skip) {
            skip = false;
            reason = regionReason;
        }
    }

    const bool rawSame = skip ? compare(true) : false;
    const bool unsyncedMiss = skip && !rawSame;
    if (unsyncedMiss) skip = false;
    const bool verifyKnown = knownValueVerify() && knownServed != 0;
    double waited = 0;
    bool same = rawSame;
    if (!(skip || unsyncedMiss) || verify || verifyKnown) {

        std::vector<std::vector<std::byte>> before;
        if (writeEvidenceEnabled()) {
            before.reserve(pending.size());
            for (const auto index : pending) {
                const auto& region = captured[index];
                std::vector<std::byte> copy;
                if (GuestMemory::Accessible(reinterpret_cast<const void*>(region.guestAddress), region.bytes.size())) {
                    copy.resize(region.bytes.size());
                    std::memcpy(copy.data(), reinterpret_cast<const void*>(region.guestAddress), copy.size());
                }
                before.push_back(std::move(copy));
            }
        }
        const auto start = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
        hookWaitsAtCopy = Graphics::Recorder::ThreadHookWaits();
        same = compare(false);
        if (profile) waited = Graphics::Recorder::ThreadWaitedMs() - start;
        if (!observationGuard()) {
            for (std::size_t i = 0; i < before.size(); ++i) observeRange(captured[pending[i]].guestAddress, before[i]);
        } else if (!before.empty()) {
            for (const auto index : observable) observeRange(captured[pending[index]].guestAddress, before[index]);
            observationsNoWait.fetch_add(reachedPending - observable.size(), std::memory_order_relaxed);
            observationsNotReached.fetch_add(pending.size() - knownServed - reachedPending, std::memory_order_relaxed);
        }
    }
    if (verifyKnown) {
        knownValueVerified.fetch_add(knownServed, std::memory_order_relaxed);
        knownValueMismatches.fetch_add(knownMismatches, std::memory_order_relaxed);
    }
    if (profile) {
        std::lock_guard lock(validateMutex);
        auto& counters = validateCounters;
        ++counters.pending;
        counters.knownValue += knownServed;
        if (verify && (skip || unsyncedMiss)) {
            ++counters.verified;
            counters.verifiedWaitMs += waited;
            if (skip && !same) ++counters.mismatches;
            if (unsyncedMiss && same) ++counters.verifiedMissesHit;
        } else if (unsyncedMiss) {
            ++counters.unsyncedMisses;
        } else if (skip) {
            ++counters.skipped;
        } else {
            ++(counters.*reason);
            counters.syncedWaitMs += waited;
        }
        if (std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) reportValidation(counters);
    }
    if (traceCapSync() && traceBudget()) {
        const char* outcome = verify && (skip || unsyncedMiss) ? "verify" : unsyncedMiss ? "missed raw" : skip ? "skipped" : reason == &ValidateCounters::syncedEvidence ? "synced (evidence short)" : reason == &ValidateCounters::syncedSample ? "synced (sampled)" : reason == &ValidateCounters::syncedNoWriter ? "synced (no dispatch writer)" : reason == &ValidateCounters::syncedForeign ? "synced (foreign writer)" : reason == &ValidateCounters::syncedWriterChanged ? "synced (writer changed)" : reason == &ValidateCounters::syncedLargeRange ? "synced (larger range)" : reason == &ValidateCounters::syncedLabel ? "synced (label)" : reason == &ValidateCounters::syncedImage ? "synced (image)" : reason == &ValidateCounters::syncedShadow ? "synced (shadow)" : "synced (off)";
        for (const auto index : pending) {
            const auto& region = captured[index];
            const auto begin = region.guestAddress;
            const auto end = begin + region.bytes.size();
            std::fprintf(stderr, "[capsync] dispatch-cache q0x%x program 0x%llx region 0x%llx+0x%zx (%zu of %zu pending) %s: waited %.1f ms, same %d raw %d;%s; writers:%s\n", queue, static_cast<unsigned long long>(program), static_cast<unsigned long long>(begin), region.bytes.size(), pending.size(), captured.size(), outcome, waited, same ? 1 : 0, rawSame ? 1 : 0, describeSelf(compiled, begin, end).c_str(), describeWriters(begin, end).c_str());
        }
    }
    if (verify || verifyKnown) return same;
    return skip || unsyncedMiss ? rawSame : same;
}

}
