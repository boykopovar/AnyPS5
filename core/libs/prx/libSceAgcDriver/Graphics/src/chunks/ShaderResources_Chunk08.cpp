#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <stdexcept>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include "ImageTableAbi.hpp"
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "prx/libc/include/GuestAllocations.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void LastRecordIndices(std::span<const void* const> logged, std::span<const void* const> objects, std::vector<std::size_t>& indices) {
    indices.assign(objects.size(), logged.size());
    thread_local std::vector<std::pair<const void*, std::size_t>>* wantedSlot = nullptr;
    auto& wanted = ShaderRecompiler::ThreadOwned(wantedSlot);
    wanted.clear();
    for (std::size_t i = 0; i < objects.size(); ++i) wanted.emplace_back(objects[i], i);
    const auto byObject = [](const std::pair<const void*, std::size_t>& left, const std::pair<const void*, std::size_t>& right) { return std::less<const void*>{}(left.first, right.first); };
    std::sort(wanted.begin(), wanted.end(), byObject);
    std::size_t open = wanted.size();
    for (auto at = logged.size(); at-- > 0 && open != 0;) {
        const auto [first, last] = std::equal_range(wanted.begin(), wanted.end(), std::pair<const void*, std::size_t>{logged[at], 0}, byObject);
        if (first == last || indices[first->second] != logged.size()) continue;
        for (auto it = first; it != last; ++it) indices[it->second] = at;
        open -= static_cast<std::size_t>(last - first);
    }
    if (wanted.capacity() > LookupLogBound + LookupLogBound / 2) {
        wanted.clear();
        wanted.shrink_to_fit();
    }
}

bool ShaderResources::keepsTemplateRecords() const {
    static const bool always = std::getenv("APS5_KEEP_TEMPLATE_RECORDS") != nullptr;
    return always || !(usesBda || (usesFaultBuffer && LoopGuardEnabled()));
}

void ShaderResources::captureValidation(bool spaceReusable) {
    if (!keepsTemplateRecords() && !(spaceReusable && !RuntimeSwitches::Set(RuntimeSwitch::NoSpaceReuseRecords))) {
        validatedTextures.clear();
        lookupLog().clear();
        return;
    }
    const auto& log = lookupLog();
    thread_local std::vector<const void*>* loggedSlot = nullptr;
    auto& logged = ShaderRecompiler::ThreadOwned(loggedSlot);
    thread_local std::vector<const void*>* objectsSlot = nullptr;
    auto& objects = ShaderRecompiler::ThreadOwned(objectsSlot);
    thread_local std::vector<std::size_t>* indicesSlot = nullptr;
    auto& indices = ShaderRecompiler::ThreadOwned(indicesSlot);
    logged.clear();
    for (const auto& entry : log) logged.push_back(entry.object);
    objects.clear();
    for (const auto& texture : textures) objects.push_back(texture.get());
    LastRecordIndices(logged, objects, indices);
    validatedTextures.assign(textures.size(), {});
    for (std::size_t i = 0; i < textures.size(); ++i) {
        if (indices[i] == log.size()) continue;
        const auto& found = log[indices[i]];
        validatedTextures[i] = {found.resource, found.bytes, found.keys, found.generation, 0, found.source, found.depth, true, found.committedGeneration, found.committed};
    }
    lookupLog().clear();
    const auto trim = [](auto& scratch) {
        if (scratch.capacity() <= LookupLogBound + LookupLogBound / 2) return;
        scratch.clear();
        scratch.shrink_to_fit();
    };
    trim(logged);
    trim(objects);
    trim(indices);
}

