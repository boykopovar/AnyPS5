#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifndef _WIN32
#include "prx/libc/include/GuestArena.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
int main() {
    std::printf("skipped, udmabuf imports are Linux only\n");
    return 77;
}
#else
using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

constexpr std::uint64_t Alignment = 65536;
constexpr std::uint64_t ChunkBytes = std::uint64_t{64} << 20u;
constexpr std::uint64_t LargestTestedLimit = std::uint64_t{1} << 30u;

struct Bind {
    VkDeviceSize resourceOffset;
    VkDeviceSize size;
    VkDeviceMemory memory;
    VkDeviceSize memoryOffset;
};

struct MockDevice {
    std::uintptr_t next = 0x1000;
    std::set<VkBuffer> buffers;
    std::set<VkFence> fences;
    std::set<VkBuffer> sparse;
    std::map<VkBuffer, VkDeviceSize> sizes;
    std::map<VkDeviceMemory, VkDeviceSize> memories;
    std::vector<Bind> binds;
    VkResult bindResult = VK_SUCCESS;
    VkResult allocationResult = VK_SUCCESS;
    std::uint64_t allocationAttempts = 0;
    std::uint64_t nextAddress = 0x500000000ull;
    VkDeviceSize sparseAlignment = Alignment;
};

MockDevice mock;

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = reinterpret_cast<VkFence>(mock.next++);
    mock.fences.insert(*fence);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    mock.fences.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(mock.next++);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t, const VkFence*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL mockCmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, std::uint32_t, const VkBufferCopy*) {}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetMemoryHostPointerProperties(VkDevice, VkExternalMemoryHandleTypeFlagBits, const void*, VkMemoryHostPointerPropertiesEXT*) {
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetMemoryFdProperties(VkDevice, VkExternalMemoryHandleTypeFlagBits, int, VkMemoryFdPropertiesKHR* properties) {
    properties->memoryTypeBits = 1;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = reinterpret_cast<VkBuffer>(mock.next++);
    mock.buffers.insert(*buffer);
    if ((info->flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0) mock.sparse.insert(*buffer);
    mock.sizes[*buffer] = info->size;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) {
    mock.buffers.erase(buffer);
}

VKAPI_ATTR void VKAPI_CALL mockGetBufferMemoryRequirements(VkDevice, VkBuffer buffer, VkMemoryRequirements* requirements) {
    const auto alignment = mock.sparse.contains(buffer) ? mock.sparseAlignment : Alignment;
    requirements->size = (mock.sizes.at(buffer) + alignment - 1) / alignment * alignment;
    requirements->alignment = alignment;
    requirements->memoryTypeBits = 1;
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    ++mock.allocationAttempts;
    if (mock.allocationResult != VK_SUCCESS) return mock.allocationResult;
    *memory = reinterpret_cast<VkDeviceMemory>(mock.next++);
    mock.memories[*memory] = info->allocationSize;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    mock.memories.erase(memory);
}

VKAPI_ATTR VkResult VKAPI_CALL mockBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueBindSparse(VkQueue, std::uint32_t count, const VkBindSparseInfo* infos, VkFence) {
    if (mock.bindResult != VK_SUCCESS) return mock.bindResult;
    for (std::uint32_t i = 0; i < count; ++i) {
        for (std::uint32_t b = 0; b < infos[i].bufferBindCount; ++b) {
            const auto& buffer = infos[i].pBufferBinds[b];
            for (std::uint32_t m = 0; m < buffer.bindCount; ++m) mock.binds.push_back({buffer.pBinds[m].resourceOffset, buffer.pBinds[m].size, buffer.pBinds[m].memory, buffer.pBinds[m].memoryOffset});
        }
    }
    return VK_SUCCESS;
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL mockGetBufferDeviceAddress(VkDevice, const VkBufferDeviceAddressInfo* info) {
    const auto address = mock.nextAddress;
    mock.nextAddress += (mock.sizes.at(info->buffer) + Alignment - 1) / Alignment * Alignment;
    return address;
}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockResetCommandBuffer)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyBuffer)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkGetMemoryHostPointerPropertiesEXT", reinterpret_cast<PFN_vkVoidFunction>(mockGetMemoryHostPointerProperties)},
        {"vkGetMemoryFdPropertiesKHR", reinterpret_cast<PFN_vkVoidFunction>(mockGetMemoryFdProperties)},
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCreateBuffer)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateMemory)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(mockFreeMemory)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(mockBindBufferMemory)},
        {"vkQueueBindSparse", reinterpret_cast<PFN_vkVoidFunction>(mockQueueBindSparse)},
        {"vkGetBufferDeviceAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferDeviceAddress)},
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

Context mockContext() {
    static const auto device = reinterpret_cast<VkDevice>(mock.next++);
    static const auto queue = reinterpret_cast<VkQueue>(mock.next++);
    Context context{};
    context.device = device;
    context.queue = queue;
    context.deviceProc = mockProc;
    context.hostImportAlignment = Alignment;
    context.dmaBufImport = true;
    context.sparseBinding = true;
    return context;
}

struct Backing {
    int file = -1;
    std::uint8_t* base = nullptr;
    std::uint64_t bytes = 0;
    std::uint64_t fileOffset = 0;
    void* reservation = nullptr;
    std::uint64_t reserved = 0;
};

std::vector<Backing*> backings;
// The descriptor the resolver last handed out: the import owns it, so after the import it is closed.
int lastBackingFile = -1;

void ExpectBackingClosed(const char* what) {
    Expect(lastBackingFile >= 0 && fcntl(lastBackingFile, F_GETFD) == -1 && errno == EBADF, what);
}

bool Resolve(std::uintptr_t address, std::size_t bytes, int* file, std::uint64_t* offset) {
    for (const auto* backing : backings) {
        const auto begin = reinterpret_cast<std::uintptr_t>(backing->base);
        if (address < begin || address + bytes > begin + backing->bytes) continue;
        *file = dup(backing->file);
        lastBackingFile = *file;
        *offset = backing->fileOffset + (address - begin);
        return *file >= 0;
    }
    return false;
}

std::uint64_t UdmabufLimitBytes() {
    std::ifstream limit("/sys/module/udmabuf/parameters/size_limit_mb");
    std::uint64_t megabytes = 64;
    limit >> megabytes;
    return megabytes << 20u;
}

struct SharedRange {
    Backing backing;

    SharedRange(std::uint64_t bytes, std::uint64_t fileOffset, bool sealed) {
        backing.bytes = bytes;
        backing.fileOffset = fileOffset;
        backing.file = memfd_create("anyps5-sparse-import-mock", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        const auto fileBytes = (fileOffset + bytes + ChunkBytes - 1) / ChunkBytes * ChunkBytes;
        if (backing.file < 0 || ftruncate(backing.file, static_cast<off_t>(fileBytes)) != 0) throw std::runtime_error("cannot make the backing file");
        if (sealed && fcntl(backing.file, F_ADD_SEALS, F_SEAL_SHRINK) != 0) throw std::runtime_error("cannot seal the backing file");
        backing.reserved = bytes + Alignment;
        backing.reservation = mmap(nullptr, backing.reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (backing.reservation == MAP_FAILED) throw std::runtime_error("cannot reserve the guest range");
        const auto aligned = (reinterpret_cast<std::uintptr_t>(backing.reservation) + Alignment - 1) / Alignment * Alignment;
        void* mapped = mmap(reinterpret_cast<void*>(aligned), bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, backing.file, static_cast<off_t>(fileOffset));
        if (mapped == MAP_FAILED) throw std::runtime_error("cannot map the backing file");
        backing.base = static_cast<std::uint8_t*>(mapped);
        backings.push_back(&backing);
        GuestAllocations::Mutation mutation;
        mutation.Add(backing.base, static_cast<std::size_t>(bytes), true, true, true);
    }

    ~SharedRange() {
        std::erase(backings, &backing);
    }

    std::uint64_t Address() const {
        return reinterpret_cast<std::uint64_t>(backing.base);
    }
};

std::uint64_t RangeBytes() {
    return (UdmabufLimitBytes() / ChunkBytes + 1) * ChunkBytes + (std::uint64_t{1} << 20u);
}

void SettlingSurvivesDeviceReplacement(bool reverse) {
    const auto baselineBuffers = mock.buffers.size();
    const auto baselineMemories = mock.memories.size();
    const auto baselineFences = mock.fences.size();
    SharedRange range(8u << 20u, 0, true);
    auto first = mockContext();
    first.device = reinterpret_cast<VkDevice>(mock.next++);
    first.settleQueue = reinterpret_cast<VkQueue>(mock.next++);
    first.settlePool = reinterpret_cast<VkCommandPool>(mock.next++);
    std::mutex queueMutex;
    first.settleQueueMutex = &queueMutex;
    auto second = first;
    second.device = reinterpret_cast<VkDevice>(mock.next++);
    second.settlePool = reinterpret_cast<VkCommandPool>(mock.next++);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    for (const auto& context : {first, second, first}) {
        const auto* imported = HostImportFor(context, range.Address(), 4096);
        ExpectBackingClosed("a chunk import retained the backing descriptor");
        Expect(imported != nullptr && imported->dmaBuf, "replacement device did not settle its dma-buf import");
    }
    ClearHostImports(reverse ? second.device : first.device);
    Expect(mock.fences.size() == baselineFences + 1, "clearing one device did not retain exactly the other device's settle fence");
    ClearHostImports(reverse ? first.device : second.device);
    Expect(mock.buffers.size() == baselineBuffers, "device replacement leaked a settle buffer");
    Expect(mock.memories.size() == baselineMemories, "device replacement leaked settle memory");
    Expect(mock.fences.size() == baselineFences, "device replacement leaked a settle fence");
    ClearHostImports(first.device);
    ClearHostImports(second.device);
    Expect(mock.fences.size() == baselineFences, "repeated device cleanup changed the live fences");
}

void ImportsALargeRangeAsOneSparseBuffer() {
    const auto bytes = RangeBytes();
    SharedRange range(bytes, 0, true);
    const auto context = mockContext();
    mock.binds.clear();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* import = HostImportFor(context, range.Address(), static_cast<std::size_t>(bytes));
    ExpectBackingClosed("a sparse import retained the backing descriptor");
    Expect(import != nullptr, "a shared range larger than udmabuf's size limit was not imported");
    if (import == nullptr) return;
    Expect(import->base == range.Address() && import->bytes == bytes && import->dmaBuf, "the import does not cover the whole range as a dma-buf import");
    Expect(mock.sparse.contains(import->buffer), "the range was not imported as one sparse buffer");
    const auto chunks = (bytes + ChunkBytes - 1) / ChunkBytes;
    Expect(mock.binds.size() == chunks, "the sparse buffer is not bound to one chunk import per 64 MiB of the range");
    VkDeviceSize resource = 0;
    std::set<VkDeviceMemory> memories;
    for (const auto& bind : mock.binds) {
        Expect(bind.resourceOffset == resource && bind.memoryOffset == 0 && bind.size == std::min(ChunkBytes, bytes - resource), "a bind does not map its chunk import to the matching part of the range");
        Expect(mock.memories.contains(bind.memory) && mock.memories.at(bind.memory) == ChunkBytes, "a bind does not use a 64 MiB chunk import");
        memories.insert(bind.memory);
        resource += bind.size;
    }
    Expect(memories.size() == chunks, "two parts of the range are bound to the same chunk import");
}

struct ScatteredRange {
    int file = -1;
    std::uint8_t* base = nullptr;
    void* reservation = nullptr;
    std::uint64_t reserved = 0;
    std::vector<GuestArena::SharedBackingRun> runs;

    ScatteredRange(std::vector<GuestArena::SharedBackingRun> pieces) : runs(std::move(pieces)) {
        std::uint64_t bytes = 0;
        std::uint64_t fileEnd = 0;
        for (const auto& run : runs) {
            bytes += run.bytes;
            fileEnd = std::max<std::uint64_t>(fileEnd, run.offset + run.bytes);
        }
        file = memfd_create("anyps5-sparse-runs-mock", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        const auto fileBytes = (fileEnd + ChunkBytes - 1) / ChunkBytes * ChunkBytes;
        if (file < 0 || ftruncate(file, static_cast<off_t>(fileBytes)) != 0 || fcntl(file, F_ADD_SEALS, F_SEAL_SHRINK) != 0) throw std::runtime_error("cannot make the scattered backing file");
        reserved = bytes + Alignment;
        reservation = mmap(nullptr, reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (reservation == MAP_FAILED) throw std::runtime_error("cannot reserve the scattered guest range");
        auto cursor = (reinterpret_cast<std::uintptr_t>(reservation) + Alignment - 1) / Alignment * Alignment;
        base = reinterpret_cast<std::uint8_t*>(cursor);
        for (const auto& run : runs) {
            if (mmap(reinterpret_cast<void*>(cursor), run.bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, file, static_cast<off_t>(run.offset)) == MAP_FAILED) throw std::runtime_error("cannot map a scattered run");
            cursor += run.bytes;
        }
        GuestAllocations::Mutation mutation;
        mutation.Add(base, static_cast<std::size_t>(bytes), true, true, true);
    }

    std::uint64_t Bytes() const {
        std::uint64_t bytes = 0;
        for (const auto& run : runs) bytes += run.bytes;
        return bytes;
    }
};

std::vector<ScatteredRange*> scattered;

bool ResolveRuns(std::uintptr_t address, std::size_t bytes, int* file, GuestArena::SharedBackingRun* runs, std::size_t capacity, std::size_t* count) {
    *count = 0;
    for (const auto* range : scattered) {
        const auto begin = reinterpret_cast<std::uintptr_t>(range->base);
        if (address < begin || address + bytes > begin + range->Bytes()) continue;
        auto cursor = begin;
        for (const auto& run : range->runs) {
            const auto first = std::max<std::uintptr_t>(cursor, address);
            const auto last = std::min<std::uintptr_t>(cursor + run.bytes, address + bytes);
            if (first < last) {
                if (*count == capacity) return false;
                runs[(*count)++] = GuestArena::SharedBackingRun{run.offset + (first - cursor), static_cast<std::size_t>(last - first)};
            }
            cursor += run.bytes;
        }
        *file = dup(range->file);
        lastBackingFile = *file;
        return *file >= 0;
    }
    std::uint64_t offset = 0;
    if (!Resolve(address, bytes, file, &offset)) return false;
    runs[0] = GuestArena::SharedBackingRun{offset, bytes};
    *count = 1;
    return true;
}

void ScatteredRunsAboveTheLimitBindTheirChunks() {
    const auto total = RangeBytes();
    const auto first = total / 2 / Alignment * Alignment;
    const auto second = total - first;
    const std::uint64_t firstOffset = 3 * ChunkBytes + 3 * Alignment;
    const std::uint64_t secondOffset = 5 * Alignment;
    ScatteredRange range({{firstOffset, static_cast<std::size_t>(first)}, {secondOffset, static_cast<std::size_t>(second)}});
    scattered.push_back(&range);
    const auto context = mockContext();
    mock.binds.clear();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* import = HostImportFor(context, reinterpret_cast<std::uint64_t>(range.base), static_cast<std::size_t>(total));
    ExpectBackingClosed("a scattered sparse import retained the backing descriptor");
    Expect(import != nullptr, "a range of two scattered runs larger than udmabuf's size limit was not imported");
    if (import != nullptr) {
        Expect(import->dmaBuf && mock.sparse.contains(import->buffer) && import->lead == 0, "the scattered range was not imported as one sparse buffer");
        VkDeviceSize resource = 0;
        bool mapped = true;
        for (const auto& bind : mock.binds) {
            const auto file = resource < first ? firstOffset + resource : secondOffset + (resource - first);
            mapped = mapped && bind.resourceOffset == resource && bind.memoryOffset == file % ChunkBytes && bind.memoryOffset + bind.size <= ChunkBytes && (resource >= first || resource + bind.size <= first);
            resource += bind.size;
        }
        Expect(mapped, "a bind does not map the chunk of its run's file offset to its place in the range");
        Expect(resource == total, "the binds do not cover the scattered range exactly");
    }
    std::erase(scattered, &range);
}

void UnalignedInteriorRunKeepsTheFallback() {
    const auto total = RangeBytes();
    const auto first = (total / 2 / (2 * Alignment)) * (2 * Alignment) + Alignment;
    const auto second = total - first;
    ScatteredRange range({{3 * ChunkBytes, static_cast<std::size_t>(first)}, {0, static_cast<std::size_t>(second)}});
    scattered.push_back(&range);
    const auto context = mockContext();
    mock.binds.clear();
    mock.sparseAlignment = 2 * Alignment;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* import = HostImportFor(context, reinterpret_cast<std::uint64_t>(range.base), static_cast<std::size_t>(total));
        Expect(import == nullptr || !mock.sparse.contains(import->buffer), "a run list whose interior run is off the sparse block size took the sparse path");
        Expect(mock.binds.empty(), "a chunk was bound for a run list that does not take the sparse path");
    }
    mock.sparseAlignment = Alignment;
    std::erase(scattered, &range);
}

void UnalignedOffsetStartsAtTheBlockBelow() {
    const auto bytes = RangeBytes();
    constexpr std::uint64_t lead = 4096;
    SharedRange range(bytes, lead, true);
    const auto context = mockContext();
    mock.binds.clear();
    const auto address = mock.nextAddress;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* import = HostImportFor(context, range.Address(), static_cast<std::size_t>(bytes));
    Expect(import != nullptr, "a range whose backing offset is not 64 KiB aligned was not imported above udmabuf's size limit");
    if (import == nullptr) return;
    Expect(mock.sparse.contains(import->buffer) && import->lead == lead, "the import does not start its sparse buffer at the 64 KiB block below the range");
    Expect(import->address == address + lead && HostImportOffset(*import, range.Address()) == lead, "the import's device address or buffer offset does not point at the range itself");
    Expect(!mock.binds.empty() && mock.binds.front().resourceOffset == 0 && mock.binds.front().memoryOffset == 0, "the first bind does not map the block below the range");
    VkDeviceSize bound = 0;
    for (const auto& bind : mock.binds) bound += bind.size;
    Expect(bound >= lead + bytes, "the binds do not cover the range after its lead");
}

void LargerSparseBlockIsDecidedBeforeImporting() {
    const auto bytes = RangeBytes();
    SharedRange range(bytes, Alignment, true);
    const auto context = mockContext();
    mock.binds.clear();
    mock.sparseAlignment = 2 * Alignment;
    const auto memories = mock.memories.size();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* import = HostImportFor(context, range.Address(), static_cast<std::size_t>(bytes));
    mock.sparseAlignment = Alignment;
    Expect(import == nullptr, "a range whose backing offset is not on the sparse block size was imported above udmabuf's size limit");
    Expect(mock.memories.size() == memories && mock.binds.empty(), "a chunk was imported or bound for a range that does not take the sparse path");
}

void RetainsFailedWindowAcrossUnrelatedMutation() {
    constexpr std::uint64_t window = 4ull << 20u;
    SharedRange range(window * 2, 0, true);
    const auto context = mockContext();
    SetHostImportRangeWindow(window);
    mock.allocationResult = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    const auto address = range.Address() + window;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto attempts = mock.allocationAttempts;
    Expect(HostImportFor(context, address, Alignment) == nullptr, "the failed import window was accepted");
    const auto failedAttempts = mock.allocationAttempts;
    Expect(failedAttempts > attempts, "the import window did not reach allocation");
    Expect(HostImportFor(context, address, Alignment) == nullptr && mock.allocationAttempts == failedAttempts, "an unchanged failed window was retried");
    SharedRange unrelated(Alignment, 0, true);
    Expect(HostImportFor(context, address, Alignment) == nullptr && mock.allocationAttempts == failedAttempts, "an unrelated registry change retried a failed import window");
    Expect(HostImportFor(context, range.Address(), Alignment) == nullptr && mock.allocationAttempts > failedAttempts, "a different import window inherited the first window's failure");
    const auto beforeRemap = mock.allocationAttempts;
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(range.backing.base);
        mutation.Add(range.backing.base, static_cast<std::size_t>(range.backing.bytes), true, true, true);
    }
    mock.allocationResult = VK_SUCCESS;
    const auto* imported = HostImportFor(context, address, Alignment);
    Expect(imported != nullptr && imported->base == address && imported->bytes == window && mock.allocationAttempts > beforeRemap, "a remapped failed import window was not retried successfully");
    SetHostImportRangeWindow(64ull << 20u);
}

void FailureAfterTheDecisionThrows(bool sealed, VkResult bind, const char* step) {
    const auto bytes = RangeBytes();
    SharedRange range(bytes, 0, sealed);
    const auto context = mockContext();
    mock.bindResult = bind;
    const auto live = mock.buffers.size();
    std::string thrown;
    try {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        HostImportFor(context, range.Address(), static_cast<std::size_t>(bytes));
    } catch (const std::runtime_error& error) {
        thrown = error.what();
    }
    mock.bindResult = VK_SUCCESS;
    ExpectBackingClosed("a throwing sparse import retained the backing descriptor");
    Expect(thrown.find("sparse dma-buf import") != std::string::npos && thrown.find(step) != std::string::npos, std::string("a failure at ") + step + " after the sparse path was chosen did not throw (got \"" + thrown + "\")");
    Expect(mock.buffers.size() == live, std::string("the sparse buffer outlived the failure at ") + step);
}

}

int main() {
    const int udmabuf = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
    if (udmabuf < 0) {
        std::printf("skipped, host imports are unavailable: /dev/udmabuf is not accessible\n");
        return 77;
    }
    close(udmabuf);
    if (UdmabufLimitBytes() > LargestTestedLimit) {
        std::printf("skipped, udmabuf size_limit_mb is above %llu: a range above it is too large to map\n", static_cast<unsigned long long>(LargestTestedLimit >> 20u));
        return 77;
    }
    setenv("APS5_WRITE_WATCH_IMPORTS", "unwatch", 1);
    SetHostImportBudget(~std::uint64_t{0});
    GuestArena::GuestArenaSetSharedBacking_nid_postfix(Resolve);
    GuestArena::GuestArenaSetSharedBackingRuns_nid_postfix(ResolveRuns);
    try {
        SettlingSurvivesDeviceReplacement(false);
        SettlingSurvivesDeviceReplacement(true);
        ImportsALargeRangeAsOneSparseBuffer();
        UnalignedOffsetStartsAtTheBlockBelow();
        LargerSparseBlockIsDecidedBeforeImporting();
        ScatteredRunsAboveTheLimitBindTheirChunks();
        UnalignedInteriorRunKeepsTheFallback();
        FailureAfterTheDecisionThrows(false, VK_SUCCESS, "UDMABUF_CREATE");
        FailureAfterTheDecisionThrows(true, VK_ERROR_DEVICE_LOST, "vkQueueBindSparse");
        RetainsFailedWindowAcrossUnrelatedMutation();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) {
        std::fprintf(stderr, "%d sparse dma-buf import checks failed\n", failures);
        return 1;
    }
    std::printf("host import sparse dma-buf tests passed\n");
    return 0;
}
#endif
