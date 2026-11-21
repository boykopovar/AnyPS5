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
bool AnyPendingCpuWrites(const Context& context) {
    if (Recorder::InCompletion() || Recorder::Active() == nullptr) return false;
    return (context.copiedWriters != nullptr && !context.copiedWriters->empty()) || !DrawCopiedWriters()->empty() || Recorder::PendingCompletionLabels() != 0;
}

bool CompletePendingCpuWrites(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || Recorder::InCompletion()) return false;
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) return false;
    const auto drawWriters = DrawCopiedWriters();
    const bool labels = Recorder::PendingCompletionLabels() != 0;
    if ((context.copiedWriters == nullptr || context.copiedWriters->empty()) && drawWriters->empty() && !labels) return false;
    if (!recorder->PendingWriteOverlaps(address, bytes)) return false;
    const auto overlaps = [&](const auto& writers) {
        return std::any_of(writers.begin(), writers.end(), [&](const auto& writer) { return writer->CopiedWritesOverlap(address, bytes); });
    };
    if ((context.copiedWriters != nullptr && overlaps(*context.copiedWriters)) || overlaps(*drawWriters) || (labels && recorder->CompletionLabelIn(address, bytes))) {
        Recorder::CountSync(4);
        recorder->SyncThrough(address, bytes);
        return true;
    }
    return false;
}

const ShaderRecompiler::DescriptorBinding* ShaderResources::tableImages(std::span<const CompiledShader> shaders, ShaderRecompiler::ShaderStage stage) {
    for (const auto& shader : shaders) {
        if (shader.stage != stage || shader.program == nullptr) continue;
        for (const auto& binding : shader.program->bindings) {
            if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable) return &binding;
        }
    }
    return nullptr;
}

bool ShaderResources::EpochProofs() {
    return DrawPipelineEnabled();
}

namespace {
void noteEpochProof(bool reused, bool verified, bool failed) {
    static std::atomic<std::uint64_t> reuses{0}, proofs{0}, checks{0}, failures{0};
    if (verified) checks.fetch_add(1, std::memory_order_relaxed);
    if (failed) failures.fetch_add(1, std::memory_order_relaxed);
    const auto count = (reused ? reuses : proofs).fetch_add(1, std::memory_order_relaxed) + 1u;
    static const bool report = std::getenv("APS5_PROFILE_DRAW") != nullptr || std::getenv("APS5_VERIFY_DRAW_PIPELINE") != nullptr;
    if (!report) return;
    if (count % 262144u == 0u || (count & (count - 1u)) == 0u) std::fprintf(stderr, "[epoch-proofs] resource proofs: %llu reused the epoch's, %llu proved; verify: %llu checked, %llu failed; storage refreshes reused %llu\n", static_cast<unsigned long long>(reuses.load()), static_cast<unsigned long long>(proofs.load()), static_cast<unsigned long long>(checks.load()), static_cast<unsigned long long>(failures.load()), static_cast<unsigned long long>(StorageTexture::epochRefreshes.load()));
}
}

namespace {

constexpr std::size_t TableSpanElement = std::numeric_limits<std::size_t>::max();

bool IndexBroken(const char* part) {
    static const std::string broken = [] {
        const char* text = std::getenv("APS5_TABLE_INDEX_BREAK");
        return std::string(text != nullptr ? text : "");
    }();
    return !broken.empty() && broken.find(part) != std::string::npos;
}

}

