#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static void UseDevice(std::shared_ptr<VulkanDevice> device) { Driver::Get().device = std::move(device); }
    static void Stop() { Driver::Get().stop(); }
    static std::uint64_t BatchCap() { return Driver::batchCap(); }
    static void SubmitDueAfterCommit(VulkanDevice& device) { Driver::Get().submitDueAfterCommit(&device); }
    static void FlushBetweenPackets() { Driver::Get().flushBetweenPackets(0, 0xc0001000u, false); }
};

}

namespace {

using AgcDriver::DriverDetail::DriverTestAccess;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint64_t DisabledCapWork = 4096;
constexpr std::uint32_t SpinIterations = 1u << 8u;
constexpr std::chrono::seconds WaitLimit{10};

alignas(256) std::array<std::uint32_t, 15> SpinCode{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};

struct Slot {
    alignas(256) std::array<std::uint32_t, 4> spinOutput{};
    alignas(256) std::array<std::uint32_t, 4> spinDescriptor{};
    alignas(256) volatile std::uint32_t label = 0;
    alignas(256) volatile std::uint32_t done = 0;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "batch cap: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    Slot& Data() { return *reinterpret_cast<Slot*>(block); }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Dispatch(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(words, offset, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void ReleaseLabel(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), Low(target), High(target), value, 0u, 0u});
}

void WriteData(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void WaitEqual(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(target), High(target), value, 0xffffffffu, 0x19u});
}

void OpenBatch(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const std::array<std::uint32_t, 4> zero{};
    Require(device.FillBuffer(block.Address(), BlockBytes, zero), "batch cap: the device must import the test block");
    device.WaitIdle();
    Require(device.FillBuffer(block.Address() + BlockBytes - 16, 16, zero), "batch cap: the fill was not recorded");
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr && recorder->OpenBatchSerial() != 0, "batch cap: no batch is open");
    Require(Recorder::RecordedWorkSinceSubmit() == 0, "batch cap: recorded work is left from an earlier step");
    Require(Recorder::PendingLabelSince() == std::nullopt, "batch cap: a label is pending");
}

void Fire(AgcDriver::VulkanDevice& device, bool packets) {
    if (packets) {
        DriverTestAccess::FlushBetweenPackets();
        return;
    }
    AgcDriver::GuestMemory::TagGpuLockSite(AgcDriver::GuestMemory::GpuLockSite::Draw);
    std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
    DriverTestAccess::SubmitDueAfterCommit(device);
}

void CheckCap(AgcDriver::VulkanDevice& device, const GuestBlock& block, std::uint64_t cap, bool packets) {
    const std::string site = packets ? "the flush between packets" : "the commit of a draw";
    OpenBatch(device, block);
    auto* recorder = Recorder::Active();
    const std::uint64_t reach = cap != 0 ? cap : DisabledCapWork;
    for (std::uint64_t index = 1; index < reach; ++index) Recorder::CountRecordedWork();
    const auto epoch = device.SubmittedEpoch();
    Fire(device, packets);
    Require(device.SubmittedEpoch() == epoch && recorder->OpenBatchSerial() != 0 && Recorder::RecordedWorkSinceSubmit() == reach - 1, "batch cap: " + site + " submitted the open batch below the cap");
    Recorder::CountRecordedWork();
    Fire(device, packets);
    if (cap == 0) {
        Require(device.SubmittedEpoch() == epoch && recorder->OpenBatchSerial() != 0 && Recorder::RecordedWorkSinceSubmit() == reach, "batch cap: " + site + " submitted the open batch although the cap is disabled");
    } else {
        Require(device.SubmittedEpoch() == epoch + 1 && recorder->OpenBatchSerial() == 0 && Recorder::RecordedWorkSinceSubmit() == 0, "batch cap: " + site + " did not submit the open batch at the cap of " + std::to_string(cap));
    }
    {
        std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        device.SubmitRecorded(false);
    }
    device.WaitIdle();
    std::cout << "batch cap " << cap << ": " << site << " checked\n";
}

int RunCap(std::uint64_t expected) {
    Require(DriverTestAccess::BatchCap() == expected, "batch cap: the cap is " + std::to_string(DriverTestAccess::BatchCap()) + ", expected " + std::to_string(expected));
    std::shared_ptr<AgcDriver::VulkanDevice> device = OpenVulkanTestDevice();
    if (device == nullptr) return VulkanTestSkipped;
    DriverTestAccess::UseDevice(device);
    {
        GuestBlock block;
        for (const bool packets : {false, true}) CheckCap(*device, block, expected, packets);
    }
    device.reset();
    DriverTestAccess::Stop();
    std::cout << "batch cap test passed\n";
    return 0;
}

void CheckWait(Slot& slot, std::uint64_t dispatches, std::uint32_t value) {
    slot.spinDescriptor = {Low(slot.spinOutput.data()), High(slot.spinOutput.data()) & 0xffffu, sizeof(slot.spinOutput), 0x31016facu};
    slot.done = 0;
    std::vector<std::uint32_t> words;
    for (std::uint64_t index = 0; index < dispatches; ++index) Dispatch(words, SpinCode.data(), {Low(slot.spinDescriptor.data()), High(slot.spinDescriptor.data()), SpinIterations});
    ReleaseLabel(words, &slot.label, value);
    WaitEqual(words, &slot.label, value);
    WriteData(words, &slot.done, 1);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "batch cap: compute submit failed");
    const auto start = std::chrono::steady_clock::now();
    while (slot.done == 0) {
        Require(std::chrono::steady_clock::now() - start < WaitLimit, "batch cap: a guest wait behind " + std::to_string(dispatches) + " recorded dispatches did not finish: the open batch was not submitted");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    AgcDriverWaitIdle_nid_postfix();
    Require(slot.label == value, "batch cap: the label did not reach memory");
}

int RunWait(std::uint64_t expected) {
    Require(DriverTestAccess::BatchCap() == expected, "batch cap: the cap is " + std::to_string(DriverTestAccess::BatchCap()) + ", expected " + std::to_string(expected));
    auto probe = OpenVulkanTestDevice();
    if (!probe) return VulkanTestSkipped;
    probe.reset();
    GuestBlock block;
    auto& slot = block.Data();
    const std::uint64_t below = expected != 0 ? expected - 1 : DisabledCapWork;
    std::uint32_t value = 1;
    for (const std::uint64_t dispatches : {std::uint64_t{1}, std::uint64_t{3}, below / 2, below}) {
        CheckWait(slot, dispatches, value++);
        std::cout << "batch cap " << expected << ": a guest wait behind " << dispatches << " recorded dispatches submitted the open batch\n";
    }
    AgcDriverShutdown_nid_postfix();
    std::cout << "batch cap wait test passed\n";
    return 0;
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "cap";
    const std::uint64_t expected = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 512;
    try {
        if (mode == "cap") return RunCap(expected);
        if (mode == "wait") return RunWait(expected);
        throw std::runtime_error("batch cap: unknown mode " + mode);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            if (mode == "wait") AgcDriverShutdown_nid_postfix();
            else DriverTestAccess::Stop();
        } catch (const std::exception& stopError) {
            std::cerr << stopError.what() << '\n';
        }
        return 1;
    }
}
