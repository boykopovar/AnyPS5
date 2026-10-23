#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t ReleaseMemHeader = 0xc0064900u;

class GuestBlock {
public:
    explicit GuestBlock(bool isolated = false) {
        const std::size_t reserved = isolated ? 3 * BlockBytes : BlockBytes;
#ifdef _WIN32
        allocation = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, reserved, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        allocation = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, reserved));
#endif
        Require(allocation != nullptr, "unimported label order: cannot allocate the guest block");
        block = isolated ? allocation + BlockBytes : allocation;
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        std::free(allocation);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    volatile std::uint32_t* Words() { return reinterpret_cast<volatile std::uint32_t*>(block); }

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* allocation = nullptr;
    std::uint8_t* block = nullptr;
};

class SubmitGate {
public:
    explicit SubmitGate(AgcDriver::VulkanDevice& device) : device(device) {
#ifdef _WIN32
        library.reset(SDL_LoadObject("vulkan-1.dll"));
#else
        library.reset(SDL_LoadObject("libvulkan.so.1"));
#endif
        Require(library != nullptr, "unimported label order: cannot load Vulkan for the submit gate");
        const auto resolve = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
        Require(resolve != nullptr, "unimported label order: cannot resolve vkGetDeviceProcAddr");
        const auto createSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(resolve(device.Device(), "vkCreateSemaphore"));
        destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(resolve(device.Device(), "vkDestroySemaphore"));
        signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device.Device(), "vkSignalSemaphoreKHR"));
        if (signalSemaphore == nullptr) signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device.Device(), "vkSignalSemaphore"));
        Require(createSemaphore != nullptr && destroySemaphore != nullptr && signalSemaphore != nullptr, "unimported label order: cannot resolve the timeline semaphore functions");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        Require(createSemaphore(device.Device(), &info, nullptr, &semaphore) == VK_SUCCESS, "unimported label order: cannot create the submit gate");
    }

    ~SubmitGate() {
        if (watchdog.joinable()) {
            watchdog.request_stop();
            watchdog.join();
        }
        Release();
        try {
            device.WaitIdle();
        } catch (...) {
        }
        destroySemaphore(device.Device(), semaphore, nullptr);
    }

    SubmitGate(const SubmitGate&) = delete;
    SubmitGate& operator=(const SubmitGate&) = delete;

    VkSemaphore Semaphore() const { return semaphore; }

    void ReleaseAfter(std::chrono::seconds timeout) {
        watchdog = std::jthread([this, timeout](std::stop_token stop) {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!stop.stop_requested()) expired = true;
            Release();
        });
    }

    bool Expired() const { return expired; }

    void Release() {
        if (released.exchange(true)) return;
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
        info.semaphore = semaphore;
        info.value = 1;
        signalResult = signalSemaphore(device.Device(), &info);
    }

    bool Released() const { return signalResult == VK_SUCCESS; }

private:
    AgcDriver::VulkanDevice& device;
    std::unique_ptr<void, decltype(&SDL_UnloadObject)> library{nullptr, &SDL_UnloadObject};
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::jthread watchdog;
    std::atomic<bool> released{false};
    std::atomic<bool> expired{false};
    VkResult signalResult = VK_NOT_READY;
};

alignas(64) volatile std::uint32_t commandMemory[16] = {};

std::uint64_t AddressOf(volatile const std::uint32_t* word) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(word));
}

int StoreLabel(AgcDriver::VulkanDevice& device, std::uint64_t address, std::uint32_t value, std::uint64_t stamp) {
    const auto bytes = std::as_bytes(std::span(&value, 1));
    const int reason = device.WriteLabelOnGpu(address, bytes, stamp, 0);
    if ((reason >= 1 && reason <= 4) || reason == 7) {
        if (reason != 1 && reason != 7) device.WaitIdle();
        AgcDriver::GuestMemory::Write(address, bytes, 4);
    }
    if (reason == 7) device.SubmitAfterEarlyLabel();
    return reason;
}

}