ShaderResources::CollectRefusals ShaderResources::CollectRefusalCounts() {
    auto& profile = Revalidations();
    CollectRefusals counts;
    counts.sampled = profile.refusedSampled.load(std::memory_order_relaxed);
    counts.storage = profile.refusedStorage.load(std::memory_order_relaxed);
    counts.uncommitted = profile.refusedUncommitted.load(std::memory_order_relaxed);
    counts.unwatched = profile.refusedUnwatched.load(std::memory_order_relaxed);
    counts.other = profile.refusedOther.load(std::memory_order_relaxed);
    std::lock_guard lock(profile.refusedMutex);
    counts.surfaces = profile.refusedSurfaces.size();
    for (const auto& surface : profile.refusedSurfaces) counts.bytes += surface.second;
    return counts;
}

ShaderResources::PendingFails ShaderResources::PendingFailCounts() {
    auto& profile = Revalidations();
    PendingFails counts;
    counts.foreign = profile.pendingForeign.load(std::memory_order_relaxed);
    counts.snapshot = profile.pendingSnapshot.load(std::memory_order_relaxed);
    counts.t1Eligible = profile.pendingT1Eligible.load(std::memory_order_relaxed);
    return counts;
}

std::array<std::uint64_t, static_cast<std::size_t>(ShaderResources::FastFail::Count)> ShaderResources::FullWalkNanoseconds() {
    std::array<std::uint64_t, static_cast<std::size_t>(FastFail::Count)> result{};
    auto& profile = Revalidations();
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = profile.fullNanoseconds[i].load(std::memory_order_relaxed);
    return result;
}

std::size_t ShaderResources::LookupLogCapacity() {
    return lookupLog().capacity();
}