void ShaderResources::subscribeIndex(std::span<const CompiledShader> shaders) {
    writeSubscription.reset();
    subscribedGeneration = 0;
    subscribedPendingSerial = 0;
    subscribedBlocks.clear();
    perElementTextures.clear();
    comparedTextures.clear();
    comparedTableSpans.clear();
    depthCopyTextures.clear();
    indexElements.assign(textures.size(), {});
    for (std::size_t i = 0; i < textures.size(); ++i) {
        if (textures[i] != nullptr && (textures[i]->IsDepthCopy() || textures[i]->HasReuseLookup())) depthCopyTextures.push_back(i);
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    ranges.reserve(textures.size() + 4);
    for (std::size_t i = 0; i < textures.size(); ++i) {
        if (i >= validatedTextures.size()) {
            perElementTextures.push_back(i);
            continue;
        }
        const auto& surface = validatedTextures[i];
        const auto begin = surface.resource.baseAddress;
        const auto bytes = surface.bytes;
        if (!surface.valid || surface.depth || bytes == 0) {
            perElementTextures.push_back(i);
            continue;
        }
        if (!GuestMemory::Watched(begin, static_cast<std::size_t>(bytes))) {
            comparedTextures.push_back(i);
            continue;
        }
        if (surface.source != nullptr || surface.resource.dccAddress != 0) {
            perElementTextures.push_back(i);
            continue;
        }
        ranges.emplace_back(begin, begin + bytes);
        for (auto block = GuestMemory::WriteBlockOf(begin); block <= GuestMemory::WriteBlockOf(begin + bytes - 1); ++block) subscribedBlocks.emplace_back(block, i);
    }
    for (const auto& table : tableStages) {
        if (table.images == nullptr || table.map.size() < ShaderRecompiler::ImageTableAbi::HeaderWords) continue;
        const auto tables = table.map[ShaderRecompiler::ImageTableAbi::HeaderTableCount];
        for (std::uint32_t index = 0; index < tables; ++index) {
            const auto header = ShaderRecompiler::ImageTableAbi::TableHeader(index);
            if (static_cast<std::size_t>(header) + ShaderRecompiler::ImageTableAbi::TableWords > table.map.size()) break;
            const auto base = static_cast<std::uint64_t>(table.map[header + ShaderRecompiler::ImageTableAbi::TableBaseLow]) | (static_cast<std::uint64_t>(table.map[header + ShaderRecompiler::ImageTableAbi::TableBaseHigh]) << 32);
            const auto size = static_cast<std::uint64_t>(table.map[header + ShaderRecompiler::ImageTableAbi::TableSizeLow]) | (static_cast<std::uint64_t>(table.map[header + ShaderRecompiler::ImageTableAbi::TableSizeHigh]) << 32);
            if (base == 0 || size == 0 || size > (std::uint64_t{64} << 20) || size > std::numeric_limits<std::uint64_t>::max() - base) continue;
            if (GuestMemory::Watched(base, static_cast<std::size_t>(size))) {
                ranges.emplace_back(base, base + size);
                for (auto block = GuestMemory::WriteBlockOf(base); block <= GuestMemory::WriteBlockOf(base + size - 1); ++block) subscribedBlocks.emplace_back(block, TableSpanElement);
            } else if (GuestMemory::Accessible(reinterpret_cast<const void*>(base), static_cast<std::size_t>(size))) {
                std::vector<std::byte> copy(static_cast<std::size_t>(size));
                std::memcpy(copy.data(), reinterpret_cast<const void*>(base), copy.size());
                comparedTableSpans.emplace_back(base, std::move(copy));
            }
        }
    }
    std::sort(subscribedBlocks.begin(), subscribedBlocks.end());
    std::size_t textureIndex = 0;
    for (const auto& shader : shaders) {
        if (shader.program == nullptr) break;
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages || binding.kind != ShaderRecompiler::DescriptorKind::SampledImage) continue;
            const auto elementWords = binding.count != 0 ? binding.guestDescriptor.size() / binding.count : 0;
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
                if (IsNullTextureResource(words)) continue;
                if (textureIndex < indexElements.size() && words.size() <= 8) {
                    auto& entry = indexElements[textureIndex];
                    std::copy(words.begin(), words.end(), entry.words.begin());
                    entry.count = static_cast<std::uint32_t>(words.size());
                    entry.shape = binding.imageShape;
                    entry.depthCompare = !binding.imageDepthCompare.empty() && binding.imageDepthCompare.at(element);
                    entry.known = true;
                }
                ++textureIndex;
            }
        }
    }
    for (const auto& table : tableStages) {
        if (table.images == nullptr) continue;
        for (std::size_t element = 0; element < table.elementTextures.size(); ++element) {
            const auto index = table.elementTextures[element];
            if (index >= indexElements.size() || (element + 1) * 8u > table.images->guestDescriptor.size()) continue;
            auto& entry = indexElements[index];
            const auto words = std::span<const std::uint32_t>(table.images->guestDescriptor).subspan(element * 8u, 8u);
            std::copy(words.begin(), words.end(), entry.words.begin());
            entry.count = 8;
            entry.shape = element < table.images->imageShapes.size() ? std::optional(table.images->imageShapes[element]) : std::nullopt;
            entry.depthCompare = element < table.images->imageDepthCompare.size() && table.images->imageDepthCompare[element];
            entry.known = true;
        }
    }
    writeSubscription = GuestMemory::SubscribeWrites(ranges);
}

