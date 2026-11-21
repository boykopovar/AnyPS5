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
namespace {
namespace {
bool ShaderResources::revalidateFull(std::span<const CompiledShader> shaders, ProofReport* report) {
    if (report != nullptr) *report = {ProofPath::Full, ProofFailure::Other};
    if (!reusable || shaders.empty()) return false;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool cpuWriters = !GuestMemory::WriteIndexEnabled() || AnyPendingCpuWrites(context);
    for (const auto& region : directRegions) {
        static_cast<void>(FlushDepthPlanes(region.begin, static_cast<std::size_t>(region.end - region.begin)));
        if (cpuWriters) CompletePendingCpuWrites(context, region.begin, static_cast<std::size_t>(region.end - region.begin));
    }
    for (const auto& range : describedRanges) {
        if (!range.imageTable) {
            if (range.storedWords) claimStoredMultisampleMemory(context, DecodeTextureResource(*range.storedWords), range.bytes, range.storedMip);
            RequireNoMultisampleColorTarget(range.address, static_cast<std::size_t>(range.bytes), "a cached image access");
        }
        if (!cpuWriters) continue;
        CompletePendingCpuWrites(context, range.address, static_cast<std::size_t>(range.bytes));
        if (range.dccAddress != 0) CompletePendingCpuWrites(context, range.dccAddress, DccKeyBytes(range.bytes));
    }
    for (const auto& image : storageTextures) {
        if (!cpuWriters) break;
        CompletePendingCpuWrites(context, image->Descriptor().baseAddress, static_cast<std::size_t>(image->GuestBytes()));
        if (image->Descriptor().dccAddress != 0) CompletePendingCpuWrites(context, image->Descriptor().dccAddress, DccKeyBytes(image->GuestBytes()));
    }
    static const bool noFast = std::getenv("APS5_NO_FAST_REVALIDATE") != nullptr;
    const auto finish = [&](bool fast, bool ok, ProofFailure failure = ProofFailure::Other) {
        if (profile) countRevalidate(fast, ok, start);
        if (report != nullptr) report->failure = ok ? ProofFailure::None : failure;
        return ok;
    };
    const auto serialBefore = StorageTexture::PendingSerial();
    const auto fullWalk = [&] {
        lastFullWalk = std::chrono::steady_clock::now();
        walkImportGeneration = HostImportSetGeneration();
        lookupLog().clear();
        BuildLookups lookups;
        std::size_t textureIndex = 0;
        std::size_t storageIndex = 0;
        for (const auto& shader : shaders) {
            if (shader.program == nullptr) return false;
            for (const auto& binding : shader.program->bindings) {
                if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages) continue;
                if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) {
                    const auto elementWords = binding.count != 0 ? binding.guestDescriptor.size() / binding.count : 0;
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        const auto raw = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
                        std::array<std::uint32_t, 8> narrowed{};
                        const auto words = firstLayerWords(raw, binding.imageShape, narrowed, false);
                        if (IsNullSampledTextureResource(words)) continue;
                        const auto resource = DecodeTextureResource(words);
                        const VkComponentMapping components = ViewComponents(resource);
                        const VkImageViewType multisampleViewType = binding.imageShape == ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
                        const bool depthCompare = !binding.imageDepthCompare.empty() && binding.imageDepthCompare.at(element);
                        if (textureIndex >= textures.size() || cachedTexture(context, words, resource, components, 0, depthCompare, multisampleViewType, layerRowsBinding(binding.imageShape, resource, depthCompare)) != textures[textureIndex]) return false;
                        ++textureIndex;
                    }
                } else if (binding.kind == ShaderRecompiler::DescriptorKind::StorageImage) {
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
                        if (IsNullTextureResource(words)) continue;
                        if (storageIndex >= storageTextures.size()) return false;
                        std::shared_ptr<StorageTexture> expected;
                        if (SameAsPreviousStorageElement(binding, element) && StorageDedupeEnabled()) expected = storageTextures[storageIndex - 1];
                        else expected = cachedStorageTexture(context, words, DecodeTextureResource(words), storageMips[storageIndex], 0, storageWritten[storageIndex]);
                        if (expected != storageTextures[storageIndex]) return false;
                        ++storageIndex;
                    }
                }
            }
        }
        const auto currentTableEntry = [&](const ShaderRecompiler::DescriptorBinding& images, std::uint32_t element) {
            if (textureIndex >= textures.size()) return false;
            const auto raw = std::span<const std::uint32_t>(images.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
            std::array<std::uint32_t, 8> narrowed{};
            const auto words = firstLayerWords(raw, images.imageShapes.at(element), narrowed, false);
            const auto resource = DecodeTextureResource(words);
            const VkImageViewType multisampleViewType = images.imageShapes.at(element) == ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            return cachedTexture(context, words, resource, ViewComponents(resource), 0, images.imageDepthCompare.at(element), multisampleViewType) == textures[textureIndex];
        };
        for (const auto& table : tableStages) {
            if (table.elementTextures.empty()) continue;
            const auto* images = tableImages(shaders, table.stage);
            if (images == nullptr || images->count != table.elementTextures.size()) return false;
            std::vector<std::shared_ptr<Texture>> cached;
            if (auto* reused = FindReusedTable(context.device, *images, CurrentTableState()); reused != nullptr && TableCurrent(*reused) && CachedTableTextures(*reused, cached)) {
                if (!reused->representatives.empty() && reused->representatives != variantRepresentatives(context.device, *images, table.elementBase, context.imageTableCapacity)) return false;
                logTableLookups(*reused, cached);
                auto next = cached.begin();
                bool same = true;
                for (std::uint32_t element = 0; same && element < images->count; ++element) {
                    const auto& entry = reused->elements[element];
                    const auto index = table.elementTextures[element];
                    if (entry.rejected) {
                        same = index == std::numeric_limits<std::size_t>::max();
                        continue;
                    }
                    if (entry.substitute) continue;
                    if (entry.live) {
                        if (index == std::numeric_limits<std::size_t>::max()) continue;
                        same = index == textureIndex && currentTableEntry(*images, element);
                        ++textureIndex;
                        continue;
                    }
                    const auto& texture = *next++;
                    if (index == std::numeric_limits<std::size_t>::max()) continue;
                    same = index == textureIndex && textureIndex < textures.size() && textures[textureIndex] == texture;
                    ++textureIndex;
                }
                if (!same) return false;
                TextureCounts().tablesReused.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (!table.representatives.empty() && table.representatives != variantRepresentatives(context.device, *images, table.elementBase, context.imageTableCapacity)) return false;
            for (std::uint32_t element = 0; element < images->count; ++element) {
                if (table.elementTextures[element] == std::numeric_limits<std::size_t>::max()) continue;
                if (!table.representatives.empty() && table.representatives[element] != NoRepresentative) continue;
                if (table.elementTextures[element] != textureIndex || !currentTableEntry(*images, element)) return false;
                ++textureIndex;
            }
        }
        if (textureIndex != textures.size() || storageIndex != storageTextures.size()) return false;
        captureValidation(spaceReuse);
        return true;
    };
    struct OverlappingTag {};
    auto& overlapping = ThreadScratch<std::vector<PendingOverlap>, OverlappingTag>();
    struct RefreshedTag {};
    auto& refreshed = ThreadScratch<std::vector<PendingOverlap>, RefreshedTag>();
    refreshed.clear();
    FastFail reason = FastFail::Count;
    bool accepted = false;
    bool indexProved = false;
    std::uint64_t indexBaseline = 0;
    if (GuestMemory::WriteIndexEnabled() && !noFast && EpochRevalidate()) {
        if (indexRevalidate(shaders, serialBefore) != IndexOutcome::Fallback) indexProved = true;
        else {
            if (writeSubscription == nullptr) subscribeIndex(shaders);
            if (writeSubscription != nullptr) indexBaseline = GuestMemory::SubscriptionGeneration(*writeSubscription);
        }
    }
    bool fast = indexProved || (!noFast && fastRevalidate(serialBefore, refreshed, reason, overlapping, accepted));
    bool ownRefreshed = false;
    if (!fast && !overlapping.empty()) {
        auto fallback = OwnRefreshFallback::Count;
        if (!OwnImageRefreshEnabled()) fallback = OwnRefreshFallback::Disabled;
        else {
            refreshed = overlapping;
            fallback = refreshOwnObjects(shaders, refreshed);
        }
        if (fallback == OwnRefreshFallback::Count) {
            fast = fastRevalidate(serialBefore, refreshed, reason, overlapping, accepted);
            if (fast) ownRefreshed = true;
            else fallback = OwnRefreshFallback::Rerun;
        }
        if (fallback != OwnRefreshFallback::Count) countOwnRefreshFallback(fallback);
    }
    if (fast && TemplateAuditDue(lastFullWalk) && (AuditEveryInterval() || (walkImportGeneration != HostImportSetGeneration() && ImportAuditAdmitted()))) {
        fast = false;
        ownRefreshed = false;
        reason = FastFail::Audit;
    }
    if (report != nullptr) {
        report->path = !fast ? ProofPath::Full : ownRefreshed ? ProofPath::OwnRefreshed : ProofPath::Fast;
        report->audited = !fast && reason == FastFail::Audit;
    }
    if (!fast) {
        countFullWalk(reason);
        const auto walkStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        const bool walked = fullWalk();
        countFullWalkTime(reason, walkStart);
        if (!walked) {
            const auto failure = [&] {
                switch (reason) {
                    case FastFail::Pending: return ProofFailure::Pending;
                    case FastFail::Evicted: case FastFail::Replaced: return ProofFailure::Evicted;
                    case FastFail::Changed: return ProofFailure::Changed;
                    case FastFail::Keys: case FastFail::ClearedView: case FastFail::StorageKeys: return ProofFailure::Keys;
                    default: return ProofFailure::Other;
                }
            }();
            return finish(false, false, failure);
        }
    } else if (ownRefreshed || accepted) {
        if (profile && ownRefreshed) Revalidations().ownRefreshed.fetch_add(1, std::memory_order_relaxed);
        if (VerifyProofs()) {
            struct VersionsTag {};
            auto& versions = ThreadScratch<std::vector<std::pair<const StorageTexture*, std::uint64_t>>, VersionsTag>();
            versions.clear();
            for (const auto& surface : validatedTextures) {
                if (surface.source != nullptr) versions.emplace_back(surface.source, surface.source->Version());
            }
            for (const auto& image : storageTextures) versions.emplace_back(image.get(), image->Version());
            const bool same = fullWalk();
            const auto moved = std::find_if(versions.begin(), versions.end(), [](const auto& entry) { return entry.first->Version() != entry.second; });
            if (!same || moved != versions.end()) {
                std::fprintf(stderr, "[rescache] APS5_VERIFY_PROOFS: the T1 proof (%s) disagrees with the full walk (%s)\n", ownRefreshed ? "own-object refresh" : "accepted overlap", !same ? "another object" : "an upload");
                std::fflush(stderr);
                std::abort();
            }
            if (profile) Revalidations().proofsVerified.fetch_add(1, std::memory_order_relaxed);
        }
    }
    const auto serialLoop = [&] {
        for (const auto& region : directRegions) {
            auto serial = HostImportSerial(context, region.begin, static_cast<std::size_t>(region.end - region.begin), true);
            if (serial == 0) serial = ImageMirrorSerial(context, region.begin, static_cast<std::size_t>(region.end - region.begin));
            if (serial != region.serial) return false;
            if (region.regionEnd != 0) {
                const auto committed = GuestMemory::DescribeCommitted(region.begin, static_cast<std::size_t>(region.regionEnd - region.begin));
                if (committed.ranges.size() != 1 || committed.ranges.front().first != region.begin || committed.ranges.front().second != region.committedEnd) return false;
            }
        }
        return true;
    };
    if (EpochRevalidate()) {
        if ((!directRegions.empty() || !guestMemory.Writes().empty()) && !(pendingSerialSeen != 0 && pendingSerialSeen == StorageTexture::PendingSerial())) {
            struct RegionsTag {};
            auto& regions = ThreadScratch<std::vector<StorageTexture::PendingQuery>, RegionsTag>();
            regions.clear();
            for (const auto& region : directRegions) regions.push_back({region.begin, region.end, nullptr, nullptr, false});
            for (const auto& [begin, end] : guestMemory.Writes()) regions.push_back({begin, end, nullptr, nullptr, false});
            StorageTexture::ScanPending(regions);
            for (const auto& region : regions) {
                if (region.overlaps || AnyShadowedOverlaps(region.begin, static_cast<std::size_t>(region.end - region.begin))) StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(region.end - region.begin), nullptr, "imported buffer region");
            }
        }
        if (importsReconcilable ? HostImportsReconciledUnchanged(context, importsProof) : HostImportsUnchanged(context, importsProof)) {
            if (profile) Revalidations().importSkips.fetch_add(1, std::memory_order_relaxed);
        } else {
            if (!serialLoop()) return finish(fast, false, ProofFailure::Imports);
            importsProof = HostImportsIdentity(context);
        }
    } else {
        for (const auto& region : directRegions) StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(region.end - region.begin), nullptr, "imported buffer region");
        for (const auto& [begin, end] : guestMemory.Writes()) StorageTexture::FlushPending(begin, static_cast<std::size_t>(end - begin), nullptr, "written buffer range");
        if (!serialLoop()) return finish(fast, false, ProofFailure::Imports);
    }
    if (!guestMemory.StagingDecisionsCurrent()) return finish(fast, false, ProofFailure::Changed);
    if (auto* recorder = Recorder::Active(); recorder != nullptr) {
        try {
            guestMemory.RecordStagingCopies(*recorder);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[resources] staging copies of a reused build failed: %s\n", error.what());
            return finish(fast, false);
        }
    }
    pendingSerialSeen = EpochRevalidate() && StorageTexture::PendingSerial() == serialBefore ? serialBefore : 0;
    if (indexBaseline != 0) {
        noteIndexProof(indexBaseline, serialBefore);
        for (auto& [address, copy] : comparedTableSpans) {
            if (GuestMemory::Accessible(reinterpret_cast<const void*>(address), copy.size())) std::memcpy(copy.data(), reinterpret_cast<const void*>(address), copy.size());
            else copy.clear();
        }
        comparedTableSpans.erase(std::remove_if(comparedTableSpans.begin(), comparedTableSpans.end(), [](const auto& span) { return span.second.empty(); }), comparedTableSpans.end());
    }
    if (writeSubscription != nullptr) {
        for (const auto index : depthCopyTextures) {
            if (textures[index]->HasReuseLookup()) {
                if (!textures[index]->ReuseLookupHolds()) return finish(fast, false, ProofFailure::Changed);
            } else {
                textures[index]->RefreshDepthCopy();
            }
        }
    } else {
        for (const auto& texture : textures) {
            if (texture == nullptr) continue;
            if (texture->HasReuseLookup()) {
                if (!texture->ReuseLookupHolds()) return finish(fast, false, ProofFailure::Changed);
            } else if (texture->IsDepthCopy()) {
                texture->RefreshDepthCopy();
            }
        }
    }
    return finish(fast, true);
}

