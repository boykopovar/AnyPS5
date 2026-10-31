#include "VulkanTestDevice.hpp"
#include "LaneGate.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DescribeSurface;
using AgcDriver::Graphics::GuestTextureResource;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::StorageTexture;
using AgcDriver::Graphics::TextureDetiler;
using AgcDriver::Graphics::TextureDimension;
using AgcDriver::Graphics::TextureTileMode;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t Format32Float = 22;
constexpr std::size_t UnitBytes = 65536;
constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::uint32_t Cleared = 0x3f800000u;
constexpr std::uint8_t Sentinel = 0xa5;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("storage flush lane wait: " + message);
}

class FenceProbe {
public:
    explicit FenceProbe(Context& context) : resolver(context.deviceProc), device(context.device), asyncQueue(context.asyncQueue) {
        Require(active == nullptr, "another fence probe is active");
        active = this;
        context.deviceProc = Resolve;
    }

    ~FenceProbe() { active = nullptr; }

    void CaptureNextAsync() {
        fence.store(VK_NULL_HANDLE);
        waited.store(false);
        capture = true;
    }

    VkFence Fence() const { return fence.load(); }
    bool Waited() const { return waited.load(); }
    void Notify() {
        std::lock_guard lock(mutex);
        condition.notify_all();
    }