int main() {
    try {
        GuestBlock labels;
        GuestBlock temporary(true);
        GuestBlock compute(true);
        GuestBlock held(true);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint32_t, 4> pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
        if (!device->FillBuffer(labels.Address() + 4096, 4096, pattern)) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        {
            const auto imported = AgcDriver::Graphics::HostImportLiveBytes();
            const std::array<std::uint32_t, 2> value{0x76543210u, 0xfedcba98u};
            const auto bytes = std::as_bytes(std::span(value));
            const auto address = temporary.Address() + 4092;
            const int reason = device->WriteLabelOnGpu(address, bytes, 90, 0);
            Require(reason == 7 && AgcDriver::Graphics::HostImportLiveBytes() == imported, "an eight-byte label did not select its unimported CPU path (reason " + std::to_string(reason) + ")");
            Require(labels.Words()[1024] == pattern[0], "the targeted label returned before preceding GPU work completed");
            device->WaitIdle();
            AgcDriver::GuestMemory::Write(address, bytes, 4);
            Require(labels.Words()[1024] == pattern[0], "the CPU label overtook earlier GPU work");
            Require(temporary.Words()[1023] == value[0] && temporary.Words()[1024] == value[1], "the unimported eight-byte label did not land");
            Require(temporary.Words()[1022] == 0 && temporary.Words()[1025] == 0, "the CPU label changed neighboring words");
            Require(device->FillBuffer(labels.Address() + 4096, 4096, pattern), "cannot record work after the unimported label");
        }
        StoreLabel(*device, AddressOf(&commandMemory[8]), 1, 1);
        StoreLabel(*device, labels.Address() + 4, 2, 2);
        device->SubmitRecorded(true);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (labels.Words()[1] != 2) {
            Require(std::chrono::steady_clock::now() < deadline, "unimported label order: the later label never landed");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        const std::uint32_t seen = commandMemory[8];
        commandMemory[8] = ReleaseMemHeader;
        device->WaitIdle();
        const std::uint32_t reused = commandMemory[8];
        std::array<char, 256> message{};
        std::snprintf(message.data(), message.size(), "unimported label order: the label held 0x%08x when a later label had landed, and the guest's reused word holds 0x%08x instead of 0x%08x", seen, reused, ReleaseMemHeader);
        Require(seen == 1 && reused == ReleaseMemHeader, message.data());
        {
            using namespace AgcDriver::DriverDetail;
            const auto synced = syncedUnimportedLabels.load();
            const auto idle = labelFallbacks[1].load();
            const auto notImported = labelFallbacks[3].load();
            countLabelOutcome(7);
            Require(syncedUnimportedLabels.load() == synced + 1 && labelFallbacks[1].load() == idle && labelFallbacks[3].load() == notImported, "an unimported label after a recorder sync was not counted on its own");
        }
        {
            auto* recorder = AgcDriver::Graphics::Recorder::Active();
            Require(recorder != nullptr, "the early label case has no recorder");
            device->WaitIdle();
            recorder->Keep(std::make_shared<int>(0));
            const auto submissions = recorder->Submissions();
            const std::uint32_t early = 0xea51u;
            const auto bytes = std::as_bytes(std::span(&early, 1));
            Require(device->WriteLabelOnGpu(temporary.Address() + 256, bytes, 93, 0) == 7, "a label after work that leaves guest memory alone did not take the unimported path");
            Require(recorder->Recording() && recorder->Submissions() == submissions, "a label after work that leaves guest memory alone waited for it or submitted before its store");
            AgcDriver::GuestMemory::Write(temporary.Address() + 256, bytes, 4);
            Require(device->SubmitAfterEarlyLabel() && !device->SubmitAfterEarlyLabel(), "the batch left open by the early label was not submitted once after the store");
            Require(recorder->Submissions() == submissions + 1 && !recorder->Idle(), "the early label's batch is not in flight");
            Require(temporary.Words()[64] == early, "the early unimported label did not land");
            recorder->Keep(std::make_shared<int>(0));
            recorder->NotePendingRead(temporary.Address() + 512, 64, AgcDriver::Graphics::Recorder::ReadKind::DispatchElement);
            const std::uint32_t late = 0x1a7eu;
            Require(StoreLabel(*device, temporary.Address() + 260, late, 94) == 7 && recorder->Idle(), "a label after work reading guest memory in place did not wait for it");
            Require(temporary.Words()[65] == late, "the waiting unimported label did not land");
        }
        {
            Require(device->FillBuffer(labels.Address() + 4096, 4096, pattern), "cannot record work before the compute-queue label");
            auto* recorder = AgcDriver::Graphics::Recorder::Active();
            Require(recorder != nullptr && !recorder->Idle(), "the compute-queue case has no recorded work before its label");
            const auto imported = AgcDriver::Graphics::HostImportLiveBytes();
            const auto submissions = recorder->Submissions();
            const std::uint32_t value = 0x600dc0deu;
            const int reason = device->WriteLabelOnGpu(compute.Address() + 64, std::as_bytes(std::span(&value, 1)), 91, 0x20);
            Require(reason == 0 && recorder->Submissions() == submissions, "a compute-queue label synced the recorder instead of importing its memory (reason " + std::to_string(reason) + ")");
            Require(AgcDriver::Graphics::HostImportLiveBytes() > imported, "the compute-queue label was recorded without importing its memory");
            device->WaitIdle();
            Require(compute.Words()[16] == value, "the compute-queue label did not land");
        }
        if (auto* recorder = AgcDriver::Graphics::Recorder::Active(); recorder != nullptr && recorder->HasTimeline()) {
            SubmitGate gate(*device);
            recorder->HoldNextSubmit(gate.Semaphore(), 1);
            Require(device->FillBuffer(labels.Address() + 4096, 4096, pattern), "cannot record work into the held batch");
            device->SubmitRecorded(false);
            Require(recorder->SubmitHeld(), "the recorder did not report its held batch");
            gate.ReleaseAfter(std::chrono::seconds(5));
            const std::uint32_t value = 0x4e1du;
            const int reason = device->WriteLabelOnGpu(held.Address() + 64, std::as_bytes(std::span(&value, 1)), 92, 0);
            Require(!gate.Expired(), "a label behind a held batch waited for the batch instead of importing its memory");
            Require(reason == 0, "a label behind a held batch was not recorded on the GPU (reason " + std::to_string(reason) + ")");
            gate.Release();
            Require(gate.Released(), "unimported label order: cannot release the submit gate");
            device->WaitIdle();
            Require(!recorder->SubmitHeld() && held.Words()[16] == value, "the label behind the held batch did not land");
        } else {
            std::puts("held-batch case skipped, the recorder has no timeline semaphore");
        }
        std::puts("unimported label order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
