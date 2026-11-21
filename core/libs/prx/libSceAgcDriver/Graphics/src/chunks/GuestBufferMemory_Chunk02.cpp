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
const char* createChunkedDmaBufImport(const Context& context, HostImport& entry, int file, std::uint64_t offset, bool& fits, VkResult& failure) {
    const auto index = offset / ImportChunkBytes;
    const auto within = offset - index * ImportChunkBytes;
    fits = entry.bytes != 0 && within + entry.bytes <= ImportChunkBytes;
    if (!fits) return nullptr;
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = entry.bytes;
    info.usage = ChunkImportUsage;
    ImportFamilies families;
    shareDmaBufImport(context, info, families, false);
    VkBuffer buffer = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) {
        failure = result;
        return "vkCreateBuffer";
    }
    const auto destroy = [&] { context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr); };
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
    if (requirements.alignment == 0 || within % requirements.alignment != 0 || within + requirements.size > ImportChunkBytes) {
        destroy();
        fits = false;
        return nullptr;
    }
    const char* step = nullptr;
    auto chunk = importChunk(context, file, index, requirements.memoryTypeBits, step, failure);
    if (chunk == nullptr) {
        destroy();
        return step;
    }
    if (((requirements.memoryTypeBits >> chunk->memoryType) & 1u) == 0) {
        destroy();
        fits = false;
        return nullptr;
    }
    if (const auto result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, buffer, chunk->memory, within); result != VK_SUCCESS) {
        destroy();
        failure = result;
        return "vkBindBufferMemory";
    }
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
    entry.buffer = buffer;
    entry.memory = VK_NULL_HANDLE;
    entry.address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    entry.chunk = std::move(chunk);
    entry.dmaBuf = true;
    return nullptr;
}

constexpr VkBufferUsageFlags SparseImportUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

[[noreturn]] void throwSparseImport(const HostImport& entry, const char* step, VkResult result) {
    char text[256];
    std::snprintf(text, sizeof(text), "AGC graphics: sparse dma-buf import of 0x%llx+0x%llx failed at %s (%d)", static_cast<unsigned long long>(entry.base), static_cast<unsigned long long>(entry.bytes), step, static_cast<int>(result));
    throw std::runtime_error(text);
}

VkMemoryRequirements dmaBufBufferRequirements(const Context& context, const HostImport& entry, VkBufferCreateFlags flags, VkBufferUsageFlags usage, VkDeviceSize size, VkBuffer* kept) {
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.flags = flags;
    info.size = size;
    info.usage = usage;
    ImportFamilies families;
    shareDmaBufImport(context, info, families, (flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0);
    VkBuffer buffer = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) throwSparseImport(entry, "vkCreateBuffer", result);
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
    if (kept != nullptr) {
    } else {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
    }
    return requirements;
}