    bool WaitForActivity(const std::atomic<bool>& finished) {
        std::unique_lock lock(mutex);
        return condition.wait_for(lock, std::chrono::seconds(3), [&] { return finished.load() || waited.load(); });
    }

private:
    static PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice device, const char* name) {
        if (std::strcmp(name, "vkQueueSubmit") == 0) return reinterpret_cast<PFN_vkVoidFunction>(QueueSubmit);
        if (std::strcmp(name, "vkWaitForFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(WaitForFences);
        return active->resolver(device, name);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit(VkQueue queue, std::uint32_t count, const VkSubmitInfo* submits, VkFence fence) {
        auto& probe = *active;
        if (queue == probe.asyncQueue && probe.capture) {
            probe.capture = false;
            probe.fence.store(fence);
        }
        return reinterpret_cast<PFN_vkQueueSubmit>(probe.resolver(probe.device, "vkQueueSubmit"))(queue, count, submits, fence);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL WaitForFences(VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) {
        auto& probe = *active;
        const auto selected = probe.fence.load();
        if (timeout != 0 && selected != VK_NULL_HANDLE && std::find(fences, fences + count, selected) != fences + count) {
            probe.waited.store(true);
            probe.Notify();
        }
        return reinterpret_cast<PFN_vkWaitForFences>(probe.resolver(device, "vkWaitForFences"))(device, count, fences, all, timeout);
    }

    inline static FenceProbe* active = nullptr;
    PFN_vkGetDeviceProcAddr resolver;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue asyncQueue;
    std::atomic<VkFence> fence{VK_NULL_HANDLE};
    std::atomic<bool> waited{false};
    bool capture = false;
    std::mutex mutex;
    std::condition_variable condition;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, UnitBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* mapped = mmap(nullptr, BlockBytes + UnitBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + UnitBytes - 1) & ~(static_cast<std::uintptr_t>(UnitBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + UnitBytes) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + UnitBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            watched = AgcDriver::GuestMemory::WriteWatched();
            if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        std::memset(block, Sentinel, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
        munmap(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }

private:
    std::uint8_t* block = nullptr;
#ifndef _WIN32
    bool watched = false;
#endif
};

enum class GatedWork { Unrelated, Writer, Lease, CompletionWriter };

void RunCase(AgcDriver::VulkanDevice& device, Recorder& recorder, Context context, FenceProbe& probe,
             std::unique_lock<AgcDriver::GuestMemory::GpuMutexType>& lock, GatedWork work) {
    recorder.Sync();
    GuestBlock block;
    GuestTextureResource resource{};
    resource.width = Width;
    resource.height = Height;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = Format32Float;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.baseAddress = block.Address();
    const auto guestBytes = static_cast<std::size_t>(DescribeSurface(resource).guestBytes);
    Require(guestBytes <= BlockBytes / 2, "the image leaves no disjoint test range");
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    context.recorder = &recorder;
    struct Drain {
        Recorder& recorder;
        std::unique_lock<AgcDriver::GuestMemory::GpuMutexType>& lock;
        ~Drain() {
            if (!lock.owns_lock()) lock.lock();
            try { recorder.Sync(); } catch (const std::exception& error) { std::fprintf(stderr, "storage flush lane wait cleanup: %s\n", error.what()); }
        }
    } drain{recorder, lock};
    auto texture = StorageTexture::Create(context, detiler, resource, 0);
    {
        const auto commands = recorder.Commands();
        Recorder::CommandBorrow borrow(&recorder, commands);
        recorder.Keep(texture);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkClearColorValue value{};
        value.uint32[0] = value.uint32[1] = value.uint32[2] = value.uint32[3] = Cleared;
        AgcDriver::Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, texture->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
        AgcDriver::Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    }
    texture->MarkDirty();
    Require(StorageTexture::FindPending(resource.baseAddress, guestBytes) != nullptr, "the marked image is not pending");
    recorder.Submit();
    auto gate = std::make_shared<AgcDriver::Tests::TimelineGate>(device);
    auto asyncCompleted = std::make_shared<std::atomic<bool>>(false);
    auto callbacks = std::make_shared<std::atomic<unsigned>>(0);
    probe.CaptureNextAsync();
    if (work == GatedWork::CompletionWriter) {
        static_cast<void>(recorder.Commands());
        recorder.NotePendingWrite(resource.baseAddress + 256, 16);
        recorder.OnComplete([&recorder, weakGate = std::weak_ptr(gate), asyncCompleted, callbacks, address = resource.baseAddress] {
            ++*callbacks;
            const auto callbackGate = weakGate.lock();
            Require(callbackGate != nullptr, "the completion outlived its gate");
            const Recorder::LaneScope async(1);
            callbackGate->Record(recorder.Commands());
            recorder.NotePendingWrite(address + 512, 16);
            recorder.OnComplete([asyncCompleted] { asyncCompleted->store(true); });
            recorder.Submit();
        });
        Require(recorder.OpenWriteOverlaps(resource.baseAddress + 256, 16), "the callback's main writer is not open");
    } else {
        const Recorder::LaneScope async(1);
        gate->Record(recorder.Commands());
        if (work == GatedWork::Lease) recorder.NotePendingRead(resource.baseAddress + 256, 16, Recorder::ReadKind::AddressBased);
        else recorder.NotePendingWrite(resource.baseAddress + (work == GatedWork::Writer ? 256 : BlockBytes / 2), 16);
        recorder.OnComplete([asyncCompleted] { asyncCompleted->store(true); });
        recorder.Submit();
    }
    Require(work == GatedWork::CompletionWriter || probe.Fence() != VK_NULL_HANDLE, "the gated submission was not observed");
    if (work == GatedWork::Unrelated) {
        static_cast<void>(recorder.Commands());
        recorder.NotePendingWrite(resource.baseAddress + 256, 16);
        Require(recorder.OpenWriteOverlaps(resource.baseAddress + 256, 16), "the main writer is not open");
    }
    std::atomic<bool> finished{false};
    std::exception_ptr failure;
    const auto fenceStatus = context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus");
    lock.unlock();
    std::jthread flusher([&] {
        try {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            Require(StorageTexture::FlushPending(resource.baseAddress, guestBytes, nullptr, "flush lane wait test"), "the pending image was not flushed");
        } catch (...) {
            failure = std::current_exception();
        }
        finished.store(true);
        probe.Notify();
    });
    const bool observed = probe.WaitForActivity(finished);
    const bool returnedWhileGated = finished.load();
    const bool waitedOnGate = probe.Waited();
    const bool completedWhileGated = asyncCompleted->load();
    const auto gatedFence = probe.Fence();
    const auto gatedStatus = gatedFence == VK_NULL_HANDLE ? VK_ERROR_UNKNOWN : fenceStatus(context.device, gatedFence);
    std::exception_ptr releaseFailure;
    try { gate->Release(); } catch (...) { releaseFailure = std::current_exception(); }
    flusher.join();
    lock.lock();
    recorder.Sync();
    if (failure) std::rethrow_exception(failure);
    if (releaseFailure) std::rethrow_exception(releaseFailure);
    Require(observed, "the flush neither returned nor reached the gated fence within its bounded observation");
    Require(gatedFence != VK_NULL_HANDLE, "the flush did not submit the gated writer");
    Require(gatedStatus == VK_NOT_READY && !completedWhileGated, "the gated work completed before its release");
    if (work == GatedWork::Unrelated) {
        Require(returnedWhileGated && !waitedOnGate, "the main writer's flush waited for an unrelated async batch");
    } else {
        Require(waitedOnGate && !returnedWhileGated, "the flush did not wait for the overlapping async writer or lease");
    }
    if (work == GatedWork::CompletionWriter) Require(callbacks->load() == 1, "the main completion did not create exactly one later writer");
    Require(asyncCompleted->load(), "the gated completion did not run after release");
    Require(StorageTexture::FindPending(resource.baseAddress, guestBytes) == nullptr, "the image remained pending after flush");
    AgcDriver::GuestMemory::CollectWritesUncached(resource.baseAddress, guestBytes);
    for (std::size_t offset = 0; offset < guestBytes; offset += 4) {
        std::uint32_t word = 0;
        std::memcpy(&word, block.Data() + offset, sizeof(word));
        Require(word == Cleared, "the flush did not store every cleared texel at byte " + std::to_string(offset));
    }
    std::printf("storage flush lane wait: %s passed\n", work == GatedWork::Unrelated ? "unrelated async work remains pending" : work == GatedWork::Writer ? "overlapping async writer waits" : work == GatedWork::Lease ? "overlapping read-only AddressBased lease waits" : "completion-created async writer waits");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_ASYNC_LANE", "51");
#else
    setenv("APS5_ASYNC_LANE", "51", 1);
#endif
    try {
        std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        {
            auto* original = Recorder::Active();
            Require(original != nullptr, "no active recorder");
            if (!original->AsyncLaneActive()) {
                std::puts("skipped: the device has no eligible compute-only queue for the async lane");
                device.reset();
                lock.unlock();
                AgcDriverShutdown_nid_postfix();
                return VulkanTestSkipped;
            }
            original->Sync();
            auto context = device->graphicsContext();
            context.recorder = nullptr;
            FenceProbe probe(context);
            struct RestoreRecorder {
                Recorder* original;
                ~RestoreRecorder() { original->Activate(); }
            } restore{original};
            Recorder recorder(context, true);
            recorder.Activate();
            Require(recorder.AsyncLaneActive(), "the instrumented recorder has no native async lane");
            RunCase(*device, recorder, context, probe, lock, GatedWork::Unrelated);
            RunCase(*device, recorder, context, probe, lock, GatedWork::Writer);
            RunCase(*device, recorder, context, probe, lock, GatedWork::Lease);
            RunCase(*device, recorder, context, probe, lock, GatedWork::CompletionWriter);
        }
        device.reset();
        lock.unlock();
        AgcDriverShutdown_nid_postfix();
        std::puts("storage flush lane wait tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (const std::exception& shutdownError) { std::cerr << shutdownError.what() << '\n'; }
        return 1;
    }
}
