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
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::uint64_t SampledLookupKeyScans() {
    return sampledLookupKeyScans;
}

std::uint64_t SampledLookupKeyProofs() {
    return sampledLookupKeyProofs;
}

std::uint64_t SampledTexturesCleared() {
    return TextureCounts().cleared.load(std::memory_order_relaxed);
}

std::uint64_t SampledTexturesImported() {
    return TextureCounts().imported.load(std::memory_order_relaxed);
}

ImageTableReuseCounts ImageTableReuses() {
    const auto& counters = TextureCounts();
    return {counters.tablesReused.load(std::memory_order_relaxed) + counters.tablesReplayed.load(std::memory_order_relaxed), counters.tablesResolved.load(std::memory_order_relaxed), counters.tablesReplayed.load(std::memory_order_relaxed), counters.tableWritesKept.load(std::memory_order_relaxed), counters.tableWritesMade.load(std::memory_order_relaxed), counters.variantSurfaces.load(std::memory_order_relaxed), counters.variantSubstituted.load(std::memory_order_relaxed), counters.variantRejected.load(std::memory_order_relaxed), counters.arrayViewsShared.load(std::memory_order_relaxed)};
}

void ShaderResources::finishImageTables() {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    if (tableStages.empty()) return;
    const auto overlaps = [](std::uint64_t base, std::uint64_t size, std::uint64_t address, std::uint64_t bytes) { return size != 0 && bytes != 0 && (base <= address ? address - base < size : base - address < bytes); };
    std::vector<BdaResources::ImageTableFaults> faults;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (auto& table : tableStages) {
        auto& map = table.map;
        for (const auto& [entry, code] : table.samplerRejections) map[map[Abi::HeaderSamplerFaults] + entry] = code;
        for (std::uint32_t index = 0; index < map[Abi::HeaderTableCount]; ++index) {
            const auto header = Abi::TableHeader(index);
            if (map[header + Abi::TableFault] != 0u) continue;
            const auto base = static_cast<std::uint64_t>(map[header + Abi::TableBaseLow]) | (static_cast<std::uint64_t>(map[header + Abi::TableBaseHigh]) << 32u);
            const auto size = static_cast<std::uint64_t>(map[header + Abi::TableSizeLow]) | (static_cast<std::uint64_t>(map[header + Abi::TableSizeHigh]) << 32u);
            const auto tableOverlaps = [&](std::uint64_t address, std::uint64_t bytes) {
                if (overlaps(base, size, address, bytes)) return true;
                if (index >= table.program->imageTableReadRanges.size()) return false;
                const auto& reads = table.program->imageTableReadRanges[index];
                return std::any_of(reads.begin(), reads.end(), [&](const auto& range) { return overlaps(range.first, range.second, address, bytes); });
            };
            bool written = false;
            for (const auto& allocation : allocations) written = written || (allocation.guest && allocation.written && tableOverlaps(allocation.address, allocation.size));
            for (std::size_t image = 0; image < storageTextures.size(); ++image) written = written || (storageWritten[image] && storageTextures[image] != nullptr && tableOverlaps(storageTextures[image]->Descriptor().baseAddress, storageTextures[image]->GuestBytes()));
            if (!written) continue;
            ShaderRecompiler::ImageTableEntryPoison poison{};
            poison.resource = index;
            poison.reason = static_cast<std::uint32_t>(Abi::PoisonReason::HeapWritten);
            table.poison.push_back(poison);
            table.messages.push_back(std::make_shared<const std::string>("the table at 0x" + HexText(base) + " is written by its own dispatch"));
            map[header + Abi::TableFault] = Abi::PoisonCode(static_cast<std::uint32_t>(table.poison.size() - 1u));
            faultChecks = true;
        }
        map[Abi::HeaderPoisonCount] = static_cast<std::uint32_t>(table.poison.size());
        auto& allocation = allocations.at(table.mapAllocation);
        Require(allocation.buffer != nullptr && allocation.size == map.size() * sizeof(std::uint32_t), "image table map buffer disagrees with the map");
        std::memcpy(allocation.buffer->Bytes().data(), map.data(), allocation.size);
        faults.push_back({IrStage(table.stage), table.program->imageTableShader, table.poison, table.messages});
        ranges.insert(ranges.end(), table.program->imageTableRanges.begin(), table.program->imageTableRanges.end());
    }
    if (bda == nullptr) return;
    bda->SetImageTables(std::move(faults), std::move(ranges));
}

