#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 32;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t RecordBytes = 16;
constexpr std::size_t StoredOffset = 8;
constexpr std::uint32_t CpuValue = 0xc0001000u;
constexpr std::uint32_t GpuValue = 1;
constexpr std::uint64_t LabelValue = 1;
constexpr std::uint64_t EarlierValue = 0x5a5a0002u;

alignas(256) constexpr std::array<std::uint32_t, 6> StoreCode{
    0x7e000281u, 0x7e020288u, 0xe0701000u, 0x80000001u, 0xbf810000u, 0xbf810000u,
};

class HostBlock {
public:
    explicit HostBlock(bool registered) : registered(registered) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "unregistered label behind a write-back: cannot allocate a host block");
        std::memset(block, 0, BlockBytes);
        if (registered) GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~HostBlock() {
        if (registered) GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    HostBlock(const HostBlock&) = delete;
    HostBlock& operator=(const HostBlock&) = delete;

    volatile std::uint32_t* Word(std::size_t offset) const { return reinterpret_cast<volatile std::uint32_t*>(block + offset); }
    std::uint64_t* Label(std::size_t offset) const { return reinterpret_cast<std::uint64_t*>(block + offset); }
    std::uint64_t Read(std::size_t offset) const { return *reinterpret_cast<volatile std::uint64_t*>(block + offset); }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
    bool registered = false;
};

struct Outcome {
    int reason = 0;
    std::uint64_t epoch = 0;
    bool OnGpu() const { return reason == 0 || reason == 5; }
};

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

void RecordDispatch(AgcDriver::VulkanDevice& device, std::uint64_t record) {
    std::vector<std::uint32_t> userData(16, 0u);
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(record), static_cast<std::uint32_t>((record >> 32u) & 0xffffu), RecordBytes, 0x31016facu};
    std::copy(descriptor.begin(), descriptor.end(), userData.begin());
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

void WaitForGpu(AgcDriver::VulkanDevice& device, std::uint64_t epoch) {
    if (device.CanWaitUnlocked()) device.WaitRecorded(epoch);
    else std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

Outcome StoreAsDriver(AgcDriver::VulkanDevice& device, std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Outcome outcome;
    outcome.reason = device.WriteLabelOnGpu(address, bytes, stamp, 0, false);
    if (outcome.OnGpu()) {
        outcome.epoch = device.SubmitAndEpoch();
        return outcome;
    }
    if (outcome.reason != 1) device.WaitIdle();
    AgcDriver::GuestMemory::Write(address, bytes, 4);
    return outcome;
}

void Settle(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.ReapRecorded();
    device.WaitIdle();
}

void LabelAfterWriteBack(AgcDriver::VulkanDevice& device, const HostBlock& records, const HostBlock& labels, std::size_t offset) {
    volatile std::uint32_t* stored = records.Word(StoredOffset);
    *stored = CpuValue;
    RecordDispatch(device, records.Address());
    Require(Recorder::PendingWriteBackCompletions() == 1, "label after a write-back: the dispatch into unregistered memory left " + std::to_string(Recorder::PendingWriteBackCompletions()) + " write-back completions pending, expected 1");
    const std::uint64_t value = LabelValue;
    const auto outcome = StoreAsDriver(device, labels.Address() + offset, std::as_bytes(std::span(&value, 1)), 1);
    if (outcome.OnGpu()) WaitForGpu(device, outcome.epoch);
    const auto seen = labels.Read(offset);
    const std::uint32_t record = *stored;
    Require(seen == 0 || record == GpuValue, "label after a write-back: the 8-byte label in unregistered memory held " + Hex(seen) + " once the GPU ran its batch (reason " + std::to_string(outcome.reason) + "), while the dispatch's record still held " + Hex(record) + " before its write-back");
    AgcDriverStoreGuestLabel_nid_postfix(labels.Label(offset), 0);
    Settle(device);
    Require(*stored == GpuValue, "label after a write-back: the record holds " + Hex(*stored) + " after the reap, expected " + Hex(GpuValue));
    const auto last = labels.Read(offset);
    Require(last == 0, "label after a write-back: the label holds " + Hex(last) + " after the reap (reason " + std::to_string(outcome.reason) + "): its store landed over the 0 the host stored once the GPU work was done");
}

void WideStoreAfterWriteBack(AgcDriver::VulkanDevice& device, const HostBlock& records, const HostBlock& labels, std::size_t offset) {
    volatile std::uint32_t* stored = records.Word(StoredOffset);
    *stored = CpuValue;
    RecordDispatch(device, records.Address());
    Require(Recorder::PendingWriteBackCompletions() == 1, "wide store after a write-back: the dispatch into unregistered memory left " + std::to_string(Recorder::PendingWriteBackCompletions()) + " write-back completions pending, expected 1");
    const std::array<std::uint32_t, 4> values{0x6b6b0001u, 0x6b6b0002u, 0x6b6b0003u, 0x6b6b0004u};
    const auto outcome = StoreAsDriver(device, labels.Address() + offset, std::as_bytes(std::span(values)), 2);
    if (outcome.OnGpu()) WaitForGpu(device, outcome.epoch);
    const std::uint32_t seen = *labels.Word(offset);
    const std::uint32_t record = *stored;
    Settle(device);
    Require(seen == 0 || record == GpuValue, "wide store after a write-back: the 16-byte store into unregistered memory held " + Hex(seen) + " once the GPU ran its batch (reason " + std::to_string(outcome.reason) + "), while the dispatch's record still held " + Hex(record) + " before its write-back");
    for (std::size_t i = 0; i < values.size(); ++i) Require(*labels.Word(offset + i * 4) == values[i], "wide store after a write-back: dword " + std::to_string(i) + " holds " + Hex(*labels.Word(offset + i * 4)) + ", expected " + Hex(values[i]));
}

bool StoreAfterCompletionStore(AgcDriver::VulkanDevice& device, const HostBlock& records, const HostBlock& direct, const HostBlock& labels, std::size_t offset, bool wide) {
    const std::string what = wide ? "wide store after a completion store" : "label after a completion store";
    volatile std::uint32_t* stored = records.Word(StoredOffset);
    *stored = CpuValue;
    *labels.Label(offset) = 0;
    *direct.Label(0x2000) = 0;
    RecordDispatch(device, records.Address());
    std::uint64_t copiedEpoch = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        copiedEpoch = device.SubmitAndEpoch();
    }
    RecordDispatch(device, direct.Address());
    if (Recorder::PendingWriteBackCompletions() != 1) {
        Settle(device);
        return false;
    }
    const std::uint64_t earlier = EarlierValue;
    std::uint64_t firstEpoch = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const int reason = device.WriteLabelOnGpu(direct.Address() + 0x2000, std::as_bytes(std::span(&earlier, 1)), 3, 0, false);
        if (reason != 5) {
            if (reason != 1) device.WaitIdle();
            AgcDriver::GuestMemory::Write(direct.Address() + 0x2000, std::as_bytes(std::span(&earlier, 1)), 4);
            device.WaitIdle();
            return false;
        }
        firstEpoch = device.SubmitAndEpoch();
    }
    WaitForGpu(device, firstEpoch);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.ReapRecorded(copiedEpoch);
        Require(Recorder::PendingWriteBackCompletions() == 0 && Recorder::PendingCompletionStores() == 1, what + ": after the copied writer's reap " + std::to_string(Recorder::PendingWriteBackCompletions()) + " write-backs and " + std::to_string(Recorder::PendingCompletionStores()) + " completion stores are pending, expected 0 and 1");
    }
    const std::array<std::uint64_t, 2> values{LabelValue, LabelValue + 1};
    const auto bytes = wide ? std::as_bytes(std::span(values)) : std::as_bytes(std::span(values).first(1));
    const auto outcome = StoreAsDriver(device, labels.Address() + offset, bytes, 4);
    if (outcome.OnGpu()) WaitForGpu(device, outcome.epoch);
    const auto seen = labels.Read(offset);
    const auto before = direct.Read(0x2000);
    Require(seen == 0 || before == EarlierValue, what + ": the store into unregistered memory held " + Hex(seen) + " once the GPU ran its batch (reason " + std::to_string(outcome.reason) + "), while the earlier label in registered memory, stored when its batch is reaped, still held " + Hex(before));
    if (!wide) AgcDriverStoreGuestLabel_nid_postfix(labels.Label(offset), 0);
    Settle(device);
    Require(direct.Read(0x2000) == EarlierValue, what + ": the earlier label holds " + Hex(direct.Read(0x2000)) + " after the reap, expected " + Hex(EarlierValue));
    const auto last = labels.Read(offset);
    Require(last == (wide ? LabelValue : 0), what + ": the store into unregistered memory holds " + Hex(last) + " after the reap (reason " + std::to_string(outcome.reason) + ")" + (wide ? "" : ": it landed over the 0 the host stored once the GPU work was done"));
    return true;
}

