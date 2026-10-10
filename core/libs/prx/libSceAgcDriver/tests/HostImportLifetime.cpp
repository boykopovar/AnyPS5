#include "HostImportLifetime.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::GuestMemory::GpuMutex;

struct ImportState {
    VkBuffer buffer;
    VkDeviceMemory memory;
    std::uint64_t address;
    std::size_t bytes;
    std::atomic<bool> destroyed{false};
    std::atomic<bool> freed{false};
    std::atomic<unsigned> frees{0};
    std::atomic<bool> outOfOrder{false};
    std::atomic<bool> inaccessible{false};
    std::mutex mutex;
    std::condition_variable wake;
    bool blockFree = false;
    bool freeing = false;
    bool allowFree = false;
    std::thread::id freeingThread;
};

struct Tracking {
    PFN_vkGetDeviceProcAddr resolve;
    VkDevice device;
    std::mutex mutex;
    std::map<VkDeviceMemory, std::shared_ptr<ImportState>> imports;
    std::atomic<bool> failAddressLookup{false};
    std::atomic<bool> failQueueSubmit{false};
    std::atomic<unsigned> rejectedSubmissions{0};
    std::atomic<unsigned> acceptedSubmissions{0};
    std::vector<VkFence> rejectedFences;
    std::shared_ptr<ImportState> lastAllocated;

    void CleanupRejectedFences() {
        std::vector<VkFence> fences;
        {
            std::lock_guard lock(mutex);
            fences.swap(rejectedFences);
        }
        const auto destroy = reinterpret_cast<PFN_vkDestroyFence>(resolve(device, "vkDestroyFence"));
        for (const auto fence : fences) destroy(device, fence, nullptr);
    }

    std::shared_ptr<ImportState> Add(const HostImport& import) {
        auto state = std::make_shared<ImportState>();
        state->buffer = import.buffer;
        state->memory = import.memory;
        state->address = import.base;
        state->bytes = import.bytes;
        std::lock_guard lock(mutex);
        imports[import.memory] = state;
        return state;
    }
};

Tracking* tracking = nullptr;

VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
    const auto original = reinterpret_cast<PFN_vkDestroyBuffer>(tracking->resolve(device, "vkDestroyBuffer"));
    original(device, buffer, allocator);
    std::lock_guard lock(tracking->mutex);
    for (const auto& [memory, state] : tracking->imports) {
        if (state->buffer == buffer && !state->freed.load()) state->destroyed.store(true);
    }
}

VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    std::shared_ptr<ImportState> state;
    {
        std::lock_guard lock(tracking->mutex);
        const auto found = tracking->imports.find(memory);
        if (found != tracking->imports.end()) state = found->second;
    }
    if (state != nullptr) {
        if (!state->destroyed.load()) state->outOfOrder.store(true);
        if (!RegisteredReadableCovers(state->address, state->bytes) || !AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(state->address), state->bytes, false)) state->inaccessible.store(true);
        std::unique_lock lock(state->mutex);
        if (state->blockFree) {
            state->freeing = true;
            state->freeingThread = std::this_thread::get_id();
            state->wake.notify_all();
            if (!state->wake.wait_for(lock, std::chrono::seconds(2), [&] { return state->allowFree; })) state->outOfOrder.store(true);
        }
    }
    const auto original = reinterpret_cast<PFN_vkFreeMemory>(tracking->resolve(device, "vkFreeMemory"));
    original(device, memory, allocator);
    if (state != nullptr) {
        if (!RegisteredReadableCovers(state->address, state->bytes) || !AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(state->address), state->bytes, false)) state->inaccessible.store(true);
        state->frees.fetch_add(1);
        state->freed.store(true);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL allocateMemory(VkDevice device, const VkMemoryAllocateInfo* allocation, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
    const auto original = reinterpret_cast<PFN_vkAllocateMemory>(tracking->resolve(device, "vkAllocateMemory"));
    const auto result = original(device, allocation, allocator, memory);
    if (result != VK_SUCCESS) return result;
    for (auto* next = static_cast<const VkBaseInStructure*>(allocation->pNext); next != nullptr; next = next->pNext) {
        if (next->sType != VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT) continue;
        const auto* host = reinterpret_cast<const VkImportMemoryHostPointerInfoEXT*>(next);
        auto state = std::make_shared<ImportState>();
        state->buffer = VK_NULL_HANDLE;
        state->memory = *memory;
        state->address = reinterpret_cast<std::uint64_t>(host->pHostPointer);
        state->bytes = static_cast<std::size_t>(allocation->allocationSize);
        std::lock_guard lock(tracking->mutex);
        tracking->imports[*memory] = state;
        tracking->lastAllocated = state;
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL bindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    {
        std::lock_guard lock(tracking->mutex);
        if (const auto found = tracking->imports.find(memory); found != tracking->imports.end()) found->second->buffer = buffer;
    }
    const auto original = reinterpret_cast<PFN_vkBindBufferMemory>(tracking->resolve(device, "vkBindBufferMemory"));
    return original(device, buffer, memory, offset);
}

VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator) {
    const auto original = reinterpret_cast<PFN_vkDestroyFence>(tracking->resolve(device, "vkDestroyFence"));
    original(device, fence, allocator);
    std::lock_guard lock(tracking->mutex);
    std::erase(tracking->rejectedFences, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL queueSubmit(VkQueue queue, std::uint32_t count, const VkSubmitInfo* submissions, VkFence fence) {
    if (tracking->failQueueSubmit.exchange(false)) {
        std::lock_guard lock(tracking->mutex);
        tracking->rejectedFences.push_back(fence);
        tracking->rejectedSubmissions.fetch_add(1);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    const auto original = reinterpret_cast<PFN_vkQueueSubmit>(tracking->resolve(tracking->device, "vkQueueSubmit"));
    const auto result = original(queue, count, submissions, fence);
    if (result == VK_SUCCESS) {
        std::lock_guard lock(tracking->mutex);
        std::erase(tracking->rejectedFences, fence);
        tracking->acceptedSubmissions.fetch_add(1);
    }
    return result;
}

PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkDestroyFence") == 0) return reinterpret_cast<PFN_vkVoidFunction>(destroyFence);
    if (std::strcmp(name, "vkQueueSubmit") == 0) return reinterpret_cast<PFN_vkVoidFunction>(queueSubmit);
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(destroyBuffer);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(freeMemory);
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(allocateMemory);
    if (std::strcmp(name, "vkBindBufferMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(bindBufferMemory);
    if (std::strcmp(name, "vkGetBufferDeviceAddressKHR") == 0 && tracking->failAddressLookup.exchange(false)) return nullptr;
    return tracking->resolve(device, name);
}

void requireFreed(const std::shared_ptr<ImportState>& state) {
    Require(state->freed.load(), "guest mutation reached host apply before vkFreeMemory completed");
    Require(state->destroyed.load() && !state->outOfOrder.load() && state->frees.load() == 1, "host import Vulkan objects were destroyed out of order or more than once");
    Require(!state->inaccessible.load(), "guest pages became inaccessible before vkFreeMemory returned");
}

class Fixture {
public:
    explicit Fixture(const Context& original) : context(original), calls{original.deviceProc, original.device} {
        Require(tracking == nullptr, "host import tracking is already active");
        tracking = &calls;
        context.deviceProc = deviceProc;
        context.functions = nullptr;
        std::lock_guard gpu(GpuMutex());
        recorder = std::make_unique<Recorder>(context);
        recorder->Activate();
    }

    ~Fixture() {
        std::lock_guard gpu(GpuMutex());
        recorder.reset();
        calls.CleanupRejectedFences();
        ClearHostImports(context.device);
        context.bufferPool.reset();
        tracking = nullptr;
    }

    std::shared_ptr<ImportState> Import(std::uint64_t address, std::size_t bytes) {
        std::lock_guard gpu(GpuMutex());
        const auto* imported = HostImportFor(context, address, bytes);
        Require(imported != nullptr, "host pointer import was refused for the lifetime test");
        return calls.Add(*imported);
    }

    void Cleanup() {
        std::lock_guard gpu(GpuMutex());
        recorder->Sync();
        ClearHostImports(context.device);
    }

    Context context;
    Tracking calls;
    std::unique_ptr<Recorder> recorder;
};

class Allocation {
public:
    Allocation(Fixture& fixture, std::size_t units = 1) : fixture(fixture) {
        alignment = static_cast<std::size_t>(std::max<VkDeviceSize>(65536, fixture.context.hostImportAlignment));
        bytes = alignment * units;
        data = ::operator new(bytes, std::align_val_t{alignment});
        std::memset(data, 0x37, bytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }

    ~Allocation() {
        try {
            if (registered) {
                fixture.Cleanup();
                GuestAllocations::Mutation mutation;
                mutation.Remove(data);
            }
            Release();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "host import test cleanup failed: %s\n", error.what());
            std::terminate();
        }
    }

    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(data); }

    void Release() {
        if (released) return;
        ::operator delete(data, std::align_val_t{alignment});
        released = true;
    }

    Fixture& fixture;
    void* data = nullptr;
    std::size_t alignment = 0;
    std::size_t bytes = 0;
    bool registered = true;
    bool released = false;
};

void descriptorFirstImport(Fixture& fixture) {
    Require(fixture.calls.lastAllocated == nullptr, "descriptor-only lifetime test did not run before the first host import");
    Allocation allocation(fixture);
    std::shared_ptr<ImportState> imported;
    {
        std::lock_guard gpu(GpuMutex());
        GuestBufferMemory memory(fixture.context);
        memory.AddReadable(allocation.Address(), allocation.bytes);
        memory.Upload(false);
        imported = fixture.calls.lastAllocated;
        Require(imported != nullptr && imported->address == allocation.Address(), "descriptor-only upload did not import its allocation");
        std::uint32_t adjustment = 0;
        const auto descriptor = memory.Descriptor(allocation.Address(), allocation.bytes, adjustment);
        Require(descriptor.buffer == imported->buffer && descriptor.offset == 0 && adjustment == 0, "descriptor-only upload did not bind the tracked import");
    }
    Require(HostImportCovers(fixture.context, allocation.Address(), allocation.bytes), "descriptor-only import did not persist after its build ended");
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(allocation.data, allocation.bytes, [&](const void*, std::size_t, const void*, bool) {
            requireFreed(imported);
            allocation.Release();
        });
        allocation.registered = false;
    }
    Require(!HostImportCovers(fixture.context, allocation.Address(), allocation.bytes), "descriptor-only import survived allocation removal");
}

class PendingRetirement {
public:
    explicit PendingRetirement(Fixture& fixture) : fixture(fixture), first(fixture) {
        std::lock_guard gpu(GpuMutex());
        build = std::make_unique<GuestBufferMemory>(fixture.context);
        build->AcquireRegistered();
        build->UploadPrepare(true);
        retry = std::make_unique<GuestBufferMemory>(fixture.context);
        retry->AcquireRegistered();
        retry->UploadPrepare(true);
        allocation = std::make_unique<Allocation>(fixture);
        imported = fixture.Import(allocation->Address(), allocation->bytes);
        result = std::make_unique<Buffer>(fixture.context, allocation->bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = fixture.recorder->Commands();
        RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        CopyBuffer(fixture.context, commands, imported->buffer, 0, result->Handle(), 0, allocation->bytes);
        RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        fixture.recorder->NotePendingRead(allocation->Address(), allocation->bytes, Recorder::ReadKind::AddressBased);
        fixture.recorder->Submit();
        Check(fixture.context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(fixture.context.queue), "vkQueueWaitIdle retirement fixture");
        generation = std::make_unique<Allocation>(fixture);
        Require(!fixture.recorder->Idle(), "retirement fixture has no submitted batch");
    }

    ~PendingRetirement() {
        if (imported != nullptr) {
            {
                std::lock_guard lock(imported->mutex);
                imported->allowFree = true;
            }
            imported->wake.notify_all();
        }
        build.reset();
        retry.reset();
        fixture.Cleanup();
    }

    void VerifyCopy() {
        result->Invalidate();
        const auto bytes = result->Bytes();
        Require(std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0x37}; }), "retirement lost the submitted import read");
    }

    Fixture& fixture;
    Allocation first;
    std::unique_ptr<Buffer> result;
    std::unique_ptr<Allocation> allocation;
    std::unique_ptr<Allocation> generation;
    std::unique_ptr<GuestBufferMemory> build;
    std::unique_ptr<GuestBufferMemory> retry;
    std::shared_ptr<ImportState> imported;
};