bool createSparseDmaBufImport(const Context& context, HostImport& entry, int file, std::span<const GuestArena::SharedBackingRun> runs) {
    if (runs.empty()) return false;
    const auto lead = runs[0].offset % SparseBlockBytes;
    const bool ownQueue = context.sparseQueue != VK_NULL_HANDLE && context.sparseQueueMutex != nullptr;
    if (!context.sparseBinding || entry.bytes <= ImportChunkBytes || (!ownQueue && !GuestMemory::GpuMutex().HeldByThisThread()) || udmabufDevice() < 0) return false;
    struct stat backing {};
    if (fstat(file, &backing) != 0) throwSparseImport(entry, "fstat of the shared backing", VK_ERROR_INVALID_EXTERNAL_HANDLE);
    const auto chunkTypes = dmaBufBufferRequirements(context, entry, 0, ChunkImportUsage, ImportChunkBytes, nullptr).memoryTypeBits;
    VkBuffer buffer = VK_NULL_HANDLE;
    const auto requirements = dmaBufBufferRequirements(context, entry, VK_BUFFER_CREATE_SPARSE_BINDING_BIT, SparseImportUsage, lead + entry.bytes, &buffer);
    const auto alignment = requirements.alignment;
    struct Piece {
        std::uint64_t fileOffset;
        VkDeviceSize resource;
        VkDeviceSize bytes;
    };
    std::vector<Piece> pieces;
    bool chosen = alignment != 0 && ImportChunkBytes % alignment == 0 && (chunkTypes & ~requirements.memoryTypeBits) == 0 && (requirements.memoryTypeBits & chunkTypes) != 0;
    VkDeviceSize resource = 0;
    for (std::size_t index = 0; chosen && index < runs.size(); ++index) {
        const auto fileOffset = index == 0 ? runs[0].offset - lead : runs[index].offset;
        const VkDeviceSize bytes = index == 0 ? lead + runs[0].bytes : runs[index].bytes;
        const auto length = index + 1 == runs.size() ? requirements.size - resource : bytes;
        chosen = fileOffset % alignment == 0 && resource % alignment == 0 && (index + 1 == runs.size() || bytes % alignment == 0) && resource + bytes <= requirements.size &&
            (fileOffset + length + ImportChunkBytes - 1) / ImportChunkBytes * ImportChunkBytes <= static_cast<std::uint64_t>(backing.st_size);
        pieces.push_back({fileOffset, resource, length});
        resource += bytes;
    }
    const auto fail = [&](const char* step, VkResult result) {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
        throwSparseImport(entry, step, result);
    };
    if (!chosen) {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
        return false;
    }
    auto chunks = std::make_shared<std::vector<std::shared_ptr<ImportChunk>>>();
    std::vector<VkSparseMemoryBind> binds;
    for (const auto& piece : pieces) {
        for (VkDeviceSize done = 0; done < piece.bytes;) {
            const auto position = piece.fileOffset + done;
            const auto index = position / ImportChunkBytes;
            const auto within = position - index * ImportChunkBytes;
            const auto size = std::min<VkDeviceSize>(ImportChunkBytes - within, piece.bytes - done);
            const char* step = nullptr;
            VkResult failure = VK_SUCCESS;
            auto chunk = importChunk(context, file, index, requirements.memoryTypeBits, step, failure);
            if (chunk == nullptr) fail(step, failure);
            if (((requirements.memoryTypeBits >> chunk->memoryType) & 1u) == 0) fail("the memory type of a chunk import", VK_ERROR_FORMAT_NOT_SUPPORTED);
            binds.push_back({piece.resource + done, size, chunk->memory, within, 0});
            chunks->push_back(std::move(chunk));
            done += size;
        }
    }
    const VkSparseBufferMemoryBindInfo bufferBinds{buffer, static_cast<std::uint32_t>(binds.size()), binds.data()};
    VkBindSparseInfo bind{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bind.bufferBindCount = 1;
    bind.pBufferBinds = &bufferBinds;
    const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence); result != VK_SUCCESS) fail("vkCreateFence", result);
    VkResult bound = VK_SUCCESS;
    VkResult waited = VK_SUCCESS;
    {
        std::unique_lock<std::mutex> queueLock;
        if (ownQueue) queueLock = std::unique_lock(*context.sparseQueueMutex);
        bound = context.Function<PFN_vkQueueBindSparse>("vkQueueBindSparse")(ownQueue ? context.sparseQueue : context.queue, 1, &bind, fence);
        if (bound == VK_SUCCESS) waited = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (bound != VK_SUCCESS) fail("vkQueueBindSparse", bound);
    if (waited != VK_SUCCESS) fail("vkWaitForFences", waited);
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
    const auto address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    if (address == 0) fail("vkGetBufferDeviceAddressKHR", VK_ERROR_UNKNOWN);
    entry.buffer = buffer;
    entry.memory = VK_NULL_HANDLE;
    entry.address = address + lead;
    entry.chunk = std::move(chunks);
    entry.dmaBuf = true;
    entry.lead = lead;
    return true;
}
#endif

#ifndef _WIN32
const char* createBackingImport(const Context& context, HostImport& entry, int file, const GuestArena::SharedBackingRun* runs, std::size_t count, VkResult& failure) {
    bool fits = false;
    VkResult chunkFailure = VK_SUCCESS;
    OwnedDescriptor owned(file);
    if (count == 1 && createChunkedDmaBufImport(context, entry, owned.file, runs[0].offset, fits, chunkFailure) == nullptr && fits) return nullptr;
    if ((count > 1 || !fits) && createSparseDmaBufImport(context, entry, owned.file, std::span<const GuestArena::SharedBackingRun>(runs, count))) return nullptr;
    return createDmaBufImport(context, entry, owned.Release(), std::span<const GuestArena::SharedBackingRun>(runs, count), failure);
}

