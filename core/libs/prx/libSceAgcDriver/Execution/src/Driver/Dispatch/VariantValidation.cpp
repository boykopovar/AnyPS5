#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

void Driver::appendEntryRegions(const DispatchVariant& variant, std::vector<ShaderRecompiler::MemoryRegion>& regions, const std::vector<std::uint32_t>* words) {
    const auto& source = words != nullptr ? *words : variant.words;
    regions.reserve(regions.size() + variant.runs.size());
    std::size_t offset = 0;
    for (const auto& [begin, end] : variant.runs) {
        const auto count = static_cast<std::size_t>((end - begin) / sizeof(std::uint32_t));
        regions.push_back({begin, std::as_bytes(std::span<const std::uint32_t>(source).subspan(offset, count))});
        offset += count;
    }
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> Driver::patchDataWords(const DispatchVariant& variant, const std::vector<std::uint32_t>& words) {
    auto patched = std::make_shared<ShaderRecompiler::RecompileResult>(*variant.compiled);
    auto& descriptor = patched->bindings[variant.flatBinding].guestDescriptor;
    for (std::size_t k = 0; k < variant.dataPositions.size(); ++k) {
        if (variant.dataSlots[k] < descriptor.size()) descriptor[variant.dataSlots[k]] = words[variant.dataPositions[k]];
    }
    return patched;
}

bool Driver::syncPendingRuns(std::uint64_t program, std::uint32_t queue, const ShaderRecompiler::RecompileResult& compiled, std::span<const ShaderRecompiler::MemoryRegion> regions, std::uint64_t& synced, PendingView& pending, std::optional<SampledReadScope>& sampling, std::span<const VouchedRegions> skipped) {
    using Policy = ShaderMemory::PendingWrite;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    for (std::size_t index = 0, range = 0; index < regions.size(); ++index) {
        if (range < skipped.size() && index == skipped[range].begin) {
            index = skipped[range++].end - 1u;
            continue;
        }
        const auto& region = regions[index];
        if (!pending.Overlaps(region.guestAddress, region.bytes.size())) continue;
        if (!sampling) sampling.emplace(siteSampling() ? evidenceValidations : evidenceReads);
        std::uint64_t ValidateCounters::*reason = &ValidateCounters::syncedOff;
        const auto policy = validateSkipEnabled() ? classifyPendingWrite(region.guestAddress, region.bytes.size(), reason, pending) : Policy::Sync;
        if (policy != Policy::Sync) continue;
        if (validateSkipEnabled() && (validateLegacy() ? !GuestMemory::EqualsCommittedUnsynced(region.guestAddress, region.bytes) : GuestMemory::CompareMapped(region.guestAddress, region.bytes) != GuestMemory::Compare::Equal)) {
            if (profile) {
                std::lock_guard lock(validateMutex);
                ++validateCounters.pending;
                ++validateCounters.unsyncedMisses;
            }
            return false;
        }
        std::vector<std::byte> before;
        if (writeEvidenceEnabled() && GuestMemory::Accessible(reinterpret_cast<const void*>(region.guestAddress), region.bytes.size())) {
            before.resize(region.bytes.size());
            std::memcpy(before.data(), reinterpret_cast<const void*>(region.guestAddress), before.size());
        }
        const auto waitsBefore = Graphics::Recorder::ThreadHookWaits();
        const bool timed = profile || traceCapSync();
        const auto waitedBefore = timed ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
        GuestMemory::FlushGpuWrites(region.guestAddress, region.bytes.size());
        const auto waited = timed ? Graphics::Recorder::ThreadWaitedMs() - waitedBefore : 0.0;
        pending.Load();
        if (!before.empty()) {
            if (!observationGuard() || Graphics::Recorder::ThreadHookWaits() != waitsBefore) observeRange(region.guestAddress, before);
            else observationsNoWait.fetch_add(1, std::memory_order_relaxed);
        }
        ++synced;
        if (profile) {
            std::lock_guard lock(validateMutex);
            auto& counters = validateCounters;
            ++counters.pending;
            ++(counters.*reason);
            counters.syncedWaitMs += waited;
            if (std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) reportValidation(counters);
        }
        if (traceCapSync() && traceBudget()) {
            const auto begin = region.guestAddress;
            const auto end = begin + region.bytes.size();
            std::fprintf(stderr, "[capsync] dispatch-cache q0x%x program 0x%llx region 0x%llx+0x%zx synced before the gate: waited %.1f ms;%s; writers:%s\n", queue, static_cast<unsigned long long>(program), static_cast<unsigned long long>(begin), region.bytes.size(), waited, describeSelf(compiled, begin, end).c_str(), describeWriters(begin, end).c_str());
        }
    }
    return true;
}

bool Driver::vouchedValidation() {
    static const bool enabled = std::getenv("APS5_NO_VOUCHED_VALIDATION") == nullptr;
    return enabled;
}

EntryOutcome Driver::validateVariant(std::uint64_t program, std::uint32_t queue, const DispatchVariant& variant, std::span<const ShaderRecompiler::MemoryRegion> regions, std::uint64_t& imagesFlushed, std::uint64_t& runsSynced, std::optional<SampledReadScope>& sampling, std::vector<std::pair<std::uint32_t, std::uint32_t>>* live) {
    if (live != nullptr) live->clear();
    if (DrawPipeline::Active()) {
        auto& pipeline = DrawPipeline::Queue0();
        if (pipeline.Busy() && pipeline.OverlapsAny(variant.runs)) pipeline.Drain(DrawPipeline::DrainReason::Capture);
    }
    const bool masked = live != nullptr && !variant.dataPositions.empty();
    const DataMask mask{variant.dataPositions, live};
    thread_local std::vector<VouchedRegions>* vouchedSlot = nullptr;
    auto& vouched = ShaderRecompiler::ThreadOwned(vouchedSlot);
    vouched.clear();
    if (!variant.shared.empty() && vouchedValidation() && !validateLegacy() && variant.runs.size() == regions.size()) VouchSharedRegions(variant.shared, regions.size(), &QueryPendingSpan, vouched);
    PendingView pending;
    pending.Load();
    std::erase_if(vouched, [&](const VouchedRegions& range) { return pending.Overlaps(range.low, static_cast<std::size_t>(range.high - range.low)); });
    thread_local std::vector<std::pair<std::uint64_t, std::uint64_t>>* checkedSlot = nullptr;
    auto& checked = ShaderRecompiler::ThreadOwned(checkedSlot);
    checked.clear();
    if (!vouched.empty()) {
        for (std::size_t index = 0, range = 0; index < variant.runs.size(); ++index) {
            if (range < vouched.size() && index == vouched[range].begin) {
                checked.emplace_back(vouched[range].low, vouched[range].high);
                index = vouched[range++].end - 1u;
                continue;
            }
            checked.push_back(variant.runs[index]);
        }
    }
    const std::span<const std::pair<std::uint64_t, std::uint64_t>> runs = vouched.empty() ? std::span<const std::pair<std::uint64_t, std::uint64_t>>(variant.runs) : std::span<const std::pair<std::uint64_t, std::uint64_t>>(checked);
    if (!syncPendingRuns(program, queue, *variant.compiled, regions, runsSynced, pending, sampling, vouched)) return EntryOutcome::Differing;
    auto publish = Graphics::Recorder::PublishGeneration();
    auto pendingSerial = Graphics::StorageTexture::PendingSerial();
    auto forget = GuestMemory::ForgetSerial();

    if (Graphics::StorageTexture::AnyPendingOverlaps(runs) || Graphics::AnyShadowedOverlaps(runs)) {
        vouched.clear();
        for (const auto& region : regions) {
            if (Graphics::StorageTexture::FlushPending(region.guestAddress, region.bytes.size(), nullptr, "dispatch-cache entry")) ++imagesFlushed;
        }
        pending.Load();
        if (!syncPendingRuns(program, queue, *variant.compiled, regions, runsSynced, pending, sampling)) return EntryOutcome::Differing;
        publish = Graphics::Recorder::PublishGeneration();
        pendingSerial = Graphics::StorageTexture::PendingSerial();
        forget = GuestMemory::ForgetSerial();
        if (Graphics::StorageTexture::AnyPendingOverlaps(variant.runs) || Graphics::AnyShadowedOverlaps(variant.runs)) return EntryOutcome::FlushingImage;
    }
    if ((forget & 1) != 0) return EntryOutcome::ForgetMoved;
    for (const auto& label : deferredLabels().labels) {
        for (const auto& [begin, end] : runs) {
            if (label.address < end && begin < label.address + label.size) return EntryOutcome::QueuedLabel;
        }
    }
    if (validateLegacy()) {
        for (const auto& region : regions) {
            if (!GuestMemory::Accessible(reinterpret_cast<const void*>(region.guestAddress), region.bytes.size())) return EntryOutcome::Inaccessible;
        }
    }

    pending.Load();
    bool unmapped = false;
    if (!validateCaptured(program, queue, regions, *variant.compiled, true, pending, &unmapped, &sampling, masked ? &mask : nullptr, vouched)) return unmapped ? EntryOutcome::Inaccessible : EntryOutcome::Differing;
    if (Graphics::Recorder::PublishGeneration() != publish) return EntryOutcome::PublishMoved;
    if (Graphics::StorageTexture::PendingSerial() != pendingSerial) return EntryOutcome::PendingMoved;
    if (GuestMemory::ForgetSerial() != forget) return EntryOutcome::ForgetMoved;
    return masked && !live->empty() ? EntryOutcome::EqualData : EntryOutcome::Equal;
}

}