void deferredRetirement(Fixture& fixture) {
    PendingRetirement pending(fixture);
    pending.imported->blockFree = true;
    {
        std::lock_guard gpu(GpuMutex());
        pending.build->UploadFinish(true);
        Require(!HostImportCovers(fixture.context, pending.allocation->Address(), pending.allocation->bytes), "deferred retirement left its import cached");
        Require(!pending.imported->freed.load(), "retirement did not keep its import through the pending batch");
        fixture.recorder->Sync();
    }
    bool entered = false;
    bool onReleaseThread = false;
    {
        std::unique_lock lock(pending.imported->mutex);
        entered = pending.imported->wake.wait_for(lock, std::chrono::seconds(2), [&] { return pending.imported->freeing; });
        onReleaseThread = pending.imported->freeingThread != std::this_thread::get_id();
    }
    std::atomic<bool> applied{false};
    const auto waitsBefore = LeaseCounters().contentionWaits;
    std::promise<void> starting;
    auto started = starting.get_future();
    auto mutation = std::async(std::launch::async, [&] {
        starting.set_value();
        GuestAllocations::Mutation mutation;
        mutation.Unmap(pending.allocation->data, pending.allocation->bytes, [&](const void*, std::size_t, const void*, bool) {
            requireFreed(pending.imported);
            applied.store(true);
            pending.allocation->Release();
        });
        pending.allocation->registered = false;
    });
    started.wait();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (LeaseCounters().contentionWaits == waitsBefore && mutation.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    const bool reachedWaiter = LeaseCounters().contentionWaits > waitsBefore;
    const bool blocked = mutation.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout;
    const bool premature = applied.load();
    const bool registered = RegisteredReadableCovers(pending.allocation->Address(), pending.allocation->bytes);
    {
        std::lock_guard lock(pending.imported->mutex);
        pending.imported->allowFree = true;
    }
    pending.imported->wake.notify_all();
    mutation.get();
    Require(entered && onReleaseThread, "retired import destruction did not run on the recorder release thread");
    Require(reachedWaiter && blocked && !premature && registered, "guest mutation passed a retired import whose Vulkan free was still running");
    Require(applied.load(), "guest mutation did not resume after deferred Vulkan free");
    requireFreed(pending.imported);
    pending.VerifyCopy();
}

void idleUnmap(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(allocation.data, allocation.bytes, [&](const void*, std::size_t, const void*, bool) {
            requireFreed(imported);
            allocation.Release();
        });
        allocation.registered = false;
    }
    Require(!HostImportCovers(fixture.context, allocation.Address(), allocation.bytes), "an unmapped range still has a live host import");
}

