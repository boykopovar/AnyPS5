#include "HostImportFailure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;

enum class Step { None, Create, Allocate, Bind };

struct Probe {
    PFN_vkGetDeviceProcAddr backend = nullptr;
    Step fail = Step::None;
    VkResult result = VK_SUCCESS;
    unsigned injected = 0;
    unsigned invalid = 0;
    std::set<VkBuffer> buffers;
    std::set<VkDeviceMemory> memories;
};

Probe probe;

VKAPI_ATTR VkResult VKAPI_CALL CreateBuffer(VkDevice device, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer) {
    if (probe.fail == Step::Create) {
        probe.fail = Step::None;
        ++probe.injected;
        *buffer = reinterpret_cast<VkBuffer>(std::uintptr_t{0xdeadbeef});
        return probe.result;
    }
    const auto result = reinterpret_cast<PFN_vkCreateBuffer>(probe.backend(device, "vkCreateBuffer"))(device, info, allocator, buffer);
    if (result == VK_SUCCESS) probe.buffers.insert(*buffer);
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL AllocateMemory(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
    if (probe.fail == Step::Allocate) {
        probe.fail = Step::None;
        ++probe.injected;
        *memory = reinterpret_cast<VkDeviceMemory>(std::uintptr_t{0xdeadbeef});
        return probe.result;
    }
    const auto result = reinterpret_cast<PFN_vkAllocateMemory>(probe.backend(device, "vkAllocateMemory"))(device, info, allocator, memory);
    if (result == VK_SUCCESS) probe.memories.insert(*memory);
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    if (probe.fail == Step::Bind) {
        probe.fail = Step::None;
        ++probe.injected;
        return probe.result;
    }
    return reinterpret_cast<PFN_vkBindBufferMemory>(probe.backend(device, "vkBindBufferMemory"))(device, buffer, memory, offset);
}

VKAPI_ATTR void VKAPI_CALL DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
    if (probe.buffers.erase(buffer) != 1) {
        ++probe.invalid;
        return;
    }
    reinterpret_cast<PFN_vkDestroyBuffer>(probe.backend(device, "vkDestroyBuffer"))(device, buffer, allocator);
}

VKAPI_ATTR void VKAPI_CALL FreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    if (probe.memories.erase(memory) != 1) {
        ++probe.invalid;
        return;
    }
    reinterpret_cast<PFN_vkFreeMemory>(probe.backend(device, "vkFreeMemory"))(device, memory, allocator);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkCreateBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateBuffer);
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(AllocateMemory);
    if (std::strcmp(name, "vkBindBufferMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(BindBufferMemory);
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyBuffer);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(FreeMemory);
    return probe.backend(device, name);
}

constexpr std::size_t Alignment = 65536;
constexpr std::size_t Bytes = 4096;

class Heap {
public:
    Heap() {
#ifdef _WIN32
        data = static_cast<std::byte*>(VirtualAlloc(nullptr, Alignment, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::byte*>(std::aligned_alloc(Alignment, Alignment));
#endif
        Require(data != nullptr, "cannot allocate the host import failure heap");
        std::memset(data, 0x37, Alignment);
        GuestAllocations::Mutation().Add(data, Alignment, true, true, true);
    }

    ~Heap() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }

    std::byte* Data() const { return data; }
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }

private:
    std::byte* data = nullptr;
};