bool resolveBackingRuns(std::uint64_t rangeBase, std::uint64_t rangeBytes, int& file, std::array<GuestArena::SharedBackingRun, DmaBufRunLimit>& runs, std::size_t& count) {
    const auto base = static_cast<std::uintptr_t>(rangeBase);
    const auto bytes = static_cast<std::size_t>(rangeBytes);
    if (GuestArena::GuestArenaSharedBackingRuns_nid_postfix(base, bytes, &file, runs.data(), runs.size(), &count)) return true;
    std::uint64_t offset = 0;
    if (!GuestArena::GuestArenaSharedBacking_nid_postfix(base, bytes, &file, &offset)) return false;
    runs[0] = GuestArena::SharedBackingRun{offset, bytes};
    count = 1;
    return true;
}

bool resolveBackingRuns(const HostImport& entry, int& file, std::array<GuestArena::SharedBackingRun, DmaBufRunLimit>& runs, std::size_t& count) {
    return resolveBackingRuns(entry.base, entry.bytes, file, runs, count);
}

bool sharedBacked(const Context& context, std::uint64_t base, std::uint64_t bytes) {
    if (!context.dmaBufImport) return false;
    int file = -1;
    std::array<GuestArena::SharedBackingRun, DmaBufRunLimit> runs{};
    std::size_t count = 0;
    if (!resolveBackingRuns(base, bytes, file, runs, count)) return false;
    OwnedDescriptor owned(file);
    return true;
}

const char* createSharedBackingImport(const Context& context, HostImport& entry, VkResult& failure) {
    int file = -1;
    std::array<GuestArena::SharedBackingRun, DmaBufRunLimit> runs{};
    std::size_t count = 0;
    if (resolveBackingRuns(entry, file, runs, count)) return createBackingImport(context, entry, file, runs.data(), count, failure);
    failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    return "shared backing runs";
}
#endif

const char* createImport(const Context& context, HostImport& entry, VkResult& failure, bool dmaBufFallback) {
    const char* step = createHostPointerImport(context, entry, failure);
#ifndef _WIN32
    if (step != nullptr && dmaBufFallback && context.dmaBufImport) step = createSharedBackingImport(context, entry, failure);
#else
    static_cast<void>(dmaBufFallback);
#endif
    return step;
}

#ifndef _WIN32
enum class ImportWatchRequest : std::uint8_t { Probe, Watch, Unwatch };

ImportWatchRequest importWatchRequest() {
    static const auto request = [] {
        const char* value = std::getenv("APS5_WRITE_WATCH_IMPORTS");
        if (value == nullptr || std::strcmp(value, "probe") == 0) return ImportWatchRequest::Probe;
        if (std::strcmp(value, "watch") == 0) return ImportWatchRequest::Watch;
        if (std::strcmp(value, "unwatch") == 0) return ImportWatchRequest::Unwatch;
        throw std::runtime_error(std::string("APS5_WRITE_WATCH_IMPORTS=") + value + ": expected probe, watch or unwatch");
    }();
    return request;
}
#endif

bool importsStale(const Context& context, const HostImports& state);

constexpr std::uint64_t settleFloorBytes = 0x100000;
#ifdef _WIN32
constexpr std::uint64_t settleBlockBytes = 0x10000;
constexpr std::uint64_t settlePageBytes = 0x1000;
constexpr auto settlePollPeriod = std::chrono::microseconds(100);
std::atomic<bool> deferredSettles{std::getenv("APS5_NO_DEFERRED_SETTLES") == nullptr};
#endif

ImportSettle& settleFor(const Context& context, HostImports& state) {
    auto& settle = state.settles[context.device];
    settle.destroyFence = context.Function<PFN_vkDestroyFence>("vkDestroyFence");
    settle.settleDestroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    settle.settleFreeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
#ifdef _WIN32
    settle.waitForFences = context.Function<PFN_vkWaitForFences>("vkWaitForFences");
#endif
    return settle;
}

std::atomic<std::uint64_t> settledImports{0};