void pendingRead(Fixture& fixture, bool submitted, bool locked) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    Buffer result(fixture.context, allocation.bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::atomic<bool> completed{false};
    struct Finish {
        Fixture& fixture;
        ~Finish() { fixture.Cleanup(); }
    } finish{fixture};
    std::unique_lock gpu(GpuMutex());
    const auto commands = fixture.recorder->Commands();
    RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    CopyBuffer(fixture.context, commands, imported->buffer, 0, result.Handle(), 0, allocation.bytes);
    RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    fixture.recorder->NotePendingRead(allocation.Address(), allocation.bytes, Recorder::ReadKind::AddressBased);
    fixture.recorder->Keep(std::make_shared<GuestAllocations::Lease>(GuestAllocations::GuestAllocationsAcquire_nid_postfix()));
    fixture.recorder->OnComplete([&] { completed.store(true); });
    if (submitted) fixture.recorder->Submit();
    if (!locked) gpu.unlock();
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(allocation.data, allocation.bytes, [&](const void*, std::size_t, const void*, bool) {
            requireFreed(imported);
            Require(completed.load(), "guest unmap passed a pending GPU read");
            result.Invalidate();
            const auto copied = result.Bytes();
            Require(std::all_of(copied.begin(), copied.end(), [](std::byte value) { return value == std::byte{0x37}; }), "GPU read lost the source contents before guest unmap");
            allocation.Release();
        });
        allocation.registered = false;
    }
}

