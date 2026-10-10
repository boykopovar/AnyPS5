#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <atomic>
#include <tuple>
#include <functional>
#include <bit>
#include <condition_variable>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <stop_token>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver::Graphics {

// Image mirrors. The main guest image (the PE at 0x7ff6b...) is MEM_IMAGE memory, which
// VK_EXT_external_memory_host refuses, and address-based shaders map every registered range, so each
// of their builds copied the whole image (~55 MiB: ~40 MiB of read-only sections snapshotted and copied
// again, ~15 MiB of writable ones copied twice). Instead one host-cached device buffer per registered
// image range persists here. Read-only ranges are filled once: their pages change only through an
// mprotect, which replaces the registered Range object, and a mirror whose Range is gone is rebuilt.
// Writable ranges keep a shadow of the guest bytes and copy the 64 KiB blocks that differ on each use.
// A mirror never holds the Range shared_ptr: a pinned range makes the guest's mprotect spin and fail.
struct ImageMirror {
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    bool writable = false;
    bool heap = false;
    std::vector<std::uint64_t> generations;
    std::weak_ptr<const GuestAllocations::Range> range;
    std::shared_ptr<Buffer> buffer;
    // Writable ranges only: the guest bytes the mirror holds, compared on refresh and at write-back.
    std::vector<std::byte> shadow;
    // Identity for the life of this mirror (see ImageMirrorSerial); the top bit keeps mirror serials
    // apart from host import serials.
    std::uint64_t serial = 0;
};

// The address space. An address-based build maps every readable registered range; building that
// list per build (the lease's ~1200 shared_ptrs, an import lookup per range, the sort and merge, the
// upload loop, the BDA table) repeated the same result while the registry stood still. One space is
// cached per device and taken by every build made while it is current: a pure function of the
// registry generation read BEFORE its lease was acquired (a mutation bumps the generation before it
// releases the registry, so an unchanged generation means the lease is the one a build would
// acquire now), the import registry's epoch (bumped by every retire, so the `direct` pointers of
// its regions stand) and the device. `base` holds the ranges served in place (imports, mirrors),
// sorted and free of per-build state; the ranges copied per build are `copied`, re-added to each
// build's own regions, as are the V#s and snapshots outside the space. The space's lease pins its
// ranges while any build, batch or the cache holds it; the registry's pin waiter (WaitForLeases)
// drops the cache's reference first, lock-free, so a guest free of a range held only by the cache
// costs no GPU wait. APS5_NO_ADDRESS_SPACE_CACHE=1 restores the per-build path.
struct GuestBufferMemory::AddressSpace {
    std::uint64_t generation = 0;
    std::uint64_t importsEpoch = 0;
    VkDevice device = VK_NULL_HANDLE;
    // Identity for the life of this space (see CachedAddressTable).
    std::uint64_t serial = 0;
    GuestAllocations::Lease lease;
    std::vector<Region> base;
    struct HeapRun {
        std::uint64_t begin;
        std::uint64_t bytes;
        std::size_t first;
        std::size_t last;
    };
    bool mirrored = false;
    std::vector<std::size_t> writableMirrors;
    std::vector<ImageMirror*> heapMirrors;
    std::vector<HeapRun> heapRuns;
    std::unique_ptr<std::atomic<std::uint64_t>[]> verified;
    std::vector<CopiedRange> copied;
    // The BDA table entries of `base`, in its order.
    std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
    mutable std::mutex tableMutex;
    mutable std::map<std::vector<std::pair<std::uint64_t, std::uint64_t>>, std::pair<std::uint64_t, std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>>>> writeTables;
};

namespace {

struct AddressSpaceCache {
    std::atomic<std::shared_ptr<const GuestBufferMemory::AddressSpace>> current;
    std::atomic<std::uint64_t> serials{0};
    // Set by the waiter's drop, cleared by the next publish: the rebuild's reason.
    std::atomic<bool> droppedByWaiter{false};
    std::atomic<std::uint64_t> hits{0};
    std::atomic<std::uint64_t> rebuiltFirst{0};
    std::atomic<std::uint64_t> rebuiltGeneration{0};
    std::atomic<std::uint64_t> rebuiltEpoch{0};
    std::atomic<std::uint64_t> rebuiltDevice{0};
    std::atomic<std::uint64_t> rebuiltWaiterDrop{0};
    std::atomic<std::uint64_t> unpublished{0};
    std::atomic<std::uint64_t> dissolvedOverlap{0};
    std::atomic<std::uint64_t> dissolvedImports{0};
    std::atomic<std::uint64_t> waiterDrops{0};
};

AddressSpaceCache& Spaces() {
    static AddressSpaceCache cache;
    return cache;
}

bool addressSpaceCacheEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ADDRESS_SPACE_CACHE") != nullptr;
    return !disabled;
}

// Imports persist across draws, keyed by allocation base, and are dropped when their allocation leaves
// the registered set; a dropped import is destroyed once the recorded work that may read it completed.
struct HostImports {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    std::map<std::uint64_t, HostImport> imports;
    std::set<std::uint64_t> failed;
    // Registry generation the imports were last reconciled with.
    std::uint64_t refreshedGeneration = 0;
    // Bumped whenever an import is dropped, so HostImport pointers taken under the lock earlier can be
    // reused while it is unchanged.
    std::uint64_t epoch = 1;
    VkDevice watchDevice = VK_NULL_HANDLE;
    bool unwatchImports = false;
    bool unwatchDmaBufImports = false;
};

HostImports& Imports() {
    static HostImports imports;
    return imports;
}

void destroyImport(const Context& context, const HostImport& entry) {
    context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, entry.buffer, nullptr);
    context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, entry.memory, nullptr);
#ifdef _WIN32
    GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
}

// Frees a dropped import's Vulkan objects when it is released. Never copied: a copy would destroy the
// same handles twice (and a temporary would destroy them at once).
struct RetiredImport {
    RetiredImport(VkDevice device, PFN_vkDestroyBuffer destroyBuffer, PFN_vkFreeMemory freeMemory, const HostImport& entry)
        : device(device), destroyBuffer(destroyBuffer), freeMemory(freeMemory), entry(entry) {}
    RetiredImport(const RetiredImport&) = delete;
    RetiredImport& operator=(const RetiredImport&) = delete;
    ~RetiredImport() {
        destroyBuffer(device, entry.buffer, nullptr);
        freeMemory(device, entry.memory, nullptr);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    VkDevice device;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    HostImport entry;
};

// Drops an import. Recorded batches (dispatches, recorded draws, GPU label writes) may still read it,
// so the objects live until the batch open now completed; with no batches in flight all GPU work that
// used it was synchronous and it is destroyed at once.
const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);

void retireImport(VkDevice device, HostImports& state, std::map<std::uint64_t, HostImport>::iterator it, const GuestAllocations::Lease& lease) {
    // Results shadowed for the import reach its (old) buffer first where the memory is still a
    // readable registered range (the import retires because its registration vanished or changed
    // size); the holder below outlives the batch that copies them.
    RetireShadow(device, it->second, [&lease](std::uint64_t begin, std::uint64_t end) { return containingRange(lease, begin, end) != nullptr; });
    auto holder = std::make_shared<RetiredImport>(state.device, state.destroyBuffer, state.freeMemory, it->second);
    if (auto* recorder = Recorder::Active(); recorder != nullptr && !recorder->Idle()) recorder->Keep(std::move(holder));
    ++state.epoch;
    state.imports.erase(it);
}

const char* bindImport(const Context& context, HostImport& entry, VkExternalMemoryHandleTypeFlagBits handleType, const void* import, std::uint32_t importTypes, bool& allocated, VkResult& failure) {
    const auto failed = [&](const char* step, VkResult result) -> const char* {
        if (entry.buffer) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, entry.buffer, nullptr);
        if (entry.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, entry.memory, nullptr);
        entry.buffer = VK_NULL_HANDLE;
        entry.memory = VK_NULL_HANDLE;
        failure = result;
        return step;
    };
    allocated = false;
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, static_cast<VkExternalMemoryHandleTypeFlags>(handleType)};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = entry.bytes;
    // INDIRECT_BUFFER: DISPATCH_INDIRECT group counts are read in place (VulkanDevice::DispatchIndirect).
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &entry.buffer); result != VK_SUCCESS) return failed("vkCreateBuffer", result);
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, entry.buffer, &requirements);
    const auto types = requirements.memoryTypeBits & importTypes;
    if (types == 0) return failed("memory type selection", VK_ERROR_FORMAT_NOT_SUPPORTED);
    const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags};
    allocation.allocationSize = entry.bytes;
    allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(types));
    if (const auto result = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &entry.memory); result != VK_SUCCESS) return failed("vkAllocateMemory", result);
    allocated = true;
    if (const auto result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, entry.buffer, entry.memory, 0); result != VK_SUCCESS) return failed("vkBindBufferMemory", result);
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, entry.buffer};
    entry.address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    if (entry.address == 0) return failed("vkGetBufferDeviceAddressKHR", VK_ERROR_UNKNOWN);
    return nullptr;
}

const char* createHostPointerImport(const Context& context, HostImport& entry, VkResult& failure) {
    void* const host = entry.alias != nullptr ? entry.alias : reinterpret_cast<void*>(entry.base);
    VkMemoryHostPointerPropertiesEXT pointer{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    if (const auto result = context.Function<PFN_vkGetMemoryHostPointerPropertiesEXT>("vkGetMemoryHostPointerPropertiesEXT")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host, &pointer); result != VK_SUCCESS) {
        failure = result;
        return "vkGetMemoryHostPointerPropertiesEXT";
    }
    const VkImportMemoryHostPointerInfoEXT import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host};
    bool allocated = false;
    return bindImport(context, entry, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, &import, pointer.memoryTypeBits, allocated, failure);
}

#ifndef _WIN32
int udmabufDevice() {
    static const int device = [] {
        const int result = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (result < 0) {
            const int error = errno;
            std::fprintf(stderr, "[gpu] open /dev/udmabuf: %s; shared direct memory is copied instead of imported, and GPU stores through FLAT/GLOBAL addresses do not reach it%s\n",
                std::strerror(error), error == EACCES ? " (give the user read-write access to /dev/udmabuf, for example through the kvm group)" : "");
        }
        return result;
    }();
    return device;
}

const char* createDmaBufImport(const Context& context, HostImport& entry, int file, std::uint64_t offset, VkResult& failure) {
    udmabuf_create request{};
    request.memfd = static_cast<std::uint32_t>(file);
    request.flags = UDMABUF_FLAGS_CLOEXEC;
    request.offset = offset;
    request.size = entry.bytes;
    const int device = udmabufDevice();
    const int buffer = device < 0 ? -1 : ioctl(device, UDMABUF_CREATE, &request);
    close(file);
    if (buffer < 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        return device < 0 ? "open /dev/udmabuf" : "UDMABUF_CREATE";
    }
    VkMemoryFdPropertiesKHR properties{VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
    if (const auto result = context.Function<PFN_vkGetMemoryFdPropertiesKHR>("vkGetMemoryFdPropertiesKHR")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer, &properties); result != VK_SUCCESS) {
        close(buffer);
        failure = result;
        return "vkGetMemoryFdPropertiesKHR";
    }
    const VkImportMemoryFdInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer};
    bool allocated = false;
    const char* step = bindImport(context, entry, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, &import, properties.memoryTypeBits, allocated, failure);
    if (!allocated) close(buffer);
    entry.dmaBuf = step == nullptr;
    return step;
}
#endif

#ifndef _WIN32
constexpr std::uint64_t ImportChunkBytes = std::uint64_t{64} << 20u;

struct ImportChunk {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkFreeMemory freeMemory = nullptr;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::uint32_t memoryType = 0;
    ImportChunk() = default;
    ImportChunk(const ImportChunk&) = delete;
    ImportChunk& operator=(const ImportChunk&) = delete;
    ~ImportChunk() {
        if (memory != VK_NULL_HANDLE) freeMemory(device, memory, nullptr);
    }
};

struct ImportChunks {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::tuple<dev_t, ino_t, std::uint64_t>, std::weak_ptr<ImportChunk>> chunks;
};

ImportChunks& Chunks() {
    static ImportChunks chunks;
    return chunks;
}

std::shared_ptr<ImportChunk> importChunk(const Context& context, int file, std::uint64_t index, std::uint32_t bufferTypes, const char*& step, VkResult& failure) {
    struct stat info {};
    if (fstat(file, &info) != 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        step = "fstat of the shared backing";
        return nullptr;
    }
    auto& chunks = Chunks();
    std::lock_guard lock(chunks.mutex);
    if (chunks.device != context.device) {
        chunks.chunks.clear();
        chunks.device = context.device;
    }
    const auto key = std::make_tuple(info.st_dev, info.st_ino, index);
    if (const auto found = chunks.chunks.find(key); found != chunks.chunks.end()) {
        if (auto chunk = found->second.lock()) return chunk;
        chunks.chunks.erase(found);
    }
    if (static_cast<std::uint64_t>(info.st_size) < (index + 1) * ImportChunkBytes) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        step = "a chunk past the end of the shared backing";
        return nullptr;
    }
    udmabuf_create request{};
    request.memfd = static_cast<std::uint32_t>(file);
    request.flags = UDMABUF_FLAGS_CLOEXEC;
    request.offset = index * ImportChunkBytes;
    request.size = ImportChunkBytes;
    const int device = udmabufDevice();
    const int buffer = device < 0 ? -1 : ioctl(device, UDMABUF_CREATE, &request);
    if (buffer < 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        step = device < 0 ? "open /dev/udmabuf" : "UDMABUF_CREATE";
        return nullptr;
    }
    VkMemoryFdPropertiesKHR properties{VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
    if (const auto result = context.Function<PFN_vkGetMemoryFdPropertiesKHR>("vkGetMemoryFdPropertiesKHR")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer, &properties); result != VK_SUCCESS) {
        close(buffer);
        failure = result;
        step = "vkGetMemoryFdPropertiesKHR";
        return nullptr;
    }
    const auto types = properties.memoryTypeBits & bufferTypes;
    if (types == 0) {
        close(buffer);
        failure = VK_ERROR_FORMAT_NOT_SUPPORTED;
        step = "memory type selection";
        return nullptr;
    }
    const VkImportMemoryFdInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer};
    const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, &import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags};
    allocation.allocationSize = ImportChunkBytes;
    allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(types));
    auto chunk = std::make_shared<ImportChunk>();
    if (const auto result = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &chunk->memory); result != VK_SUCCESS) {
        close(buffer);
        failure = result;
        step = "vkAllocateMemory";
        return nullptr;
    }
    chunk->device = context.device;
    chunk->freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
    chunk->memoryType = allocation.memoryTypeIndex;
    for (auto it = chunks.chunks.begin(); it != chunks.chunks.end();) it = it->second.expired() ? chunks.chunks.erase(it) : std::next(it);
    chunks.chunks[key] = chunk;
    return chunk;
}