void ShaderResources::subscribeBuilt(std::span<const CompiledShader> shaders) {
    if (!reusable || !EpochRevalidate() || std::getenv("APS5_NO_FAST_REVALIDATE") != nullptr) return;
    subscribeIndex(shaders);
    if (writeSubscription == nullptr) return;
    const auto generation = GuestMemory::SubscriptionGeneration(*writeSubscription);
    std::vector<GuestMemory::UnchangedQuery> queries;
    queries.reserve(textures.size());
    std::size_t last = std::numeric_limits<std::size_t>::max();
    for (const auto& [block, element] : subscribedBlocks) {
        if (element == TableSpanElement || element == last) continue;
        last = element;
        const auto& surface = validatedTextures[element];
        if (surface.generation == 0) return;
        queries.push_back({surface.resource.baseAddress, static_cast<std::size_t>(surface.bytes), surface.generation});
    }
    if (!queries.empty() && !GuestMemory::UnchangedSinceAll(queries)) return;
    subscribedGeneration = generation;
    subscribedPendingSerial = 0;
}

void ShaderResources::noteIndexProof(std::uint64_t generation, std::uint64_t serialBefore) {
    if (writeSubscription == nullptr) return;
    subscribedGeneration = generation;
    subscribedPendingSerial = StorageTexture::PendingSerial() == serialBefore ? serialBefore : 0;
    GuestMemory::TrimSubscriptionBumps(*writeSubscription, generation);
}

bool ShaderResources::reresolveElements(const std::vector<std::size_t>& elements, std::vector<std::size_t>& replacedElements) {
    auto* recorder = Recorder::Active();
    if (elements.empty()) return true;
    if (recorder == nullptr) return false;
    if (indexImagePositions.empty()) {
        std::size_t position = 0;
        for (const auto& binding : bindings) {
            if (binding.flags != 0) {
                for (const auto index : binding.imageAllocations) {
                    if (index == std::numeric_limits<std::size_t>::max()) continue;
                    if (binding.layout.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER) indexImagePositions.push_back({index, position, true});
                    ++position;
                }
                continue;
            }
            if (binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) continue;
            for (const auto index : binding.imageAllocations) {
                if (binding.layout.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && index != std::numeric_limits<std::size_t>::max()) indexImagePositions.push_back({index, position, false});
                ++position;
            }
        }
        if (position != descriptorImages.size()) {
            indexImagePositions.clear();
            return false;
        }
        std::sort(indexImagePositions.begin(), indexImagePositions.end(), [](const ImagePosition& left, const ImagePosition& right) { return left.texture < right.texture; });
    }
    lookupLog().clear();
    BuildLookups lookups;
    std::vector<std::pair<std::size_t, std::shared_ptr<Texture>>> replaced;
    for (const auto i : elements) {
        if (i >= indexElements.size() || i >= textures.size() || !indexElements[i].known) return false;
        const auto& element = indexElements[i];
        std::array<std::uint32_t, 8> narrowed{};
        const auto words = firstLayerWords(std::span<const std::uint32_t>(element.words.data(), element.count), element.shape, narrowed, false);
        const auto resource = DecodeTextureResource(words);
        const VkImageViewType multisampleViewType = element.shape == ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        auto texture = cachedTexture(context, words, resource, ViewComponents(resource), 0, element.depthCompare, multisampleViewType);
        if (texture == nullptr) return false;
        if (texture != textures[i]) replaced.emplace_back(i, std::move(texture));
    }
    if (!replaced.empty() && (!drawBuild || usesBda)) {
        lookupLog().clear();
        return false;
    }
    for (auto& [i, texture] : replaced) {
        replacedElements.push_back(i);
        recorder->Keep(textures[i]);
        textures[i] = std::move(texture);
        depthCopyTextures.erase(std::remove(depthCopyTextures.begin(), depthCopyTextures.end(), i), depthCopyTextures.end());
        if (textures[i]->IsDepthCopy() || textures[i]->HasReuseLookup()) depthCopyTextures.insert(std::lower_bound(depthCopyTextures.begin(), depthCopyTextures.end(), i), i);
        const auto first = std::lower_bound(indexImagePositions.begin(), indexImagePositions.end(), i, [](const ImagePosition& entry, std::size_t texture) { return entry.texture < texture; });
        for (auto entry = first; entry != indexImagePositions.end() && entry->texture == i; ++entry) {
            const auto& current = textures[i];
            const auto view = entry->table ? (textureFirstLayer[i] ? current->FirstLayerView() : current->View()) : textureSrgb[i] ? current->SrgbView(textureFirstLayer[i], textureBaseLevel[i]) : textureBaseLevel[i] ? current->BaseLevelView() : textureFirstLayer[i] ? current->FirstLayerView() : current->View();
            descriptorImages[entry->position] = {VK_NULL_HANDLE, view, current->Layout()};
        }
        rebound = true;
        countIndex(IndexRebound);
    }
    const auto& log = lookupLog();
    for (const auto i : elements) {
        const auto found = std::find_if(log.rbegin(), log.rend(), [&](const LookupRecord& record) { return record.object == textures[i].get(); });
        if (found == log.rend()) {
            validatedTextures[i].valid = false;
            lookupLog().clear();
            return false;
        }
        validatedTextures[i] = {found->resource, found->bytes, found->keys, found->generation, 0, found->source, found->depth, true, found->committedGeneration, found->committed};
    }
    lookupLog().clear();
    return true;
}