void productionWaiterFailure(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    Buffer result(fixture.context, allocation.bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    bool completed = false;
    struct Finish {
        Fixture& fixture;
        ~Finish() { fixture.Cleanup(); }
    } finish{fixture};
    const auto recordCopy = [&] {
        const auto commands = fixture.recorder->Commands();
        RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        CopyBuffer(fixture.context, commands, imported->buffer, 0, result.Handle(), 0, allocation.bytes);
        RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        fixture.recorder->NotePendingRead(allocation.Address(), allocation.bytes, Recorder::ReadKind::AddressBased);
        fixture.recorder->Keep(std::make_shared<GuestAllocations::Lease>(GuestAllocations::GuestAllocationsAcquire_nid_postfix()));
    };
    {
        std::lock_guard gpu(GpuMutex());
        recordCopy();
    }
    const auto rejectedBefore = fixture.calls.rejectedSubmissions.load();
    const auto acceptedBefore = fixture.calls.acceptedSubmissions.load();
    fixture.calls.failQueueSubmit.store(true);
    bool applied = false;
    std::string failure;
    {
        GuestAllocations::Mutation mutation;
        try {
            mutation.Protect(allocation.data, allocation.bytes, true, true, true, [&] { applied = true; });
        } catch (const std::runtime_error& error) {
            failure = error.what();
        }
        const auto range = mutation.Find(allocation.data);
        Require(range.readable && range.writable && range.gpu, "failed production waiter changed the guest registration");
    }
    Require(!fixture.calls.failQueueSubmit.load() && fixture.calls.rejectedSubmissions.load() == rejectedBefore + 1, "production pin waiter did not reach the injected queue submission failure");
    Require(fixture.calls.acceptedSubmissions.load() == acceptedBefore, "failed production waiter accepted GPU work before reporting failure");
    Require(failure == "AGC graphics: vkQueueSubmit recorder: Vulkan result -1" && !applied, "production pin waiter did not propagate the original Vulkan error before host apply");
    Require(!imported->destroyed.load() && !imported->freed.load(), "failed production waiter retired its host import");
    Require(HostImportCovers(fixture.context, allocation.Address(), allocation.bytes), "failed production waiter removed the live import");
    {
        std::lock_guard gpu(GpuMutex());
        recordCopy();
        fixture.recorder->OnComplete([&] { completed = true; });
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(allocation.data, allocation.bytes, [&](const void*, std::size_t, const void*, bool) {
            requireFreed(imported);
            Require(completed, "production waiter retry passed a pending GPU read");
            result.Invalidate();
            const auto copied = result.Bytes();
            Require(std::all_of(copied.begin(), copied.end(), [](std::byte value) { return value == std::byte{0x37}; }), "production waiter retry lost its real GPU copy");
            allocation.Release();
        });
        allocation.registered = false;
    }
    Require(fixture.calls.acceptedSubmissions.load() == acceptedBefore + 1, "production waiter retry did not submit exactly its new copy");
    requireFreed(imported);
}

void partialUnmap(Fixture& fixture) {
    Allocation allocation(fixture, 3);
    Allocation unrelated(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    const auto other = fixture.Import(unrelated.Address(), unrelated.bytes);
    auto* middle = static_cast<std::byte*>(allocation.data) + allocation.alignment;
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(middle, allocation.alignment, [&](const void* piece, std::size_t bytes, const void* original, bool last) {
            requireFreed(imported);
            Require(!other->freed.load(), "partial unmap retired a disjoint host import");
            Require(piece == middle && bytes == allocation.alignment && original == allocation.data && !last, "partial unmap changed the wrong guest extent");
        });
    }
    Require(!RegisteredReadableCovers(reinterpret_cast<std::uint64_t>(middle), allocation.alignment), "the middle of a partial unmap remains registered");
    const auto low = fixture.Import(allocation.Address(), allocation.alignment);
    const auto high = fixture.Import(allocation.Address() + 2 * allocation.alignment, allocation.alignment);
    Require(!low->freed.load() && !high->freed.load() && !other->freed.load(), "remaining partial mappings could not be imported");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(allocation.data);
        allocation.registered = false;
    }
    requireFreed(low);
    requireFreed(high);
    Require(!other->freed.load(), "removing a split allocation retired a disjoint import");
}