const char* createChunkedDmaBufImport(const Context& context, HostImport& entry, int file, std::uint64_t offset, bool& fits, VkResult& failure) {
    const auto index = offset / ImportChunkBytes;
    const auto within = offset - index * ImportChunkBytes;
    fits = entry.bytes != 0 && within + entry.bytes <= ImportChunkBytes;
    if (!fits) return nullptr;
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = entry.bytes;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
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
#endif

const char* createImport(const Context& context, HostImport& entry, VkResult& failure) {
    const char* step = createHostPointerImport(context, entry, failure);
#ifndef _WIN32
    int file = -1;
    std::uint64_t offset = 0;
    if (step != nullptr && context.dmaBufImport && GuestArena::GuestArenaSharedBacking_nid_postfix(static_cast<std::uintptr_t>(entry.base), static_cast<std::size_t>(entry.bytes), &file, &offset)) {
        bool fits = false;
        VkResult chunkFailure = VK_SUCCESS;
        if (createChunkedDmaBufImport(context, entry, file, offset, fits, chunkFailure) == nullptr && fits) {
            close(file);
            return nullptr;
        }
        step = createDmaBufImport(context, entry, file, offset, failure);
    }
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

void decideImportWatch(const Context& context, HostImports& state) {
    if (state.watchDevice == context.device) return;
#ifdef _WIN32
    state.watchDevice = context.device;
    state.unwatchImports = true;
    state.unwatchDmaBufImports = false;
    if (context.hostImportAlignment != 0 && GuestMemory::WriteWatched()) std::fprintf(stderr, "[write-watch] Windows direct host imports use comparisons; separate shared aliases retain guest write tracking\n");
#else
    const auto request = importWatchRequest();
    state.watchDevice = context.device;
    state.unwatchImports = false;
    state.unwatchDmaBufImports = false;
    if (context.hostImportAlignment == 0 || !GuestMemory::WriteWatched()) return;
    if (request == ImportWatchRequest::Watch) {
        std::fprintf(stderr, "[write-watch] host imports stay watched (APS5_WRITE_WATCH_IMPORTS=watch)\n");
        return;
    }
    if (request == ImportWatchRequest::Unwatch) {
        state.unwatchImports = true;
        state.unwatchDmaBufImports = true;
        std::fprintf(stderr, "[write-watch] host imports are compared, not watched (APS5_WRITE_WATCH_IMPORTS=unwatch)\n");
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    if (probe.failure != nullptr) {
        state.unwatchImports = true;
        std::fprintf(stderr, "[write-watch] host imports resolve write protection: unknown (probe failed at %s, %d); imported ranges are compared\n", probe.failure, static_cast<int>(probe.result));
    } else {
        state.unwatchImports = probe.writtenAfterSubmit != 0;
        std::fprintf(stderr, "[write-watch] host imports resolve write protection: %s (%u of %u scratch pages written after a GPU read, %u after the import); imported ranges %s\n", state.unwatchImports ? "yes" : "no", probe.writtenAfterSubmit, probe.pages, probe.writtenAtImport, state.unwatchImports ? "are compared" : "stay watched");
    }
    state.unwatchDmaBufImports = state.unwatchImports;
    if (!context.dmaBufImport) return;
    const auto dmaBuf = ProbeDmaBufImportWriteProtection(context);
    if (dmaBuf.failure != nullptr) {
        state.unwatchDmaBufImports = true;
        std::fprintf(stderr, "[write-watch] dma-buf imports keep write protection: unknown (probe failed at %s, %d); imported ranges are compared\n", dmaBuf.failure, static_cast<int>(dmaBuf.result));
        return;
    }
    state.unwatchDmaBufImports = dmaBuf.writtenAfterSubmit != 0 || dmaBuf.writtenByCpu == 0;
    std::fprintf(stderr, "[write-watch] dma-buf imports keep write protection: %s (%u of %u scratch pages written after a GPU read, %u after the import, %u seen after a CPU store); imported ranges %s\n", state.unwatchDmaBufImports ? "no" : "yes", dmaBuf.writtenAfterSubmit, dmaBuf.pages, dmaBuf.writtenAtImport, dmaBuf.writtenByCpu, state.unwatchDmaBufImports ? "are compared" : "stay watched");
#endif
}

std::shared_ptr<const GuestAllocations::Range> leasedRangeOwner(const GuestAllocations::Lease& lease, std::uint64_t base) {
    const auto found = std::lower_bound(lease.begin(), lease.end(), base, [](const auto& range, std::uint64_t value) { return range->address < value; });
    return found != lease.end() && (*found)->address == base ? *found : nullptr;
}

bool sameRange(const HostImport& entry, const GuestAllocations::Lease& lease) {
    const auto current = leasedRangeOwner(lease, entry.base);
    return current != nullptr && !entry.range.owner_before(current) && !current.owner_before(entry.range);
}

const HostImport* importAllocation(const Context& context, HostImports& state, std::uint64_t base, std::uint64_t bytes, const GuestAllocations::Lease& lease) {
    if (const auto found = state.imports.find(base); found != state.imports.end()) {
        if (found->second.bytes == bytes && sameRange(found->second, lease)) return &found->second;
        retireImport(context.device, state, found, lease);
    }
    const auto alignment = context.hostImportAlignment;
    if (alignment == 0 || base % alignment != 0 || bytes % alignment != 0 || state.failed.contains(base)) return nullptr;
    // Pinned imports count against the driver's system memory budget; past it ordinary host
    // allocations fail, so imports stop at APS5_HOST_IMPORT_MIB (default 6 GiB, which covers the
    // registered memory of address-based shaders; past it they copy gigabytes per dispatch).
    static const std::uint64_t budget = [] {
        const char* value = std::getenv("APS5_HOST_IMPORT_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 6144ull) << 20u;
    }();
    std::uint64_t live = 0;
    for (const auto& [address, existing] : state.imports) live += existing.bytes;
    if (live + bytes > budget) {
        // A refused import turns every later use of the range into CPU copies, so say so.
        static std::uint64_t refused = 0, refusedBytes = 0;
        static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
        ++refused;
        refusedBytes += bytes;
        if (std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
            lastReport = std::chrono::steady_clock::now();
            std::fprintf(stderr, "[gpu] host import budget: %llu MiB live of %llu MiB (APS5_HOST_IMPORT_MIB); %llu imports (%llu MiB) refused so far, last 0x%llx+0x%llx\n", static_cast<unsigned long long>(live >> 20u), static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(refused), static_cast<unsigned long long>(refusedBytes >> 20u), static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes));
        }
        return nullptr;
    }
    HostImport entry{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
    entry.range = leasedRangeOwner(lease, base);
#ifdef _WIN32
    // Drivers pin imported pages, so every page must be committed and accessible.
    bool writable = true;
    bool readOnly = true;
    MEMORY_BASIC_INFORMATION refused{};
    for (std::uint64_t cursor = base; cursor < base + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0 || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            state.failed.insert(base);
            return nullptr;
        }
        const auto protection = info.Protect & 0xffu;
        const bool pageWritable = (info.Type == MEM_PRIVATE || info.Type == MEM_IMAGE) && (protection == PAGE_READWRITE || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_WRITECOPY);
        const bool pageReadOnly = info.Type != MEM_MAPPED && (protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ);
        if (!pageReadOnly) readOnly = false;
        if (info.Type != MEM_MAPPED && !pageWritable && !pageReadOnly && refused.BaseAddress == nullptr) refused = info;
        if (!pageWritable) writable = false;
        cursor = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
    }
    if (!writable && readOnly) {
        state.failed.insert(base);
        return nullptr;
    }
    if (!writable) {
        if (refused.BaseAddress != nullptr) {
            char text[256];
            std::snprintf(text, sizeof(text), "AGC graphics: host import of 0x%llx+0x%llx: memory at 0x%llx (type 0x%lx, protection 0x%lx) is neither read-write, read-only nor a shared mapping", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), reinterpret_cast<unsigned long long>(refused.BaseAddress), refused.Type, refused.Protect);
            throw std::runtime_error(text);
        }
        entry.alias = GuestArena::GuestArenaMapAlias_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes));
    }
#else
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(base), static_cast<std::size_t>(bytes), true)) {
        state.failed.insert(base);
        return nullptr;
    }
#endif
    decideImportWatch(context, state);
    VkResult result = VK_SUCCESS;
    const char* step = nullptr;
    GuestMemory::ImportWatched(base, bytes, [&] {
        step = createImport(context, entry, result);
        return step == nullptr;
    });
    entry.unwatched = step == nullptr && (entry.dmaBuf ? state.unwatchDmaBufImports : state.unwatchImports);
#ifdef _WIN32
    if (entry.alias != nullptr) entry.unwatched = false;
#endif
    if (entry.unwatched) GuestMemory::Unwatch(base, bytes);
    if (step != nullptr) {
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
        state.failed.insert(base);
        std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx failed at %s (%d); falling back to copies\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), step, static_cast<int>(result));
#ifdef _WIN32
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        for (std::uint64_t cursor = base; trace && cursor < base + bytes;) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) break;
            const auto regionEnd = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
            std::fprintf(stderr, "[gpu]   0x%llx+0x%llx state 0x%lx protect 0x%lx type 0x%lx allocation 0x%llx\n", static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(std::min(regionEnd, base + bytes) - cursor), info.State, info.Protect, info.Type, reinterpret_cast<unsigned long long>(info.AllocationBase));
            cursor = regionEnd;
        }
#else
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        if (std::FILE* maps = trace ? std::fopen("/proc/self/maps", "r") : nullptr) {
            char line[512];
            while (std::fgets(line, sizeof(line), maps) != nullptr) {
                unsigned long long first = 0;
                unsigned long long last = 0;
                if (std::sscanf(line, "%llx-%llx", &first, &last) != 2 || last <= base || first >= base + bytes) continue;
                std::fprintf(stderr, "[gpu]   %s", line);
            }
            std::fclose(maps);
        }
#endif
        return nullptr;
    }
    static std::uint64_t importedBytes = 0;
    importedBytes += bytes;
    static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
    std::uint64_t liveBytes = bytes;
    for (const auto& [address, existing] : state.imports) liveBytes += existing.bytes;
    if (trace) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx ok (%zu live, %.1f MiB live, %.1f MiB ever)\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), state.imports.size() + 1, liveBytes / 1048576.0, importedBytes / 1048576.0);
    return &state.imports.emplace(base, entry).first->second;
}

// The lease is in address order (it is built from the registry map), so a range is found by binary search.
const GuestAllocations::Range* leasedRangeAt(const GuestAllocations::Lease& lease, std::uint64_t base) {
    const auto found = std::lower_bound(lease.begin(), lease.end(), base, [](const auto& range, std::uint64_t value) { return range->address < value; });
    return found != lease.end() && (*found)->address == base ? found->get() : nullptr;
}

// The readable registered range containing [begin, end), or null.
const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *std::prev(found);
    return range->readable && begin >= range->address && end <= range->address + range->bytes ? range.get() : nullptr;
}

// Drops imports whose registered range changed or disappeared: their pages may no longer back the
// guest addresses. Walks the imports only when the registry changed since the last walk.
void refreshImports(const Context& context, HostImports& state, const GuestAllocations::Lease& lease) {
    if (state.device != context.device) {
        for (const auto& [address, entry] : state.imports) {
            if (state.device != VK_NULL_HANDLE && state.destroyBuffer != nullptr && state.freeMemory != nullptr) {
                state.destroyBuffer(state.device, entry.buffer, nullptr);
                state.freeMemory(state.device, entry.memory, nullptr);
            }
#ifdef _WIN32
            GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
        }
        state.imports.clear();
        state.failed.clear();
        state.device = context.device;
        state.destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
        state.freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
        state.refreshedGeneration = 0;
        ++state.epoch;
    }
    if (state.destroyBuffer == nullptr) state.destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    if (state.freeMemory == nullptr) state.freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (generation == state.refreshedGeneration) return;
    state.refreshedGeneration = generation;
    for (auto it = state.imports.begin(); it != state.imports.end();) {
        const auto* range = leasedRangeAt(lease, it->first);
        if (range != nullptr && range->bytes == it->second.bytes && sameRange(it->second, lease)) {
            if (it->second.unwatched) GuestMemory::Unwatch(it->first, it->second.bytes);
            ++it;
            continue;
        }
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        if (trace && range != nullptr && range->bytes == it->second.bytes) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx retired: the range was mapped again\n", static_cast<unsigned long long>(it->first), static_cast<unsigned long long>(it->second.bytes));
        const auto next = std::next(it);
        retireImport(context.device, state, it, lease);
        it = next;
    }
    for (auto it = state.failed.begin(); it != state.failed.end();) it = leasedRangeAt(lease, *it) != nullptr ? std::next(it) : state.failed.erase(it);
}

const HostImport* findImport(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    auto found = state.imports.upper_bound(begin);
    if (found == state.imports.begin()) return nullptr;
    --found;
    const auto& entry = found->second;
    return begin >= entry.base && end <= entry.base + entry.bytes ? &entry : nullptr;
}

// Whether the imports need reconciling before a lookup: the registry changed since the last walk.
bool importsStale(const Context& context, const HostImports& state) {
    return state.device != context.device || GuestAllocations::GuestAllocationsGeneration_nid_postfix() != state.refreshedGeneration;
}

// The mirror registry: by range base. The map is protected by `mutex` (findMirror runs in a build's
// unlocked stage A, UploadPrepare); the mirrors' contents (fills, refreshes, sweeps) change under
// GuestMemory::GpuMutex only (every completion and every stage B holds it). APS5_NO_LEASE_MIRROR=1
// restores copies.
struct ImageMirrors {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    std::set<std::uint64_t> failed;
    std::uint64_t heapBytes = 0;
    // APS5_PROFILE_DRAW counters, reported every 10 s as [buffers] image mirrors.
    std::uint64_t builds = 0;
    std::uint64_t heapRefills = 0;
    std::uint64_t heapUnwatched = 0;
    std::uint64_t heapImported = 0;
    std::uint64_t heapImportedBytes = 0;
    std::uint64_t subranges = 0;
    std::uint64_t rebuilds = 0;
    std::uint64_t refreshes = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
    // Refreshes that had to wait for recorded work writing the range.
    std::uint64_t syncs = 0;
    std::uint64_t serials = 0;
    std::uint64_t version = 0;
    std::uint64_t pinnedSpace = 0;
    std::uint64_t pinnedVersion = 0;
    std::uint64_t sweeps = 0;
    std::uint64_t heapChecks = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

// APS5_PROFILE_DRAW: the parts of one address-based build (see GuestBufferMemory::CountAddressBuild),
// timed on the building thread by AcquireRegistered, summed into the totals with the caller's
// snapshot compares and printed as the [address] line every 10 s.
struct AddressBuildTiming {
    double leaseUs = 0;
    double importsUs = 0;
    double mirrorsUs = 0;
    double compareUs = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
};

AddressBuildTiming& ThreadAddressTiming() {
    thread_local AddressBuildTiming timing;
    return timing;
}

struct AddressBuildTotals {
    std::mutex mutex;
    std::uint64_t builds = 0;
    AddressBuildTiming sums;
    double snapshotsUs = 0;
    BdaResources::TableCacheStats tableSeen;
    AddressSpaceStats spaceSeen;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

AddressBuildTotals& AddressBuilds() {
    static AddressBuildTotals totals;
    return totals;
}

ImageMirrors& Mirrors() {
    static ImageMirrors mirrors;
    return mirrors;
}

bool mirrorsEnabled() {
    static const bool disabled = std::getenv("APS5_NO_LEASE_MIRROR") != nullptr;
    return !disabled;
}

std::uint64_t heapMirrorBudget() {
    static const std::uint64_t bytes = [] {
        const char* value = std::getenv("APS5_HEAP_MIRROR_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 24576ull) << 20u;
    }();
    return bytes;
}

std::string heapMirrorError(const GuestAllocations::Range& range, std::uint64_t held, std::string_view reason) {
    constexpr std::string_view prefix = "AGC graphics: ";
    if (reason.starts_with(prefix)) reason.remove_prefix(prefix.size());
    char where[64];
    std::snprintf(where, sizeof(where), "heap mirror of 0x%llx+0x%llx: ", static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes));
    char holding[96];
    std::snprintf(holding, sizeof(holding), "; heap mirrors hold %llu MiB of %llu MiB (APS5_HEAP_MIRROR_MIB)", static_cast<unsigned long long>(held >> 20u), static_cast<unsigned long long>(heapMirrorBudget() >> 20u));
    return std::string(prefix) + where + std::string(reason) + holding;
}

std::uint64_t heapMirrorsBeside(const GuestAllocations::Range& range) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    std::uint64_t held = state.heapBytes;
    if (const auto found = state.entries.find(range.address); found != state.entries.end() && found->second->heap) held -= found->second->bytes;
    return held;
}

void sweepMirrors();

bool sameRange(const std::weak_ptr<const GuestAllocations::Range>& mirrored, const std::shared_ptr<const GuestAllocations::Range>& range) {
    return !mirrored.owner_before(range) && !range.owner_before(mirrored);
}

// One 64 KiB (or shorter, at a range's ends) block of a writable mirror to compare with guest memory.
struct RefreshBlock {
    ImageMirror* mirror;
    std::uint64_t address;
    std::size_t length;
    std::uint64_t generation = 0;
};

// Compares a block's guest bytes with the mirror's shadow and copies a changed block into the device
bool compareBlock(const RefreshBlock& block) {
    const auto offset = static_cast<std::size_t>(block.address - block.mirror->base);
    const auto* guest = reinterpret_cast<const std::byte*>(block.address);
    if (block.mirror->heap) {
        std::memcpy(block.mirror->buffer->Bytes().data() + offset, guest, block.length);
        block.mirror->generations[static_cast<std::size_t>(block.address / 65536 - block.mirror->base / 65536)] = block.generation;
        return true;
    }
    if (std::memcmp(block.mirror->shadow.data() + offset, guest, block.length) == 0) return false;
    std::memcpy(block.mirror->buffer->Bytes().data() + offset, guest, block.length);
    std::memcpy(block.mirror->shadow.data() + offset, guest, block.length);
    return true;
}

class RefreshPool {
public:
    static RefreshPool& Get() {
        if (instance == nullptr) instance = new RefreshPool;
        return *instance;
    }

    static void Shutdown() {
        delete std::exchange(instance, nullptr);
    }

    // Compares `blocks` and returns how many were copied.
    std::uint64_t Run(std::span<const RefreshBlock> blocks) {
        // Small jobs are not worth waking the helpers (a sub-range refresh at Upload is a few blocks).
        if (helpers == 0 || blocks.size() < 8) {
            std::uint64_t copied = 0;
            for (const auto& block : blocks) copied += compareBlock(block) ? 1 : 0;
            return copied;
        }
        // Every caller holds the device lock today; the pool serializes its jobs regardless.
        std::lock_guard running(runMutex);
        {
            std::lock_guard lock(mutex);
            job = blocks;
            next.store(0, std::memory_order_relaxed);
            copiedBlocks.store(0, std::memory_order_relaxed);
            finished = 0;
            ++serial;
        }
        wake.notify_all();
        work(blocks);
        std::unique_lock lock(mutex);
        done.wait(lock, [&] { return finished == helpers; });
        job = {};
        return copiedBlocks.load(std::memory_order_relaxed);
    }

private:
    RefreshPool() {
        static const bool disabled = std::getenv("APS5_NO_PARALLEL_MIRROR_REFRESH") != nullptr;
        const char* text = std::getenv("APS5_MIRROR_REFRESH_THREADS");
        const auto requested = text != nullptr ? std::atoi(text) : 3;
        const auto wanted = disabled ? 0u : static_cast<unsigned>(std::clamp(requested, 0, 16));
        for (unsigned i = 0; i < wanted; ++i) {
            threads.emplace_back([this](std::stop_token token) {
                CpuTopology::PinHelperThread("mirror refresh");
                helper(token);
            });
            ++helpers;
        }
    }