std::vector<PresyncSurface> ShaderResources::PresyncSurfaces() const {
    std::vector<PresyncSurface> surfaces;
    const auto consider = [&](std::uint32_t format, std::uint64_t address, std::uint64_t guestBytes, bool sampled) {
        const bool gpuDirect = sampled ? SampledFromStorageEligible(context, format, address, guestBytes) : HostImportCovers(context, address, static_cast<std::size_t>(guestBytes));
        if (!gpuDirect) surfaces.push_back({address, guestBytes, false});
    };
    static const bool keyRanges = std::getenv("APS5_NO_DCC_PRESYNC") == nullptr;
    const auto considerKeys = [&](std::uint64_t dccAddress, std::uint64_t guestBytes) {
        const auto keyBytes = DccKeyBytes(guestBytes);
        if (keyRanges && dccAddress != 0 && keyBytes != 0) surfaces.push_back({dccAddress, keyBytes, true});
    };
    if (completed) {
        std::size_t textureIndex = 0;
        for (const auto& range : describedRanges) {
            if (std::strcmp(range.kind, "texture") == 0 && textureIndex < textures.size()) {
                const auto& texture = textures[textureIndex++];
                if (texture->ViewsStorageImage()) consider(range.format, range.address, range.bytes, false);
                else consider(range.format, range.address, range.bytes, true);
                considerKeys(range.dccAddress, range.bytes);
            }
        }
        for (std::size_t index = 0; index < storageTextures.size(); ++index) {
            if (storageTextures[index] == nullptr || (index != 0 && storageTextures[index] == storageTextures[index - 1])) continue;
            const auto& image = *storageTextures[index];
            consider(image.Descriptor().format, image.Descriptor().baseAddress, image.GuestBytes(), false);
            considerKeys(image.Descriptor().dccAddress, image.GuestBytes());
        }
        return surfaces;
    }
    if (!imageRecords.empty()) {
        for (const auto& record : imageRecords) {
            if (!record.decoded) continue;
            consider(record.resource.format, record.resource.baseAddress, record.guestBytes, record.sampled);
            considerKeys(record.resource.dccAddress, record.guestBytes);
        }
        return surfaces;
    }
    if (deferredCompute.program == nullptr) return surfaces;
    forEachImageElement(*deferredCompute.program, [&](const ShaderRecompiler::DescriptorBinding& binding, std::uint32_t, std::span<const std::uint32_t> words) {
        if (IsNullTextureResource(words)) return;
        const auto resource = DecodeTextureResource(words);
        const auto guestBytes = SurfaceGuestBytes(resource);
        consider(resource.format, resource.baseAddress, guestBytes, binding.kind == ShaderRecompiler::DescriptorKind::SampledImage);
        considerKeys(resource.dccAddress, guestBytes);
    });
    for (const auto& binding : deferredCompute.program->bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::ImageTable) continue;
        for (std::size_t element = 0; element < binding.guestDescriptor.size() / 8u; ++element) {
            try {
                const auto resource = DecodeTextureResource(std::span<const std::uint32_t>(binding.guestDescriptor).subspan(element * 8u, 8u));
                const auto guestBytes = SurfaceGuestBytes(resource);
                consider(resource.format, resource.baseAddress, guestBytes, true);
                considerKeys(resource.dccAddress, guestBytes);
            } catch (const std::runtime_error&) {
            }
        }
    }
    return surfaces;
}

ShaderResources::~ShaderResources() {
    release();
}

std::shared_ptr<const TableDescriptors> ShaderResources::keptTable(std::size_t set, std::shared_ptr<const std::vector<std::weak_ptr<Texture>>>& held) noexcept {
    const auto& table = tableWrites[set];
    if (!table.written || KeptSets() == 0u || table.firstImage + table.images > descriptorImages.size()) return nullptr;
    try {
        if (held == nullptr) {
            std::vector<std::weak_ptr<Texture>> weak;
            weak.reserve(textures.size());
            for (const auto& texture : textures) {
                if (texture != nullptr) weak.push_back(texture);
            }
            held = std::make_shared<const std::vector<std::weak_ptr<Texture>>>(std::move(weak));
        }
        auto kept = std::make_shared<TableDescriptors>();
        kept->binding = table.binding;
        kept->runs = table.runs;
        kept->images.assign(descriptorImages.begin() + static_cast<std::ptrdiff_t>(table.firstImage), descriptorImages.begin() + static_cast<std::ptrdiff_t>(table.firstImage + table.images));
        kept->textures = held;
        return kept;
    } catch (...) {
        return nullptr;
    }
}