const char* settleImport(const Context& context, HostImports& state, const HostImport& entry, VkResult& result) {
    std::unique_lock queueLock(*context.settleQueueMutex);
    auto& settle = settleFor(context, state);
    if (settle.settleTarget == VK_NULL_HANDLE) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = 256;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ((result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &settle.settleTarget)) != VK_SUCCESS) {
            settle.settleTarget = VK_NULL_HANDLE;
            return "vkCreateBuffer (settle)";
        }
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, settle.settleTarget, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((result = MemoryAccounting::Allocate(context, allocation, "host import settle", &settle.settleMemory)) != VK_SUCCESS || (result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, settle.settleTarget, settle.settleMemory, 0)) != VK_SUCCESS) {
            context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, settle.settleTarget, nullptr);
            if (settle.settleMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, settle.settleMemory);
            settle.settleTarget = VK_NULL_HANDLE;
            settle.settleMemory = VK_NULL_HANDLE;
            return "the settle target";
        }
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.settlePool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if ((result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, &settle.settleCommands)) != VK_SUCCESS) {
            settle.settleCommands = VK_NULL_HANDLE;
            return "vkAllocateCommandBuffers (settle)";
        }
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &settle.settleFence)) != VK_SUCCESS) {
            settle.settleFence = VK_NULL_HANDLE;
            return "vkCreateFence (settle)";
        }
    }
    if (settle.settleCommands == VK_NULL_HANDLE || settle.settleFence == VK_NULL_HANDLE) return "the settle commands";
    VkBuffer source = entry.buffer;
    VkBuffer alias = VK_NULL_HANDLE;
    const auto run = [&]() -> const char* {
#ifdef _WIN32
        if (context.settleFamily != context.queueFamily) {
            VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
            external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
            VkBufferCreateInfo aliasInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
            aliasInfo.size = std::min<VkDeviceSize>(entry.bytes, 65536);
            aliasInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if ((result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &aliasInfo, nullptr, &alias)) != VK_SUCCESS) return "vkCreateBuffer (settle alias)";
            if ((result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, alias, entry.memory, 0)) != VK_SUCCESS) return "vkBindBufferMemory (settle alias)";
            source = alias;
        }
#endif
        if ((result = context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &settle.settleFence)) != VK_SUCCESS) return "vkResetFences (settle)";
        if ((result = context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(settle.settleCommands, 0)) != VK_SUCCESS) return "vkResetCommandBuffer (settle)";
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if ((result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(settle.settleCommands, &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer (settle)";
        const VkBufferCopy region{0, 0, 4};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(settle.settleCommands, source, settle.settleTarget, 1, &region);
        if ((result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(settle.settleCommands)) != VK_SUCCESS) return "vkEndCommandBuffer (settle)";
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &settle.settleCommands;
        if ((result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.settleQueue, 1, &submit, settle.settleFence)) != VK_SUCCESS) return "vkQueueSubmit (settle)";
#ifdef _WIN32
        ++state.settlesWaited;
#endif
        if ((result = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &settle.settleFence, VK_TRUE, 10'000'000'000ull)) != VK_SUCCESS) return "vkWaitForFences (settle)";
        return nullptr;
    };
    const char* step = run();
    if (step != nullptr) context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.settleQueue);
    if (alias != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, alias, nullptr);
    if (step == nullptr) settledImports.fetch_add(1, std::memory_order_relaxed);
    return step;
}

#ifdef _WIN32
const char* prepareSettleBatch(const Context& context, ImportSettle& settle, SettleBatch& batch, VkResult& result) {
    if (settle.batchTarget == VK_NULL_HANDLE) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = VkDeviceSize{settleBatchCount} * settleBatchCopies * 4u;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ((result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &settle.batchTarget)) != VK_SUCCESS) {
            settle.batchTarget = VK_NULL_HANDLE;
            return "vkCreateBuffer (settle batch)";
        }
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, settle.batchTarget, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((result = MemoryAccounting::Allocate(context, allocation, "host import batch", &settle.batchMemory)) != VK_SUCCESS || (result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, settle.batchTarget, settle.batchMemory, 0)) != VK_SUCCESS) {
            context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, settle.batchTarget, nullptr);
            if (settle.batchMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, settle.batchMemory);
            settle.batchTarget = VK_NULL_HANDLE;
            settle.batchMemory = VK_NULL_HANDLE;
            return "the settle batch target";
        }
    }
    if (batch.commands == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if ((result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, &batch.commands)) != VK_SUCCESS) {
            batch.commands = VK_NULL_HANDLE;
            return "vkAllocateCommandBuffers (settle batch)";
        }
    }
    if (batch.fence == VK_NULL_HANDLE) {
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &batch.fence)) != VK_SUCCESS) {
            batch.fence = VK_NULL_HANDLE;
            return "vkCreateFence (settle batch)";
        }
    }
    return nullptr;
}