    // Takes blocks until none is left.
    void work(std::span<const RefreshBlock> blocks) {
        std::uint64_t copied = 0;
        for (auto index = next.fetch_add(1, std::memory_order_relaxed); index < blocks.size(); index = next.fetch_add(1, std::memory_order_relaxed)) copied += compareBlock(blocks[index]) ? 1 : 0;
        copiedBlocks.fetch_add(copied, std::memory_order_relaxed);
    }

    void helper(std::stop_token token) {
        std::uint64_t seen = 0;
        for (;;) {
            std::span<const RefreshBlock> blocks;
            {
                std::unique_lock lock(mutex);
                if (!wake.wait(lock, token, [&] { return serial != seen; })) return;
                seen = serial;
                blocks = job;
            }
            work(blocks);
            {
                std::lock_guard lock(mutex);
                ++finished;
            }
            done.notify_all();
        }
    }

    inline static RefreshPool* instance = nullptr;
    unsigned helpers = 0;
    std::mutex runMutex;
    std::mutex mutex;
    std::condition_variable_any wake;
    std::condition_variable done;
    std::span<const RefreshBlock> job;
    std::uint64_t serial = 0;
    unsigned finished = 0;
    std::atomic<std::size_t> next{0};
    std::atomic<std::uint64_t> copiedBlocks{0};
    std::vector<std::jthread> threads;
};

// The device-lock work before [address, address + bytes) of a writable mirror can be compared with
// guest memory: recorded GPU work that writes the range (a descriptor dispatch bound to a sub-range)
// completes first, so its results reach the guest and the shadow before the compare and no batch
// writes a block being replaced; GPU reads of the mirror by earlier recorded work (an address-based
// dispatch whose lease is released at completion) see the newer bytes, as they would on hardware.
void prepareRange(std::uint64_t address, std::uint64_t bytes) {
    // The compare reads the pages directly; a mirror is only reused while its Range exists, but a
    // lease of a build in flight can keep a replaced Range alive past a protection change.
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes)), "image mirror range became inaccessible");
    // Named for the [hooksync] attribution: the flush goes through the hook, which waits when
    // recorded work notes a write inside the range (a descriptor written inside a writable exe
    // range; the lease itself maps the range read-only). The access is the whole range.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    auto& state = Mirrors();
    if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(address, static_cast<std::size_t>(bytes))) {
        Recorder::CountSync(4);
        ++state.syncs;
        recorder->Sync();
    }
    ++state.refreshes;
}

bool prepareRefresh(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if ((!mirror.writable && !mirror.heap) || bytes == 0) return false;
    prepareRange(address, bytes);
    return true;
}

void appendBlocks(std::vector<RefreshBlock>& blocks, ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    constexpr std::uint64_t block = 65536;
    const auto end = address + bytes;
    for (auto at = address; at < end;) {
        const auto next = std::min(end, (at / block + 1) * block);
        blocks.push_back({&mirror, at, static_cast<std::size_t>(next - at)});
        at = next;
    }
}

// Compares the gathered blocks (of one or many mirrors) and counts them.
void compareBlocks(std::span<const RefreshBlock> blocks) {
    if (blocks.empty()) return;
    const auto copied = RefreshPool::Get().Run(blocks);
    auto& state = Mirrors();
    state.blocksCompared += blocks.size();
    state.blocksCopied += copied;
}

// Brings [address, address + bytes) of a writable mirror up to date with guest memory.
void refreshMirror(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if (!prepareRefresh(mirror, address, bytes)) return;
    std::vector<RefreshBlock> blocks;
    appendBlocks(blocks, mirror, address, bytes);
    compareBlocks(blocks);
}

void appendChangedBlocks(std::span<ImageMirror* const> mirrors, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    constexpr std::uint64_t block = 65536;
    thread_local std::vector<std::uint8_t>* changedSlot = nullptr;
    auto& changed = ShaderRecompiler::ThreadOwned(changedSlot);
    Mirrors().heapChecks += mirrors.size();
    for (auto* const pointer : mirrors) {
        auto& mirror = *pointer;
        changed.assign(mirror.generations.size(), 0);
        GuestMemory::ChangedBlocks(mirror.base, static_cast<std::size_t>(mirror.bytes), mirror.generations, changed);
        const auto aligned = mirror.base / block * block;
        const auto end = mirror.base + mirror.bytes;
        bool refilled = false;
        for (std::size_t at = 0; at < changed.size(); ++at) {
            if (changed[at] == 0) continue;
            const auto from = std::max(mirror.base, aligned + at * block);
            blocks.push_back({&mirror, from, static_cast<std::size_t>(std::min(end, aligned + (at + 1) * block) - from), generation});
            refilled = true;
        }
        if (refilled) ++Mirrors().heapRefills;
    }
}

void refreshHeapMirrors(std::vector<ImageMirror*>& mirrors, std::vector<RefreshBlock>& blocks) {
    std::sort(mirrors.begin(), mirrors.end(), [](const ImageMirror* left, const ImageMirror* right) { return left->base < right->base; });
    for (std::size_t first = 0; first < mirrors.size();) {
        auto last = first + 1;
        while (last < mirrors.size() && mirrors[last]->base == mirrors[last - 1]->base + mirrors[last - 1]->bytes) ++last;
        const auto begin = mirrors[first]->base;
        const auto bytes = mirrors[last - 1]->base + mirrors[last - 1]->bytes - begin;
        prepareRange(begin, bytes);
        const auto generation = GuestMemory::CollectWrites(begin, static_cast<std::size_t>(bytes));
        Require(generation != 0, "a heap mirror's range is no longer write-watched");
        appendChangedBlocks(std::span<ImageMirror* const>(mirrors).subspan(first, last - first), generation, blocks);
        first = last;
    }
}

void planSpaceMirrors(GuestBufferMemory::AddressSpace& space) {
    for (std::size_t index = 0; index < space.base.size(); ++index) {
        const auto& region = space.base[index];
        if (region.mirror == nullptr) continue;
        space.mirrored = true;
        if (!region.mirror->heap) {
            if (region.mirror->writable) space.writableMirrors.push_back(index);
            continue;
        }
        auto* mirror = region.mirror.get();
        if (space.heapRuns.empty() || mirror->base != space.heapMirrors.back()->base + space.heapMirrors.back()->bytes) space.heapRuns.push_back({mirror->base, 0, space.heapMirrors.size(), 0});
        space.heapMirrors.push_back(mirror);
        auto& run = space.heapRuns.back();
        run.bytes = mirror->base + mirror->bytes - run.begin;
        run.last = space.heapMirrors.size();
    }
    space.verified = std::make_unique<std::atomic<std::uint64_t>[]>(space.heapRuns.size());
}

std::vector<std::pair<std::size_t, std::uint64_t>> refreshHeapRuns(const GuestBufferMemory::AddressSpace& space, std::vector<RefreshBlock>& blocks) {
    std::vector<std::pair<std::size_t, std::uint64_t>> checked;
    checked.reserve(space.heapRuns.size());
    for (std::size_t index = 0; index < space.heapRuns.size(); ++index) {
        const auto& run = space.heapRuns[index];
        prepareRange(run.begin, run.bytes);
        const auto generation = GuestMemory::CollectWrites(run.begin, static_cast<std::size_t>(run.bytes));
        Require(generation != 0, "a heap mirror's range is no longer write-watched");
        const auto since = space.verified[index].load(std::memory_order_relaxed);
        if (since == 0 || !GuestMemory::UnchangedSince(run.begin, static_cast<std::size_t>(run.bytes), since)) appendChangedBlocks(std::span<ImageMirror* const>(space.heapMirrors).subspan(run.first, run.last - run.first), generation, blocks);
        checked.emplace_back(index, generation);
    }
    return checked;
}

// The mirror for a leased image range: the existing one, or a new one when none exists or the
// registered Range object changed (a protection change). Null when the range cannot be mirrored. An
// existing writable mirror's blocks are appended to `blocks` for the caller's one compare of every
std::shared_ptr<ImageMirror> acquireMirror(const Context& context, const std::shared_ptr<const GuestAllocations::Range>& range, std::vector<RefreshBlock>& blocks, bool heap) {
    auto& state = Mirrors();
    std::shared_ptr<ImageMirror> mirror;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != context.device) {
            state.entries.clear();
            state.failed.clear();
            state.heapBytes = 0;
            state.device = context.device;
            ++state.version;
        }
        if (state.failed.contains(range->address)) return nullptr;
        const auto found = state.entries.find(range->address);
        if (found != state.entries.end() && sameRange(found->second->range, range) && found->second->bytes == range->bytes && found->second->writable == range->writable && found->second->heap == heap) mirror = found->second;
    }
    if (mirror != nullptr) {
        if (!heap && prepareRefresh(*mirror, range->address, range->bytes)) appendBlocks(blocks, *mirror, range->address, range->bytes);
        return mirror;
    }
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range->address), range->bytes, range->writable && !heap)) return nullptr;
    mirror = std::make_shared<ImageMirror>();
    mirror->base = range->address;
    mirror->bytes = range->bytes;
    mirror->writable = range->writable;
    mirror->heap = heap;
    mirror->range = range;
    if (heap) {
        prepareRefresh(*mirror, range->address, range->bytes);
        const auto generation = GuestMemory::CollectWrites(range->address, range->bytes);
        constexpr std::uint64_t block = 65536;
        mirror->generations.assign(static_cast<std::size_t>(((range->address + range->bytes + block - 1) / block) - range->address / block), generation);
        if (generation == 0) {
            std::lock_guard lock(state.mutex);
            ++state.heapUnwatched;
            return nullptr;
        }
        if (heapMirrorsBeside(*range) + range->bytes > heapMirrorBudget()) {
            sweepMirrors();
            if (const auto held = heapMirrorsBeside(*range); held + range->bytes > heapMirrorBudget()) throw std::runtime_error(heapMirrorError(*range, held, "past the budget"));
        }
    }
    try {
        mirror->buffer = std::make_shared<Buffer>(context, range->bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    } catch (const std::runtime_error& error) {
        if (heap) throw std::runtime_error(heapMirrorError(*range, heapMirrorsBeside(*range), error.what()));
        std::fprintf(stderr, "[gpu] image mirror of 0x%llx+0x%llx failed: %s; falling back to copies\n", static_cast<unsigned long long>(range->address), static_cast<unsigned long long>(range->bytes), error.what());
        std::lock_guard lock(state.mutex);
        state.failed.insert(range->address);
        return nullptr;
    }
    // Read stores GPU results pending in the range first (write-backs of earlier copies of it);
    // named for the [hooksync] attribution like the refresh's flush.
    {
        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
        GuestMemory::Read(range->address, mirror->buffer->Bytes());
    }
    if (range->writable && !heap) mirror->shadow.assign(mirror->buffer->Bytes().begin(), mirror->buffer->Bytes().end());
    std::lock_guard lock(state.mutex);
    ++state.rebuilds;
    mirror->serial = (1ull << 63u) | ++state.serials;
    // A replaced mirror lives on in the regions of recorded work that bound it.
    if (const auto found = state.entries.find(range->address); found != state.entries.end() && found->second->heap) state.heapBytes -= found->second->bytes;
    if (heap) state.heapBytes += mirror->bytes;
    state.entries[range->address] = mirror;
    ++state.version;
    return mirror;
}

std::shared_ptr<ImageMirror> findMirror(const Context& context, std::uint64_t begin, std::uint64_t end) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return nullptr;
    auto found = state.entries.upper_bound(begin);
    if (found == state.entries.begin()) return nullptr;
    --found;
    const auto& mirror = found->second;
    if (mirror->heap || begin < mirror->base || end > mirror->base + mirror->bytes || mirror->range.expired()) return nullptr;
    return mirror;
}

// Drops mirrors whose registered range is gone (a later build made a new one for the replacement).
void sweepMirrors() {
    const auto space = Spaces().current.load();
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (space != nullptr && state.pinnedSpace == space->serial && state.pinnedVersion == state.version) return;
    ++state.sweeps;
    bool pinned = space != nullptr;
    std::size_t leased = 0;
    for (auto it = state.entries.begin(); it != state.entries.end();) {
        if (!it->second->range.expired()) {
            if (pinned) {
                while (leased < space->lease.size() && space->lease[leased]->address < it->first) ++leased;
                pinned = leased < space->lease.size() && sameRange(it->second->range, space->lease[leased]);
            }
            ++it;
            continue;
        }
        if (it->second->heap) state.heapBytes -= it->second->bytes;
        it = state.entries.erase(it);
        ++state.version;
    }
    state.pinnedSpace = pinned ? space->serial : 0;
    state.pinnedVersion = state.version;
}

void releaseImportedMirrors(const Context& context, const std::vector<std::uint64_t>& imported) {
    if (imported.empty()) return;
    std::vector<std::shared_ptr<ImageMirror>> released;
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return;
    for (const auto base : imported) {
        const auto found = state.entries.find(base);
        if (found == state.entries.end() || !found->second->heap) continue;
        state.heapBytes -= found->second->bytes;
        ++state.heapImported;
        state.heapImportedBytes += found->second->bytes;
        released.push_back(std::move(found->second));
        state.entries.erase(found);
    }
}

void reportMirrors() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& state = Mirrors();
    const auto now = std::chrono::steady_clock::now();
    if (now - state.lastReport < std::chrono::seconds(10)) return;
    state.lastReport = now;
    std::size_t count = 0;
    std::size_t writable = 0;
    std::size_t heaps = 0;
    std::uint64_t bytes = 0;
    std::uint64_t heapBytes = 0;
    {
        std::lock_guard lock(state.mutex);
        for (const auto& [base, mirror] : state.entries) {
            ++count;
            bytes += mirror->bytes;
            if (mirror->writable) ++writable;
            if (mirror->heap) ++heaps;
        }
        heapBytes = state.heapBytes;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] image mirrors: %zu ranges (%.1f MiB, %zu writable, %zu heap %.1f MiB), %llu address-based builds served, %llu descriptor sub-ranges bound, %llu rebuilds, %llu refreshes: %llu blocks compared, %llu copied, %llu refresh syncs; heap refills %llu, unwatched heaps %llu, %llu heap mirrors released for imports (%.1f MiB)\n", count, bytes / 1048576.0, writable, heaps, heapBytes / 1048576.0, static_cast<unsigned long long>(state.builds), static_cast<unsigned long long>(state.subranges), static_cast<unsigned long long>(state.rebuilds), static_cast<unsigned long long>(state.refreshes), static_cast<unsigned long long>(state.blocksCompared), static_cast<unsigned long long>(state.blocksCopied), static_cast<unsigned long long>(state.syncs), static_cast<unsigned long long>(state.heapRefills), static_cast<unsigned long long>(state.heapUnwatched), static_cast<unsigned long long>(state.heapImported), state.heapImportedBytes / 1048576.0);
}

// Deferred lease release. The lease an address-based build takes (AcquireRegistered) is dropped by its
// write-back, which runs as a completion action when the batch that recorded the work finished; the
// work is no longer synced as soon as it is recorded (that sync was ~30000 waits per 200 s). A guest
// thread that unmaps, mprotects or frees a leased allocation finds it pinned in the registry and calls
// this waiter (with the registry lock released, see GuestAllocationsRequireUnpinned): under the device
// lock it submits the recorder and finishes its batches up to the newest one that was recorded with a
// lease (CountLeaseOutcome notes that serial); the completions release the leases, later batches stay
// in flight, and the registry rescans. A lease held by a build in progress belongs to a worker holding
// the device lock, so the waiter waits for that worker first; once it holds the lock every lease of
// recorded work is in a batch. A lease whose batch was not noted (the record threw between the keep
// and the note) is covered by draining the recorder when the noted serial is already finished. The
// waiter yields instead when its own thread holds the device lock (a driver thread mutating the
// registry mid-packet, as the plain spin did) and reports whether it finished any work.
struct LeaseState {
    std::mutex mutex;
    LeaseStats stats;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    // Under GuestMemory::GpuMutex: the recorder whose batches hold leases (serials restart with a
    // replaced device's recorder), the serial of its newest batch recorded with a lease, and the
    // newest one a waiter finished.
    const Recorder* leaseRecorder = nullptr;
    std::uint64_t newestLeaseSerial = 0;
    std::uint64_t finishedLeaseSerial = 0;
};

LeaseState& Leases() {
    static LeaseState state;
    return state;
}

// APS5_PROFILE_DRAW: AddSnapshot's consistency compares of a captured region against the region
// serving it, made and skipped (see AddSnapshot); printed in the [address-sync] line. Atomic: the
// unlocked stage A of descriptor builds adds snapshots too.
struct SnapshotStats {
    std::atomic<std::uint64_t> checked{0};
    std::atomic<std::uint64_t> skipped{0};
};

SnapshotStats& Snapshots() {
    static SnapshotStats stats;
    return stats;
}