void protectRollback(Fixture& fixture) {
    Allocation allocation(fixture);
    auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    bool called = false;
    try {
        GuestAllocations::Mutation mutation;
        mutation.Protect(allocation.data, allocation.bytes, true, false, true, [&] {
            requireFreed(imported);
            called = true;
            throw std::runtime_error("expected host protection failure");
        });
        throw std::runtime_error("host protection failure was ignored");
    } catch (const std::runtime_error& error) {
        Require(called && std::string(error.what()) == "expected host protection failure", "unexpected protection failure");
    }
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(allocation.data);
        Require(range.readable && range.writable && range.gpu, "failed host protection changed the guest registration");
    }
    imported = fixture.Import(allocation.Address(), allocation.bytes);
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(allocation.data, allocation.bytes, true, false, true, [&] { requireFreed(imported); });
    }
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(allocation.data);
        Require(range.readable && !range.writable && range.gpu, "successful protection did not update the guest registration");
        mutation.Protect(allocation.data, allocation.bytes, true, true, true, [] {});
    }
    imported = fixture.Import(allocation.Address(), allocation.bytes);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(allocation.data);
        allocation.registered = false;
    }
    requireFreed(imported);
}

void blockedFree(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    imported->blockFree = true;
    std::atomic<bool> applied{false};
    auto mutation = std::async(std::launch::async, [&] {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(allocation.data, allocation.bytes, [&](const void*, std::size_t, const void*, bool) {
            requireFreed(imported);
            applied.store(true);
            allocation.Release();
        });
        allocation.registered = false;
    });
    bool entered = false;
    {
        std::unique_lock lock(imported->mutex);
        entered = imported->wake.wait_for(lock, std::chrono::seconds(2), [&] { return imported->freeing; });
    }
    const bool premature = applied.load();
    const bool stillRegistered = RegisteredReadableCovers(allocation.Address(), allocation.bytes);
    {
        std::lock_guard lock(imported->mutex);
        imported->allowFree = true;
    }
    imported->wake.notify_all();
    mutation.get();
    Require(entered, "guest unmap never reached Vulkan import destruction");
    Require(!premature && stillRegistered, "guest unmap released its registration while vkFreeMemory was blocked");
    Require(applied.load(), "guest unmap did not resume after vkFreeMemory returned");
}

void failedConstruction(Fixture& fixture) {
    Allocation allocation(fixture);
    fixture.calls.failAddressLookup.store(true);
    bool failed = false;
    try {
        fixture.Import(allocation.Address(), allocation.bytes);
    } catch (const std::runtime_error& error) {
        failed = std::string(error.what()).find("vkGetBufferDeviceAddressKHR") != std::string::npos;
    }
    Require(failed && !fixture.calls.failAddressLookup.load(), "import construction did not reach the injected Vulkan resolver failure");
    const auto allocated = fixture.calls.lastAllocated;
    Require(allocated != nullptr && allocated->address == allocation.Address(), "resolver failure happened before host memory was imported");
    if (!allocated->freed.load()) {
        destroyBuffer(fixture.context.device, allocated->buffer, nullptr);
        freeMemory(fixture.context.device, allocated->memory, nullptr);
        throw std::runtime_error("failed host import construction leaked its Vulkan memory");
    }
    requireFreed(allocated);
    Require(!HostImportCovers(fixture.context, allocation.Address(), allocation.bytes), "failed host import construction remained cached");
    const auto retried = fixture.Import(allocation.Address(), allocation.bytes);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(allocation.data);
        allocation.registered = false;
    }
    requireFreed(retried);
    Require(allocated->frees.load() == 1, "retry destroyed the failed import a second time");
}

void mutationDuringCompletion(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    Buffer result(fixture.context, allocation.bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    bool rejected = false;
    bool applied = false;
    bool laterCompletion = false;
    bool copied = false;
    std::lock_guard gpu(GpuMutex());
    const auto commands = fixture.recorder->Commands();
    RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    CopyBuffer(fixture.context, commands, imported->buffer, 0, result.Handle(), 0, allocation.bytes);
    RecordMemoryBarrier(fixture.context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    fixture.recorder->OnComplete([&] {
        try {
            GuestAllocations::Mutation mutation;
            mutation.Protect(allocation.data, allocation.bytes, false, false, false, [&] { applied = true; });
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("active GPU completion") != std::string::npos;
        }
    });
    fixture.recorder->OnComplete([&] {
        laterCompletion = true;
        result.Invalidate();
        const auto bytes = result.Bytes();
        copied = std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0x37}; });
    });
    fixture.recorder->Sync();
    Require(rejected && !applied, "an active completion allowed guest memory mutation before later completions");
    Require(laterCompletion && copied, "rejecting a completion mutation lost later GPU results");
    Require(!imported->freed.load(), "rejecting a completion mutation retired its host import");
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(allocation.data);
        Require(range.readable && range.writable && range.gpu, "rejecting a completion mutation changed its guest range");
        mutation.Remove(allocation.data);
        allocation.registered = false;
    }
    requireFreed(imported);
}