namespace {

struct KeyMissWindow {
    std::uint64_t compared = 0;
    std::uint64_t noCandidate = 0;
    std::uint64_t unlabelled = 0;
    std::uint64_t identical = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(ContentKeyComponent::Count)> differing{};
};

struct ChurnCounts {
    std::array<std::uint64_t, 8> roles{};
    std::array<std::uint64_t, 4> bufferWords{};
    std::array<std::uint64_t, 8> imageWords{};
    std::uint64_t firstSeen = 0;
    std::uint64_t sameKey = 0;
    std::uint64_t layout = 0;
    std::uint64_t drawWords = 0;
    KeyMissWindow window;
};

struct ChurnProfile {
    AgcDriver::Mutex mutex;
    std::unordered_map<std::uint64_t, ResourceCache::Key> lastByVariant;
    ChurnCounts dispatch;
    ChurnCounts draws;
    std::unordered_map<std::uint64_t, std::uint32_t> tracedByVariant;
    std::size_t tracedLines = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

constexpr std::size_t MaxTracedLines = 256;
constexpr std::uint32_t MaxTracedLinesPerVariant = 4;

const char* componentName(std::size_t component) {
    constexpr std::array<const char*, static_cast<std::size_t>(ContentKeyComponent::Count)> names{"layout", "V# word 0", "V# word 1", "V# word 2", "V# word 3", "image", "sampler", "shader-data", "other", "draw target/index"};
    return names[component];
}

void traceMiss(ChurnProfile& churn, bool draw, std::uint64_t variants, const ContentKeyDiff* parts);

ChurnProfile& Churn() {
    static ChurnProfile profile;
    return profile;
}

bool chargeShaderKey(ChurnCounts& profile, const ResourceCache::Key& key, std::size_t& at, std::size_t end, std::size_t diff) {
    if (at + 5 > end || diff < at + 5) {
        ++profile.layout;
        return true;
    }
    const bool dataWords = key[at] != 0;
    const auto bindings = key[at + 4];
    at += 5;
    for (std::uint32_t binding = 0; binding < bindings; ++binding) {
        if (at + 8 > end || diff < at + 8) {
            ++profile.layout;
            return true;
        }
        const auto kind = key[at];
        const auto role = key[at + 1];
        const auto descriptorWords = dataWords || !DataRole(static_cast<ShaderRecompiler::DescriptorRole>(role)) ? key[at + 7] : 0u;
        at += 8;
        if (diff < at + descriptorWords) {
            if (role < profile.roles.size()) ++profile.roles[role];
            const auto index = diff - at;
            if (role == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorRole::GuestBuffers)) ++profile.bufferWords[index % 4];
            else if (role == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorRole::GuestImages) && (kind == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorKind::SampledImage) || kind == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorKind::StorageImage))) ++profile.imageWords[index % 8];
            return true;
        }
        at += descriptorWords;
        for (int flags = 0; flags < 10; ++flags) {
            if (at >= end) {
                ++profile.layout;
                return true;
            }
            const auto words = 1 + (static_cast<std::size_t>(key[at]) + 31) / 32;
            if (diff < at + words) {
                ++profile.layout;
                return true;
            }
            at += words;
        }
        for (const auto listRole : {ShaderRecompiler::DescriptorRole::GuestSamplers, ShaderRecompiler::DescriptorRole::GuestImages}) {
            if (at >= end) {
                ++profile.layout;
                return true;
            }
            const auto words = 1 + static_cast<std::size_t>(key[at]);
            if (diff < at + words) {
                ++profile.roles[static_cast<std::size_t>(listRole)];
                return true;
            }
            at += words;
        }
    }
    return false;
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
}
}
}