ShaderResources::IndexOutcome ShaderResources::indexRevalidate(std::span<const CompiledShader> shaders, std::uint64_t serialBefore) {
    static_cast<void>(shaders);
    if (writeSubscription == nullptr || subscribedGeneration == 0) return IndexOutcome::Fallback;
    const bool profiled = BuildProfiled();
    const auto start = profiled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto outcome = [&](IndexOutcome result) {
        countIndex(result == IndexOutcome::Accepted ? IndexAccepted : result == IndexOutcome::Partial ? IndexPartial : IndexFallback);
        if (!profiled) return result;
        Revalidations().indexNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
        return result;
    };
    const auto fallback = [&] { return outcome(IndexOutcome::Fallback); };
    auto& subscription = *writeSubscription;
    for (const auto& [begin, end] : GuestMemory::SubscriptionSpans(subscription)) {
        if (!FlushDepthPlanes(begin, static_cast<std::size_t>(end - begin)).empty()) return fallback();
    }
    if (!GuestMemory::SweepSubscription(subscription)) return fallback();
    if ((subscribedPendingSerial == 0 || subscribedPendingSerial != serialBefore) && !IndexBroken("pending")) {
        thread_local std::vector<StorageTexture::PendingQuery>* regionsSlot = nullptr;
        auto& regions = ShaderRecompiler::ThreadOwned(regionsSlot);
        regions.clear();
        for (const auto& [begin, end] : GuestMemory::SubscriptionSpans(subscription)) regions.push_back({begin, end, nullptr, nullptr, false});
        if (!regions.empty()) {
            StorageTexture::ScanPending(regions);
            if (std::any_of(regions.begin(), regions.end(), [](const StorageTexture::PendingQuery& region) { return region.overlaps; })) return fallback();
        }
    }
    for (const auto& [address, bytes] : comparedTableSpans) {
        if (GuestMemory::CompareMapped(address, bytes) != GuestMemory::Compare::Equal) return fallback();
    }
    const auto generation = GuestMemory::SubscriptionGeneration(subscription);
    thread_local std::vector<std::size_t>* checkSlot = nullptr;
    auto& check = ShaderRecompiler::ThreadOwned(checkSlot);
    thread_local std::vector<std::size_t>* bumpedSlot = nullptr;
    auto& bumped = ShaderRecompiler::ThreadOwned(bumpedSlot);
    thread_local std::vector<std::size_t>* lookedSlot = nullptr;
    auto& looked = ShaderRecompiler::ThreadOwned(lookedSlot);
    thread_local std::vector<std::uint64_t>* blocksSlot = nullptr;
    auto& blocks = ShaderRecompiler::ThreadOwned(blocksSlot);
    thread_local std::vector<std::uint64_t>* lookupBlocksSlot = nullptr;
    auto& lookupBlocks = ShaderRecompiler::ThreadOwned(lookupBlocksSlot);
    check = perElementTextures;
    bumped.clear();
    looked.clear();
    const auto elementsOf = [&](const std::vector<std::uint64_t>& from, std::vector<std::size_t>& into) {
        for (const auto block : from) {
            auto entry = std::lower_bound(subscribedBlocks.begin(), subscribedBlocks.end(), std::pair<std::uint64_t, std::size_t>{block, 0});
            for (; entry != subscribedBlocks.end() && entry->first == block; ++entry) {
                if (entry->second == TableSpanElement) return false;
                into.push_back(entry->second);
            }
        }
        std::sort(into.begin(), into.end());
        into.erase(std::unique(into.begin(), into.end()), into.end());
        return true;
    };
    const bool partial = generation != subscribedGeneration;
    if (partial) {
        if (!GuestMemory::SubscriptionBumpsSince(subscription, subscribedGeneration, blocks, lookupBlocks)) return fallback();
        if (!elementsOf(blocks, bumped) || !elementsOf(lookupBlocks, looked)) return fallback();
        countIndex(IndexBumped, bumped.size());
        check.insert(check.end(), bumped.begin(), bumped.end());
        check.insert(check.end(), looked.begin(), looked.end());
        std::sort(check.begin(), check.end());
        check.erase(std::unique(check.begin(), check.end()), check.end());
    }
    thread_local std::vector<std::size_t>* replacedSlot = nullptr;
    auto& replaced = ShaderRecompiler::ThreadOwned(replacedSlot);
    replaced.clear();
    if (!looked.empty()) {
        countIndex(IndexLookedUp, looked.size());
        if (!reresolveElements(looked, replaced)) return fallback();
    }
    if (!comparedTextures.empty() && !IndexBroken("compared")) {
        countIndex(IndexCompared, comparedTextures.size());
        if (!reresolveElements(comparedTextures, replaced)) return fallback();
    }
    if (!check.empty() || !storageTextures.empty()) {
        thread_local std::vector<PendingOverlap>* overlappingSlot = nullptr;
        auto& overlapping = ShaderRecompiler::ThreadOwned(overlappingSlot);
        FastFail reason = FastFail::Count;
        bool accepted = false;
        indexProof = true;
        struct Done {
            bool& flag;
            ~Done() { flag = false; }
        } done{indexProof};
        if (!fastRevalidate(serialBefore, {}, reason, overlapping, accepted, &check, true)) {
            if (!partial || bumped.empty() || reason != FastFail::Changed) return fallback();
            if (!reresolveElements(bumped, replaced)) return fallback();
            if (!fastRevalidate(serialBefore, {}, reason, overlapping, accepted, &check, true)) return fallback();
        }
    }
    auto proved = generation;
    if (!replaced.empty()) {
        std::sort(replaced.begin(), replaced.end());
        const auto now = GuestMemory::SubscriptionGeneration(subscription);
        thread_local std::vector<std::size_t>* absorbedSlot = nullptr;
        auto& absorbed = ShaderRecompiler::ThreadOwned(absorbedSlot);
        absorbed.clear();
        const auto own = [&](std::size_t element) { return std::binary_search(replaced.begin(), replaced.end(), element) && !textures[element]->Released(); };
        if (now != generation && GuestMemory::SubscriptionBumpsSince(subscription, generation, blocks, lookupBlocks) && blocks.empty() && elementsOf(lookupBlocks, absorbed) && std::all_of(absorbed.begin(), absorbed.end(), own)) proved = now;
    }
    noteIndexProof(proved, serialBefore);
    return outcome(partial || !replaced.empty() ? IndexOutcome::Partial : IndexOutcome::Accepted);
}