bool WaitForLeasesAndImports(std::uintptr_t address, std::size_t bytes) noexcept {
    const auto start = std::chrono::steady_clock::now();
    bool synced = false;
    bool drained = false;
    bool retiredImport = false;
    // A range pinned only by the cached address space is released by dropping the cache's
    // reference: no GPU wait, no device lock (the mutating thread may be a driver thread holding
    // it). The registry rescans; a range still pinned by a build or a batch holding the space comes
    // back here and finishes lease batches below.
    if (auto dropped = Spaces().current.exchange(nullptr); dropped != nullptr) {
        Spaces().droppedByWaiter.store(true, std::memory_order_relaxed);
        Spaces().waiterDrops.fetch_add(1, std::memory_order_relaxed);
        dropped.reset();
        auto& state = Leases();
        std::lock_guard lock(state.mutex);
        ++state.stats.contentionWaits;
        ++state.stats.cacheDrops;
        state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return true;
    }
    if (GuestMemory::GpuMutex().HeldByThisThread()) {
        try {
            auto* recorder = Recorder::Active();
            if (recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            }
            const auto end = static_cast<std::uint64_t>(address) + bytes;
            auto& imports = Imports();
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            {
                std::lock_guard importsLock(imports.mutex);
                for (auto it = imports.imports.begin(); it != imports.imports.end();) {
                    const auto base = it->first;
                    if (base >= end || address >= base + it->second.bytes) {
                        ++it;
                        continue;
                    }
                    const auto next = std::next(it);
                    retireImport(imports.device, imports, it, lease);
                    it = next;
                    retiredImport = true;
                }
            }
            if (retiredImport && recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] lease wait failed: %s\n", error.what());
        }
    } else {
        try {
            std::lock_guard lock(GuestMemory::GpuMutex());
            auto& state = Leases();
            auto* recorder = Recorder::Active();
            const auto target = state.newestLeaseSerial;
            if (recorder != nullptr && recorder == state.leaseRecorder && target > state.finishedLeaseSerial) {
                // The open batch may be the target; FinishUpTo waits front to back up to it only.
                Recorder::CountSync(4);
                recorder->Submit();
                recorder->FinishUpTo(target);
                state.finishedLeaseSerial = target;
                synced = true;
            } else if (recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            } else {
                // Nothing recorded holds the lease: the holder is a lease-holding object that has
                // yet to record (a test's, or a build whose worker gave up the lock); give it time.
                std::this_thread::yield();
            }

            const auto end = static_cast<std::uint64_t>(address) + bytes;
            auto& imports = Imports();
            bool overlapsImport = false;
            {
                std::lock_guard importsLock(imports.mutex);
                for (const auto& [base, entry] : imports.imports) {
                    if (base < end && address < base + entry.bytes) {
                        overlapsImport = true;
                        break;
                    }
                }
            }
            if (overlapsImport) {
                if (recorder != nullptr && !recorder->Idle()) {
                    Recorder::CountSync(4);
                    recorder->Sync();
                    synced = true;
                    drained = true;
                }
                const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
                {
                    std::lock_guard importsLock(imports.mutex);
                    for (auto it = imports.imports.begin(); it != imports.imports.end();) {
                        const auto base = it->first;
                        if (base >= end || address >= base + it->second.bytes) {
                            ++it;
                            continue;
                        }
                        const auto next = std::next(it);
                        retireImport(imports.device, imports, it, lease);
                        it = next;
                        retiredImport = true;
                    }
                }
                if (retiredImport && recorder != nullptr && !recorder->Idle()) {
                    Recorder::CountSync(4);
                    recorder->Sync();
                    synced = true;
                    drained = true;
                }
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] lease wait failed: %s\n", error.what());
        }
    }
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    ++state.stats.contentionWaits;
    if (synced) ++state.stats.contentionSyncs;
    if (drained) ++state.stats.contentionDrains;
    state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return synced || retiredImport;
}

void ensurePinWaiter() {
    static const bool registered = [] {
        GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(&WaitForLeasesAndImports);
        return true;
    }();
    static_cast<void>(registered);
}

}

bool SyncLeaseWork() {
    static const bool sync = std::getenv("APS5_SYNC_LEASE_DISPATCH") != nullptr;
    return sync;
}

void CountLeaseOutcome(bool synced, std::uint64_t batchSerial) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& state = Leases();
    if (!synced) {
        // Serials grow with the submissions, so the newest noted one covers every earlier lease batch
        // of the same recorder.
        GuestMemory::AssertGpuLockHeld("CountLeaseOutcome");
        if (const auto* recorder = Recorder::Active(); recorder != state.leaseRecorder) {
            state.leaseRecorder = recorder;
            state.newestLeaseSerial = 0;
            state.finishedLeaseSerial = 0;
        }
        state.newestLeaseSerial = std::max(state.newestLeaseSerial, batchSerial);
    }
    std::lock_guard lock(state.mutex);
    ++(synced ? state.stats.synced : state.stats.deferred);
    if (!profile) return;
    // APS5_PROFILE_DRAW: the leases (dispatches and draws) deferred and synced, the guest's
    // pin-contention waits and the BDA table cache, every 10 s (cumulative counts, like the
    // [recorder] line's), printed by whichever path counts a lease.
    const auto now = std::chrono::steady_clock::now();
    if (now - state.lastReport < std::chrono::seconds(10)) return;
    state.lastReport = now;
    const auto& stats = state.stats;
    const auto table = BdaResources::TableCacheCounters();
    const auto& snapshots = Snapshots();
    AgcDriver::ProfilePrint_nid_no_patch("[address-sync] leases: %llu released at completion, %llu synced at once; %llu pin-contention waits by guest threads (%llu finished the lease batch, %llu drained the recorder, %llu cache-only drops) %.1f s; BDA table cache %llu hits / %llu misses (%zu tables held); snapshot compares: %llu skipped (live-backed region), %llu made\n", static_cast<unsigned long long>(stats.deferred), static_cast<unsigned long long>(stats.synced), static_cast<unsigned long long>(stats.contentionWaits), static_cast<unsigned long long>(stats.contentionSyncs), static_cast<unsigned long long>(stats.contentionDrains), static_cast<unsigned long long>(stats.cacheDrops), stats.contentionMs / 1000, static_cast<unsigned long long>(table.hits), static_cast<unsigned long long>(table.misses), table.held, static_cast<unsigned long long>(snapshots.skipped.load(std::memory_order_relaxed)), static_cast<unsigned long long>(snapshots.checked.load(std::memory_order_relaxed)));
}

LeaseStats LeaseCounters() {
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    return state.stats;
}

AddressSpaceStats AddressSpaceCounters() {
    const auto& cache = Spaces();
    const auto load = [](const std::atomic<std::uint64_t>& counter) { return counter.load(std::memory_order_relaxed); };
    return {addressSpaceCacheEnabled(), load(cache.hits), load(cache.rebuiltFirst), load(cache.rebuiltGeneration), load(cache.rebuiltEpoch), load(cache.rebuiltDevice), load(cache.rebuiltWaiterDrop), load(cache.unpublished), load(cache.dissolvedOverlap), load(cache.dissolvedImports), load(cache.waiterDrops)};
}

std::string AddressCopyOverflow(std::vector<AddressCopy> copies, std::uint64_t limit) {
    std::uint64_t total = 0;
    for (const auto& copy : copies) total += copy.committed;
    if (total <= limit) return {};
    std::sort(copies.begin(), copies.end(), [](const AddressCopy& left, const AddressCopy& right) { return left.committed > right.committed; });
    char text[256];
    std::snprintf(text, sizeof(text), "an address-based build copies %llu MiB of %zu registered ranges, past %llu MiB (APS5_ADDRESS_COPY_MAX_MIB); the largest:", static_cast<unsigned long long>(total >> 20u), copies.size(), static_cast<unsigned long long>(limit >> 20u));
    std::string message = text;
    for (std::size_t index = 0; index < copies.size() && index < 8; ++index) {
        const auto& copy = copies[index];
        std::snprintf(text, sizeof(text), " 0x%llx+0x%llx (%.1f MiB committed, %s)", static_cast<unsigned long long>(copy.begin), static_cast<unsigned long long>(copy.end - copy.begin), copy.committed / 1048576.0, copy.reason);
        message += text;
    }
    return message;
}

MirrorStats MirrorCounters() {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    MirrorStats stats{0, state.heapBytes, state.rebuilds, state.blocksCopied, state.heapRefills, state.sweeps, state.heapChecks};
    for (const auto& [base, mirror] : state.entries) stats.heapMirrors += mirror->heap ? 1 : 0;
    return stats;
}

void ClearImageMirrors(VkDevice device) {
    auto& state = Mirrors();
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != device) return;
        entries.swap(state.entries);
        state.failed.clear();
        state.heapBytes = 0;
        state.device = VK_NULL_HANDLE;
    }
    Spaces().current.store(nullptr);
}

void ClearHostImports(VkDevice device) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (state.device != device) return;
    for (const auto& [address, entry] : state.imports) {
        state.destroyBuffer(state.device, entry.buffer, nullptr);
        state.freeMemory(state.device, entry.memory, nullptr);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    state.imports.clear();
    state.failed.clear();
    state.device = VK_NULL_HANDLE;
    state.refreshedGeneration = 0;
    ++state.epoch;
}

#ifndef _WIN32
namespace {

void runImportProbe(const Context& context, std::uint64_t base, std::uint64_t bytes, const std::function<const char*(HostImport&, VkResult&)>& importStep, ImportProbe& probe) {
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    HostImport import{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
    VkBuffer destination = VK_NULL_HANDLE;
    VkDeviceMemory destinationMemory = VK_NULL_HANDLE;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    const auto collect = [&](std::uint32_t& pages) {
        pages = 0;
        return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes), [](void* count, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::uint32_t*>(count) += static_cast<std::uint32_t>((end - begin) / 4096); }, &pages);
    };
    const auto run = [&]() -> const char* {
        std::uint32_t quiet = 0;
        if (!collect(quiet)) return "the first collect";
        if (!collect(quiet)) return "the second collect";
        if (quiet != 0) return "an unwritten scratch range";
        if (const char* step = importStep(import, probe.result)) return step;
        if (!collect(probe.writtenAtImport)) return "the collect after the import";
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if ((probe.result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &destination)) != VK_SUCCESS) return "vkCreateBuffer";
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, destination, &requirements);
        if (requirements.memoryTypeBits == 0) return "the destination memory type";
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((probe.result = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &destinationMemory)) != VK_SUCCESS) return "vkAllocateMemory";
        if ((probe.result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, destination, destinationMemory, 0)) != VK_SUCCESS) return "vkBindBufferMemory";
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if ((probe.result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, &commands)) != VK_SUCCESS) return "vkAllocateCommandBuffers";
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if ((probe.result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer";
        const VkBufferCopy region{0, 0, bytes};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, import.buffer, destination, 1, &region);
        if ((probe.result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands)) != VK_SUCCESS) return "vkEndCommandBuffer";
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((probe.result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence)) != VK_SUCCESS) return "vkCreateFence";
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commands;
        if ((probe.result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, fence)) != VK_SUCCESS) return "vkQueueSubmit";
        submitted = true;
        if ((probe.result = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 10'000'000'000ull)) != VK_SUCCESS) return "vkWaitForFences";
        submitted = false;
        if (!collect(probe.writtenAfterSubmit)) return "the collect after the submission";
        *reinterpret_cast<volatile std::uint8_t*>(base) = 2;
        if (!collect(probe.writtenByCpu)) return "the collect after a CPU store";
        return nullptr;
    };
    probe.failure = run();
    if (submitted) context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue);
    if (fence != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (commands != VK_NULL_HANDLE) context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &commands);
    if (destination != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, destination, nullptr);
    if (destinationMemory != VK_NULL_HANDLE) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, destinationMemory, nullptr);
    if (import.buffer != VK_NULL_HANDLE) destroyImport(context, import);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
}

}
#endif

ImportProbe ProbeImportWriteProtection(const Context& context) {
    ImportProbe probe;
#ifdef _WIN32
    static_cast<void>(context);
    probe.failure = "the Linux write watch";
    return probe;
#else
    if (context.hostImportAlignment == 0) {
        probe.failure = "host import support";
        return probe;
    }
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        probe.failure = "the write watch";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        probe.failure = "mmap";
        return probe;
    }
    const auto base = (reinterpret_cast<std::uint64_t>(raw) + alignment - 1) / alignment * alignment;
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) { return createImport(context, import, result); }, probe);
    munmap(raw, bytes + alignment);
    return probe;
#endif
}

ImportProbe ProbeDmaBufImportWriteProtection(const Context& context) {
    ImportProbe probe;
#ifdef _WIN32
    static_cast<void>(context);
    probe.failure = "the Linux write watch";
    return probe;
#else
    if (!context.dmaBufImport) {
        probe.failure = "dma-buf import support";
        return probe;
    }
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        probe.failure = "the write watch";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    const int file = memfd_create("aps5-dma-buf-import-probe", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (file < 0) {
        probe.failure = "memfd_create";
        return probe;
    }
    void* mapped = MAP_FAILED;
    if (ftruncate(file, static_cast<off_t>(bytes)) == 0 && fcntl(file, F_ADD_SEALS, F_SEAL_SHRINK) == 0) mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (mapped == MAP_FAILED) {
        close(file);
        probe.failure = "the shared scratch mapping";
        return probe;
    }
    const auto base = reinterpret_cast<std::uint64_t>(mapped);
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) -> const char* {
        const int copy = dup(file);
        if (copy < 0) {
            result = VK_ERROR_INITIALIZATION_FAILED;
            return "dup";
        }
        return createDmaBufImport(context, import, copy, 0, result);
    }, probe);
    munmap(mapped, bytes);
    close(file);
    return probe;
#endif
}

ImportWatch PrepareImportWatch(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    decideImportWatch(context, state);
    return state.unwatchImports ? ImportWatch::Unwatch : ImportWatch::Watch;
}

void SetImportWatch(const Context& context, ImportWatch watch) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.watchDevice = context.device;
    state.unwatchImports = watch == ImportWatch::Unwatch;
    state.unwatchDmaBufImports = state.unwatchImports;
}

const HostImport* HostImportFor(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    ensurePinWaiter();
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // A hit is only valid while the registry has not changed since the imports were reconciled.
    if (!importsStale(context, state)) {
        if (const auto* entry = findImport(state, address, address + bytes)) return entry;
    }
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    refreshImports(context, state, lease);
    if (const auto* range = containingRange(lease, address, address + bytes)) return importAllocation(context, state, range->address, range->bytes, lease);
    return nullptr;
}

bool RegisteredReadableCovers(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    return containingRange(lease, address, address + bytes) != nullptr;
}

bool HostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // Imports of a replaced device are not this device's; a stale registry only means an import
    // may be dropped at the next reconcile, which the binding path handles.
    return state.device == context.device && findImport(state, address, address + bytes) != nullptr;
}

GuestBufferMemory::GuestBufferMemory(const Context& context) : context(context) {}

