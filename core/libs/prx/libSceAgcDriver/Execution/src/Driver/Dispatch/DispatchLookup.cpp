#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

void Driver::lookupDispatch(std::uint64_t address, const Submission& submission, std::uint64_t key,
                            bool noDispatchCache, bool traceCache, bool profile,
                            std::span<const ShaderRecompiler::MemoryRegion> memory, DispatchPhaseTiming& phaseTiming,
                            std::array<double, DriverPhaseCount>& phaseMs,
                            std::shared_ptr<const ShaderRecompiler::RecompileResult>& compiledResult,
                            std::shared_ptr<DispatchVariant>& keepVariant,
                            std::vector<ShaderRecompiler::MemoryRegion>& captured,
                            std::vector<std::uint32_t>& liveWords, bool& dataHit, bool& cached, bool& validated,
                            std::shared_ptr<DispatchEntry>& missedEntry, bool& missedDiffering) {
    if (!noDispatchCache) {

        static const bool validateUnlocked = std::getenv("APS5_NO_UNLOCKED_VALIDATE") == nullptr;
        std::unique_lock cacheLock(dispatchCacheMutex);
        ++entryCounters.lookups;
        const auto found = dispatchCache.find(key);
        std::shared_ptr<DispatchEntry> entry = found != dispatchCache.end() ? found->second : nullptr;
        if (entry == nullptr)
            ++entryCounters.absent;
        if (validateUnlocked)
            cacheLock.unlock();
        phaseTiming.Phase(PhaseLookup);
        if (entry != nullptr) {
            validated = true;
            const auto& variants = entry->variants;

            std::shared_ptr<DispatchVariant> variant;
            std::size_t rank = 0;
            const auto generation = variants.front()->generation.load(std::memory_order_acquire);
            bool current = false;
            std::uint64_t restamped = 0;

            auto outcome = EntryOutcome::Differing;
            std::uint64_t imagesFlushed = 0, runsSynced = 0, retriesEqual = 0, retriesMoved = 0, compared = 0;
            std::vector<ShaderRecompiler::MemoryRegion> regions;

            std::vector<std::pair<std::uint32_t, std::uint32_t>> liveData;
            const auto waitedBeforeValidate = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
            if (!stampValidate()) {
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);

                std::optional<SampledReadScope> sampling;

                for (std::size_t i = 0; i < variants.size(); ++i) {
                    regions.clear();
                    appendEntryRegions(*variants[i], regions);
                    ++compared;
                    auto result = validateVariant(address, submission.queue, *variants[i], regions, imagesFlushed,
                                                  runsSynced, sampling, &liveData);

                    if (gateRetry() && (result == EntryOutcome::PublishMoved || result == EntryOutcome::PendingMoved)) {
                        result = validateVariant(address, submission.queue, *variants[i], regions, imagesFlushed,
                                                 runsSynced, sampling, &liveData);
                        ++(result == EntryOutcome::Equal || result == EntryOutcome::EqualData ? retriesEqual
                                                                                              : retriesMoved);
                    }
                    if (i == 0)
                        outcome = result;
                    if (result == EntryOutcome::Equal || result == EntryOutcome::EqualData) {
                        outcome = result;
                        variant = variants[i];
                        rank = i;
                        dataHit = result == EntryOutcome::EqualData;
                        break;
                    }
                }
                current = variant != nullptr;
            } else {
                variant = variants.front();
                compared = 1;
                current = true;
                for (const auto& [begin, bytes] : variant->spans) {
                    GuestMemory::CollectWrites(begin, bytes);
                    if (!GuestMemory::UnchangedSince(begin, bytes, generation)) {
                        current = false;
                        break;
                    }
                }
                if (!current) {

                    std::uint64_t collected = 0;
                    for (const auto& [begin, bytes] : variant->spans)
                        collected = std::max(collected, GuestMemory::CollectWrites(begin, bytes));
                    bool same = collected != 0;
                    if (same) {
                        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                        PendingView pending;
                        pending.Load();
                        same = validateCaptured(address, submission.queue, variant->captured, *variant->compiled, false,
                                                pending);
                    }
                    if (same) {
                        restamped = collected;
                        current = true;
                    }
                }
                if (current)
                    outcome = EntryOutcome::Equal;
            }
            phaseTiming.Phase(PhaseValidate);
            if (profile) {
                const auto waited =
                    std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBeforeValidate, phaseMs[PhaseValidate]);
                phaseMs[PhaseValidate] -= waited;
                phaseMs[PhaseValidateWait] += waited;
            }
            if (!cacheLock.owns_lock())
                cacheLock.lock();

            const auto again = dispatchCache.find(key);
            const bool untouched = again != dispatchCache.end() && again->second == entry &&
                                   variants.front()->generation.load(std::memory_order_acquire) == generation;
            auto& counters = entryCounters;
            counters.validateUs += phaseMs[PhaseValidate] * 1000;
            counters.imagesFlushed += imagesFlushed;
            counters.runsSynced += runsSynced;
            for (std::size_t i = 0; i < compared && i < variants.size(); ++i)
                counters.runsValidated += variants[i]->runs.size();
            counters.retriesEqual += retriesEqual;
            counters.retriesMoved += retriesMoved;
            counters.variantsCompared += compared;
            switch (outcome) {
            case EntryOutcome::Equal:
                ++counters.equal;
                break;
            case EntryOutcome::EqualData:
                ++counters.equal;
                break;
            case EntryOutcome::Differing:
                ++counters.differing;
                break;
            case EntryOutcome::Inaccessible:
                ++counters.inaccessible;
                break;
            case EntryOutcome::QueuedLabel:
                ++counters.queuedLabel;
                break;
            case EntryOutcome::FlushingImage:
                ++counters.flushingImage;
                break;
            case EntryOutcome::PublishMoved:
                ++counters.publishMoved;
                break;
            case EntryOutcome::PendingMoved:
                ++counters.pendingMoved;
                break;
            case EntryOutcome::ForgetMoved:
                ++counters.forgetMoved;
                break;
            }
            if (current) {
                ++counters.variantHitsByRank[rank];
                compiledResult = variant->compiled;
                if (dataHit) {

                    liveWords = variant->words;
                    for (const auto& [position, value] : liveData)
                        liveWords[position] = value;
                    regions.clear();
                    appendEntryRegions(*variant, regions, &liveWords);
                    auto patched = std::make_shared<ShaderRecompiler::RecompileResult>(*variant->compiled);
                    auto& descriptor = patched->bindings[variant->flatBinding].guestDescriptor;
                    for (std::size_t k = 0; k < variant->dataPositions.size(); ++k) {
                        if (variant->dataSlots[k] < descriptor.size())
                            descriptor[variant->dataSlots[k]] = liveWords[variant->dataPositions[k]];
                    }
                    compiledResult = std::move(patched);
                    ++counters.dataHits;
                    counters.dataWordsRefreshed += liveData.size();
                    ++counters.dataHitsByRank[rank];
                }
                if (stampValidate()) {
                    captured = variant->captured;
                } else {

                    captured.reserve(memory.size() + regions.size());
                    captured.assign(memory.begin(), memory.end());
                    captured.insert(captured.end(), regions.begin(), regions.end());
                    if (variant->forgetSerial != GuestMemory::ForgetSerial())
                        ++counters.forgetSinceInsert;
                }
                keepVariant = variant;
                cached = true;
                ++dispatchCacheHits;
                if (untouched) {
                    if (restamped != 0)
                        variant->generation.store(restamped, std::memory_order_release);

                    if (rank != 0) {
                        auto rotated = std::make_shared<DispatchEntry>();
                        rotated->variants.reserve(variants.size());
                        rotated->variants.push_back(variant);
                        for (std::size_t i = 0; i < variants.size(); ++i) {
                            if (i != rank)
                                rotated->variants.push_back(variants[i]);
                        }
                        rotated->touched = entry->touched;
                        rotated->order = entry->order;
                        again->second = std::move(rotated);
                    }

                    if (dispatchCacheHits - again->second->touched > dispatchCacheEntries() / 8) {
                        dispatchOrder.splice(dispatchOrder.begin(), dispatchOrder, again->second->order);
                        again->second->touched = dispatchCacheHits;
                        ++counters.touches;
                    }
                }
            } else {
                if (traceCache)
                    std::fprintf(stderr, "[dispatch-cache] 0x%llx captured memory changed\n",
                                 static_cast<unsigned long long>(address));

                missedEntry = entry;
                missedDiffering = outcome == EntryOutcome::Differing;
                if (!untouched)
                    ++counters.replaced;
            }
            if (profile && std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) {
                counters.lastReport = std::chrono::steady_clock::now();
                reportDispatchCache(counters);
            }
            phaseTiming.Phase(PhaseRelock);
        }
    }
}

}