bool ShaderResources::fastRevalidate(std::uint64_t serialBefore, std::span<const PendingOverlap> refreshed, FastFail& reason, std::vector<PendingOverlap>& overlapping, bool& accepted, const std::vector<std::size_t>* only, bool withStorage) {
    reason = FastFail::Count;
    overlapping.clear();
    accepted = false;
    if (!EpochRevalidate()) return fastRevalidateEach();
    const auto elementCount = only != nullptr ? only->size() : textures.size();
    const auto elementAt = [only](std::size_t k) { return only != nullptr ? (*only)[k] : k; };
    const auto fail = [&reason](FastFail why) {
        reason = why;
        countFastFail(why);
        return false;
    };
    if (validatedTextures.size() != textures.size()) return fail(FastFail::NoRecord);
    const bool unchanged = pendingSerialSeen != 0 && pendingSerialSeen == serialBefore;
    const bool keyProofs = KeyFastPath();
    struct QueriesTag {};
    auto& queries = ThreadScratch<std::vector<GuestMemory::UnchangedQuery>, QueriesTag>();
    struct PendingTag {};
    auto& pending = ThreadScratch<std::vector<StorageTexture::PendingQuery>, PendingTag>();
    struct OwnersTag {};
    auto& owners = ThreadScratch<std::vector<PendingOverlap>, OwnersTag>();
    struct ImagesTag {};
    auto& images = ThreadScratch<std::vector<const StorageTexture*>, ImagesTag>();
    struct ScannedKeysTag {};
    auto& scannedKeys = ThreadScratch<std::vector<DccKeys>, ScannedKeysTag>();
    queries.clear();
    pending.clear();
    owners.clear();
    images.clear();
    scannedKeys.assign(textures.size(), DccKeys::Uncompressed);
    const auto query = [&](std::uint64_t begin, std::uint64_t end, const StorageTexture* except, const StorageTexture* identity, PendingOverlap owner) {
        pending.push_back({begin, end, except, identity, false});
        owners.push_back(owner);
    };
    for (std::size_t k = 0; k < elementCount; ++k) {
        const auto i = elementAt(k);
        auto& surface = validatedTextures[i];
        if (!surface.valid) return fail(FastFail::NoRecord);
        surface.committedProved = false;
        if (surface.depth) {
            if (!textures[i]->HasReuseLookup() && !DepthSurfaceHolds(context, surface.resource, textures[i].get())) return fail(FastFail::Depth);
            continue;
        }
        if (textures[i]->Released()) return fail(FastFail::Replaced);
        const auto address = surface.resource.baseAddress;
        const auto bytes = static_cast<std::size_t>(surface.bytes);
        if (!FlushDepthPlanes(address, bytes).empty()) return fail(FastFail::Changed);
        surface.collected = GuestMemory::CollectWrites(address, bytes);
        if (surface.collected == 0 && surface.generation == 0 && surface.committedGeneration != 0 && surface.source == nullptr && !RuntimeSwitches::Set(RuntimeSwitch::NoExtendedFastProofs)) {
            struct CommittedTag {};
            auto& ranges = ThreadScratch<std::vector<std::pair<std::uint64_t, std::uint64_t>>, CommittedTag>();
            const auto partial = GuestMemory::CollectCommittedWrites(address, bytes, ranges);
            if (partial != 0 && ranges == surface.committed) {
                if (Recorder::SnapshotWriteOverlaps(address, bytes) || !GuestMemory::UnchangedCommittedSince(surface.committed, surface.committedGeneration)) return fail(FastFail::Changed);
                surface.collected = partial;
                surface.committedProved = true;
            }
        }
        if (surface.collected == 0) {
            countCollectRefusal(false, address, bytes);
            return fail(FastFail::Collect);
        }
        const auto* source = surface.source;
        if (!keyProofs) {
            if (surface.resource.dccAddress != 0 && TextureClearKeys(surface.resource, surface.bytes) != surface.keys) return fail(FastFail::Keys);
            scannedKeys[i] = surface.keys;
            if (!unchanged) query(address, address + bytes, source, source != nullptr && surface.keys != DccKeys::Uncompressed ? source : nullptr, {i, false, source != nullptr && surface.keys == DccKeys::Uncompressed});
        } else if (source != nullptr) {
            const auto& own = source->Descriptor();
            const auto sourceKeys = source->ProvedKeys();
            if (sourceKeys != source->UploadedKeys()) return fail(FastFail::Keys);
            const auto keys = surface.resource.dccAddress == 0 ? DccKeys::Uncompressed : SameKeySurface(source, surface.resource, surface.bytes) ? sourceKeys : ProvedClearKeys(surface.resource, surface.bytes, textures[i]->KeyProof());
            if (surface.resource.dccAddress != 0 && surface.resource.dccAddress != own.dccAddress && (keys != DccKeys::Uncompressed || !StorageImageServesKeys(*source, surface.resource.dccAddress))) return fail(FastFail::Keys);
            scannedKeys[i] = keys;
            if (keys != DccKeys::Uncompressed && !(own.dccAddress == surface.resource.dccAddress && ClearedViewEnabled() && StorageClearAvailable(context, surface.resource.format, keys))) query(address, address + bytes, source, source, {i, false, false});
            else if (!unchanged) query(address, address + bytes, source, nullptr, {i, false, keys == DccKeys::Uncompressed});
            if (surface.resource.dccAddress != 0 || own.dccAddress != 0) countKeysProven(false);
        } else {
            const auto keys = surface.resource.dccAddress != 0 ? ProvedClearKeys(surface.resource, surface.bytes, textures[i]->KeyProof()) : DccKeys::Uncompressed;
            if (keys != surface.keys) return fail(FastFail::Keys);
            scannedKeys[i] = keys;
            if (!unchanged) query(address, address + bytes, nullptr, nullptr, {i, false, false});
            if (surface.resource.dccAddress != 0) countKeysProven(false);
        }
        if (source != nullptr) {
            if (!source->Cached()) return fail(FastFail::Evicted);
            queries.push_back({address, bytes, source->GenerationFor(address, bytes)});
            images.push_back(source);
        } else if (!surface.committedProved) {
            queries.push_back({address, bytes, surface.generation});
        }
    }
    for (std::size_t i = 0; withStorage && i < storageTextures.size(); ++i) {
        if (storageTextures[i] != nullptr && (i >= storageKeys.size() || !StorageImageServesKeys(*storageTextures[i], storageKeys[i]))) return fail(FastFail::StorageKeys);
        if (i != 0 && storageTextures[i] == storageTextures[i - 1]) continue;
        const auto* image = storageTextures[i].get();
        if (image == nullptr) return fail(FastFail::NoRecord);
        const auto& own = image->Descriptor();
        const auto address = own.baseAddress;
        const auto bytes = static_cast<std::size_t>(image->GuestBytes());
        if (DepthSurfaceOverlaps(context.device, address, bytes)) {
            if (!indexProof) depthGuardRefusals.fetch_add(1, std::memory_order_relaxed);
            return fail(FastFail::Depth);
        }
        if (GuestMemory::CollectWrites(address, bytes) == 0) {
            countCollectRefusal(true, address, bytes);
            return fail(FastFail::Collect);
        }
        if (own.dccAddress != 0) {
            if (!keyProofs || image->ProvedKeys() != image->UploadedKeys()) return fail(FastFail::StorageKeys);
            countKeysProven(true);
        }
        if (!unchanged) query(address, address + bytes, image, nullptr, {i, true, false});
        if (!image->Cached()) return fail(FastFail::Evicted);
        queries.push_back({address, bytes, image->GenerationFor(address, bytes)});
        images.push_back(image);
    }
    if (!pending.empty()) {
        if (!StorageTexture::ScanPending(pending)) return fail(FastFail::ClearedView);
        const auto refreshedOwner = [&](const PendingOverlap& owner) {
            return std::find_if(refreshed.begin(), refreshed.end(), [&](const PendingOverlap& entry) { return entry.element == owner.element && entry.storage == owner.storage; });
        };
        for (std::size_t k = 0; k < pending.size(); ++k) {
            if (!pending[k].overlaps) continue;
            if (OwnImageRefreshEnabled() && owners[k].viewUncompressed && pending[k].found == pending[k].except) {
                if (BuildProfiled()) Revalidations().ownSourceViews.fetch_add(1, std::memory_order_relaxed);
                accepted = true;
                continue;
            }
            const auto lookupAccepts = [&](const PendingOverlap& entry) {
                const auto& surface = validatedTextures[entry.element];
                if (surface.resource.dccAddress != 0) return false;
                const auto viewed = pendingLookupSource(context, surface.resource, surface.bytes, entry.depthCompare).source;
                return viewed.get() == pending[k].except || (viewed == nullptr && entry.sourceEligible && entry.sourceRefreshed);
            };
            if (const auto entry = refreshedOwner(owners[k]); entry != refreshed.end() && (entry->storage || pending[k].found == pending[k].except || (pending[k].found == nullptr && entry->sourceEligible) || lookupAccepts(*entry))) {
                if (BuildProfiled()) Revalidations().refreshedOverlaps.fetch_add(1, std::memory_order_relaxed);
                accepted = true;
                continue;
            }
            overlapping.push_back(owners[k]);
        }
        if (!overlapping.empty()) {
            countPendingFail(pending);
            return fail(FastFail::Pending);
        }
    } else if (unchanged && BuildProfiled()) {
        Revalidations().serialSkips.fetch_add(1, std::memory_order_relaxed);
    }
    if (!GuestMemory::UnchangedSinceAll(queries)) return fail(FastFail::Changed);
    if (!StorageImagesCached(context, images)) return fail(FastFail::Evicted);
    for (const auto* image : images) image->NoteProved();
    for (std::size_t k = 0; k < elementCount; ++k) {
        const auto i = elementAt(k);
        auto& surface = validatedTextures[i];
        if (surface.source == nullptr) {
            if (surface.committedProved) surface.committedGeneration = surface.collected;
            else surface.generation = surface.collected;
        }
        surface.keys = scannedKeys[i];
    }
    return true;
}