bool GuestBufferMemory::WritesOverlap(std::uint64_t address, std::size_t bytes) const {
    return std::any_of(writes.begin(), writes.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

void GuestBufferMemory::CountAddressBuild(double snapshotsUs) {
    auto& totals = AddressBuilds();
    auto& timing = ThreadAddressTiming();
    std::lock_guard lock(totals.mutex);
    ++totals.builds;
    totals.sums.leaseUs += timing.leaseUs;
    totals.sums.importsUs += timing.importsUs;
    totals.sums.mirrorsUs += timing.mirrorsUs;
    totals.sums.compareUs += timing.compareUs;
    totals.sums.blocksCompared += timing.blocksCompared;
    totals.sums.blocksCopied += timing.blocksCopied;
    totals.snapshotsUs += snapshotsUs;
    timing = {};
    const auto now = std::chrono::steady_clock::now();
    if (now - totals.lastReport < std::chrono::seconds(10)) return;
    totals.lastReport = now;
    const auto per = [&](double us) { return us / static_cast<double>(totals.builds); };
    const auto table = BdaResources::TableCacheCounters();
    const auto& seen = totals.tableSeen;
    const auto delta = [](std::uint64_t now, std::uint64_t before) { return static_cast<unsigned long long>(now - before); };
    const auto space = AddressSpaceCounters();
    const auto& spaceSeen = totals.spaceSeen;
    AgcDriver::ProfilePrint_nid_no_patch("[address] %llu address-based builds (10 s), us per build: lease %.0f, imports pass %.0f, mirror prepare %.0f, compare %.0f (%.0f blocks, %.1f copied), snapshots %.0f; space hits %llu / rebuilds: generation %llu, epoch %llu, device %llu, waiter drop %llu, first %llu; unpublished %llu, dissolved: overlap %llu, imports %llu%s; [bda-table] hits %llu / misses %llu (space tables %llu), first entry: expired %llu, hash differs low %llu / heap %llu, same hash %llu, none held %llu\n", static_cast<unsigned long long>(totals.builds), per(totals.sums.leaseUs), per(totals.sums.importsUs), per(totals.sums.mirrorsUs), per(totals.sums.compareUs), per(static_cast<double>(totals.sums.blocksCompared)), per(static_cast<double>(totals.sums.blocksCopied)), per(totals.snapshotsUs), delta(space.hits, spaceSeen.hits), delta(space.rebuiltGeneration, spaceSeen.rebuiltGeneration), delta(space.rebuiltEpoch, spaceSeen.rebuiltEpoch), delta(space.rebuiltDevice, spaceSeen.rebuiltDevice), delta(space.rebuiltWaiterDrop, spaceSeen.rebuiltWaiterDrop), delta(space.rebuiltFirst, spaceSeen.rebuiltFirst), delta(space.unpublished, spaceSeen.unpublished), delta(space.dissolvedOverlap, spaceSeen.dissolvedOverlap), delta(space.dissolvedImports, spaceSeen.dissolvedImports), space.enabled ? "" : " (cache off)", delta(table.hits, seen.hits), delta(table.misses, seen.misses), delta(table.spaceTables, seen.spaceTables), delta(table.firstExpired, seen.firstExpired), delta(table.firstDiffersLow, seen.firstDiffersLow), delta(table.firstDiffersHeap, seen.firstDiffersHeap), delta(table.firstSameHash, seen.firstSameHash), delta(table.firstEmpty, seen.firstEmpty));
    totals.tableSeen = table;
    totals.spaceSeen = space;
    totals.builds = 0;
    totals.sums = {};
    totals.snapshotsUs = 0;
}

void GuestBufferMemory::AcquireRegistered() {
    Require(!uploaded && regions.empty() && lease.empty() && space == nullptr, "guest allocation lease must precede resource registration");
    ensurePinWaiter();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& timing = ThreadAddressTiming();
    const auto lap = [&](double& into, std::chrono::steady_clock::time_point& from) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        into += std::chrono::duration<double, std::micro>(now - from).count();
        from = now;
    };
    auto at = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool importable = context.hostImportAlignment != 0;
    // APS5_NO_SORTED_SNAPSHOT_LOOKUP=1: AddSnapshot scans the regions as before instead of searching.
    static const bool sortedLookup = std::getenv("APS5_NO_SORTED_SNAPSHOT_LOOKUP") == nullptr;
    std::vector<RefreshBlock> blocks;
    std::vector<ImageMirror*> heaps;
    bool mirrored = false;
    bool served = false;
    auto& spaces = Spaces();
    // Read before the lease below is acquired: a mutation ending in between leaves the space
    // stale by its generation, never newer than it (see AddressSpace).
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (addressSpaceCacheEnabled()) {
        auto current = spaces.current.load();
        bool hit = false;
        {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            if (current == nullptr) (spaces.droppedByWaiter.exchange(false, std::memory_order_relaxed) ? spaces.rebuiltWaiterDrop : spaces.rebuiltFirst).fetch_add(1, std::memory_order_relaxed);
            else if (current->device != context.device) spaces.rebuiltDevice.fetch_add(1, std::memory_order_relaxed);
            else if (current->generation != generation) spaces.rebuiltGeneration.fetch_add(1, std::memory_order_relaxed);
            else if (current->importsEpoch != state.epoch) spaces.rebuiltEpoch.fetch_add(1, std::memory_order_relaxed);
            else hit = true;
        }
        if (hit) {
            spaces.hits.fetch_add(1, std::memory_order_relaxed);
            space = std::move(current);
            lap(timing.leaseUs, at);
            for (const auto& range : space->copied) addCopiedRange(range);
            regionsSorted = sortedLookup;
            lap(timing.importsUs, at);
            // Writable mirrors are compared with guest memory per build, as acquireMirror does for
            // an existing mirror. The preparation may wait for recorded work, whose completions can
            // retire an import the space's regions point at: UploadFinish re-checks the epoch
            // before any of them is dereferenced.
            served = true;
            mirrored = space->mirrored;
            for (const auto index : space->writableMirrors) {
                const auto& region = space->base[index];
                if (prepareRefresh(*region.mirror, region.begin, region.end - region.begin)) appendBlocks(blocks, *region.mirror, region.begin, region.end - region.begin);
            }
            lap(timing.mirrorsUs, at);
        }
    }
    if (space == nullptr) {
        // Pending GPU results in these ranges are stored when Upload prepares each region.
        lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        lap(timing.leaseUs, at);
        {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            if (importable) refreshImports(context, state, lease);
            // Taken before the pass: the imports it takes are the registry's ones (just reconciled, so
            // none is retired by a make below), and a mirror's preparation may sync the recorder, whose
            // completions can retire an import already taken; that moves the epoch, and Upload then
            // re-checks every pointer instead of trusting it.
            importsEpoch = state.epoch;
        }
        lap(timing.importsUs, at);
        // One pass in the registry's (ascending) order, so the regions come out sorted for AddSnapshot.
        // The import lock is taken per range: a mirror's preparation may wait for recorded work, which
        // must not happen under it.
        regions.reserve(lease.size());
        std::vector<AddressCopy> copies;
        std::vector<std::uint64_t> imported;
        for (const auto& range : lease) {
            if (!range->readable) continue;
            validate(range->address, range->bytes);
            Region region{range->address, range->address + range->bytes, range->writable, {}, nullptr};
            const HostImport* entry = nullptr;
            if (importable) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                entry = importAllocation(context, state, range->address, range->bytes, lease);
            }
            lap(timing.importsUs, at);
            if (entry != nullptr) {
                region.hostBacked = true;
                // Reused by Upload while no import was dropped since (see importsEpoch).
                region.direct = entry;
                regions.push_back(std::move(region));
                imported.push_back(range->address);
                continue;
            }
            if (mirrorsEnabled()) {
                region.mirror = acquireMirror(context, range, blocks, range->releasable);
                lap(timing.mirrorsUs, at);
                if (region.mirror != nullptr) {
                    mirrored = true;
                    if (region.mirror->heap) heaps.push_back(region.mirror.get());
                    regions.push_back(std::move(region));
                    continue;
                }
            }
            addCopiedRange({range->address, range->address + range->bytes, range->writable});
            const auto& copied = regions.back();
            std::uint64_t committed = copied.end - copied.begin;
            if (copied.sparse) {
                committed = 0;
                for (const auto& [first, last] : copied.backed) committed += last - first;
            }
            const char* reason = !mirrorsEnabled() ? "mirrors disabled by APS5_NO_LEASE_MIRROR" : copied.sparse ? "unreadable pages" : !range->releasable ? "image mirror refused" : "outside the write-watched arena";
            copies.push_back({copied.begin, copied.end, committed, reason});
        }
        releaseImportedMirrors(context, imported);
        regionsSorted = sortedLookup;
        static const std::uint64_t copyLimit = [] {
            const char* value = std::getenv("APS5_ADDRESS_COPY_MAX_MIB");
            return (value ? std::strtoull(value, nullptr, 10) : 64ull) << 20u;
        }();
        if (const auto overflow = AddressCopyOverflow(std::move(copies), copyLimit); !overflow.empty()) {
            std::fprintf(stderr, "FATAL: %s\n", overflow.c_str());
            std::fflush(stderr);
            std::abort();
        }
        lap(timing.importsUs, at);
        if (addressSpaceCacheEnabled()) {
            // Published only while the imports the regions point at stand: the table entries below
            // read them, and a retire during the pass (a mirror preparation's sync) has already
            // moved the epoch, in which case this build keeps the per-build form and Upload
            // re-resolves every pointer as before.
            std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
            bool publish = true;
            {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                if (state.epoch != importsEpoch) {
                    publish = false;
                    spaces.unpublished.fetch_add(1, std::memory_order_relaxed);
                } else {
                    for (const auto& region : regions) {
                        if (region.direct != nullptr || region.mirror != nullptr) ranges.push_back(addressRange(region));
                    }
                }
            }
            if (publish) {
                auto built = std::make_shared<AddressSpace>();
                built->generation = generation;
                built->importsEpoch = importsEpoch;
                built->device = context.device;
                built->serial = spaces.serials.fetch_add(1, std::memory_order_relaxed) + 1;
                built->lease = std::move(lease);
                built->ranges = std::move(ranges);
                std::vector<Region> extras;
                for (auto& region : regions) {
                    if (region.direct != nullptr || region.mirror != nullptr) {
                        built->base.push_back(std::move(region));
                    } else {
                        built->copied.push_back({region.begin, region.end, region.writable});
                        extras.push_back(std::move(region));
                    }
                }
                regions = std::move(extras);
                lease.clear();
                planSpaceMirrors(*built);
                space = std::move(built);
                spaces.droppedByWaiter.store(false, std::memory_order_relaxed);
                spaces.current.store(space);
            }
        }
    }
    std::vector<std::pair<std::size_t, std::uint64_t>> checked;
    if (served) checked = refreshHeapRuns(*space, blocks);
    else refreshHeapMirrors(heaps, blocks);
    lap(timing.mirrorsUs, at);
    const auto copiedBefore = Mirrors().blocksCopied;
    compareBlocks(blocks);
    for (const auto& [run, collected] : checked) space->verified[run].store(collected, std::memory_order_relaxed);
    lap(timing.compareUs, at);
    if (profile) {
        timing.blocksCompared += blocks.size();
        timing.blocksCopied += Mirrors().blocksCopied - copiedBefore;
    }
    if (mirrorsEnabled()) {
        if (mirrored) ++Mirrors().builds;
        sweepMirrors();
        reportMirrors();
    }
}

void GuestBufferMemory::validate(std::uint64_t address, std::size_t bytes) const {
    Require(!uploaded, "guest memory ownership is frozen for GPU execution");
    Require(address != 0 && bytes != 0, "empty guest memory range");
    Require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest memory range overflow");
}