bool ShaderResources::Revalidate(std::span<const CompiledShader> shaders, ProofReport* report) {
    if (!registeredImageViewsCurrent(registeredImageViews)) {
        if (report != nullptr) *report = {ProofPath::Full, ProofFailure::Other};
        provedEpoch = 0;
        return false;
    }
    if (EpochProofs()) {
        const auto epoch = GuestMemory::CollectEpoch();
        const auto proof = GuestMemory::ProofSerial();
        const auto pending = StorageTexture::PendingSerial();
        const auto retired = RetiredImportSerial();
        if (epoch != 0 && provedEpoch == epoch && provedProof == proof && provedPending == pending && provedRetired == retired) {
            static const bool verify = std::getenv("APS5_VERIFY_DRAW_PIPELINE") != nullptr;
            thread_local std::uint64_t reuses = 0;
            if (verify && ++reuses % 64u == 0u) {
                const bool ok = revalidateFull(shaders, report);
                noteEpochProof(true, true, !ok);
                return ok;
            }
            if (report != nullptr) *report = {ProofPath::Full, ProofFailure::None};
            noteEpochProof(true, false, false);
            return true;
        }
        const bool ok = revalidateFull(shaders, report);
        noteEpochProof(false, false, false);
        if (ok) {
            provedEpoch = std::any_of(textures.begin(), textures.end(), [](const std::shared_ptr<Texture>& texture) { return texture != nullptr && texture->HasReuseLookup(); }) ? 0 : epoch;
            provedProof = proof;
            provedPending = pending;
            provedRetired = retired;
        } else {
            provedEpoch = 0;
        }
        return ok;
    }
    return revalidateFull(shaders, report);
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