void ShaderResources::release() noexcept {
    std::shared_ptr<const std::vector<std::weak_ptr<Texture>>> held;
    if (cacheAllocation.pool && context.descriptorCache != nullptr) context.descriptorCache->Free(cacheAllocation, keptTable(0, held));
    if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    if (_layout && ownsLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, _layout, nullptr);
    if (fragment.cacheAllocation.pool && context.descriptorCache != nullptr) context.descriptorCache->Free(fragment.cacheAllocation, keptTable(1, held));
    if (fragment.pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, fragment.pool, nullptr);
    if (fragment.layout && fragment.ownsLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, fragment.layout, nullptr);
    cacheAllocation = {};
    pool = VK_NULL_HANDLE;
    _set = VK_NULL_HANDLE;
    _layout = VK_NULL_HANDLE;
    fragment = {};
    tableWrites = {};
}

std::uint32_t ShaderResources::setOf(const Binding& binding) const {
    constexpr auto count = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Count);
    if (binding.set >= 0) return static_cast<std::uint32_t>(binding.set);
    return splitSets && binding.layout.binding / count == static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Stage::Fragment) ? 1u : 0u;
}

VkDescriptorSetLayout ShaderResources::Layout() const {
    return _layout;
}

std::vector<VkDescriptorSetLayoutBinding> ShaderResources::LayoutBindings() const {
    std::vector<VkDescriptorSetLayoutBinding> result;
    for (const auto& binding : bindings) result.push_back(binding.layout);
    return result;
}

bool ShaderResources::LayoutUpdateAfterBind() const {
    return updateAfterBind;
}

ShaderResources::DrawBindings::~DrawBindings() {
    if (cache == nullptr) return;
    const auto release = [&](VkDescriptorSetLayout setLayout, const DescriptorCache::SetAllocation& set) {
        if (set.set == VK_NULL_HANDLE) return;
        if (setLayout != VK_NULL_HANDLE) cache->Recycle(setLayout, set);
        else cache->Free(set);
    };
    release(layout, allocation);
    release(fragmentLayout, fragmentAllocation);
}