void GuestBufferMemory::addCopiedRange(const CopiedRange& range) {
    validate(range.begin, range.end - range.begin);
    Region region{range.begin, range.end, range.writable, {}, nullptr};
    // Read-only ranges are read from live guest memory at Upload: the lease pins them and the guest
    // cannot write them, so that equals a snapshot taken now without the extra copy. Reserved heaps
    // are registered whole while the guest commits pages on demand: committed pages only.
    region.hostBacked = !range.writable;
    auto committed = GuestMemory::DescribeCommitted(range.begin, range.end - range.begin);
    if (!committed.whole) {
        region.sparse = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
}

GuestBufferMemory::BaseOverlap GuestBufferMemory::baseOverlap(std::uint64_t begin, std::uint64_t end, const Region** owner) const {
    if (space == nullptr || space->base.empty()) return BaseOverlap::None;
    const auto& base = space->base;
    const auto found = std::upper_bound(base.begin(), base.end(), begin, [](std::uint64_t value, const Region& region) { return value < region.begin; });
    if (found != base.begin()) {
        const auto& previous = *std::prev(found);
        if (begin < previous.end) {
            if (end > previous.end) return BaseOverlap::Partial;
            if (owner != nullptr) *owner = &previous;
            return BaseOverlap::Inside;
        }
    }
    return found != base.end() && found->begin < end ? BaseOverlap::Partial : BaseOverlap::None;
}

void GuestBufferMemory::dissolveSpace(bool resolve) {
    Require(space != nullptr, "no address space to dissolve");
    std::vector<Region> merged;
    merged.reserve(space->base.size() + regions.size());
    if (prepared) {
        // Both sorted and disjoint (UploadPrepare merged this build's regions; the space's are
        // registered ranges), so the merge keeps the order Descriptor searches.
        std::merge(space->base.begin(), space->base.end(), std::make_move_iterator(regions.begin()), std::make_move_iterator(regions.end()), std::back_inserter(merged), [](const Region& left, const Region& right) { return left.begin < right.begin; });
    } else {
        merged.assign(space->base.begin(), space->base.end());
        merged.insert(merged.end(), std::make_move_iterator(regions.begin()), std::make_move_iterator(regions.end()));
        regionsSorted = false;
    }
    if (resolve) {
        for (auto& region : merged) {
            if (region.direct != nullptr) region.pending = true;
        }
    }
    lease = space->lease;
    importsEpoch = space->importsEpoch;
    regions = std::move(merged);
    space.reset();
}

const GuestBufferMemory::Region* GuestBufferMemory::owner(std::uint64_t address) const {
    // The region with the greatest begin at or below the address, as the one merged list gave;
    // the caller checks the end, as before.
    const auto candidate = [address](const std::vector<Region>& list) -> const Region* {
        const auto found = std::upper_bound(list.begin(), list.end(), address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
        return found == list.begin() ? nullptr : &*std::prev(found);
    };
    const auto* own = candidate(regions);
    const auto* shared = space != nullptr ? candidate(space->base) : nullptr;
    if (own == nullptr || (shared != nullptr && shared->begin > own->begin)) return shared;
    return own;
}

void GuestBufferMemory::addDescriptorRegion(std::uint64_t address, std::size_t bytes, bool atomic) {
    validate(address, bytes);
    const auto begin = address & ~std::uint64_t{3};
    const auto end = begin == address ? address + bytes : (address + bytes + 3) & ~std::uint64_t{3};
    switch (baseOverlap(begin, end, nullptr)) {
        case BaseOverlap::Inside:
            return;
        case BaseOverlap::Partial:
            Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
            dissolveSpace(false);
            break;
        case BaseOverlap::None:
            break;
    }
    // `writable` here means the bytes come from live guest memory (not a snapshot), whether or not
    // the shader stores to them; what is written back is decided by Writes() alone.
    Region region{begin, end, true, {}, nullptr};
    region.atomic = atomic;
    auto committed = GuestMemory::DescribeCommitted(begin, static_cast<std::size_t>(end - begin));
    if (!committed.whole) {
        // A GPU heap bound whole while the guest commits its pages on demand, or a descriptor left
        region.sparse = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
    regionsSorted = false;
}

void GuestBufferMemory::AddWritable(std::uint64_t address, std::size_t bytes, bool atomic) {
    addDescriptorRegion(address, bytes, atomic);
    writes.emplace_back(address, address + bytes);
}

void GuestBufferMemory::AddReadable(std::uint64_t address, std::size_t bytes) {
    // Not in `writes`: no reference copy (copyRegion), no write-back, no pending-write note, no
    // direct-write mark, and UploadPrepare copies it without the device lock. A written descriptor
    // overlapping the range still covers it through its own Writes() entry.
    addDescriptorRegion(address, bytes, false);
}

void GuestBufferMemory::AddSnapshot(const GuestMemorySnapshot& snapshot) {
    validate(snapshot.address, snapshot.bytes.size());
    const auto end = snapshot.address + snapshot.bytes.size();
    // A snapshot inside a region needs no copy: the region's bytes serve it (they are checked to
    // agree). Right after AcquireRegistered the regions are sorted and non-overlapping (registered
    // ranges), so the one that can contain the snapshot is found by search rather than by scanning
    // the ~1200 of an address-based build for each captured region.
    const Region* owner = nullptr;
    if (baseOverlap(snapshot.address, end, &owner) == BaseOverlap::Partial) {
        Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
        dissolveSpace(false);
    }
    if (owner == nullptr && regionsSorted) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), snapshot.address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
        if (found != regions.begin() && end <= std::prev(found)->end) owner = &*std::prev(found);
    } else if (owner == nullptr) {
        for (const auto& region : regions) {
            if (region.begin <= snapshot.address && end <= region.end) {
                owner = &region;
                break;
            }
        }
    }
    if (owner != nullptr) {
        // A region read live (an import or mirror bound in place, a copy of live bytes) serves the
        // GPU whatever the snapshot holds: the compare only checked that the capture (made before
        // the device lock) still agreed with memory the guest, or a dispatch writing a descriptor
        // range around the captured words, may since have changed, and a difference failed the
        // whole dispatch where hardware would have run with the bytes in memory. A snapshot-backed
        // region uploads its own snapshot, so two snapshots of one range must agree. The live
        // compare reads the pages directly (no flush hook), so it costs no sync, only the memcmp;
        // it stays on because it is the one check that the words the recompiler baked into
        // descriptors and specialization still equal what the GPU reads. APS5_NO_SNAPSHOT_CHECK=1
        // skips the compare of live-backed regions (the region serves the GPU whatever the snapshot
        // held; only the diagnostic is lost).
        static const bool checkLive = std::getenv("APS5_NO_SNAPSHOT_CHECK") == nullptr;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const bool live = owner->writable || owner->hostBacked || owner->mirror != nullptr;
        // A live region over a unit shadow's fresh results reads the import's stale bytes here
        // (the publish happens when the region is bound, under the device lock): not compared.
        if (live && (!checkLive || AnyShadowedOverlaps(snapshot.address, snapshot.bytes.size()))) {
            if (profile) Snapshots().skipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto offset = static_cast<std::size_t>(snapshot.address - owner->begin);
        const auto* source = live ? reinterpret_cast<const std::byte*>(snapshot.address) : owner->snapshot.data() + offset;
        Require(std::memcmp(source, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "guest snapshot differs from registered memory");
        if (profile) Snapshots().checked.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    regions.push_back({snapshot.address, end, false, {snapshot.bytes.begin(), snapshot.bytes.end()}, nullptr});
    regionsSorted = false;
}

void GuestBufferMemory::Upload(bool addressable) {
    UploadPrepare(addressable);
    UploadFinish(addressable);
}

namespace {

// APS5_PROFILE_DRAW: sub-phase totals of the uploads in microseconds, reported every 2000 uploads
// ([buffers] line); atomic because the prepare stage of several builds runs at once.
std::atomic<std::uint64_t> importUs{0};
std::atomic<std::uint64_t> allocateUs{0};
std::atomic<std::uint64_t> readUs{0};
std::atomic<std::uint64_t> uploadsProfiled{0};

std::uint64_t microsecondsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}

// GPU-side copies. A descriptor sub-range inside a host import whose offset is not a multiple of
// minStorageBufferOffsetAlignment cannot be bound in place; ~31000 of them per run (about 34 bytes
// each) were copied through the CPU, and every such read went through the flush hook and waited
// for the producer batch ([hooksync] 'buffer-upload'). Instead the copy is a vkCmdCopyBuffer from
// the import into the region's buffer, recorded into the open batch after the producer, and the
// written sub-ranges are copied back into the import by the GPU (RecordCopyBacks), so neither
// direction touches guest memory from the CPU. APS5_CPU_COPIES=1 copies on the CPU as before;
// APS5_GPU_COPY_MAX_KIB (default 1024) bounds the region size the GPU copies (larger ones and
// regions outside imports keep the CPU path).
bool gpuCopiesEnabled() {
    static const bool disabled = std::getenv("APS5_CPU_COPIES") != nullptr;
    return !disabled;
}

std::uint64_t gpuCopyLimit() {
    static const std::uint64_t limit = [] {
        const char* value = std::getenv("APS5_GPU_COPY_MAX_KIB");
        return (value != nullptr ? std::strtoull(value, nullptr, 10) : 1024ull) << 10u;
    }();
    return limit;
}

VkBufferUsageFlags gpuCopyUsage(bool addressable) {
    return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
}

// Device-local staging. A written V# element bound in place makes the shader store into host
// memory over PCIe: the froxel culling kernel's ~248K scattered 4-byte stores took 2.8 ms per
// dispatch and the histogram kernels' ~60K atomics on a 1 KB bin table 1.4 ms, on a GPU that is
// otherwise idle. Instead an element the recompiler marks written (DescriptorBinding::bufferWritten,
// within a size window) or atomic (bufferAtomic, up to its own cap) that lies inside a host import
// takes the gpuCopy path above with a DEVICE_LOCAL buffer: copied in from the import before the
// work (never skipped for a write-only element: the copy-back is the whole written range, so every
// byte the shader leaves alone must hold the import's bytes), bound in the import's place, and
// copied back by RecordCopyBacks after the work, with the pending-write note and the direct-write
// marks of an in-place write. Only builds whose every use records the work and calls
// RecordCopyBacks take it (dispatches, see AllowDeviceStaging; a synchronous draw would store the
// staging bytes from the CPU, which a device-local buffer cannot serve), and never address-based
// ones (their pointers read the import in place, and AddressRanges needs a device address). The
// shadow is per use, not resident: a reused build records its copy-in anew (RecordStagingCopies).
// Accepted trade-off, as for the misaligned copies above: a CPU write into a staged range between
// the copy-in and the copy-back is rolled back by the copy-back (the whole written element range,
// since store extents are not known), for the batch's latency; the staging window keeps the
// exposure to the elements named on the [buffers] staging line and APS5_TRACE_STAGING.
// APS5_NO_WRITTEN_SHADOW=1 and APS5_NO_ATOMIC_STAGING=1 bind those elements in place as before;
// APS5_WRITTEN_SHADOW_MIN_KIB / APS5_WRITTEN_SHADOW_MAX_KIB (16 / 2048) bound the written window
// (a kernel streaming once through a large buffer, the engine's memcpy kernel over 4-8 MiB video
// frames, would only gain the two copies), APS5_ATOMIC_STAGE_MAX_KIB (1024) the atomic one.
bool writtenShadowEnabled() {
    static const bool disabled = std::getenv("APS5_NO_WRITTEN_SHADOW") != nullptr;
    return !disabled;
}

bool atomicStagingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ATOMIC_STAGING") != nullptr;
    return !disabled;
}

std::uint64_t kibSetting(const char* name, std::uint64_t defaultKib) {
    const char* value = std::getenv(name);
    return (value != nullptr ? std::strtoull(value, nullptr, 10) : defaultKib) << 10u;
}

std::uint64_t writtenShadowMin() {
    static const std::uint64_t bytes = kibSetting("APS5_WRITTEN_SHADOW_MIN_KIB", 16);
    return bytes;
}

std::uint64_t writtenShadowMax() {
    static const std::uint64_t bytes = kibSetting("APS5_WRITTEN_SHADOW_MAX_KIB", 2048);
    return bytes;
}

std::uint64_t atomicStageMax() {
    static const std::uint64_t bytes = kibSetting("APS5_ATOMIC_STAGE_MAX_KIB", 1024);
    return bytes;
}

VkMemoryPropertyFlags copyBufferProperties(bool deviceLocal) {
    return deviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

// [gputime] classes (APS5_PROFILE_GPU) of a build's GPU copies, barriers included: the copy-ins
// of its regions and their copy-backs (staged or host shadows alike).
constexpr auto StagingCopyInKey = Recorder::CommandClass::StagingIn;
constexpr auto StagingCopyBackKey = Recorder::CommandClass::StagingOut;

// APS5_PROFILE_DRAW: the GPU copies, reported in the [buffers] uploads line (cumulative), and the
// device-local staging counts, reported as [buffers] staging every 10 s (see reportStaging).
struct CopyStats {
    std::atomic<std::uint64_t> gpuCopies{0};
    std::atomic<std::uint64_t> gpuCopyBytes{0};
    std::atomic<std::uint64_t> gpuCopyBacks{0};
    std::atomic<std::uint64_t> gpuCopyBackBytes{0};
    // Written gpuCopy regions of a use that recorded no copy-back (a synchronous draw): stored
    // from the staging buffer by the CPU in WriteBack.
    std::atomic<std::uint64_t> stagingStores{0};
    // Staged regions copied in (every use), those with an atomic element, those of a reused build,
    // the bytes copied in and back, staged regions whose use recorded a copy-in but no copy-back
    // (counted when the build is reused or destroyed), and shadows the device refused.
    std::atomic<std::uint64_t> staged{0};
    std::atomic<std::uint64_t> stagedAtomic{0};
    std::atomic<std::uint64_t> stagedReused{0};
    std::atomic<std::uint64_t> stagedInBytes{0};
    std::atomic<std::uint64_t> stagedOutBytes{0};
    std::atomic<std::uint64_t> stagedLost{0};
    std::atomic<std::uint64_t> stagingRefused{0};
    std::atomic<std::int64_t> lastStagingReport{0};
};

CopyStats& Copies() {
    static CopyStats stats;
    return stats;
}

// A shadow the device cannot provide (device memory or the allocation count exhausted: every
// shadow is its own VkDeviceMemory, retained by cached builds) is no error: nullptr, and the
// region takes the path it would take without staging (Region::unstaged).
std::shared_ptr<Buffer> stagingBuffer(const Context& context, std::size_t bytes, VkBufferUsageFlags usage) {
    try {
        return std::make_shared<Buffer>(context, bytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    } catch (const std::exception& error) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 4) std::fprintf(stderr, "[buffers] staging shadow of %zu bytes refused: %s\n", bytes, error.what());
        Copies().stagingRefused.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
}

// APS5_TRACE_STAGING=1: every distinct staged range once, with its size and whether an atomic
// element lies in it, to see which elements the staging window admits at a stage.
void traceStaged(std::uint64_t begin, std::uint64_t end, bool atomic) {
    static const bool trace = std::getenv("APS5_TRACE_STAGING") != nullptr;
    if (!trace) return;
    static std::mutex mutex;
    static std::set<std::pair<std::uint64_t, std::uint64_t>> seen;
    std::lock_guard lock(mutex);
    if (!seen.insert({begin, end}).second) return;
    std::fprintf(stderr, "[staging] 0x%llx+0x%llx (%.1f KiB)%s\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), (end - begin) / 1024.0, atomic ? " atomic" : "");
}

void reportStaging() {
    auto& stats = Copies();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = stats.lastStagingReport.load();
    if (nowMs - last < 10000 || !stats.lastStagingReport.compare_exchange_strong(last, nowMs)) return;
    // One reporter per period (the exchange above), so the previous totals need no lock.
    static std::uint64_t lastStaged = 0, lastIn = 0, lastOut = 0;
    const auto staged = stats.staged.load();
    const auto in = stats.stagedInBytes.load();
    const auto out = stats.stagedOutBytes.load();
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] staging: %llu regions staged device-local (%llu with atomics, %llu of reused builds), %.0f KiB copied in, %.0f KiB copied back, %llu without copy-back, %llu shadows refused; last 10 s: %llu regions, %.0f KiB in, %.0f KiB back\n", static_cast<unsigned long long>(staged), static_cast<unsigned long long>(stats.stagedAtomic.load()), static_cast<unsigned long long>(stats.stagedReused.load()), in / 1024.0, out / 1024.0, static_cast<unsigned long long>(stats.stagedLost.load()), static_cast<unsigned long long>(stats.stagingRefused.load()), static_cast<unsigned long long>(staged - lastStaged), (in - lastIn) / 1024.0, (out - lastOut) / 1024.0);
    lastStaged = staged;
    lastIn = in;
    lastOut = out;
}

}

void ShutdownGuestBufferWorkers() {
    RefreshPool::Shutdown();
}

GuestBufferMemory::~GuestBufferMemory() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    // A staged region whose last use recorded its copy-in but no copy-back (the use threw before
    // RecordCopyBacks, or never dispatched): whatever the shader stored stayed in the shadow.
    for (const auto& region : regions) {
        if (region.gpuCopy && region.deviceLocal && !region.copiedBack) Copies().stagedLost.fetch_add(1, std::memory_order_relaxed);
    }
}

bool GuestBufferMemory::gpuCopyEligible(const Region& region) const {
    if (!gpuCopiesEnabled() || region.sparse || !(region.hostBacked || region.writable)) return false;
    return region.end - region.begin <= gpuCopyLimit();
}

bool GuestBufferMemory::bindableInPlace(std::uint64_t offset, bool addressable) const {
    if (adjustedRegions && !addressable) return offset % 4 == 0;
    return offset % context.limits.minStorageBufferOffsetAlignment == 0;
}

bool GuestBufferMemory::stagingEligible(const Region& region, bool addressable) const {
    if (!stagingAllowed || addressable || region.unstaged || !gpuCopiesEnabled() || region.sparse || region.mirror != nullptr) return false;
    const auto bytes = region.end - region.begin;
    if (!WritesOverlap(region.begin, static_cast<std::size_t>(bytes))) return false;
    if (region.atomic && atomicStagingEnabled() && bytes <= atomicStageMax()) return true;
    return writtenShadowEnabled() && bytes >= writtenShadowMin() && bytes <= writtenShadowMax();
}

void GuestBufferMemory::UploadPrepare(bool addressable) {
    Require(!prepared && !uploaded, "guest memory was already uploaded");
    prepared = true;
    std::sort(regions.begin(), regions.end(), [](const Region& left, const Region& right) { return left.begin < right.begin; });
    // Merged regions stay sparse when either part covered uncommitted pages.
    const auto mergeBacked = [](Region& into, const Region& from) {
        if (!into.sparse && !from.sparse) return;
        auto ranges = into.sparse ? into.backed : decltype(into.backed){{into.begin, into.end}};
        if (from.sparse) ranges.insert(ranges.end(), from.backed.begin(), from.backed.end());
        else ranges.emplace_back(from.begin, from.end);
        std::sort(ranges.begin(), ranges.end());
        into.backed.clear();
        for (const auto& range : ranges) {
            if (!into.backed.empty() && range.first <= into.backed.back().second) into.backed.back().second = std::max(into.backed.back().second, range.second);
            else into.backed.push_back(range);
        }
        const auto end = std::max(into.end, from.end);
        into.sparse = !(into.backed.size() == 1 && into.backed.front().first <= into.begin && into.backed.front().second >= end);
        if (!into.sparse) into.backed.clear();
    };
    std::vector<Region> merged;
    for (auto& region : regions) {
        if (!merged.empty() && region.begin < merged.back().end) {
            auto& previous = merged.back();
            if (previous.mirror != nullptr || region.mirror != nullptr) {
                // A descriptor range inside a mirrored image range binds the mirror's bytes (registered
                // ranges never overlap, so the other part is such a descriptor; it sorts first when it
                // starts at the range's base). Writes into a read-only mirror are never stored. One
                // crossing the mirror's end cannot be served by it: this build copies the range.
                if (previous.mirror == nullptr) std::swap(previous, region);
                Require(region.mirror == nullptr, "image mirrors overlap");
                if (region.begin >= previous.begin && region.end <= previous.end) continue;
                previous.mirror = nullptr;
                previous.hostBacked = true;
            }
            mergeBacked(previous, region);
            if (region.end > previous.end) previous.direct = nullptr;
            previous.atomic = previous.atomic || region.atomic;
            // A range starting before the mirror (only possible after the swap) keeps its prefix; the
            // earlier merged region ends at or before it, so the merged list stays sorted.
            previous.begin = std::min(previous.begin, region.begin);
            if (previous.hostBacked || region.hostBacked) {
                // Live guest bytes back the merged range, so snapshots inside it add nothing.
                previous.hostBacked = true;
                previous.writable = previous.writable || region.writable;
                previous.snapshot.clear();
                previous.end = std::max(previous.end, region.end);
                continue;
            }
            Require(previous.writable == region.writable, "writable guest memory overlaps an immutable snapshot");
            if (!region.writable) {
                const auto offset = static_cast<std::size_t>(region.begin - previous.begin);
                const auto overlap = static_cast<std::size_t>(std::min(previous.end, region.end) - region.begin);
                Require(std::memcmp(previous.snapshot.data() + offset, region.snapshot.data(), overlap) == 0, "inconsistent overlapping guest snapshots");
                if (region.end > previous.end) previous.snapshot.insert(previous.snapshot.end(), region.snapshot.begin() + overlap, region.snapshot.end());
            }
            previous.end = std::max(previous.end, region.end);
        } else {
            merged.push_back(std::move(region));
        }
    }
    regions = std::move(merged);
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = std::chrono::steady_clock::now();
    for (auto& region : regions) {
        Require(region.end - region.begin <= std::numeric_limits<std::size_t>::max(), "guest GPU allocation size overflow");
        // Served by a lease mirror, brought up to date when it was acquired.
        if (region.mirror != nullptr || region.direct != nullptr || !mirrorsEnabled()) continue;
        // A descriptor range inside a mirrored image range binds the mirror's sub-range. Looked up
        // before the imports: the image can never be imported, and the import fallback would acquire
        // a registry lease for it on nearly every descriptor upload.
        auto mirror = findMirror(context, region.begin, region.end);
        if (mirror == nullptr || (region.begin - mirror->base) % context.limits.minStorageBufferOffsetAlignment != 0) continue;
        // A read-only mirror's bytes never change while its Range exists; a writable one is
        // refreshed by UploadFinish (the refresh may wait for recorded work). Either is confirmed
        // there, under the lock, before it serves the region: the Range can go between the stages,
        // and until then the region keeps its snapshot for the copy it would need instead.
        region.pending = mirror->writable;
        region.mirror = std::move(mirror);
        region.subrangeMirror = true;
    }
    // Imports already serving regions, taken under one hold of the import lock so every pointer
    // belongs to the same registry epoch (UploadFinish trusts them while the epoch is unchanged). No
    // reconcile and no new import here: retiring one hands its buffer to the recorder, and a region
    // without an import waits for UploadFinish, which may make one.
    if (context.hostImportAlignment != 0) {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        const bool stale = importsStale(context, state);
        for (auto& region : regions) {
            if (region.mirror != nullptr) continue;
            // An import found when the lease was acquired is reused while none was dropped since.
            const HostImport* entry = region.direct != nullptr && state.epoch == importsEpoch ? region.direct : nullptr;
            region.direct = nullptr;
            if (stale) {
                region.pending = true;
                continue;
            }
            if (entry == nullptr) entry = findImport(state, region.begin, region.end);
            if (entry == nullptr) {
                region.pending = true;
                continue;
            }
            // Misaligned inside an import: copied, whatever the registry does meanwhile. By the GPU
            // when eligible (see Region::gpuCopy): UploadFinish records the copy from the import it
            // confirms then (the pointer is provisional, as for an aligned region); the buffer the
            // copy fills is made here, without the device lock, and serves the CPU fallback too. A
            // staged region (see stagingEligible) takes the same path, aligned or not, with a
            // device-local buffer that the CPU fallback replaces (none without a recorder to
            // record the copies: UploadFinish then binds the region in place).
            bool staged = Recorder::Active() != nullptr && stagingEligible(region, addressable);
            const bool misaligned = !bindableInPlace(region.begin - entry->base, addressable);
            if (staged) {
                const auto allocateStart = std::chrono::steady_clock::now();
                region.buffer = stagingBuffer(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(addressable));
                if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                if (region.buffer == nullptr) {
                    // No shadow to be had: in place when aligned, else the host copy below.
                    region.unstaged = true;
                    staged = false;
                }
            }
            if (misaligned || staged) {
                if (!staged) {
                    if (!gpuCopyEligible(region)) continue;
                    const auto allocateStart = std::chrono::steady_clock::now();
                    region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(addressable), copyBufferProperties(false));
                    if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                }
                region.deviceLocal = staged;
                region.direct = entry;
                region.pending = true;
                continue;
            }
            // Provisional until UploadFinish confirms it against the epoch; the snapshot stays for
            // the copy the region falls back to when the import is gone and none can be made.
            region.direct = entry;
            // Storage results pending in the range are flushed by UploadFinish.
            region.pending = true;
        }
        importsEpoch = state.epoch;
    }
    if (profile) importUs.fetch_add(microsecondsSince(started), std::memory_order_relaxed);
    // Copies that need no device work: a range no descriptor writes needs no reference copy, and
    // GuestMemory::Read stores pending GPU results first (the flush hook locks for itself).
    for (auto& region : regions) {
        if (region.pending || region.mirror != nullptr || region.direct != nullptr) continue;
        if (region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(region.end - region.begin))) {
            region.pending = true;
            continue;
        }
        copyRegion(region, addressable);
    }
}

