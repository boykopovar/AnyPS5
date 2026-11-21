#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AdaptiveStaging.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SnapshotCopies.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#ifdef __APPLE__
struct udmabuf_create {
    std::uint32_t memfd;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_item {
    std::uint32_t memfd;
    std::uint32_t pad;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_list {
    std::uint32_t flags;
    std::uint32_t count;
    udmabuf_create_item list[];
};
constexpr std::uint32_t UDMABUF_FLAGS_CLOEXEC = 0x01;
constexpr unsigned long UDMABUF_CREATE = 0;
constexpr unsigned long UDMABUF_CREATE_LIST = 0;
#else
#include <linux/fs.h>
#include <linux/udmabuf.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <array>
#include <vector>
#include <span>
#include <atomic>
#include <tuple>
#include <utility>
#include <functional>
#include <bit>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_set>
#include <thread>
#include <stop_token>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::shared_ptr<Buffer> changedCopySnapshot(const Context& context, std::size_t bytes, bool addressed) {
    auto buffer = stagingBuffer(context, bytes, gpuCopyUsage(context, addressed));
    if (buffer == nullptr) return nullptr;
    changedCopySnapshotBytes.fetch_add(bytes, std::memory_order_relaxed);
    changedCopySnapshotsMade.fetch_add(1, std::memory_order_relaxed);
    auto* snapshot = buffer.get();
    return std::shared_ptr<Buffer>(snapshot, [owner = std::move(buffer), bytes](Buffer*) mutable {
        owner.reset();
        changedCopySnapshotBytes.fetch_sub(bytes, std::memory_order_relaxed);
    });
}

bool carriedShadowsEnabled() {
    static const bool enabled = std::getenv("APS5_NO_CARRIED_SHADOWS") == nullptr;
    return enabled;
}

std::atomic<std::uint64_t> carriedShadowCopies{0};

struct CarriedShadow {
    std::shared_ptr<Buffer> buffer;
    VkDevice device = VK_NULL_HANDLE;
    std::uint64_t recorderId = 0;
    std::uint64_t importSerial = 0;
    std::uint64_t note = 0;
    std::uint64_t generation = 0;
};

struct CarriedShadows {
    static constexpr std::size_t MaxEntries = 256;
    static constexpr std::uint64_t MaxBytes = std::uint64_t{512} << 20u;
    AgcDriver::Mutex mutex;
    std::map<std::pair<std::uint64_t, std::uint64_t>, CarriedShadow> entries;
    std::uint64_t bytes = 0;
};

CarriedShadows& Carried() {
    static CarriedShadows shadows;
    return shadows;
}

void clearCarriedShadows(VkDevice device) {
    auto& shadows = Carried();
    std::lock_guard lock(shadows.mutex);
    for (auto entry = shadows.entries.begin(); entry != shadows.entries.end();) {
        if (entry->second.device != device) {
            ++entry;
            continue;
        }
        shadows.bytes -= entry->first.second - entry->first.first;
        entry = shadows.entries.erase(entry);
    }
}

std::shared_ptr<Buffer> takeCarriedShadow(const Context& context, const Recorder& recorder, std::uint64_t begin, std::uint64_t end) {
    if (!carriedShadowsEnabled()) return nullptr;
    auto& shadows = Carried();
    CarriedShadow shadow;
    {
        std::lock_guard lock(shadows.mutex);
        const auto found = shadows.entries.find({begin, end});
        if (found == shadows.entries.end()) return nullptr;
        shadow = std::move(found->second);
        shadows.bytes -= end - begin;
        shadows.entries.erase(found);
    }
    const auto bytes = static_cast<std::size_t>(end - begin);
    if (shadow.device != context.device || shadow.recorderId != recorder.Id() || shadow.note == 0 || recorder.NewestWriteNote(begin, bytes) != shadow.note) return nullptr;
    if (shadow.importSerial == 0 || HostImportSerial(context, begin, bytes, false) != shadow.importSerial) return nullptr;
    static_cast<void>(GuestMemory::CollectWrites(begin, bytes));
    if (!GuestMemory::UnchangedSince(begin, bytes, shadow.generation)) return nullptr;
    return shadow.buffer;
}

}

std::uint64_t GuestBufferMemory::CarriedShadowCopies() {
    return carriedShadowCopies.load(std::memory_order_relaxed);
}