std::optional<std::vector<ShaderResources::MovedBuffer>> ShaderResources::MovedReadOnlyBuffers(std::span<const CompiledShader> shaders, Recorder& recorder, const ColorTarget* target, bool* patchedData) const {
    std::vector<MovedBuffer> moved;
    std::size_t movedBytes = 0;
    if ((_set == VK_NULL_HANDLE && fragment.set == VK_NULL_HANDLE) || (usesBda && !spaceReuse)) return moved;
    for (const auto& shader : shaders) {
        if (shader.program == nullptr) return std::nullopt;
        for (const auto& binding : shader.program->bindings) {
            if (DataRole(binding.role)) {
                const auto kept = std::find_if(bindings.begin(), bindings.end(), [&](const Binding& item) { return item.layout.binding == binding.binding; });
                if (kept == bindings.end() || kept->allocations.size() != 1) return std::nullopt;
                const auto index = kept->allocations.front();
                const auto& item = allocations[index];
                if (item.guest || item.buffer == nullptr || item.size != binding.guestDescriptor.size() * sizeof(std::uint32_t)) return std::nullopt;
                const bool patched = std::any_of(dataPatches.begin(), dataPatches.end(), [&](const DataPatch& patch) { return patch.allocation == index; });
                const bool same = !item.dataWords.empty() ? item.dataWords == binding.guestDescriptor : !patched && std::memcmp(item.buffer->Bytes().data(), binding.guestDescriptor.data(), item.size) == 0;
                if (same) continue;
                if (item.dataWords.empty() && patched) {
                    if (patchedData != nullptr) *patchedData = true;
                    return std::nullopt;
                }
                if (moved.empty()) moved.reserve(allocations.size());
                moved.push_back({index, 0, item.size, binding.guestDescriptor});
                continue;
            }
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
            const auto kept = std::find_if(bindings.begin(), bindings.end(), [&](const Binding& item) { return item.layout.binding == binding.binding; });
            if (kept == bindings.end() || kept->allocations.size() != binding.count || binding.guestDescriptor.size() != static_cast<std::size_t>(binding.count) * 4u) return std::nullopt;
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const auto* words = binding.guestDescriptor.data() + static_cast<std::size_t>(element) * 4u;
                const ShaderRecompiler::ShaderBufferResource descriptor{{words[0], words[1], words[2], words[3]}};
                const auto address = descriptor.Base48();
                const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                const auto size = BytesUpToTarget(address, descriptor.GetSize(), descriptor.Stride(), written ? nullptr : target);
                const auto index = kept->allocations[element];
                const auto& item = allocations[index];
                const bool empty = size == 0 || address == 0;
                if (!item.guest) {
                    if (empty) continue;
                    return std::nullopt;
                }
                if (item.address == address && item.size == size) continue;
                if (written || empty || item.written || size > context.limits.maxStorageBufferRange) return std::nullopt;
                const auto begin = address - item.adjustment;
                const auto bytes = static_cast<std::size_t>(GuestBufferMemory::ViewBytes(size, item.adjustment));
                if (bytes > maxDrawSnapshotBytes - movedBytes) return std::nullopt;
                if (!GuestMemory::Accessible(reinterpret_cast<const void*>(begin), bytes)) return std::nullopt;
                if (guestMemory.WritesOverlap(begin, bytes) || PendingStorageOverlaps(begin, bytes, nullptr) || AnyShadowedOverlaps(begin, bytes)) return std::nullopt;
                MovedBuffer entry{index, address, static_cast<std::size_t>(size)};
                const bool pending = recorder.PendingWriteOverlaps(begin, bytes);
                if (pending && !movedInPlace(recorder, item, address, size, entry)) return std::nullopt;
                if (moved.empty()) moved.reserve(allocations.size());
                moved.push_back(entry);
                if (!pending) movedBytes += bytes;
            }
        }
    }
    return moved;
}

bool ShaderResources::movedInPlace(Recorder& recorder, const Allocation& item, std::uint64_t address, std::uint64_t size, MovedBuffer& entry) const {
    static const bool enabled = std::getenv("APS5_NO_MOVED_BUFFERS_IN_PLACE") == nullptr;
    auto& counters = MovedInPlace();
    const auto refuse = [&] {
        counters.refused.fetch_add(1, std::memory_order_relaxed);
        return false;
    };
    if (!enabled || context.hostImportAlignment == 0) return false;
    const auto* import = HostImportFor(context, address, static_cast<std::size_t>(size));
    if (import == nullptr) return refuse();
    const auto offset = HostImportOffset(*import, address);
    if (offset % 4 != 0) return refuse();
    const auto adjustment = static_cast<std::uint32_t>(offset % std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4));
    const bool inData = item.pushByte < 0 && item.dataAllocation >= 0;
    const auto* data = inData ? &allocations[static_cast<std::size_t>(item.dataAllocation)] : nullptr;
    const bool told = item.pushByte >= 0 || (data != nullptr && !data->guest && data->buffer != nullptr && item.dataByte < data->size);
    if (adjustment != item.adjustment && !told) return refuse();
    const auto begin = address - adjustment;
    const auto bytes = GuestBufferMemory::ViewBytes(size, adjustment);
    if (bytes > context.limits.maxStorageBufferRange || begin + bytes > import->base + import->bytes) return refuse();
    const auto length = static_cast<std::size_t>(bytes);
    if (guestMemory.WritesOverlap(begin, length) || PendingStorageOverlaps(begin, length, nullptr) || AnyShadowedOverlaps(begin, length)) return refuse();
    if (recorder.PendingLabelIn(begin, length) || recorder.QueuedStoreOverlaps(begin, length)) return refuse();
    const auto overlaps = [&](const auto& writer) { return writer->WritesOverlap(begin, length); };
    if (context.copiedWriters != nullptr && std::any_of(context.copiedWriters->begin(), context.copiedWriters->end(), overlaps)) return refuse();
    const auto drawWriters = DrawCopiedWriters();
    if (std::any_of(drawWriters->begin(), drawWriters->end(), overlaps)) return refuse();
    entry.inPlace = import->buffer;
    entry.inPlaceOffset = offset - adjustment;
    entry.adjustment = adjustment;
    counters.taken.fetch_add(1, std::memory_order_relaxed);
    return true;
}