void GuestBufferMemory::UploadFinish(bool addressable) {
    Require(prepared && !uploaded, "guest memory upload was not prepared");
    uploaded = true;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = std::chrono::steady_clock::now();
    // Registered ranges to import from. Address-based shaders hold their lease until write-back;
    // descriptor-only uploads acquire one only when the registry changed or an import must be made
    // (a lease copies every registered range's shared_ptr under the registry lock), so the guest can
    // keep managing memory.
    GuestAllocations::Lease acquired;
    const auto importRanges = [&]() -> const GuestAllocations::Lease& {
        if (!lease.empty()) return lease;
        if (space != nullptr) return space->lease;
        if (acquired.empty()) acquired = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        return acquired;
    };
    // Sub-range mirrors were found by UploadPrepare, possibly long before this lock was taken: one
    // whose Range went meanwhile (the guest reprotected its image) is dropped as findMirror would
    // have dropped it, and the region takes the import or copy path below instead; a confirmed one
    // serves the region, so its snapshot goes. (Lease mirrors need no check: the lease pins their
    // Range.)
    for (auto& region : regions) {
        if (region.mirror == nullptr || !region.subrangeMirror) continue;
        if (region.mirror->range.expired()) {
            region.mirror = nullptr;
            region.subrangeMirror = false;
            region.pending = true;
            continue;
        }
        region.snapshot.clear();
        std::lock_guard lock(Mirrors().mutex);
        ++Mirrors().subranges;
    }
    bool importsRefreshed = false;
    if (space != nullptr && context.hostImportAlignment != 0) {
        // The space's regions were confirmed against the imports when it was built; one check that
        // none was retired since (a mirror refresh or a flush between the stages can reconcile
        // them). If one was, every region takes the per-build path below, which re-resolves each
        // pointer instead of trusting it.
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (importsStale(context, state)) refreshImports(context, state, importRanges());
        importsRefreshed = true;
        if (state.epoch != space->importsEpoch) {
            Spaces().dissolvedImports.fetch_add(1, std::memory_order_relaxed);
            dissolveSpace(true);
        }
    }
    // Regions the GPU copies out of their import (see Region::gpuCopy), recorded together below.
    // No recorder (tests) keeps them on the CPU path.
    auto* recorder = Recorder::Active();
    std::vector<Region*> gpuCopies;
    // One clock pair around the loop (an address-based build has ~1200 regions): the [buffers]
    // 'import lookup' is the loop less the mirror refreshes and the CPU copies, timed apart.
    const auto loopStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    std::uint64_t refreshUs = 0;
    std::uint64_t copyUs = 0;
    for (auto& region : regions) {
        if (!region.pending) continue;
        region.pending = false;
        const auto bytes = region.end - region.begin;
        if (region.mirror != nullptr) {
            // A writable sub-range mirror: refreshed here, where waiting for recorded work is allowed.
            // Read-only mirrors are never written back; a writable one is diffed for this sub-range
            // at write-back.
            const auto refreshStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            refreshMirror(*region.mirror, region.begin, bytes);
            if (profile) refreshUs += microsecondsSince(refreshStart);
            continue;
        }
        bool copyOnGpu = false;
        if (context.hostImportAlignment != 0) {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            // Imports are made on demand for the registered range around each region.
            if (!importsRefreshed) {
                if (importsStale(context, state)) refreshImports(context, state, importRanges());
                importsRefreshed = true;
            }
            // An import taken by UploadPrepare (or at the lease) is still the registry's unless one
            // was dropped since.
            const HostImport* entry = region.direct != nullptr && state.epoch == importsEpoch ? region.direct : nullptr;
            region.direct = nullptr;
            if (entry == nullptr) entry = findImport(state, region.begin, region.end);
            if (entry == nullptr) {
                if (const auto* range = containingRange(importRanges(), region.begin, region.end)) entry = importAllocation(context, state, range->address, range->bytes, importRanges());
            }
            // A staged region (see stagingEligible) is copied out of the import even when aligned;
            // without a recorder to record the copies it binds in place like any other.
            const bool staged = recorder != nullptr && stagingEligible(region, addressable);
            if (entry != nullptr && !staged && bindableInPlace(region.begin - entry->base, addressable)) {
                region.direct = entry;
                region.snapshot.clear();
                // A buffer UploadPrepare made for a GPU copy is not needed: the import serves the
                // region in place (Descriptor prefers `direct`), and a kept buffer would count as copied.
                region.buffer.reset();
                region.deviceLocal = false;
            } else if (entry != nullptr && recorder != nullptr && (staged || gpuCopyEligible(region))) {
                // Misaligned in the import (or staged): the GPU copies the sub-range out of it. The
                // handle and base are taken now, under the import lock: a reconcile by the flush
                // below could retire the entry (the batch opened below keeps a retired import's
                // buffer alive).
                copyOnGpu = true;
                region.deviceLocal = staged;
                region.copySource = entry->buffer;
                region.copySourceBase = entry->base;
            }
        }
        if (region.direct != nullptr) {
            // The GPU reads this range in place, so storage results pending in it must be stored first
            // (outside the import lock: the store may import memory itself). The whole registered heaps
            // an address-based shader maps are exempt (they hold every pending image, and such shaders
            // do not read texture memory through pointers); APS5_BDA_FLUSH=1 flushes for them too.
            static const bool bdaFlush = std::getenv("APS5_BDA_FLUSH") != nullptr;
            if (!(addressable && region.hostBacked) || bdaFlush) StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(bytes), nullptr, "imported buffer region");
            continue;
        }
        if (copyOnGpu) {
            // As for a direct region, storage results pending in the range reach the import before
            // the copy reads it: the flush is recorded (GPU-direct into the import) ahead of the copy
            // in the same batch. The batch is opened first so that an import the flush's own
            // reconcile retires is handed to it (retireImport keeps nothing while the recorder is
            // idle) and outlives the copy.
            static_cast<void>(recorder->Commands());
            StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(bytes), nullptr, "copied buffer region");
            region.gpuCopy = true;
            gpuCopies.push_back(&region);
            continue;
        }
        if (region.deviceLocal) {
            // The staging shadow UploadPrepare made cannot take the CPU copy (no host mapping): the
            // region falls back to a host buffer of its own.
            region.buffer.reset();
            region.deviceLocal = false;
        }
        const auto copyStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        copyRegion(region, addressable);
        if (profile) copyUs += microsecondsSince(copyStart);
    }
    if (!gpuCopies.empty()) recordGpuCopies(gpuCopies, addressable);
    takeHeapReferences();
    if (!profile) return;
    const auto loopUs = microsecondsSince(loopStart);
    readUs.fetch_add(refreshUs, std::memory_order_relaxed);
    importUs.fetch_add(loopUs > refreshUs + copyUs ? loopUs - refreshUs - copyUs : 0, std::memory_order_relaxed);
    const auto uploads = uploadsProfiled.fetch_add(1, std::memory_order_relaxed) + 1;
    if (uploads % 2000 == 0) {
        const auto& copies = Copies();
        AgcDriver::ProfilePrint_nid_no_patch("[buffers] %llu uploads: import lookup %.0f ms, buffer allocation %.0f ms, guest read %.0f ms; copies on the GPU: %llu regions (%.0f KiB) copied out of imports, %llu written sub-ranges (%.0f KiB) copied back, %llu staging stores by the CPU at write-back\n", static_cast<unsigned long long>(uploads), importUs.load(std::memory_order_relaxed) / 1000.0, allocateUs.load(std::memory_order_relaxed) / 1000.0, readUs.load(std::memory_order_relaxed) / 1000.0, static_cast<unsigned long long>(copies.gpuCopies.load(std::memory_order_relaxed)), copies.gpuCopyBytes.load(std::memory_order_relaxed) / 1024.0, static_cast<unsigned long long>(copies.gpuCopyBacks.load(std::memory_order_relaxed)), copies.gpuCopyBackBytes.load(std::memory_order_relaxed) / 1024.0, static_cast<unsigned long long>(copies.stagingStores.load(std::memory_order_relaxed)));
    }
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (ms > 50) {
        std::uint64_t total = 0;
        std::uint64_t largest = 0;
        std::uint64_t copied = 0;
        for (const auto& region : regions) {
            total += region.end - region.begin;
            largest = std::max<std::uint64_t>(largest, region.end - region.begin);
            if (region.buffer != nullptr) copied += region.end - region.begin;
        }
        AgcDriver::ProfilePrint_nid_no_patch("[resources] guest upload of %zu regions, %.1f MiB (largest %.1f MiB, copied %.1f MiB, addressable %d) took %.0f ms to finish\n", regions.size(), total / 1048576.0, largest / 1048576.0, copied / 1048576.0, addressable ? 1 : 0, ms);
    }
    // Debug aid: APS5_TRACE_BIGBUF lists every copied region of at least 1 MiB.
    static const bool traceBig = std::getenv("APS5_TRACE_BIGBUF") != nullptr;
    if (traceBig) {
        for (const auto& region : regions) {
            if (region.buffer != nullptr && region.end - region.begin >= (1u << 20u)) std::fprintf(stderr, "[bigbuf] upload 0x%llx+0x%llx writable=%d hostBacked=%d sparse=%d addressable=%d\n", static_cast<unsigned long long>(region.begin), static_cast<unsigned long long>(region.end - region.begin), region.writable ? 1 : 0, region.hostBacked ? 1 : 0, region.sparse ? 1 : 0, addressable ? 1 : 0);
        }
    }
}

void GuestBufferMemory::copyRegion(Region& region, bool addressable) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto bytes = region.end - region.begin;
    const auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
    if (profile) {
        // Why the region is copied rather than bound in place, totalled every 1000 uploads (under a
        // mutex: prepare stages of several builds copy at once).
        static std::mutex reasonsMutex;
        static std::uint64_t uploads = 0, noImport = 0, noImportBytes = 0, misaligned = 0, misalignedBytes = 0, snapshotOnly = 0, snapshotBytes = 0;
        static std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> outside;
        bool inImport = false;
        if (context.hostImportAlignment != 0) {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            inImport = findImport(state, region.begin, region.end) != nullptr;
        }
        std::lock_guard lock(reasonsMutex);
        if (!region.hostBacked && !region.writable) { ++snapshotOnly; snapshotBytes += bytes; }
        else if (inImport) { ++misaligned; misalignedBytes += bytes; }
        else {
            ++noImport;
            noImportBytes += bytes;
            auto& bucket = outside[region.begin >> 28u];
            ++bucket.first;
            bucket.second += bytes;
        }
        if (++uploads % 1000 == 0) {
            // 'misaligned in imports' are the ones the GPU did not copy: over APS5_GPU_COPY_MAX_KIB,
            // sparse, APS5_CPU_COPIES, no recorder, or an import gone by UploadFinish.
            AgcDriver::ProfilePrint_nid_no_patch("[buffers] %llu copied regions on the CPU: %llu snapshots (%.0f MiB), %llu misaligned in imports (%.0f MiB), %llu outside imports (%.0f MiB):", static_cast<unsigned long long>(uploads), static_cast<unsigned long long>(snapshotOnly), snapshotBytes / 1048576.0, static_cast<unsigned long long>(misaligned), misalignedBytes / 1048576.0, static_cast<unsigned long long>(noImport), noImportBytes / 1048576.0);
            for (const auto& [granule, counts] : outside) AgcDriver::ProfilePrint_nid_no_patch(" 0x%llx0000000:%llu/%.0fMiB", static_cast<unsigned long long>(granule), static_cast<unsigned long long>(counts.first), counts.second / 1048576.0);
            AgcDriver::ProfilePrint_nid_no_patch("\n");
        }
    }
    const auto allocateStart = std::chrono::steady_clock::now();
    // A buffer made by UploadPrepare for a GPU copy that fell back here (its import went) is kept.
    if (region.buffer == nullptr) region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), usage);
    const auto readStart = std::chrono::steady_clock::now();
    if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
    // Named for the [hooksync] attribution: the reads below go through the flush hook.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::BufferUpload);
    if (region.hostBacked || region.writable) {
        // Only ranges a descriptor may write need the copy the write-back diffs against; the
        // registered ranges of an address-based shader are read-only to it (AddressRanges).
        const bool written = region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(bytes));
        if (!region.sparse) {
            GuestMemory::Read(region.begin, region.buffer->Bytes());
            if (written) region.uploaded.assign(region.buffer->Bytes().begin(), region.buffer->Bytes().end());
        } else {
            // `uploaded` holds the backed pieces back to back.
            for (const auto& [first, last] : region.backed) {
                const auto piece = region.buffer->Bytes().subspan(static_cast<std::size_t>(first - region.begin), static_cast<std::size_t>(last - first));
                GuestMemory::Read(first, piece);
                if (written) region.uploaded.insert(region.uploaded.end(), piece.begin(), piece.end());
            }
        }
    }
    else std::memcpy(region.buffer->Bytes().data(), region.snapshot.data(), region.snapshot.size());
    region.snapshot.clear();
    if (profile) readUs.fetch_add(microsecondsSince(readStart), std::memory_order_relaxed);
}

