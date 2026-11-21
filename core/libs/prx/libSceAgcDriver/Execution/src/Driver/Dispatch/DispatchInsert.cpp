#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "ImageTableAbi.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

bool CacheableResult(const ShaderRecompiler::RecompileResult& compiled) {
    return compiled.poisonedSrtReads == 0u && std::none_of(compiled.imageTablePoison.begin(), compiled.imageTablePoison.end(), [](const ShaderRecompiler::ImageTableEntryPoison& poison) {
        return poison.reason == static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::Unmapped) || poison.reason == static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::Inactive);
    });
}

std::vector<ShaderRecompiler::MemoryRegion> Driver::dispatchRegions(std::span<const ShaderRecompiler::MemoryRegion> memory, std::span<const ShaderRecompiler::MemoryRegion> captured, const PreparedShaderState::UserDataLink* linking) {
    std::vector<ShaderRecompiler::MemoryRegion> linkRegions;
    if (linking != nullptr) linking->AppendRegions(linkRegions);
    std::stable_sort(linkRegions.begin(), linkRegions.end(), [](const auto& left, const auto& right) { return left.guestAddress < right.guestAddress; });
    std::vector<ShaderRecompiler::MemoryRegion> srtRegions;
    for (const auto& region : captured) {
        const bool registered = std::any_of(memory.begin(), memory.end(), [&](const auto& known) { return region.guestAddress >= known.guestAddress && region.guestAddress < known.guestAddress + known.bytes.size(); });
        const bool dependency = std::any_of(linkRegions.begin(), linkRegions.end(), [&](const auto& linked) { return region.guestAddress == linked.guestAddress && region.bytes.data() == linked.bytes.data() && region.bytes.size() == linked.bytes.size(); });
        if (registered || dependency) continue;
        auto address = region.guestAddress;
        const auto end = address + region.bytes.size();
        const auto append = [&](std::uint64_t begin, std::uint64_t stop, std::span<const std::byte> bytes) {
            if (begin < stop) srtRegions.push_back({begin, bytes.first(static_cast<std::size_t>(stop - begin))});
        };
        for (const auto& linked : linkRegions) {
            const auto low = std::max(address, linked.guestAddress);
            const auto high = std::min(end, linked.guestAddress + linked.bytes.size());
            if (low >= high || (low - region.guestAddress) % sizeof(std::uint32_t) != 0 || (high - low) % sizeof(std::uint32_t) != 0) continue;
            append(address, low, region.bytes.subspan(static_cast<std::size_t>(address - region.guestAddress)));
            const auto original = region.bytes.subspan(static_cast<std::size_t>(low - region.guestAddress), static_cast<std::size_t>(high - low));
            const auto dependency = linked.bytes.subspan(static_cast<std::size_t>(low - linked.guestAddress), original.size());
            append(low, high, std::ranges::equal(original, dependency) ? dependency : original);
            address = high;
        }
        append(address, end, region.bytes.subspan(static_cast<std::size_t>(address - region.guestAddress)));
    }
    srtRegions.insert(srtRegions.end(), linkRegions.begin(), linkRegions.end());
    std::stable_sort(srtRegions.begin(), srtRegions.end(), [](const auto& left, const auto& right) { return left.guestAddress < right.guestAddress; });
    return srtRegions;
}

void Driver::insertDispatch(std::uint64_t address, std::uint64_t key, bool noDispatchCache, bool profile, const std::shared_ptr<const ShaderSnapshot>& registeredShader, std::uint64_t forgetAtCapture, std::span<const ShaderRecompiler::MemoryRegion> memory, const std::shared_ptr<ShaderMemory>& shaderMemory, const std::vector<ShaderRecompiler::MemoryRegion>& captured, const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture, const std::shared_ptr<const ShaderRecompiler::RecompileResult>& compiledResult, const std::shared_ptr<DispatchEntry>& missedEntry, bool missedDiffering, std::shared_ptr<DispatchVariant>& attachVariant, DispatchPhaseTiming& phaseTiming, const std::shared_ptr<const PreparedShaderState::UserDataLink>& linking) {
    if (!noDispatchCache) {
        auto fresh = std::make_shared<DispatchVariant>();
        fresh->compiled = compiledResult;
        if (capture != nullptr && !capture->snapshot.registeredImages.empty()) fresh->registeredViews = std::make_shared<const std::vector<ShaderRecompiler::RegisteredImageView>>(capture->snapshot.registeredImages);
        fresh->shader = registeredShader;
        fresh->linking = linking;
        fresh->forgetSerial = forgetAtCapture;

        const auto srtRegions = dispatchRegions(memory, captured, linking.get());
        std::size_t capturedWords = 0;
        for (const auto& region : srtRegions) capturedWords += region.bytes.size() / sizeof(std::uint32_t);
        auto supplied = shaderMemory->SuppliedBlocks();
        if (linking != nullptr) supplied.insert(supplied.end(), linking->dependencies.begin(), linking->dependencies.end());
        fresh->runs.reserve(srtRegions.size());
        fresh->words.reserve(capturedWords);
        for (const auto& region : srtRegions) {
            fresh->runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
            const auto count = region.bytes.size() / sizeof(std::uint32_t);
            const auto offset = fresh->words.size();
            fresh->words.resize(offset + count);
            std::memcpy(fresh->words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
            AppendSharedWords(fresh->shared, fresh->runs.size() - 1u, offset, region.bytes, supplied);
        }

        DataWordPositionCounts dataCounts;
        if (dataHits() && !stampValidate() && capture != nullptr && !capture->readTrace.leaves.empty()) {
            auto otherReads = capture->readTrace.otherReads;
            if (linking != nullptr) {
                for (const auto& [slot, address] : capture->readTrace.leaves) {
                    if (linking->Overlaps(address, sizeof(std::uint32_t))) otherReads.push_back(address);
                }
                std::sort(otherReads.begin(), otherReads.end());
                otherReads.erase(std::unique(otherReads.begin(), otherReads.end()), otherReads.end());
            }
            for (std::size_t b = 0; b < compiledResult->bindings.size(); ++b) {
                if (compiledResult->bindings[b].role != ShaderRecompiler::DescriptorRole::FlattenedSrt) continue;
                fresh->flatBinding = static_cast<std::uint32_t>(b);
                dataCounts = DataWordPositions(fresh->runs, capture->readTrace.leaves, otherReads, fresh->words, compiledResult->bindings[b].guestDescriptor, fresh->dataPositions, fresh->dataSlots);
                break;
            }
        }
        bool stable = true;
        if (stampValidate() && linking == nullptr) {

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
            } else if (found->second != missedEntry && !found->second->variants.empty() && found->second->variants.front()->runs == fresh->runs && SameCapturedWords(found->second->variants.front()->words, found->second->variants.front()->shared, fresh->words, fresh->shared)) {

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
            const auto over = [&] { return dispatchCache.size() > dispatchCacheEntries() || (dispatchCache.size() > 1 && dispatchCacheVariantBytes > DispatchCacheBytes()); };
            if (over()) {
                if (lru) {
                    while (over()) {
                        eraseDispatchEntry(dispatchCache.find(dispatchOrder.back()));
                        ++dispatchCacheEvictions;
                    }
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