bool WideStoreWithoutWriteBack(AgcDriver::VulkanDevice& device, const HostBlock& direct, const HostBlock& labels, std::size_t offset) {
    RecordDispatch(device, direct.Address());
    if (Recorder::PendingWriteBackCompletions() != 0) {
        Settle(device);
        return false;
    }
    const std::array<std::uint32_t, 4> values{0x7c7c0001u, 0x7c7c0002u, 0x7c7c0003u, 0x7c7c0004u};
    const auto outcome = StoreAsDriver(device, labels.Address() + offset, std::as_bytes(std::span(values)), 5);
    Require(outcome.reason == 0, "wide store without a write-back: a 16-byte store into unregistered memory with no write-back or completion store pending was handled with reason " + std::to_string(outcome.reason) + ", expected 0");
    WaitForGpu(device, outcome.epoch);
    Settle(device);
    for (std::size_t i = 0; i < values.size(); ++i) Require(*labels.Word(offset + i * 4) == values[i], "wide store without a write-back: dword " + std::to_string(i) + " holds " + Hex(*labels.Word(offset + i * 4)) + ", expected " + Hex(values[i]));
    return true;
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_OPPORTUNISTIC_REAP", "1");
#else
        setenv("APS5_NO_OPPORTUNISTIC_REAP", "1", 1);
#endif
        const HostBlock records(false);
        const HostBlock labels(false);
        const HostBlock direct(true);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        LabelAfterWriteBack(*device, records, labels, 0x100);
        WideStoreAfterWriteBack(*device, records, labels, 0x200);
        const bool imports = WideStoreWithoutWriteBack(*device, direct, labels, 0x300);
        if (!imports) std::puts("the device wrote back a dispatch into registered memory, so the cases that need a write-back-free dispatch did not run");
        if (imports && (!StoreAfterCompletionStore(*device, records, direct, labels, 0x400, false) || !StoreAfterCompletionStore(*device, records, direct, labels, 0x500, true))) std::puts("the device stored a label in registered memory without a completion store, so the cases after a completion store did not run");
        std::puts("unregistered label behind a write-back tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