const char* ShaderResources::MoveRefusalName(MoveRefusal refusal) {
    switch (refusal) {
        case MoveRefusal::None: return "none";
        case MoveRefusal::Shape: return "bindings differ";
        case MoveRefusal::NotGuest: return "not read in place";
        case MoveRefusal::Written: return "written or emptied";
        case MoveRefusal::Limit: return "over the limit";
        case MoveRefusal::Inaccessible: return "unmapped";
        case MoveRefusal::Overlap: return "pending write";
        case MoveRefusal::Bindings: return "no bindings made";
        case MoveRefusal::Count: break;
    }
    return "?";
}

std::optional<std::vector<ShaderResources::MovedBuffer>> ShaderResources::MovedDispatchBuffers(const CompiledShader& shader, Recorder* recorder, std::size_t maxBytes, MoveProof& proof) const {
    proof = {};
    const auto refuse = [&](MoveRefusal why) {
        proof.refusal = why;
        return std::optional<std::vector<MovedBuffer>>{};
    };
    std::vector<MovedBuffer> moved;
    if (shader.program == nullptr || usesBda) return refuse(MoveRefusal::Shape);
    if (_set == VK_NULL_HANDLE) return moved;
    for (const auto& binding : shader.program->bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        const auto kept = std::find_if(bindings.begin(), bindings.end(), [&](const Binding& item) { return item.layout.binding == binding.binding; });
        if (kept == bindings.end() || kept->allocations.size() != binding.count || binding.guestDescriptor.size() != static_cast<std::size_t>(binding.count) * 4u) return refuse(MoveRefusal::Shape);
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto* words = binding.guestDescriptor.data() + static_cast<std::size_t>(element) * 4u;
            const ShaderRecompiler::ShaderBufferResource descriptor{{words[0], words[1], words[2], words[3]}};
            const auto address = descriptor.Base48();
            const auto size = guestBufferBytes(std::span<const std::uint32_t>(words, 4));
            const auto& item = allocations[kept->allocations[element]];
            const bool empty = size == 0 || address == 0;
            if (!item.guest) {
                if (empty) continue;
                return refuse(MoveRefusal::NotGuest);
            }
            if (item.address == address && item.size == size) continue;
            const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
            if (written || empty || item.written) return refuse(MoveRefusal::Written);
            if (size > context.limits.maxStorageBufferRange) return refuse(MoveRefusal::Limit);
            const auto begin = address - item.adjustment;
            const auto bytes = static_cast<std::size_t>(GuestBufferMemory::ViewBytes(size, item.adjustment));
            if (maxBytes != 0 && proof.bytes + bytes > maxBytes) return refuse(MoveRefusal::Limit);
            if (!GuestMemory::Accessible(reinterpret_cast<const void*>(begin), bytes)) return refuse(MoveRefusal::Inaccessible);
            const bool pending = recorder != nullptr ? recorder->PendingWriteOverlaps(begin, bytes) : Recorder::SnapshotWriteOverlaps(begin, bytes);
            const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{begin, begin + bytes}}};
            if (guestMemory.WritesOverlap(begin, bytes) || pending || StorageTexture::AnyPendingOverlaps(range) || AnyShadowedOverlaps(begin, bytes)) return refuse(MoveRefusal::Overlap);
            moved.push_back({kept->allocations[element], address, static_cast<std::size_t>(size)});
            ++proof.buffers;
            proof.bytes += bytes;
        }
    }
    return moved;
}

std::size_t ShaderResources::MaxDrawSnapshotBytes() {
    const char* text = std::getenv("APS5_DRAW_SNAPSHOT_MAX_MIB");
    const auto mib = text != nullptr ? std::strtoull(text, nullptr, 10) : 64ull;
    return static_cast<std::size_t>(mib * 1024ull * 1024ull);
}

std::size_t ShaderResources::DrawUploadLimit() {
    static const std::size_t limit = [] {
        const char* value = std::getenv("APS5_DRAW_UPLOAD_MAX");
        return value != nullptr ? static_cast<std::size_t>(std::strtoull(value, nullptr, 10)) : std::size_t{4096};
    }();
    return limit;
}