bool ShaderResources::fastRevalidateEach() {
    if (validatedTextures.size() != textures.size()) return false;
    for (std::size_t i = 0; i < textures.size(); ++i) {
        auto& surface = validatedTextures[i];
        if (!surface.valid) return false;
        if (surface.depth) {
            if (!textures[i]->HasReuseLookup() && !DepthSurfaceHolds(context, surface.resource, textures[i].get())) return false;
            continue;
        }
        if (textures[i]->Released()) return false;
        const auto address = surface.resource.baseAddress;
        const auto bytes = static_cast<std::size_t>(surface.bytes);
        if (!FlushDepthPlanes(address, bytes).empty()) return false;
        surface.collected = GuestMemory::CollectWrites(address, bytes);
        if (surface.collected == 0) return false;
        if (PendingStorageOverlaps(address, bytes, surface.source)) return false;
        if (surface.source != nullptr && surface.resource.dccAddress != 0 && surface.resource.dccAddress != surface.source->Descriptor().dccAddress) return false;
        if (surface.source != nullptr) {
            if (!StorageImageCached(context, surface.source) || !GuestMemory::UnchangedSince(address, bytes, surface.source->GenerationFor(address, bytes))) return false;
        } else if (!GuestMemory::UnchangedSince(address, bytes, surface.generation)) {
            return false;
        }
        if (surface.resource.dccAddress != 0 && TextureClearKeys(surface.resource, surface.bytes) != surface.keys) return false;
        if (surface.source != nullptr && surface.keys != DccKeys::Uncompressed && StorageTexture::FindPending(address, surface.bytes).get() != surface.source) return false;
    }
    for (std::size_t i = 0; i < storageTextures.size(); ++i) {
        if (storageTextures[i] != nullptr && (i >= storageKeys.size() || (storageKeys[i] != 0 && storageKeys[i] != storageTextures[i]->Descriptor().dccAddress))) return false;
        if (i != 0 && storageTextures[i] == storageTextures[i - 1]) continue;
        const auto* image = storageTextures[i].get();
        if (image == nullptr) return false;
        const auto& own = image->Descriptor();
        if (own.dccAddress != 0) return false;
        const auto address = own.baseAddress;
        const auto bytes = static_cast<std::size_t>(image->GuestBytes());
        if (DepthSurfaceOverlaps(context.device, address, bytes)) {
            depthGuardRefusals.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (GuestMemory::CollectWrites(address, bytes) == 0) return false;
        if (PendingStorageOverlaps(address, bytes, image)) return false;
        if (!StorageImageCached(context, image)) return false;
        if (!GuestMemory::UnchangedSince(address, bytes, image->GenerationFor(address, bytes))) return false;
    }
    for (auto& surface : validatedTextures) {
        if (surface.source == nullptr) surface.generation = surface.collected;
    }
    return true;
}

ShaderResources::OwnRefreshFallback ShaderResources::refreshOwnObjects(std::span<const CompiledShader> shaders, std::span<PendingOverlap> overlapping) {
    const auto listed = [&](std::size_t element, bool storage) {
        return std::find_if(overlapping.begin(), overlapping.end(), [&](const PendingOverlap& overlap) { return overlap.element == element && overlap.storage == storage; });
    };
    const auto refreshView = [&](std::size_t i, PendingOverlap& overlap, bool depthCompare) {
        const auto& surface = validatedTextures[i];
        const auto& source = textures[i]->SharedStorageSource();
        if (source == nullptr || source.get() != surface.source) return OwnRefreshFallback::Snapshot;
        if (!source->Cached()) return OwnRefreshFallback::Uncached;
        if (!StorageImageServesKeys(*source, surface.resource.dccAddress)) return OwnRefreshFallback::Keys;
        const auto address = surface.resource.baseAddress;
        const auto bytes = static_cast<std::size_t>(surface.bytes);
        const bool imported = SampledFromStorageEligible(context, surface.resource, surface.bytes);
        overlap.sourceEligible = imported && SurfaceKey(context, source->Descriptor()) == SurfaceKey(context, surface.resource);
        overlap.depthCompare = depthCompare;
        overlap.sourceRefreshed = false;
        const auto found = pendingLookupSource(context, surface.resource, surface.bytes, depthCompare).source;
        if (found != nullptr) {
            if (found != source) return OwnRefreshFallback::ForeignView;
            if (!GuestMemory::UnchangedSince(address, bytes, source->GenerationFor(address, bytes))) {
                countOwnRefresh(false);
                refreshForView(*source, surface.resource);
            }
            return OwnRefreshFallback::Count;
        }
        if (!imported) return OwnRefreshFallback::NotImported;
        if (!overlap.sourceEligible) return OwnRefreshFallback::SurfaceKey;
        countOwnRefresh(false);
        source->Refresh();
        overlap.sourceRefreshed = true;
        return OwnRefreshFallback::Count;
    };
    const auto refreshStorage = [&](std::size_t i) {
        auto& image = storageTextures[i];
        if (image == nullptr || !image->Cached()) return OwnRefreshFallback::Uncached;
        if (i >= storageKeys.size() || !StorageImageServesKeys(*image, storageKeys[i])) return OwnRefreshFallback::Keys;
        countOwnRefresh(true);
        image->Refresh();
        return OwnRefreshFallback::Count;
    };
    std::size_t textureIndex = 0;
    std::size_t storageIndex = 0;
    for (const auto& shader : shaders) {
        if (shader.program == nullptr) return OwnRefreshFallback::Uncached;
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages) continue;
            if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) {
                for (std::uint32_t element = 0; element < binding.count; ++element) {
                    const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
                    if (IsNullSampledTextureResource(words)) continue;
                    if (textureIndex >= textures.size()) return OwnRefreshFallback::Uncached;
                    const auto index = textureIndex++;
                    const auto overlap = listed(index, false);
                    if (overlap == overlapping.end()) continue;
                    if (const auto fallback = refreshView(index, *overlap, !binding.imageDepthCompare.empty() && binding.imageDepthCompare.at(element)); fallback != OwnRefreshFallback::Count) return fallback;
                }
            } else if (binding.kind == ShaderRecompiler::DescriptorKind::StorageImage) {
                for (std::uint32_t element = 0; element < binding.count; ++element) {
                    const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
                    if (IsNullTextureResource(words)) continue;
                    if (storageIndex >= storageTextures.size()) return OwnRefreshFallback::Uncached;
                    const auto index = storageIndex++;
                    if (listed(index, true) == overlapping.end()) continue;
                    if (const auto fallback = refreshStorage(index); fallback != OwnRefreshFallback::Count) return fallback;
                }
            }
        }
    }
    for (const auto& table : tableStages) {
        const auto* images = table.elementTextures.empty() ? nullptr : tableImages(shaders, table.stage);
        for (std::size_t element = 0; element < table.elementTextures.size(); ++element) {
            const auto index = table.elementTextures[element];
            if (index == std::numeric_limits<std::size_t>::max()) continue;
            if (!table.representatives.empty() && table.representatives[element] != NoRepresentative) continue;
            if (index != textureIndex++ || index >= textures.size()) return OwnRefreshFallback::Uncached;
            const auto overlap = listed(index, false);
            if (overlap == overlapping.end()) continue;
            if (images == nullptr || images->count != table.elementTextures.size()) return OwnRefreshFallback::Uncached;
            if (const auto fallback = refreshView(index, *overlap, images->imageDepthCompare.at(element)); fallback != OwnRefreshFallback::Count) return fallback;
        }
    }
    return OwnRefreshFallback::Count;
}

}
}
}
}
}
}
}
}
}
}
}
}