void completedLeaseUnderLock(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    std::lock_guard gpu(GpuMutex());
    fixture.recorder->Keep(std::make_shared<GuestAllocations::Lease>(GuestAllocations::GuestAllocationsAcquire_nid_postfix()));
    fixture.recorder->Sync();
    Require(fixture.recorder->Idle(), "completed lease test recorder is not idle");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(allocation.data);
        allocation.registered = false;
    }
    requireFreed(imported);
}


struct MutationRace {
    Fixture* fixture;
    Allocation* allocation;
    std::shared_ptr<ImportState> replacement;
    unsigned calls = 0;
};

MutationRace* mutationRace = nullptr;

bool raceWaiter(std::uintptr_t address, std::size_t bytes) {
    auto& race = *mutationRace;
    Require(address == race.allocation->Address() && bytes == race.allocation->bytes, "pin waiter received the wrong mutation extent");
    std::lock_guard gpu(GpuMutex());
    race.fixture->Cleanup();
    if (++race.calls == 1) race.replacement = race.fixture->Import(address, bytes);
    return true;
}

bool failingWaiter(std::uintptr_t, std::size_t) {
    throw std::runtime_error("expected pin waiter failure");
}

void waiterFailure(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(&failingWaiter);
    bool failed = false;
    bool excluded = false;
    std::future<void> reader;
    {
        GuestAllocations::Mutation mutation;
        try {
            mutation.Protect(allocation.data, allocation.bytes, false, false, false, [] { throw std::runtime_error("host apply ran after a pin waiter failure"); });
        } catch (const std::runtime_error& error) {
            failed = std::string(error.what()) == "expected pin waiter failure";
        }
        std::promise<void> starting;
        auto ready = starting.get_future();
        reader = std::async(std::launch::async, [&] {
            starting.set_value();
            static_cast<void>(GuestAllocations::GuestAllocationsAcquire_nid_postfix());
        });
        ready.wait();
        excluded = reader.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
        const auto range = mutation.Find(allocation.data);
        Require(range.readable && range.writable && range.gpu, "a pin waiter failure changed the registered permissions");
    }
    reader.get();
    Require(failed, "a pin waiter error did not propagate");
    Require(excluded, "a failed pin waiter did not restore the registry lock");
    Require(!imported->freed.load(), "a failed pin waiter retired an import");
}

void reimportBeforeRelock(Fixture& fixture) {
    Allocation allocation(fixture);
    const auto imported = fixture.Import(allocation.Address(), allocation.bytes);
    MutationRace race{&fixture, &allocation};
    mutationRace = &race;
    GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(&raceWaiter);
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(allocation.data, allocation.bytes, [&](const void*, std::size_t, const void*, bool) {
            Require(race.calls == 2 && race.replacement != nullptr, "guest mutation did not recheck an import acquired before registry relock");
            requireFreed(imported);
            requireFreed(race.replacement);
            allocation.Release();
        });
        allocation.registered = false;
    }
    mutationRace = nullptr;
}

}

int RunHostImportLifetimeTests(const Context& context) {
    if (context.hostImportAlignment == 0) {
        std::puts("skipped, the device has no VK_EXT_external_memory_host");
        return 77;
    }
    Fixture fixture(context);
    descriptorFirstImport(fixture);
    idleUnmap(fixture);
    pendingRead(fixture, false, false);
    pendingRead(fixture, true, false);
    pendingRead(fixture, false, true);
    pendingRead(fixture, true, true);
    partialUnmap(fixture);
    protectRollback(fixture);
    completedLeaseUnderLock(fixture);
    mutationDuringCompletion(fixture);
    blockedFree(fixture);
    failedConstruction(fixture);
    productionWaiterFailure(fixture);
    deferredRetirement(fixture);
    std::puts("host import lifetime tests passed");
    return 0;
}

int RunHostImportMutationRaceTests(const Context& context) {
    if (context.hostImportAlignment == 0) {
        std::puts("skipped, the device has no VK_EXT_external_memory_host");
        return 77;
    }
    Fixture fixture(context);
    waiterFailure(fixture);
    reimportBeforeRelock(fixture);
    std::puts("host import mutation race tests passed");
    return 0;
}
