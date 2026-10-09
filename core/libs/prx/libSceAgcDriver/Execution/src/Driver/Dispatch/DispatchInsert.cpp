#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

bool CacheableResult(const ShaderRecompiler::RecompileResult& compiled) {
    return compiled.poisonedSrtReads == 0u;
}

void Driver::insertDispatch(std::uint64_t address, std::uint64_t key, bool noDispatchCache, bool profile, const std::shared_ptr<const ShaderSnapshot>& registeredShader, std::uint64_t forgetAtCapture, std::span<const ShaderRecompiler::MemoryRegion> memory, const std::shared_ptr<ShaderMemory>& shaderMemory, const std::vector<ShaderRecompiler::MemoryRegion>& captured, const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture, const std::shared_ptr<const ShaderRecompiler::RecompileResult>& compiledResult, const std::shared_ptr<DispatchEntry>& missedEntry, bool missedDiffering, std::shared_ptr<DispatchVariant>& attachVariant, DispatchPhaseTiming& phaseTiming) {
    if (!noDispatchCache) {
        auto fresh = std::make_shared<DispatchVariant>();
        fresh->compiled = compiledResult;
        fresh->shader = registeredShader;
        fresh->forgetSerial = forgetAtCapture;

        std::vector<ShaderRecompiler::MemoryRegion> srtRegions;
        for (const auto& region : captured) {
            const bool registered = std::any_of(memory.begin(), memory.end(), [&](const auto& known) { return region.guestAddress >= known.guestAddress && region.guestAddress < known.guestAddress + known.bytes.size(); });
            if (registered) continue;
            srtRegions.push_back(region);
            fresh->runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
            const auto count = region.bytes.size() / sizeof(std::uint32_t);
            const auto offset = fresh->words.size();
            fresh->words.resize(offset + count);
            std::memcpy(fresh->words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
        }

        DataWordPositionCounts dataCounts;
        if (dataHits() && !stampValidate() && capture != nullptr && !capture->readTrace.leaves.empty()) {
            for (std::size_t b = 0; b < compiledResult->bindings.size(); ++b) {
                if (compiledResult->bindings[b].role != ShaderRecompiler::DescriptorRole::FlattenedSrt) continue;
                fresh->flatBinding = static_cast<std::uint32_t>(b);
                dataCounts = DataWordPositions(fresh->runs, capture->readTrace.leaves, capture->readTrace.otherReads, fresh->words, compiledResult->bindings[b].guestDescriptor, fresh->dataPositions, fresh->dataSlots);
                break;
            }
        }
        bool stable = true;
        if (stampValidate()) {

            fresh->memory = shaderMemory;
            fresh->captured = captured;
            for (const auto& region : captured) {
                const auto begin = region.guestAddress;
                const auto end = begin + region.bytes.size();
                if (!fresh->spans.empty() && begin <= fresh->spans.back().first + fresh->spans.back().second + 65536) {
                    auto& last = fresh->spans.back();
                    last.second = static_cast<std::size_t>(std::max(last.first + last.second, end) - last.first);
                } else {
                    fresh->spans.emplace_back(begin, static_cast<std::size_t>(end - begin));
                }
            }
            std::uint64_t generation = 0;
            for (const auto& [begin, bytes] : fresh->spans) generation = std::max(generation, GuestMemory::CollectWrites(begin, bytes));
            stable = generation != 0;
            if (stable) {
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                stable = captureStable(captured);
            }
            fresh->generation.store(generation, std::memory_order_release);
        } else if (insertCompare()) {
            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
            stable = captureStable(srtRegions);
        }
        std::lock_guard cacheLock(dispatchCacheMutex);
        if (profile && missedEntry != nullptr && missedDiffering) classifyDiffering(address, key, *missedEntry->variants.front(), *fresh, capture.get(), entryCounters);
        if (stable) {
            ++entryCounters.inserts;
            entryCounters.runsInserted += fresh->runs.size();
            if (!fresh->dataPositions.empty()) {
                ++entryCounters.dataInserts;
                entryCounters.dataPositionsInserted += fresh->dataPositions.size();
            }
            entryCounters.dataLeavesUnmapped += dataCounts.unmapped;
            entryCounters.dataLeavesMismatched += dataCounts.mismatched;
            entryCounters.dataLeavesAliased += dataCounts.aliased;
            attachVariant = fresh;
            const auto found = dispatchCache.find(key);
            if (found == dispatchCache.end()) {
                auto created = std::make_shared<DispatchEntry>();
                created->touched = dispatchCacheHits;
                accountVariant(*fresh, true);
                created->variants.push_back(std::move(fresh));
                dispatchOrder.push_front(key);
                created->order = dispatchOrder.begin();
                dispatchCache.emplace(key, std::move(created));
            } else if (found->second != missedEntry && !found->second->variants.empty() && found->second->variants.front()->runs == fresh->runs && found->second->variants.front()->words == fresh->words) {

                attachVariant = found->second->variants.front();
            } else {

                const auto& old = found->second;
                auto replacement = std::make_shared<DispatchEntry>();
                replacement->variants.reserve(dispatchVariants());
                accountVariant(*fresh, true);
                replacement->variants.push_back(std::move(fresh));
                for (const auto& kept : old->variants) {
                    if (replacement->variants.size() < dispatchVariants()) {
                        replacement->variants.push_back(kept);
                    } else {
                        accountVariant(*kept, false);
                        ++entryCounters.variantsEvicted;
                    }
                }
                ++entryCounters.variantsInserted;
                replacement->touched = dispatchCacheHits;
                replacement->order = old->order;
                dispatchOrder.splice(dispatchOrder.begin(), dispatchOrder, replacement->order);
                found->second = std::move(replacement);
            }

            static const bool lru = std::getenv("APS5_NO_DISPATCH_LRU") == nullptr;
            if (dispatchCache.size() > dispatchCacheEntries()) {
                if (lru) {
                    eraseDispatchEntry(dispatchCache.find(dispatchOrder.back()));
                    ++dispatchCacheEvictions;
                } else {
                    dispatchCacheEvictions += dispatchCache.size();
                    dispatchCache.clear();
                    dispatchOrder.clear();
                    dispatchCacheVariants = 0;
                    dispatchCacheVariantBytes = 0;
                }
            }
        } else {
            ++entryCounters.unstable;
        }
        phaseTiming.Phase(PhaseInsert);
    }
}

}