#ifdef _WIN32
void PartialResidentAliasRefused(const Context& context, bool left) {
    constexpr std::size_t allocationBytes = 4 * Alignment;
    auto* data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(allocationBytes, 0x200000));
    Require(data != nullptr, "cannot reserve the partial resident alias fixture");
    struct Cleanup {
        std::byte* data;
        VkDevice device;
        HANDLE section = nullptr;
        void* registered = nullptr;
        ~Cleanup() {
            ClearHostImports(device);
            if (registered != nullptr) GuestAllocations::Mutation().Remove(registered);
            GuestArena::GuestArenaReset_nid_postfix(data, allocationBytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, allocationBytes);
            if (section != nullptr) {
                GuestArena::GuestArenaForgetSection_nid_postfix(section);
                CloseHandle(section);
            }
        }
    } cleanup{data, context.device};
    cleanup.section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, allocationBytes, nullptr);
    Require(cleanup.section != nullptr, "cannot reserve the partial resident alias backing");
    ClearHostImports(context.device);
    const auto sharedOffset = left ? 3 * Alignment : 0;
    const auto residentOffset = left ? 0 : Alignment;
    GuestArena::GuestArenaMap_nid_postfix(data + sharedOffset, Alignment, cleanup.section, sharedOffset, PAGE_READWRITE);
    GuestArena::GuestArenaMapDirect_nid_postfix(data + residentOffset, 3 * Alignment, cleanup.section, residentOffset, PAGE_READWRITE);
    std::memset(data, 0x37, allocationBytes);
    auto* requested = data + (left ? Alignment : 0);
    const auto address = reinterpret_cast<std::uintptr_t>(requested);
    bool refused = false;
    try {
        auto* alias = GuestArena::GuestArenaMapAlias_nid_postfix(address, 3 * Alignment);
        GuestArena::GuestArenaUnmapAlias_nid_postfix(alias);
    } catch (const std::runtime_error& error) {
        refused = std::string_view(error.what()).find("a resident run of direct memory extends outside the range") != std::string_view::npos;
    }
    Require(refused, "partial resident fixture did not reproduce the alias bounds refusal");
    GuestAllocations::Mutation().Add(requested, 3 * Alignment, true, true, true);
    cleanup.registered = requested;
    Require(HostImportFor(context, address, Bytes) == nullptr, "partial resident alias did not fall back");
    Require(HostImportFor(context, address, Bytes) == nullptr && HostImportLiveBytes() == 0, "partial resident alias refusal left a live import");
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    Require(GuestArena::GuestArenaResidentSpan_nid_postfix(address, 3 * Alignment, &begin, &end) && begin == reinterpret_cast<std::uintptr_t>(data + residentOffset) && end == begin + 3 * Alignment, "refused alias changed the resident run");
    GuestAllocations::Mutation().Remove(requested);
    cleanup.registered = nullptr;
    GuestAllocations::Mutation().Add(data, allocationBytes, true, true, true);
    cleanup.registered = data;
    const auto* complete = HostImportFor(context, reinterpret_cast<std::uintptr_t>(data), Bytes);
    Require(complete != nullptr && complete->alias != nullptr && complete->bytes == allocationBytes, "a complete resident run did not import after the partial refusal");
    Require(std::all_of(data, data + allocationBytes, [](std::byte value) { return value == std::byte{0x37}; }), "alias refusal or retry changed guest bytes");
    std::printf("partial resident alias %s: refused without a live import, complete run imported, guest bytes preserved\n", left ? "left" : "right");
}
#endif

}

void AgcDriver::Tests::RunHostImportFailureTests(const Graphics::Context& original, bool native) {
#ifdef _WIN32
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        ClearHostImports(original.device);
        for (const bool left : {false, true}) PartialResidentAliasRefused(original, left);
    }
#endif
    unsigned failures = 0;
    for (const auto step : {Step::Create, Step::Allocate, Step::Bind}) {
        for (const auto result : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) {
            Heap heap;
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            ClearHostImports(original.device);
            probe = {};
            probe.backend = original.deviceProc;
            probe.fail = step;
            probe.result = result;
            auto context = original;
            context.deviceProc = DeviceProc;
            struct Cleanup {
                VkDevice device;
                ~Cleanup() { ClearHostImports(device); }
            } cleanup{context.device};
            SetImportWatch(context, ImportWatch::Unwatch);
            const HostImport* failed = nullptr;
            bool threw = false;
            try {
                failed = HostImportFor(context, heap.Address(), Bytes);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            const bool rejected = failed == nullptr && probe.injected == 1 && ExistingHostImportFor(context, heap.Address(), Bytes) == nullptr && HostImportLiveBytes() == 0;
            const bool released = probe.buffers.empty() && probe.memories.empty();
            const auto invalid = probe.invalid;
            ClearHostImports(context.device);
            probe.fail = Step::None;
            const auto* recovered = HostImportFor(context, heap.Address(), Bytes);
            Require(recovered != nullptr && recovered->buffer != VK_NULL_HANDLE && recovered->memory != VK_NULL_HANDLE && recovered->address != 0, "host import retry failed");
            if (native) {
                Buffer readback(original, Bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                std::memset(readback.Bytes().data(), 0, Bytes);
                CommandBatch batch(original);
                RecordMemoryBarrier(original, batch.Handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                CopyBuffer(original, batch.Handle(), recovered->buffer, heap.Address() - recovered->base, readback.Handle(), 0, Bytes);
                RecordMemoryBarrier(original, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
                batch.SubmitAndWait();
                readback.Invalidate();
                Require(std::memcmp(heap.Data(), readback.Bytes().data(), Bytes) == 0, "host import retry returned incorrect GPU readback data");
            }
            ClearHostImports(context.device);
            const bool clean = probe.buffers.empty() && probe.memories.empty() && HostImportLiveBytes() == 0;
            std::printf("host import step=%u result=%d injected=%u rejected=%u threw=%u invalid releases=%u failure cleaned=%u retry cleaned=%u readbacks=%u\n", static_cast<unsigned>(step), static_cast<int>(result), probe.injected, rejected, threw, invalid, released, clean, native ? 1u : 0u);
            if (!rejected || !released || !clean || invalid != 0 || probe.invalid != invalid) ++failures;
        }
    }
    Require(failures == 0, "failed host import creation released an undefined handle or leaked a resource");
}