void GuestBufferMemory::NoteCarriedShadows(const Recorder& recorder) const {
    if (!carriedShadowsEnabled()) return;
    auto& shadows = Carried();
    std::uint64_t generation = 0;
    for (const auto& region : regions) {
        if (!region.gpuCopy || !region.deviceLocal || !region.copiedBack || region.buffer == nullptr || region.sparse || region.mirror != nullptr || region.bufferEnd != 0) continue;
        const auto bytes = region.end - region.begin;
        const auto serial = HostImportSerial(context, region.begin, static_cast<std::size_t>(bytes), false);
        if (serial == 0) continue;
        if (generation == 0) generation = GuestMemory::TrackerGeneration();
        const auto note = recorder.NewestWriteNote(region.begin, static_cast<std::size_t>(bytes));
        std::lock_guard lock(shadows.mutex);
        auto [entry, inserted] = shadows.entries.try_emplace({region.begin, region.end});
        if (inserted) shadows.bytes += bytes;
        entry->second = {region.buffer, context.device, recorder.Id(), serial, note, generation};
        if (shadows.entries.size() > CarriedShadows::MaxEntries || shadows.bytes > CarriedShadows::MaxBytes) {
            shadows.entries.clear();
            shadows.bytes = 0;
        }
    }
}

void GuestBufferMemory::recordGpuCopies(std::span<Region* const> copies, bool addressable) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static const bool capture = std::getenv("APS5_CAPTURE_GPU_COPIES") != nullptr;
    Require(!capture || CaptureTrace::Enabled(), "GPU copy capture requires APS5_CAPTURE_TRACE");
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "GPU buffer copies need an active recorder");
    for (const auto* region : copies) {
        recorder->FlushKeyStoresOverlapping(region->begin, static_cast<std::size_t>(region->end - region->begin));
        recorder->FlushStoresOverlapping(region->begin, static_cast<std::size_t>(region->end - region->begin));
    }
    VkAccessFlags covered = 0;
    bool inRun = false;
    std::vector<Recorder::RunAccess> runAccesses;
    const auto runKey = Recorder::DispatchOverlapEnabled() && recorder->RunOpen() ? Recorder::PeekUnorderedDispatch() : 0u;
    if (runKey != 0) RunAccesses(runAccesses);
    const auto commands = runKey != 0 ? recorder->RunCommands(runKey, this, runAccesses, inRun, &covered) : recorder->Commands(&covered);
    const auto timing = recorder->BeginGpuTiming(StagingCopyInKey);
    if (Recorder::BarrierValidate()) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> reads;
        for (const auto* region : copies) reads.emplace_back(region->begin, region->end);
        recorder->NoteAccess(Recorder::CommandClass::StagingIn, Recorder::Access{reads, {}, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    constexpr BarrierMask leading{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    if (!inRun && !Recorder::ElideCovered(Recorder::CommandClass::StagingIn, leading, covered)) {
        RecordMemoryBarrier(context, commands, leading.srcStage, leading.dstStage, leading.srcAccess, leading.dstAccess);
        Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
    }
    bool stagedAny = false;
    std::uint64_t copiedBytes = 0;
    for (auto* region : copies) {
        const auto bytes = region->end - region->begin;
        copiedBytes += bytes;
        if (region->buffer == nullptr) {
            const auto allocateStart = std::chrono::steady_clock::now();
            if (region->deviceLocal) region->buffer = stagingBuffer(context, static_cast<std::size_t>(bytes), gpuCopyUsage(context, addressable));
            if (region->buffer == nullptr) {
                region->deviceLocal = false;
                region->unstaged = true;
                region->buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), gpuCopyUsage(context, addressable), copyBufferProperties(false));
            }
            if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
        }
        std::vector<std::byte> expected;
        const auto address = region->begin;
        const auto batch = static_cast<unsigned long long>(recorder->Submissions() + 1);
        const bool pendingWriter = recorder->PendingWriteOverlaps(address, static_cast<std::size_t>(bytes));
        const bool writable = WritesOverlap(address, static_cast<std::size_t>(bytes));
        std::shared_ptr<Buffer> inputSnapshot;
        auto copySource = region->copySource;
        auto copyOffset = region->begin - region->copySourceBase;
        if (!pendingWriter && !writable) {
            inputSnapshot = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            std::memcpy(inputSnapshot->Bytes().data(), reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes));
            copySource = inputSnapshot->Handle();
            copyOffset = 0;
            recorder->Keep(inputSnapshot, inputSnapshot->Bytes().size());
            CaptureTrace::Log("copy-input-snapshot batch=%llu address=%llx bytes=%llu", batch, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        }
        if (capture && bytes == 32) {
            CaptureTrace::Log("copy-input-candidate batch=%llu address=%llx bytes=%llu pendingWriter=%d writable=%d", batch, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), pendingWriter, writable);
            if (inputSnapshot != nullptr) {
                const auto snapshotBytes = inputSnapshot->Bytes();
                expected.assign(snapshotBytes.begin(), snapshotBytes.end());
            }
        }
        recorder->Keep(region->buffer);
        std::shared_ptr<Buffer> carried;
        if (region->deviceLocal && inputSnapshot == nullptr && !region->sparse && region->mirror == nullptr && region->bufferEnd == 0) carried = takeCarriedShadow(context, *recorder, region->begin, region->end);
        if (carried != nullptr) {
            carriedShadowCopies.fetch_add(1, std::memory_order_relaxed);
            if (carried->Handle() != region->buffer->Handle()) {
                CopyBuffer(context, commands, carried->Handle(), 0, region->buffer->Handle(), 0, bytes);
                recorder->Keep(carried);
            }
        } else {
            CopyBuffer(context, commands, copySource, copyOffset, region->buffer->Handle(), 0, bytes);
        }
        if (!expected.empty()) {
            auto readback = std::make_shared<Buffer>(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            recorder->Keep(readback, readback->Bytes().size());
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, region->buffer->Handle(), 0, readback->Handle(), 0, bytes);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            recorder->OnComplete([readback, expected = std::move(expected), address, batch] {
                const auto actual = readback->Bytes();
                std::size_t changed = 0;
                for (std::size_t index = 0; index < expected.size(); ++index) {
                    if (expected[index] == actual[index]) continue;
                    ++changed;
                    CaptureTrace::Log("copy-input-byte batch=%llu address=%llx offset=%zu cpu=%02x gpu=%02x", batch, static_cast<unsigned long long>(address), index, std::to_integer<unsigned>(expected[index]), std::to_integer<unsigned>(actual[index]));
                }
                CaptureTrace::Log("copy-input-result batch=%llu address=%llx bytes=%zu changed=%zu", batch, static_cast<unsigned long long>(address), expected.size(), changed);
            });
        }
        if (inputSnapshot == nullptr) recorder->NotePendingRead(region->begin, static_cast<std::size_t>(bytes), Recorder::ReadKind::GpuCopy);
        region->snapshot.clear();
        if (region->deviceLocal) traceStaged(region->begin, region->end, region->atomic);
        if (profile) {
            Copies().gpuCopies.fetch_add(1, std::memory_order_relaxed);
            Copies().gpuCopyBytes.fetch_add(bytes, std::memory_order_relaxed);
            if (region->deviceLocal) {
                stagedAny = true;
                Copies().staged.fetch_add(1, std::memory_order_relaxed);
                Copies().stagedInBytes.fetch_add(bytes, std::memory_order_relaxed);
                if (region->atomic) Copies().stagedAtomic.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    std::vector<Region*> shadowed;
    for (auto* region : copies) {
        const auto bytes = static_cast<std::size_t>(region->end - region->begin);
        const bool addressed = addressedChangedCopyBacks(context);
        if (!changedCopyBacks(context) || (addressed && region->buffer->DeviceAddress() == 0) || !WritesOverlap(region->begin, bytes)) {
            region->shadowCopy = nullptr;
            continue;
        }
        if (region->shadowCopy == nullptr) region->shadowCopy = region->releasedShadowCopy.lock();
        if (region->shadowCopy == nullptr) region->shadowCopy = changedCopySnapshot(context, bytes, addressed);
        if (region->shadowCopy == nullptr || (addressed && region->shadowCopy->DeviceAddress() == 0)) {
            region->shadowCopy = nullptr;
            continue;
        }
        shadowed.push_back(region);
    }
    if (!shadowed.empty()) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
        for (auto* region : shadowed) {
            CopyBuffer(context, commands, region->buffer->Handle(), 0, region->shadowCopy->Handle(), 0, region->end - region->begin);
            recorder->Keep(region->shadowCopy);
        }
    }
    if (stagedAny) reportStaging();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
    recorder->MarkCovered(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    recorder->EndGpuTiming(timing, copiedBytes);
}

void GuestBufferMemory::RecordCopyBacks(Recorder& recorder) {
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    bool recording = false;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    std::uint64_t copiedBytes = 0;
    if (!writes.empty()) {
        auto sorted = writes;
        std::sort(sorted.begin(), sorted.end());
        for (const auto& range : sorted) {
            if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
            else merged.push_back(range);
        }
    }
    for (const auto& [begin, end] : merged) {
        const auto* found = owner(begin, end);
        if (found == nullptr || found->mirror == nullptr || !found->mirror->heap || !found->mirror->writable || found->mirror->resident == nullptr) continue;
        const auto& region = *found;
        {
            const auto from = begin;
            const auto to = std::min(end, servedEnd(region));
            if (from >= to || std::any_of(heapReadbacks.begin(), heapReadbacks.end(), [&](const auto& entry) { return entry.first == from; })) continue;
            if (!recording) {
                commands = recorder.Commands();
                timing = recorder.BeginGpuTiming(StagingCopyBackKey);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
                recording = true;
            }
            heapReadbacks.emplace_back(from, region.mirror->resident->RecordReadback(commands, from, to - from));
            recorder.Keep(heapReadbacks.back().second);
            copiedBytes += to - from;
        }
    }
    for (auto& region : regions) {
        if (!region.gpuCopy || region.copiedBack) continue;
        for (const auto& [begin, end] : merged) {
            const auto from = std::max(begin, region.begin);
            const auto to = std::min(end, region.end);
            if (from >= to) continue;
            if (!recording) {
                VkAccessFlags covered = 0;
                commands = recorder.Commands(&covered);
                timing = recorder.BeginGpuTiming(StagingCopyBackKey);
                constexpr BarrierMask leading{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT};
                if (!Recorder::ElideCovered(Recorder::CommandClass::StagingOut, leading, covered)) {
                    RecordMemoryBarrier(context, commands, leading.srcStage, leading.dstStage, leading.srcAccess, leading.dstAccess);
                    Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
                }
                recording = true;
            }
            auto whole = from;
            auto wholeEnd = to;
            const bool addressed = addressedChangedCopyBacks(context);
            if (region.shadowCopy != nullptr && (addressed ? region.copySourceAddress != 0 : region.copySource != VK_NULL_HANDLE) && region.begin % 4u == 0 && region.copySourceBase % 4u == 0) {
                const auto alignedFrom = (from + 3u) & ~std::uint64_t{3};
                const auto alignedTo = to & ~std::uint64_t{3};
                static const bool traceDivergence = std::getenv("APS5_TRACE_COPY_BACK") != nullptr;
                std::shared_ptr<Buffer> counter;
                if (traceDivergence && alignedFrom < alignedTo) {
                    counter = std::make_shared<Buffer>(context, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (addressed ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u));
                    std::memset(counter->Bytes().data(), 0, 16);
                }
                const auto dwords = static_cast<std::uint32_t>((alignedTo - alignedFrom) / 4u);
                const auto recordChanged = [&] {
                    if (addressed) return recorder.RecordChangedCopy(commands, region.buffer->DeviceAddress() + (alignedFrom - region.begin), region.shadowCopy->DeviceAddress() + (alignedFrom - region.begin), region.copySourceAddress + (alignedFrom - region.copySourceBase), dwords, counter != nullptr ? counter->DeviceAddress() : 0);
                    return recorder.RecordBoundChangedCopy(commands, {region.buffer->Handle(), alignedFrom - region.begin}, {region.shadowCopy->Handle(), alignedFrom - region.begin}, {region.copySource, alignedFrom - region.copySourceBase}, dwords, counter != nullptr ? counter->Handle() : VK_NULL_HANDLE);
                };
                if (alignedFrom < alignedTo && recordChanged()) {
                    if (counter != nullptr) {
                        recorder.Keep(counter);
                        recorder.OnComplete([counter, alignedFrom, alignedTo, regionBegin = region.begin, regionEnd = region.end] {
                            std::uint32_t diverged = 0;
                            std::memcpy(&diverged, counter->Bytes().data(), sizeof(diverged));
                            static const bool alwaysCopy = std::getenv("APS5_CHANGED_COPY_ALWAYS") != nullptr;
                            if (diverged != 0) std::fprintf(stderr, "[copy-back] 0x%llx+0x%llx (region 0x%llx+0x%llx): %u dwords where the shader and the CPU changed the same bytes; %s\n", static_cast<unsigned long long>(alignedFrom), static_cast<unsigned long long>(alignedTo - alignedFrom), static_cast<unsigned long long>(regionBegin), static_cast<unsigned long long>(regionEnd - regionBegin), diverged, alwaysCopy ? "the shader's bytes were stored (APS5_CHANGED_COPY_ALWAYS)" : "the CPU's bytes were kept");
                        });
                    }
                    if (from < alignedFrom) CopyBuffer(context, commands, region.buffer->Handle(), from - region.begin, region.copySource, from - region.copySourceBase, alignedFrom - from);
                    if (alignedTo < to) CopyBuffer(context, commands, region.buffer->Handle(), alignedTo - region.begin, region.copySource, alignedTo - region.copySourceBase, to - alignedTo);
                    whole = wholeEnd;
                    recorder.Keep(region.shadowCopy);
                }
            }
            if (whole < wholeEnd) CopyBuffer(context, commands, region.buffer->Handle(), whole - region.begin, region.copySource, whole - region.copySourceBase, wholeEnd - whole);
            copiedBytes += to - from;
            recorder.Keep(region.buffer);
            if (profile) {
                Copies().gpuCopyBacks.fetch_add(1, std::memory_order_relaxed);
                Copies().gpuCopyBackBytes.fetch_add(to - from, std::memory_order_relaxed);
                if (region.deviceLocal) Copies().stagedOutBytes.fetch_add(to - from, std::memory_order_relaxed);
            }
        }
        region.copiedBack = true;
        region.releasedShadowCopy = region.shadowCopy;
        region.shadowCopy = nullptr;
    }
    if (!recording) return;
    if (Recorder::BarrierValidate()) recorder.NoteAccess(Recorder::CommandClass::StagingOut, Recorder::Access{{}, merged, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    constexpr VkAccessFlags copiedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, copiedAccess);
    Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
    recorder.MarkCovered(copiedAccess);
    recorder.EndGpuTiming(timing, copiedBytes);
}

VkDescriptorBufferInfo GuestBufferMemory::Descriptor(std::uint64_t address, std::size_t bytes, std::uint32_t& adjustment) const {
    Require(uploaded && !committed, "guest GPU memory is not available");
    Require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest buffer view");
    const auto* found = owner(address, address + bytes);
    Require(found != nullptr, "guest buffer has no GPU owner");
    const auto& region = *found;
    const auto describe = [&](const char* what) {
        char text[256];
        std::snprintf(text, sizeof(text), "guest buffer view 0x%llx+0x%llx %s its GPU owner 0x%llx-0x%llx (buffer %d, direct %d, mirror %d)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), what, static_cast<unsigned long long>(region.begin), static_cast<unsigned long long>(region.end), region.buffer != nullptr, region.direct != nullptr, region.mirror != nullptr);
        return std::string(text);
    };
    if (!(address >= region.begin && address + bytes <= region.end && (region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr))) Require(false, describe("exceeds"));
    const auto base = region.direct != nullptr ? region.direct->base - region.direct->lead : region.mirror != nullptr ? mirrorStart(*region.mirror) : region.begin;
    const auto offset = address - base;
    Require(context.limits.minStorageBufferOffsetAlignment != 0, "no storage buffer offset alignment");
    Require(base % 4 == 0, "a guest buffer view in a GPU owner that does not start at a DWORD boundary is not implemented");
    adjustment = static_cast<std::uint32_t>(offset % std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4));
    auto range = ViewBytes(bytes, adjustment);
    const auto end = region.direct != nullptr ? region.direct->base + region.direct->bytes : servedEnd(region);
    const bool cut = (region.direct == nullptr || region.runCut) && end < region.end;
    if (cut) {
        Require(address - adjustment < end, "guest buffer view starts past its copy");
        range = std::min<std::uint64_t>(range, end - (address - adjustment));
    }
    if (bytes <= context.limits.maxStorageBufferRange) range = std::min<std::uint64_t>(range, context.limits.maxStorageBufferRange & ~std::uint64_t{3});
    if (address - adjustment + range > end) Require(false, describe("rounded up to its view range exceeds"));
    Require(range <= context.limits.maxStorageBufferRange, "guest buffer view exceeds descriptor range limit");
    const auto handle = region.direct != nullptr ? region.direct->buffer : region.mirror != nullptr ? mirrorHandle(*region.mirror) : region.buffer->Handle();
    return {handle, offset - adjustment, range};
}

}
}
}
}
}
}
}
}