void GuestBufferMemory::recordGpuCopies(std::span<Region* const> copies, bool addressable) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static const bool capture = std::getenv("APS5_CAPTURE_GPU_COPIES") != nullptr;
    Require(!capture || CaptureTrace::Enabled(), "GPU copy capture requires APS5_CAPTURE_TRACE");
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "GPU buffer copies need an active recorder");
    // A queued DCC key store or label store over a copied range lands before the copy reads it.
    for (const auto* region : copies) {
        recorder->FlushKeyStoresOverlapping(region->begin, static_cast<std::size_t>(region->end - region->begin));
        recorder->FlushStoresOverlapping(region->begin, static_cast<std::size_t>(region->end - region->begin));
    }
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(StagingCopyInKey);
    if (Recorder::BarrierValidate()) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> reads;
        for (const auto* region : copies) reads.emplace_back(region->begin, region->end);
        recorder->NoteAccess(Recorder::CommandClass::StagingIn, Recorder::Access{reads, {}, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
    bool stagedAny = false;
    std::uint64_t copiedBytes = 0;
    for (auto* region : copies) {
        const auto bytes = region->end - region->begin;
        copiedBytes += bytes;
        if (region->buffer == nullptr) {
            // Not made by UploadPrepare (the registry was stale then): made here, under the lock.
            const auto allocateStart = std::chrono::steady_clock::now();
            if (region->deviceLocal) region->buffer = stagingBuffer(context, static_cast<std::size_t>(bytes), gpuCopyUsage(addressable));
            if (region->buffer == nullptr) {
                // Or no shadow to be had: a host copy serves the region instead, aligned or not.
                region->deviceLocal = false;
                region->unstaged = true;
                region->buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), gpuCopyUsage(addressable), copyBufferProperties(false));
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
            recorder->Keep(inputSnapshot);
            CaptureTrace::Log("copy-input-snapshot batch=%llu address=%llx bytes=%llu", batch, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        }
        if (capture && bytes == 32) {
            CaptureTrace::Log("copy-input-candidate batch=%llu address=%llx bytes=%llu pendingWriter=%d writable=%d", batch, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), pendingWriter, writable);
            if (inputSnapshot != nullptr) {
                const auto snapshotBytes = inputSnapshot->Bytes();
                expected.assign(snapshotBytes.begin(), snapshotBytes.end());
            }
        }
        CopyBuffer(context, commands, copySource, copyOffset, region->buffer->Handle(), 0, bytes);
        if (!expected.empty()) {
            auto readback = std::make_shared<Buffer>(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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
        // Kept by the batch at record time, as every other recorded target is: the caller keeps
        // the resources only after descriptor and pipeline work that may throw, and a released
        // buffer would go back to the pool (reused or destroyed) under this copy command. The
        // source is a live import or one retireImport already handed to this batch.
        recorder->Keep(region->buffer);
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
    if (stagedAny) reportStaging();
    // The copied bytes are visible to the shaders bound to the buffers, which may also store into
    // them. Every stage: dispatches and draws share this path, and draws run mesh, task and
    // tessellation shaders too (the batch's own barriers around dispatches are as wide).
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
    recorder->MarkCovered(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    recorder->EndGpuTiming(timing, copiedBytes);
}

void GuestBufferMemory::RecordCopyBacks(Recorder& recorder) {
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // The written ranges, merged: a V# bound twice would otherwise copy the same bytes twice.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    bool recording = false;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    std::uint64_t copiedBytes = 0;
    for (auto& region : regions) {
        if (!region.gpuCopy || region.copiedBack) continue;
        if (merged.empty() && !writes.empty()) {
            auto sorted = writes;
            std::sort(sorted.begin(), sorted.end());
            for (const auto& range : sorted) {
                if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
                else merged.push_back(range);
            }
        }
        for (const auto& [begin, end] : merged) {
            const auto from = std::max(begin, region.begin);
            const auto to = std::min(end, region.end);
            if (from >= to) continue;
            if (!recording) {
                commands = recorder.Commands();
                timing = recorder.BeginGpuTiming(StagingCopyBackKey);
                // The shader's stores into the buffer (whichever stage made them) precede the copy.
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
                recording = true;
            }
            // Exactly the sub-range the shader may write, in place in the import: what the shader
            // would have stored there itself had the range been bindable.
            CopyBuffer(context, commands, region.buffer->Handle(), from - region.begin, region.copySource, from - region.copySourceBase, to - from);
            copiedBytes += to - from;
            recorder.Keep(region.buffer);
            if (profile) {
                Copies().gpuCopyBacks.fetch_add(1, std::memory_order_relaxed);
                Copies().gpuCopyBackBytes.fetch_add(to - from, std::memory_order_relaxed);
                if (region.deviceLocal) Copies().stagedOutBytes.fetch_add(to - from, std::memory_order_relaxed);
            }
        }
        // Only once its copies are recorded: a throw above leaves the region counted by
        // HasCopiedWrites, so the caller still registers the CPU write-back that stores the
        // staging bytes, instead of losing the shader's results.
        region.copiedBack = true;
    }
    if (!recording) return;
    if (Recorder::BarrierValidate()) recorder.NoteAccess(Recorder::CommandClass::StagingOut, Recorder::Access{{}, merged, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    // As a GPU label store or fill: visible to everything recorded after (shaders, transfers, an
    // indirect dispatch's arguments) and to the host once the batch completed; the caller's
    // pending-write note makes CPU readers wait for that.
    constexpr VkAccessFlags copiedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, copiedAccess);
    Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
    recorder.MarkCovered(copiedAccess);
    recorder.EndGpuTiming(timing, copiedBytes);
}

VkDescriptorBufferInfo GuestBufferMemory::Descriptor(std::uint64_t address, std::size_t bytes, std::uint32_t& adjustment) const {
    Require(uploaded && !committed, "guest GPU memory is not available");
    Require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest buffer view");
    const auto* found = owner(address);
    Require(found != nullptr, "guest buffer has no GPU owner");
    const auto& region = *found;
    Require(address >= region.begin && address + bytes <= region.end && (region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr), "guest buffer view exceeds its GPU owner");
    const auto base = region.direct != nullptr ? region.direct->base : region.mirror != nullptr ? region.mirror->base : region.begin;
    const auto offset = address - base;
    Require(context.limits.minStorageBufferOffsetAlignment != 0, "no storage buffer offset alignment");
    Require(base % 4 == 0, "a guest buffer view in a GPU owner that does not start at a DWORD boundary is not implemented");
    adjustment = static_cast<std::uint32_t>(offset % std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4));
    const auto range = ViewBytes(bytes, adjustment);
    const auto end = region.direct != nullptr ? region.direct->base + region.direct->bytes : region.end;
    Require(address - adjustment + range <= end, "guest buffer view exceeds its GPU owner");
    Require(range <= context.limits.maxStorageBufferRange, "guest buffer view exceeds descriptor range limit");
    const auto handle = region.direct != nullptr ? region.direct->buffer : region.mirror != nullptr ? region.mirror->buffer->Handle() : region.buffer->Handle();
    return {handle, offset - adjustment, range};
}

std::uint64_t GuestBufferMemory::ViewBytes(std::uint64_t bytes, std::uint32_t adjustment) {
    return adjustment % 4 == 0 ? bytes + adjustment : 4 * (adjustment / 4 + bytes / 4 + 1);
}

ShaderRecompiler::BdaAbi::Range GuestBufferMemory::addressRange(const Region& region) {
    Require(region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr, "incomplete guest GPU upload");
    const auto address = region.direct != nullptr ? region.direct->address + (region.begin - region.direct->base) : region.mirror != nullptr ? region.mirror->buffer->DeviceAddress() + (region.begin - region.mirror->base) : region.buffer->DeviceAddress();
    Require(region.end - region.begin <= std::numeric_limits<std::uint64_t>::max() - address, "GPU address range overflow");
    const auto permissions = ShaderRecompiler::BdaAbi::Read | (region.direct != nullptr && region.writable ? ShaderRecompiler::BdaAbi::Write : 0u);
    return {region.begin, region.end, address, permissions, 0};
}

std::vector<ShaderRecompiler::BdaAbi::Range> GuestBufferMemory::AddressRanges() const {
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    std::vector<ShaderRecompiler::BdaAbi::Range> result;
    result.reserve(regions.size() + (space != nullptr ? space->ranges.size() : 0));
    for (const auto& region : regions) result.push_back(addressRange(region));
    if (space != nullptr) {
        std::vector<ShaderRecompiler::BdaAbi::Range> combined;
        combined.reserve(result.size() + space->ranges.size());
        std::merge(space->ranges.begin(), space->ranges.end(), result.begin(), result.end(), std::back_inserter(combined), [](const auto& left, const auto& right) { return left.begin < right.begin; });
        result = std::move(combined);
    }
    if (writes.empty()) return result;
    auto sortedWrites = writes;
    std::sort(sortedWrites.begin(), sortedWrites.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> intervals;
    for (const auto& interval : sortedWrites) {
        if (!intervals.empty() && interval.first <= intervals.back().second) intervals.back().second = std::max(intervals.back().second, interval.second);
        else intervals.push_back(interval);
    }
    std::vector<ShaderRecompiler::BdaAbi::Range> merged;
    merged.reserve(result.size() + intervals.size() * 2u);
    std::size_t firstWrite = 0;
    for (const auto& range : result) {
        if ((range.permissions & ShaderRecompiler::BdaAbi::Write) != 0u) {
            merged.push_back(range);
            continue;
        }
        while (firstWrite < intervals.size() && intervals[firstWrite].second <= range.begin) ++firstWrite;
        auto cursor = range.begin;
        const auto append = [&](std::uint64_t end, bool written) {
            if (cursor == end) return;
            auto part = range;
            part.begin = cursor;
            part.end = end;
            part.deviceAddress += cursor - range.begin;
            if (written) {
                const auto* region = owner(cursor);
                Require(region != nullptr && region->writable && (region->mirror == nullptr || region->mirror->writable), "runtime buffer write has no writable GPU owner");
                part.permissions |= ShaderRecompiler::BdaAbi::Write;
            }
            merged.push_back(part);
            cursor = end;
        };
        for (auto index = firstWrite; index < intervals.size() && intervals[index].first < range.end; ++index) {
            append(std::max(cursor, intervals[index].first), false);
            append(std::min(range.end, intervals[index].second), true);
        }
        append(range.end, false);
    }
    return merged;
}

std::optional<GuestBufferMemory::CachedTable> GuestBufferMemory::CachedAddressTable() const {
    if (space == nullptr || !regions.empty()) return std::nullopt;
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    if (writes.empty()) return CachedTable{space->serial, &space->ranges};
    if (writeTableRanges != nullptr) return CachedTable{writeTableSerial, writeTableRanges.get()};
    std::lock_guard lock(space->tableMutex);
    const auto found = space->writeTables.find(writes);
    if (found != space->writeTables.end()) {
        writeTableSerial = found->second.first;
        writeTableRanges = found->second.second;
    } else {
        writeTableRanges = std::make_shared<const std::vector<ShaderRecompiler::BdaAbi::Range>>(AddressRanges());
        writeTableSerial = Spaces().serials.fetch_add(1, std::memory_order_relaxed) + 1;
        if (space->writeTables.size() >= 64u) space->writeTables.erase(space->writeTables.begin());
        space->writeTables.emplace(writes, std::pair{writeTableSerial, writeTableRanges});
    }
    return CachedTable{writeTableSerial, writeTableRanges.get()};
}

bool GuestBufferMemory::HasCopiedWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr) continue;
        // A GPU copy whose written sub-ranges were copied back by the GPU lands like a direct
        // write; until RecordCopyBacks ran it still counts (a use without it stores in WriteBack).
        // A staged copy never counts: its use always records the copy-back (AllowDeviceStaging),
        // and nothing could be stored from it by the CPU.
        if (region.gpuCopy && (region.copiedBack || region.deviceLocal)) continue;
        // Writes into a read-only image mirror are never stored (its pages could not take them).
        if (region.mirror != nullptr && !region.mirror->writable) continue;
        return true;
    }
    return false;
}

void GuestBufferMemory::MarkDirectWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr || (region.gpuCopy && region.copiedBack)) GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
    }
}

namespace {

void storeChanged(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original) {
    const auto writable = GuestMemory::DescribeCommitted(address, current.size(), true);
    if (writable.whole) {
        GuestMemory::WriteChanged(address, current, original);
        return;
    }
    const auto unchanged = [&](std::uint64_t from, std::uint64_t to) {
        const auto at = static_cast<std::size_t>(from - address);
        if (std::memcmp(current.data() + at, original.data() + at, static_cast<std::size_t>(to - from)) == 0) return;
        char message[192];
        std::snprintf(message, sizeof(message), "the GPU changed guest memory 0x%llx+0x%llx that is not writable (a direct memory page shared by several views; aliased writes are not implemented)", static_cast<unsigned long long>(from), static_cast<unsigned long long>(to - from));
        throw std::runtime_error(message);
    };
    auto cursor = address;
    for (const auto& [first, last] : writable.ranges) {
        if (cursor < first) unchanged(cursor, first);
        const auto at = static_cast<std::size_t>(first - address);
        const auto length = static_cast<std::size_t>(last - first);
        GuestMemory::WriteChanged(first, current.subspan(at, length), original.subspan(at, length));
        cursor = last;
    }
    if (cursor < address + current.size()) unchanged(cursor, address + current.size());
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> mergeWrites(std::vector<std::pair<std::uint64_t, std::uint64_t>> writes) {
    std::sort(writes.begin(), writes.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : writes) {
        if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    return merged;
}

}

void GuestBufferMemory::takeHeapReferences() {
    heapReferences.clear();
    for (const auto& [begin, end] : mergeWrites(writes)) {
        const auto* found = owner(begin);
        if (found == nullptr || found->mirror == nullptr || !found->mirror->heap || !found->mirror->writable) continue;
        Require(end <= found->end, "written range exceeds its heap mirror");
        const auto& mirror = *found->mirror;
        const auto bytes = mirror.buffer->Bytes().subspan(static_cast<std::size_t>(begin - mirror.base), static_cast<std::size_t>(end - begin));
        heapReferences.emplace_back(begin, std::vector<std::byte>(bytes.begin(), bytes.end()));
    }
}

void GuestBufferMemory::WriteBack() {
    Require(uploaded && !committed, "guest memory cannot be committed twice or before upload");
    const auto merged = mergeWrites(writes);
    // The CPU keeps running while the GPU works, so storing whole ranges would roll back its writes to
    // bytes the shader never touched. Only runs that differ from the uploaded copy are stored.
    for (const auto& [begin, end] : merged) {
        const auto* found = owner(begin);
        Require(found != nullptr, "write-back range has no GPU owner");
        const auto& region = *found;
        // Every branch below but the direct, copied-back and staged ones stores from the CPU.
        if (region.direct == nullptr && !(region.gpuCopy && (region.copiedBack || region.deviceLocal)) && !(region.mirror != nullptr && !region.mirror->writable)) Recorder::NoteWrittenBack(begin, static_cast<std::size_t>(end - begin));
        if (region.direct != nullptr) {
            // The GPU wrote imported guest memory directly; page write watching did not see it.
            GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
            continue;
        }
        if (region.mirror != nullptr) {
            if (!region.mirror->writable) continue;
            Require(end <= region.end, "write-back range exceeds its GPU owner");
            // The shadow is what the GPU started from; it then takes the mirror's bytes so the next
            // refresh does not mistake the GPU's results for a guest change.
            auto& mirror = *region.mirror;
            const auto offset = static_cast<std::size_t>(begin - mirror.base);
            const auto length = static_cast<std::size_t>(end - begin);
            const auto current = mirror.buffer->Bytes().subspan(offset, length);
            if (mirror.heap) {
                const auto reference = std::find_if(heapReferences.begin(), heapReferences.end(), [&](const auto& entry) { return entry.first == begin; });
                Require(reference != heapReferences.end() && reference->second.size() == length, "heap mirror write-back has no reference");
                storeChanged(begin, current, reference->second);
                continue;
            }
            GuestMemory::WriteChanged(begin, current, std::span<const std::byte>(mirror.shadow).subspan(offset, length));
            std::memcpy(mirror.shadow.data() + offset, current.data(), length);
            continue;
        }
        if (region.gpuCopy) {
            Require(region.buffer != nullptr && end <= region.end, "write-back range exceeds its GPU owner");
            if (region.copiedBack) {
                // The GPU copied the written sub-range back into the import; page write watching
                // did not see it.
                GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
                continue;
            }
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            if (region.deviceLocal) {
                // A staged region's use recorded no copy-back (it threw before RecordCopyBacks):
                // the results stay in the shadow, which has no mapping to store from; counted
                // once, when the build is destroyed or reused.
                continue;
            }
            // No copy-back was recorded (a synchronous draw): the staging bytes of the written
            // sub-range are stored whole, which is what the copy-back would have done. There is no
            // reference copy to diff against (the copy in never passed through the CPU).
            GuestMemory::Write(begin, region.buffer->Bytes().subspan(static_cast<std::size_t>(begin - region.begin), static_cast<std::size_t>(end - begin)));
            if (profile) Copies().stagingStores.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        Require(region.buffer != nullptr && region.writable && end <= region.end, "write-back range exceeds its GPU owner");
        static const bool traceBig = std::getenv("APS5_TRACE_BIGBUF") != nullptr;
        if (traceBig && end - begin >= (1u << 20u)) std::fprintf(stderr, "[bigbuf] writeback 0x%llx+0x%llx sparse=%d\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), region.sparse ? 1 : 0);
        if (!region.sparse) {
            const auto first = static_cast<std::size_t>(begin - region.begin);
            const auto length = static_cast<std::size_t>(end - begin);
            storeChanged(begin, region.buffer->Bytes().subspan(first, length), std::span<const std::byte>(region.uploaded).subspan(first, length));
            continue;
        }
        std::size_t packed = 0;
        for (const auto& [first, last] : region.backed) {
            const auto from = std::max(first, begin);
            const auto to = std::min(last, end);
            if (from < to) {
                const auto length = static_cast<std::size_t>(to - from);
                storeChanged(from, region.buffer->Bytes().subspan(static_cast<std::size_t>(from - region.begin), length), std::span<const std::byte>(region.uploaded).subspan(packed + static_cast<std::size_t>(from - first), length));
            }
            packed += static_cast<std::size_t>(last - first);
        }
    }
    committed = true;
    lease.clear();
    space.reset();
    heapReferences.clear();
}

std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> GuestBufferMemory::DirectRegions() const {
    if (!uploaded || committed) return std::nullopt;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    if (space != nullptr) {
        for (const auto& region : space->base) {
            if (region.direct == nullptr && (region.mirror == nullptr || region.mirror->writable || region.mirror->heap)) return std::nullopt;
            result.emplace_back(region.begin, region.end);
        }
    }
    for (const auto& region : regions) {
        // A read-only mirror is as fixed as an import: its bytes and VkBuffer never change while its
        // A staged region is keyed by its import like one bound in place: the import is the source
        // of its copy-in and the destination of its copy-back, both recorded per use.
        const bool fixedMirror = region.mirror != nullptr && !region.mirror->writable && !region.mirror->heap;
        const bool staged = region.gpuCopy && region.deviceLocal;
        if (region.direct == nullptr && !fixedMirror && !staged) return std::nullopt;
        if (staged) {
            // A later use copies from `copySource` again, so it must still be the import serving
            // the range (the flush before the copy-in can retire and remake imports): the serial
            // the caller records names the current import, and its buffer must be that one.
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            const auto* entry = state.device == context.device ? findImport(state, region.begin, region.end) : nullptr;
            if (entry == nullptr || entry->buffer != region.copySource) return std::nullopt;
        }
        result.emplace_back(region.begin, region.end);
    }
    return result;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> GuestBufferMemory::InPlaceReads() const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    if (!uploaded || committed) return result;
    if (space != nullptr) {
        for (const auto& region : space->base) {
            if (region.direct != nullptr) result.emplace_back(region.begin, region.end);
        }
    }
    for (const auto& region : regions) {
        if (region.direct != nullptr) result.emplace_back(region.begin, region.end);
    }
    return result;
}

void GuestBufferMemory::RecordStagingCopies(Recorder& recorder) {
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::vector<Region*> copies;
    for (auto& region : regions) {
        if (!region.gpuCopy || !region.deviceLocal) continue;
        // The previous use's copy-back was recorded by its MarkGpuWrites (or never, when that use
        // threw first: counted); this use records its own after its work, from the shadow this
        // copy-in refills. The import is the one the region was taken from (the caller checked its
        // serial), so the copy source still stands.
        if (!region.copiedBack && profile) Copies().stagedLost.fetch_add(1, std::memory_order_relaxed);
        region.copiedBack = false;
        copies.push_back(&region);
    }
    if (copies.empty()) return;
    Require(Recorder::Active() == &recorder, "staging copies recorded into a recorder that is not the device's");
    if (profile) Copies().stagedReused.fetch_add(copies.size(), std::memory_order_relaxed);
    recordGpuCopies(copies, false);
}

std::uint64_t ImageMirrorSerial(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address || !mirrorsEnabled()) return 0;
    // A replaced Range expires the mirror (findMirror), and a replacement mirror has a new serial.
    const auto mirror = findMirror(context, address, address + bytes);
    return mirror != nullptr && !mirror->writable ? mirror->serial : 0;
}

std::uint64_t HostImportSerial(const Context& context, std::uint64_t address, std::size_t bytes, bool reconcile) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return 0;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // An import whose registered range changed must not be reused: reconcile first, as an upload
    // does (the walk only runs when the registry changed since the last one).
    if (reconcile && GuestAllocations::GuestAllocationsGeneration_nid_postfix() != state.refreshedGeneration) static_cast<void>(refreshImports(context, state, GuestAllocations::GuestAllocationsAcquire_nid_postfix()));
    auto found = state.imports.upper_bound(address);
    if (found == state.imports.begin()) return 0;
    --found;
    auto& entry = found->second;
    if (address < entry.base || address + bytes > entry.base + entry.bytes) return 0;
    // Serials are handed out on first use: a fresh import starts at 0, so one made after an earlier
    // import of the same range was dropped can never repeat that import's serial.
    static std::uint64_t serials = 0;
    if (entry.serial == 0) entry.serial = ++serials;
    return entry.serial;
}

bool HostImportsUnchanged(const Context& context, const HostImportsProof& proof) {
    if (proof.device == VK_NULL_HANDLE || proof.device != context.device) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.device == proof.device && state.epoch == proof.epoch && state.refreshedGeneration == proof.refreshedGeneration && !importsStale(context, state);
}

HostImportsProof HostImportsIdentity(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (importsStale(context, state)) return {};
    return {state.device, state.epoch, state.refreshedGeneration};
}

}
