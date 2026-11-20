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
void ShaderResources::writeDrawSets(DrawBindings& result, std::span<const std::size_t> selected, const BdaResources* faultRecord) const {
    Require(context.descriptorCache != nullptr, "draw snapshots require a descriptor cache");
    std::array<bool, 2> described{};
    thread_local std::array<std::vector<VkDescriptorPoolSize>, 2>* countsSlot = nullptr;
    auto& counts = ShaderRecompiler::ThreadOwned(countsSlot);
    for (auto& sizes : counts) sizes.clear();
    for (const auto& binding : bindings) {
        const auto set = setOf(binding);
        described[set] = true;
        const auto count = (binding.flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0 ? std::max(1u, static_cast<std::uint32_t>(binding.imageAllocations.size())) : binding.layout.descriptorCount;
        if (count == 0) continue;
        auto& sizes = counts[set];
        const auto held = std::find_if(sizes.begin(), sizes.end(), [&](const VkDescriptorPoolSize& entry) { return entry.type == binding.layout.descriptorType; });
        if (held != sizes.end()) held->descriptorCount += count;
        else sizes.push_back({binding.layout.descriptorType, count});
    }
    for (auto& sizes : counts) std::sort(sizes.begin(), sizes.end(), [](const VkDescriptorPoolSize& a, const VkDescriptorPoolSize& b) { return a.type < b.type; });
    const auto sizesOf = [&](std::size_t set) { return std::span<const VkDescriptorPoolSize>(counts[set]); };
    const auto variableOf = [&](std::size_t set) { return tableBindings[set] != std::numeric_limits<std::size_t>::max() ? std::optional<std::uint32_t>(std::max(1u, static_cast<std::uint32_t>(bindings[tableBindings[set]].imageAllocations.size()))) : std::nullopt; };
    result.cache = context.descriptorCache;
    if (described[0] || splitSets) {
        result.allocation = result.cache->AllocateRewritten(_layout, sizesOf(0), updateAfterBind, variableOf(0));
        result.layout = _layout;
        Require(result.allocation.set != VK_NULL_HANDLE, "draw snapshot descriptor allocation failed");
    }
    if (described[1] || splitSets) {
        result.fragmentAllocation = result.cache->AllocateRewritten(fragment.layout, sizesOf(1), updateAfterBind, variableOf(1));
        result.fragmentLayout = fragment.layout;
        Require(result.fragmentAllocation.set != VK_NULL_HANDLE, "draw snapshot descriptor allocation failed");
    }
    thread_local std::vector<VkDescriptorBufferInfo>* buffersSlot = nullptr;
    auto& buffers = ShaderRecompiler::ThreadOwned(buffersSlot);
    buffers = descriptorBuffers;
    if (faultRecord != nullptr) {
        for (const auto& fault : faultDescriptors) buffers[fault.buffer] = faultRecord->Fault();
    }
    thread_local std::vector<VkWriteDescriptorSet>* writesSlot = nullptr;
    auto& writes = ShaderRecompiler::ThreadOwned(writesSlot);
    writes = descriptorWrites;
    thread_local std::vector<std::size_t>* indicesSlot = nullptr;
    auto& snapshotIndices = ShaderRecompiler::ThreadOwned(indicesSlot);
    snapshotIndices.assign(allocations.size(), selected.size());
    for (std::size_t index = 0; index < selected.size(); ++index) snapshotIndices[selected[index]] = index;
    auto nextWrite = writes.begin();
    for (const auto& binding : bindings) {
        const auto original = setOf(binding) == 0 ? _set : fragment.set;
        while (nextWrite != writes.end() && nextWrite->dstBinding == binding.layout.binding && nextWrite->dstSet == original) {
            auto& write = *nextWrite++;
            write.dstSet = setOf(binding) == 0 ? result.allocation.set : result.fragmentAllocation.set;
            if (binding.allocations.empty()) continue;
            const auto offset = static_cast<std::size_t>(write.pBufferInfo - descriptorBuffers.data());
            write.pBufferInfo = buffers.data() + offset;
            for (std::size_t element = 0; element < binding.allocations.size(); ++element) {
                const auto snapshotIndex = snapshotIndices[binding.allocations[element]];
                if (snapshotIndex == selected.size()) continue;
                const auto& snapshot = result.snapshots[snapshotIndex];
                buffers[offset + element] = wholeDwordRange({snapshot.buffer != nullptr ? snapshot.buffer->Handle() : snapshot.handle, snapshot.offset, snapshot.bytes});
            }
        }
    }
    Require(nextWrite == writes.end(), "draw snapshot writes disagree with their descriptor bindings");
    context.Resolved(&DeviceFunctions::updateDescriptorSets, "vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

ShaderResources::DrawUse::~DrawUse() {
    if (ownRecord != nullptr) ownRecord->store(false, std::memory_order_release);
}

std::shared_ptr<ShaderResources::DrawUse> ShaderResources::BeginDrawUse(Recorder& recorder) const {
    if (!spaceReuse || bda == nullptr || !faultChecks || faultDescriptors.empty()) return {};
    auto use = std::make_shared<DrawUse>();
    if (!ownRecordBusy->exchange(true, std::memory_order_acq_rel)) {
        use->ownRecord = ownRecordBusy;
        return use;
    }
    use->record = std::make_shared<BdaResources>(context, faultReportOnly);
    use->record->NoteUse(recorder.Device() == context.device ? recorder.OpenSerial() : std::numeric_limits<std::uint64_t>::max());
    use->bindings = std::make_shared<DrawBindings>();
    writeDrawSets(*use->bindings, {}, use->record.get());
    recorder.Keep(use->bindings);
    return use;
}

void ShaderResources::CompleteDrawUse(const DrawUse& use) const {
    if (bda != nullptr) bda->CheckUse(use.record.get());
}

void ShaderResources::BindDrawUse(VkCommandBuffer commands, VkPipelineLayout layout, const DrawUse* use) const {
    if (use == nullptr || use->bindings == nullptr) {
        Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout);
        return;
    }
    requireGuestBuffers();
    const auto set = use->bindings->allocation.set;
    const auto fragmentSet = use->bindings->fragmentAllocation.set;
    if (set != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
    if (fragmentSet != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet, 1, &fragmentSet, 0, nullptr);
}

void ShaderResources::prepareFaultSlot() const {
    if (bda == nullptr) return;
    const auto* recorder = Recorder::Active();
    bda->NoteUse(recorder != nullptr && recorder->Device() == context.device ? recorder->OpenSerial() : std::numeric_limits<std::uint64_t>::max());
    if (!bda->TakeFaultSlot(context)) return;
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(faultDescriptors.size());
    for (const auto& fault : faultDescriptors) {
        descriptorBuffers[fault.buffer] = bda->Fault();
        auto write = descriptorWrites[fault.write];
        write.dstArrayElement = fault.element;
        write.descriptorCount = 1;
        write.pBufferInfo = &descriptorBuffers[fault.buffer];
        writes.push_back(write);
    }
    if (!writes.empty()) context.Resolved(&DeviceFunctions::updateDescriptorSets, "vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ShaderResources::requireGuestBuffers(std::span<const MovedBuffer> moved) const {
    for (std::size_t index = 0; index < allocations.size(); ++index) {
        const auto& item = allocations[index];
        if (!item.guest) continue;
        const auto override = std::find_if(moved.begin(), moved.end(), [&](const MovedBuffer& entry) { return entry.allocation == index; });
        const auto address = override != moved.end() ? override->address : item.address;
        const auto bytes = override != moved.end() ? override->size : item.size;
        RequireNoResidentDepthRead_nid_no_patch(address, bytes, htileClear);
        RequireNoMultisampleColorTarget(address, bytes, "a shader buffer access");
    }
}

void ShaderResources::Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const {
    requireGuestBuffers();
    prepareFaultSlot();
    if (_set != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, bindPoint, layout, 0, 1, &_set, 0, nullptr);
    if (fragment.set != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, bindPoint, layout, ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet, 1, &fragment.set, 0, nullptr);
}

namespace {
// Debug aid: APS5_GPU_NO_WRITEBACK=1 keeps GPU results out of guest memory.
bool SkipWriteBack() {
    static const bool skip = std::getenv("APS5_GPU_NO_WRITEBACK") != nullptr;
    return skip;
}
}

void ShaderResources::WriteBack() {
    std::exception_ptr unaligned;
    try {
        WriteBackBuffers();
    } catch (const BdaUnalignedAccess&) {
        unaligned = std::current_exception();
    }
    if (!guestMemory.Writes().empty() || std::find(storageWritten.begin(), storageWritten.end(), true) != storageWritten.end() || (usesBda && !bdaReadOnly)) GuestMemory::BreakProofs();
    if (!SkipWriteBack()) {
        for (std::size_t index = 0; index < storageTextures.size(); ++index) {
            if (!storageWritten[index] || storageTextures[index] == nullptr) continue;
            storageTextures[index]->MarkDirty();
            static_cast<void>(DepthSurfaceStoreBack(context, *storageTextures[index]));
        }
    }
    if (unaligned) std::rethrow_exception(unaligned);
}

void ShaderResources::MarkGpuWrites(Recorder& recorder) {
    if (bda != nullptr && recorder.Device() == context.device) bda->NoteUse(recorder.OpenSerial());
    // The ranges this use reads in place through their host imports (read-only and written elements
    // alike, and an address-based build's whole leased heaps), before the writes: a CPU store into
    // one of them (the copy HLE) must not land before the recorded work read it.
    static const bool readOnlyLeaseSkip = std::getenv("APS5_NO_READ_ONLY_LEASE_SKIP") == nullptr;
    const auto kind = !guestMemory.HoldsLease() ? Recorder::ReadKind::DispatchElement : readOnlyLeaseSkip && usesBda && bdaReadOnly ? Recorder::ReadKind::AddressBasedReadOnly : Recorder::ReadKind::AddressBased;
    if (auto split = guestMemory.SplitInPlaceReads()) {
        recorder.NotePendingReadList(std::move(split->space), kind);
        recorder.NotePendingReads(split->own, kind);
    } else {
        recorder.NotePendingReads(guestMemory.InPlaceReads(), kind);
    }
    const auto inRun = std::exchange(afterRunSelf, nullptr);
    if (SkipWriteBack()) return;
    for (std::size_t index = 0; index < storageTextures.size(); ++index) {
        if (!storageWritten[index] || storageTextures[index] == nullptr) continue;
        storageTextures[index]->MarkDirty();
        static_cast<void>(DepthSurfaceStoreBack(context, *storageTextures[index]));
    }
    // Written sub-ranges of buffers the GPU copied out of a host import go back into it by the GPU,
    // recorded here after the work: those regions then need no CPU write-back (HasCopiedWrites),
    // and the note and mark below cover them like direct writes.
    if (inRun != nullptr && recorder.RunOpen() && guestMemory.HasGpuCopies()) {
        recorder.AfterRun([inRun](Recorder& after) { inRun->guestMemory.RecordCopyBacks(after); });
        guestMemory.MarkStagedWrites();
    } else {
        guestMemory.RecordCopyBacks(recorder);
    }
    // Only the written elements' ranges (AddWritable): a read-only element is neither noted here
    // nor marked as a direct write, so CPU reads of its memory never wait for this work.
    recorder.NotePendingWrites(guestMemory.Writes());
    if (usesBda && !bdaReadOnly) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> reachable;
        guestMemory.AddressStores(reachable);
        recorder.NotePendingReach(reachable);
    }
    guestMemory.MarkDirectWrites();
    guestMemory.NoteCarriedShadows(recorder);
    if (!BuildProfiled()) return;
    auto& counters = BufferWrites();
    // Recorded uses only (dispatches and recorded draws): a synchronous draw writes back in
    // WriteBack() and publishes no pending writes, so it has no notes to skip.
    counters.notesSkipped.fetch_add(readOnlyBuffers, std::memory_order_relaxed);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load();
    if (nowMs - last < 10000 || !counters.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto elements = counters.elements.load();
    const auto readOnly = counters.readOnly.load();
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] descriptor elements bound (cumulative): %llu total, %llu written, %llu read-only; %llu pending-write notes skipped\n", static_cast<unsigned long long>(elements), static_cast<unsigned long long>(elements - readOnly), static_cast<unsigned long long>(readOnly), static_cast<unsigned long long>(counters.notesSkipped.load()));
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] moved read-only elements under a recorded GPU write (cumulative): %llu read in place on a cached set, %llu refused and rebuilt\n", static_cast<unsigned long long>(MovedInPlace().taken.load()), static_cast<unsigned long long>(MovedInPlace().refused.load()));
}

void ShaderResources::CheckFaults() const {
    if (bda) bda->CheckFault();
}

void ShaderResources::WriteBackBuffers() {
    std::exception_ptr unaligned;
    if (bda) {
        try {
            bda->CheckFault();
        } catch (const BdaUnalignedAccess&) {
            unaligned = std::current_exception();
        }
    }
    if (!SkipWriteBack()) guestMemory.WriteBack();
    if (unaligned) std::rethrow_exception(unaligned);
}

std::span<const std::byte> ShaderResources::AddressTableBytes() const {
    return bda != nullptr ? bda->TableBytes() : std::span<const std::byte>{};
}

bool ShaderResources::AddressTableGuardCurrent() {
    if (bda == nullptr) return true;
    if (const auto cached = guestMemory.CachedAddressTable(); cached.has_value()) return bda->TableGuardCurrent(context.device, *cached->ranges);
    return bda->TableGuardCurrent(context.device, guestMemory.AddressRanges());
}

bool ShaderResources::ReadsOverlap(std::uint64_t address, std::size_t bytes) const {
    static const bool walk = std::getenv("APS5_NO_READS_OVERLAP_WALK") == nullptr;
    if (walk) return guestMemory.InPlaceReadsOverlap(address, bytes);
    const auto reads = guestMemory.InPlaceReads();
    return std::any_of(reads.begin(), reads.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

void ShaderResources::RunAccesses(std::vector<std::array<std::uint64_t, 3>>& out) const {
    out.clear();
    guestMemory.RunAccesses(out);
    if (usesBda) out.push_back({0, std::numeric_limits<std::uint64_t>::max(), Recorder::RunAccessWritten});
    for (const auto& [image, written] : StorageImages()) {
        const auto handle = reinterpret_cast<std::uint64_t>(image);
        out.push_back({handle, handle + 1, Recorder::RunAccessImage | (written ? Recorder::RunAccessWritten : 0u)});
    }
}

std::vector<std::pair<VkImage, bool>> ShaderResources::StorageImages() const {
    std::vector<std::pair<VkImage, bool>> images;
    for (std::size_t index = 0; index < storageTextures.size(); ++index) {
        if (storageTextures[index] != nullptr) images.emplace_back(storageTextures[index]->Image(), storageWritten[index]);
    }
    for (const auto& texture : textures) {
        if (texture != nullptr && texture->StorageSource() != nullptr) images.emplace_back(texture->StorageSource()->Image(), false);
    }
    return images;
}

bool ShaderResources::HasCachedStorageImages() const {
    if (std::any_of(storageTextures.begin(), storageTextures.end(), [](const auto& image) { return image != nullptr && !image->Cached(); })) return false;
    return std::none_of(textures.begin(), textures.end(), [](const auto& texture) { return texture != nullptr && texture->StorageSource() != nullptr && !texture->StorageSource()->Cached(); });
}

bool ShaderResources::HoldsReleasedTexture() const {
    return std::any_of(textures.begin(), textures.end(), [](const auto& texture) { return texture != nullptr && texture->Released(); });
}

bool ShaderResources::ReadsImage(const StorageTexture* image) const {
    if (image == nullptr) return false;
    for (const auto& texture : textures) {
        if (texture != nullptr && texture->StorageSource() == image) return true;
    }
    return std::any_of(storageTextures.begin(), storageTextures.end(), [&](const auto& storage) { return storage.get() == image; });
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