void dropWantedSettles(HostImports& state) {
    for (const auto base : state.settleWanted) {
        if (const auto settle = state.pendingSettles.find(base); settle != state.pendingSettles.end() && settle->second.phase == SettlePhase::Wanted) state.pendingSettles.erase(settle);
    }
    state.settleWanted.clear();
}

void submitSettleBatch(const Context& context, HostImports& state) {
    if (state.settleWanted.empty()) return;
    auto& deviceSettle = settleFor(context, state);
    const auto free = std::find_if(deviceSettle.batches.begin(), deviceSettle.batches.end(), [](const SettleBatch& batch) { return !batch.busy; });
    if (free == deviceSettle.batches.end()) return;
    const auto index = static_cast<std::size_t>(free - deviceSettle.batches.begin());
    auto& batch = *free;
    std::unique_lock queueLock(*context.settleQueueMutex);
    VkResult result = VK_SUCCESS;
    const auto destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    const auto fail = [&](const char* step) {
        for (const auto alias : batch.aliases) destroyBuffer(context.device, alias, nullptr);
        batch.aliases.clear();
        for (const auto base : batch.bases) state.pendingSettles.erase(base);
        batch.bases.clear();
        dropWantedSettles(state);
        std::fprintf(stderr, "[gpu] settling host imports under %llu KiB without waiting failed at %s (%d); they stay compared\n", static_cast<unsigned long long>(settleFloorBytes >> 10u), step, static_cast<int>(result));
    };
    if (const char* step = prepareSettleBatch(context, deviceSettle, batch, result)) return fail(step);
    if ((result = context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &batch.fence)) != VK_SUCCESS) return fail("vkResetFences (settle batch)");
    if ((result = context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(batch.commands, 0)) != VK_SUCCESS) return fail("vkResetCommandBuffer (settle batch)");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if ((result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(batch.commands, &begin)) != VK_SUCCESS) return fail("vkBeginCommandBuffer (settle batch)");
    const auto copy = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
    RecordMemoryBarrier(context, batch.commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    std::size_t consumed = 0;
    for (; consumed < state.settleWanted.size() && batch.bases.size() < settleBatchCopies; ++consumed) {
        const auto base = state.settleWanted[consumed];
        const auto settle = state.pendingSettles.find(base);
        if (settle == state.pendingSettles.end() || settle->second.phase != SettlePhase::Wanted) continue;
        const auto import = state.imports.find(base);
        if (import == state.imports.end() || !import->second.unwatched) {
            state.pendingSettles.erase(settle);
            continue;
        }
        const VkBufferCopy region{0, (VkDeviceSize{index} * settleBatchCopies + batch.bases.size()) * 4u, 4};
        copy(batch.commands, import->second.buffer, deviceSettle.batchTarget, 1, &region);
        settle->second = PendingSettle{SettlePhase::Submitted, index, context.device};
        batch.bases.push_back(base);
    }
    state.settleWanted.erase(state.settleWanted.begin(), state.settleWanted.begin() + static_cast<std::ptrdiff_t>(consumed));
    RecordMemoryBarrier(context, batch.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_WRITE_BIT);
    if ((result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(batch.commands)) != VK_SUCCESS) return fail("vkEndCommandBuffer (settle batch)");
    if (batch.bases.empty()) return;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &batch.commands;
    if ((result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, batch.fence)) != VK_SUCCESS) return fail("vkQueueSubmit (settle batch)");
    batch.busy = true;
    state.settlesSubmitted += batch.bases.size();
}

}
}