std::shared_ptr<ShaderResources::DrawBindings> ShaderResources::PrepareDrawBindings(Recorder& recorder, std::span<const MovedBuffer> moved, bool movedOnly, const BdaResources* faultRecord) const {
    requireGuestBuffers(moved);
    if ((_set == VK_NULL_HANDLE && fragment.set == VK_NULL_HANDLE) || (usesBda && !spaceReuse)) return {};
    prepareFaultSlot();
    auto split = guestMemory.SplitInPlaceReads();
    const bool sharedOnly = split && split->own.empty();
    thread_local std::vector<std::pair<std::uint64_t, std::uint64_t>>* readsSlot = nullptr;
    auto& reads = ShaderRecompiler::ThreadOwned(readsSlot);
    if (!sharedOnly) guestMemory.InPlaceReads(reads);
    const auto maxSnapshotBytes = maxDrawSnapshotBytes;
    const auto uploadLimit = DrawUploadLimit();
    std::size_t totalSnapshotBytes = 0;
    auto result = std::make_shared<DrawBindings>();
    thread_local std::vector<std::size_t>* selectedSlot = nullptr;
    auto& selected = ShaderRecompiler::ThreadOwned(selectedSlot);
    selected.clear();
    std::array<std::uint64_t, 3> stamp{};
    bool stamped = false;
    const auto keep = [&](DrawBindings::Snapshot snapshot) {
        if (result->snapshots.empty()) result->snapshots.reserve(allocations.size());
        result->snapshots.push_back(std::move(snapshot));
    };
    const auto toldInData = [&](const MovedBuffer& entry, std::size_t data) {
        const auto& element = allocations[entry.allocation];
        return entry.inPlace != VK_NULL_HANDLE && element.pushByte < 0 && element.dataAllocation == static_cast<std::int64_t>(data);
    };
    const bool inPlaceData = std::any_of(moved.begin(), moved.end(), [&](const MovedBuffer& entry) { return entry.inPlace != VK_NULL_HANDLE && allocations[entry.allocation].pushByte < 0; });
    for (std::size_t index = 0; index < allocations.size(); ++index) {
        const auto& item = allocations[index];
        const auto override = std::find_if(moved.begin(), moved.end(), [&](const MovedBuffer& entry) { return entry.allocation == index; });
        const bool words = override != moved.end() && !override->words.empty();
        const bool offsets = inPlaceData && !item.guest && item.buffer != nullptr && std::any_of(moved.begin(), moved.end(), [&](const MovedBuffer& entry) { return toldInData(entry, index); });
        if (words || (offsets && override == moved.end())) {
            const auto size = words ? override->size : item.size;
            auto slice = recorder.DrawUpload(size);
            if (words) std::memcpy(slice.bytes, override->words.data(), size);
            else if (!item.dataWords.empty()) std::memcpy(slice.bytes, item.dataWords.data(), std::min(size, item.dataWords.size() * sizeof(std::uint32_t)));
            else std::memcpy(slice.bytes, item.buffer->Bytes().data(), size);
            for (const auto& patch : dataPatches) {
                if (patch.allocation == index && patch.byte < size) slice.bytes[patch.byte] = static_cast<std::byte>(patch.adjustment);
            }
            if (offsets) {
                for (const auto& entry : moved) {
                    if (toldInData(entry, index) && allocations[entry.allocation].dataByte < size) slice.bytes[allocations[entry.allocation].dataByte] = static_cast<std::byte>(entry.adjustment);
                }
            }
            selected.push_back(index);
            keep({0, std::move(slice.buffer), slice.offset, size});
            continue;
        }
        if (override != moved.end() && override->inPlace != VK_NULL_HANDLE) {
            const auto begin = override->address - override->adjustment;
            const auto bytes = static_cast<std::size_t>(GuestBufferMemory::ViewBytes(override->size, override->adjustment));
            recorder.NotePendingRead(begin, bytes, Recorder::ReadKind::DispatchElement);
            selected.push_back(index);
            keep({begin, nullptr, override->inPlaceOffset, bytes, override->inPlace});
            continue;
        }
        std::uint64_t address = item.address;
        std::size_t size = item.size;
        if (override != moved.end()) {
            address = override->address;
            size = override->size;
        } else {
            if (movedOnly || !item.guest || item.written || guestMemory.WritesOverlap(item.address, item.size)) continue;
            const bool direct = [&] {
                if (sharedOnly) {
                    const auto& ranges = *split->space;
                    const auto found = std::partition_point(ranges.begin(), ranges.end(), [&](const auto& range) { return range.second <= item.address; });
                    return found != ranges.end() && found->first <= item.address && item.address < found->second && item.size <= found->second - item.address;
                }
                return std::any_of(reads.begin(), reads.end(), [&](const auto& range) { return item.address >= range.first && item.address < range.second && item.size <= range.second - item.address; });
            }();
            if (!direct || recorder.PendingWriteOverlaps(item.address, item.size)) continue;
        }
        const auto begin = address - item.adjustment;
        const auto bytes = static_cast<std::size_t>(GuestBufferMemory::ViewBytes(size, item.adjustment));
        if (override == moved.end() && (bytes > maxSnapshotBytes || totalSnapshotBytes + bytes > maxSnapshotBytes)) continue;
        const auto registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        if (!stamped) {
            stamp = {GuestMemory::TrackerGeneration(), GuestMemory::CollectEpoch(), registryGeneration};
            stamped = true;
        }
        VkDeviceSize offset = 0;
        auto buffer = recorder.UnwrittenDrawSnapshot(begin, bytes, stamp, &offset);
        static const bool verifySnapshots = std::getenv("APS5_VERIFY_DRAW_SNAPSHOTS") != nullptr;
        if (verifySnapshots && buffer != nullptr) {
            static std::atomic<std::uint64_t> checked{0}, differing{0};
            const auto fast = buffer->Bytes().subspan(static_cast<std::size_t>(offset), bytes);
            VkDeviceSize slowOffset = 0;
            const auto slow = recorder.ReusableDrawSnapshot(begin, bytes, Recorder::SnapshotUse::Storage, nullptr, GuestMemory::CollectWrites(begin, bytes), &slowOffset);
            const bool same = (slow == buffer && slowOffset == offset) || (slow != nullptr && slow->Bytes().size() >= slowOffset + fast.size() && std::memcmp(slow->Bytes().data() + slowOffset, fast.data(), fast.size()) == 0) || (slow == nullptr && std::memcmp(fast.data(), reinterpret_cast<const void*>(begin), bytes) == 0);
            if (!same) differing.fetch_add(1);
            if (checked.fetch_add(1) % 200000 == 199999) std::fprintf(stderr, "[draw-snapshots] verify: %llu fast answers checked, %llu differing%c", static_cast<unsigned long long>(checked.load()), static_cast<unsigned long long>(differing.load()), 10);
        }
        if (buffer == nullptr) {
            const bool uploaded = bytes != 0 && bytes <= uploadLimit;
            if (uploaded) {
                auto slice = recorder.DrawUpload(bytes);
                std::memcpy(slice.bytes, reinterpret_cast<const void*>(begin), bytes);
                buffer = std::move(slice.buffer);
                offset = slice.offset;
            } else {
                const auto generation = GuestMemory::CollectWrites(begin, bytes);
                buffer = recorder.ReusableDrawSnapshot(begin, bytes, Recorder::SnapshotUse::Storage, nullptr, generation, &offset);
                if (buffer == nullptr) {
                    buffer = std::make_shared<Buffer>(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    std::memcpy(buffer->Bytes().data(), reinterpret_cast<const void*>(begin), bytes);
                    recorder.KeepDrawSnapshot(begin, bytes, generation, registryGeneration, buffer);
                }
            }
            if (GuestMemory::TrackerGeneration() != stamp[0]) stamped = false;
            else if (!uploaded || stamp[1] != 0) recorder.NoteUnwrittenDrawSnapshot(begin, bytes, stamp, buffer, offset);
        }
        selected.push_back(index);
        keep({begin, std::move(buffer), offset, bytes});
        totalSnapshotBytes += bytes;
        CaptureTrace::Log("draw-snapshot batch=%llu address=%llx bytes=%zu", static_cast<unsigned long long>(recorder.Submissions() + 1), static_cast<unsigned long long>(begin), bytes);
    }
    if (selected.empty() && !rebound) return {};
    writeDrawSets(*result, selected, faultRecord);
    recorder.Keep(result);
    for (const auto& snapshot : result->snapshots) {
        if (snapshot.buffer != nullptr) recorder.KeepBytes(snapshot.buffer.get(), snapshot.buffer->Bytes().size());
    }
    return result;
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
}
}
}
}
}
}
