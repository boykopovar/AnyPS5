#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t RefreshedHeight = 64;
constexpr std::size_t ImageBytes = Width * Height * 4u;
constexpr std::size_t RefreshedBytes = Width * RefreshedHeight * 4u;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Initial = 0x3f800000u;
constexpr std::uint32_t Stored = 0x11111111u;
constexpr std::uint32_t Overwritten = 0x22222222u;
constexpr std::uint32_t Refreshed = 0x33333333u;
constexpr std::size_t OverwrittenDword = 0x100u / 4u;
constexpr std::size_t FarDword = 100u * Width;

alignas(256) constexpr std::array<std::uint32_t, 11> StoreImageCode{0x7e000280u, 0x7e020280u, 0x7e0402ffu, Stored, 0xf0200108u, 0x00010200u, 0x7e0202ffu, 0x00000064u, 0xf0200108u, 0x00010200u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 11> RefreshImageCode{0x7e000280u, 0x7e020280u, 0x7e0402ffu, Refreshed, 0xf0200108u, 0x00010200u, 0x7e0202ffu, 0x0000000au, 0xf0200108u, 0x00010200u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> StoreGlobalCode{0x7e000208u, 0x7e020209u, 0x7e0402ffu, Overwritten, 0xdc708100u, 0x007d0200u, 0xbf810000u};

class GuestBlock {
public:
    explicit GuestBlock(bool watched) : watched(watched) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "storage pre-sync writer: cannot allocate the guest block");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), BlockBytes), "storage pre-sync writer: the guest block is not write-watched");
        std::fill(Words(), Words() + BlockBytes / 4u, Initial);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint32_t* Words() const { return reinterpret_cast<std::uint32_t*>(block); }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    bool watched;
    std::uint8_t* block = nullptr;
};

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::uintptr_t ProgramAddress(std::span<const std::uint32_t> code) {
    return reinterpret_cast<std::uintptr_t>(code.data());
}

std::vector<std::uint32_t> ImageUserData(std::uint64_t base, std::uint32_t height) {
    std::vector<std::uint32_t> userData(12, 0u);
    userData[4] = static_cast<std::uint32_t>(base >> 8u);
    userData[5] = static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Width - 1u) & 3u) << 30u);
    userData[6] = ((Width - 1u) >> 2u) | ((height - 1u) << 14u);
    userData[7] = 0xfacu | (Type2D << 28u);
    return userData;
}

std::vector<std::uint32_t> GlobalUserData(std::uint64_t base) {
    std::vector<std::uint32_t> userData(10, 0u);
    userData[8] = static_cast<std::uint32_t>(base);
    userData[9] = static_cast<std::uint32_t>(base >> 32u);
    return userData;
}

struct Scenario {
    const std::uint32_t* words;
    std::uint64_t image;
    ShaderRecompiler::RecompileResult storeImage;
    ShaderRecompiler::RecompileResult storeGlobal;
    ShaderRecompiler::RecompileResult refresh;
};

Scenario Prepare(AgcDriver::VulkanDevice& device, const GuestBlock& guest) {
    const auto image = guest.Address();
    const auto target = device.Target();
    return {guest.Words(), image, Compile(StoreImageCode, ImageUserData(image, Height), target), Compile(StoreGlobalCode, GlobalUserData(image), target), Compile(RefreshImageCode, ImageUserData(image, RefreshedHeight), target)};
}

void Record(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& shader, std::span<const std::uint32_t> code, std::shared_ptr<AgcDriver::PreparedDispatch> prepared = nullptr) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.Dispatch(shader, 1u, 1u, 1u, {}, ProgramAddress(code), std::move(prepared));
}

void StoreRefreshed(AgcDriver::VulkanDevice& device, const Scenario& scenario, std::shared_ptr<AgcDriver::PreparedDispatch> prepared = nullptr) {
    Record(device, scenario.refresh, RefreshImageCode, std::move(prepared));
    device.WaitIdle();
    StorageTexture::FlushPending(scenario.image, RefreshedBytes, nullptr, "test");
    device.WaitIdle();
}

void StoreImage(AgcDriver::VulkanDevice& device, const Scenario& scenario, bool addressBased, bool submit) {
    Record(device, scenario.storeImage, StoreImageCode);
    if (addressBased) Record(device, scenario.storeGlobal, StoreGlobalCode);
    if (!submit) return;
    std::uint64_t serial = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        serial = device.SubmitAndEpoch();
    }
    device.WaitRecorded(serial);
}

bool AddressBasedStorePending(std::uint64_t image, bool open) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    const auto read = recorder != nullptr ? recorder->DescribePendingRead(image + ImageBytes, 4u) : std::nullopt;
    return read.has_value() && read->kind == AgcDriver::Graphics::Recorder::ReadKind::AddressBased && read->open == open;
}

std::uint64_t LockedWriterWaits() {
    const auto waits = StorageTexture::WriterWaitCounts();
    return waits.openLease + waits.openWrite + waits.inFlightLease + waits.inFlightWrite;
}

void RequireStored(AgcDriver::VulkanDevice& device, const Scenario& scenario, const std::string& what, bool addressBased) {
    StorageTexture::FlushPending(scenario.image, ImageBytes, nullptr, "test");
    device.WaitIdle();
    Require(scenario.words[FarDword] == Stored, what + "the pending image's texel outside the refreshed image was not stored");
    Require(!addressBased || scenario.words[OverwrittenDword] == Overwritten, what + "the pending image's store put its older texels over a newer address-based store");
}

void RunLocked(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    const std::string what = "storage pre-sync writer, dispatch built under the GPU mutex: ";
    StoreRefreshed(device, scenario);
    StoreImage(device, scenario, true, false);
    Require(AddressBasedStorePending(scenario.image, true), what + "the address-based store was not pending in the open batch");
    const auto before = StorageTexture::WriterWaitCounts();
    const auto locked = LockedWriterWaits();
    Record(device, scenario.refresh, RefreshImageCode);
    Require(StorageTexture::WriterWaitCounts().openLease == before.openLease + 1 && LockedWriterWaits() == locked + 1, what + "the storage refresh of the cached build did not wait once under the GPU mutex for the open batch that only leases the image's memory");
    RequireStored(device, scenario, what, true);
}

void RunPresync(AgcDriver::VulkanDevice& device, const Scenario& scenario, bool submit, const char* name) {
    const std::string what = std::string("storage pre-sync writer, ") + name + ": ";
    StoreRefreshed(device, scenario);
    StoreImage(device, scenario, true, submit);
    Require(AddressBasedStorePending(scenario.image, !submit), what + "the address-based store was not pending in the expected batch");
    Require(!AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(scenario.image, RefreshedBytes), what + "recorded work notes a write over the refreshed image, so the surface pre-sync waits for it");
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(scenario.refresh, {}, Threads, ProgramAddress(RefreshImageCode));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.writerChecks > before.writerChecks, what + "stage A did not list the storage image of the cached build as a writer range");
    Require(after.writerWaits == before.writerWaits + 1 && after.presyncs == before.presyncs + 1, what + "stage A did not wait for the address-based store before the GPU mutex");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto locked = LockedWriterWaits();
        device.Dispatch(scenario.refresh, 1u, 1u, 1u, {}, ProgramAddress(RefreshImageCode), std::move(prepared));
        Require(LockedWriterWaits() == locked, what + "the storage refresh waited for the address-based store under the GPU mutex after the pre-sync waited for it");
    }
    RequireStored(device, scenario, what, true);
}

void RunNoWriter(AgcDriver::VulkanDevice& device, const Scenario& scenario) {
    const std::string what = "storage pre-sync writer, no writer: ";
    StoreRefreshed(device, scenario);
    const auto idle = AgcDriver::VulkanDevice::PresyncCounts();
    auto prepared = device.PrepareDispatch(scenario.refresh, {}, Threads, ProgramAddress(RefreshImageCode));
    const auto listed = AgcDriver::VulkanDevice::PresyncCounts();
    Require(listed.writerChecks > idle.writerChecks, what + "stage A did not list the storage image of the cached build as a writer range");
    Require(listed.writerWaits == idle.writerWaits && listed.presyncs == idle.presyncs, what + "stage A waited although nothing was pending");
    StoreRefreshed(device, scenario, std::move(prepared));
    StoreImage(device, scenario, false, false);
    const auto before = AgcDriver::VulkanDevice::PresyncCounts();
    prepared = device.PrepareDispatch(scenario.refresh, {}, Threads, ProgramAddress(RefreshImageCode));
    const auto after = AgcDriver::VulkanDevice::PresyncCounts();
    Require(after.writerChecks > before.writerChecks, what + "stage A did not list the storage image of the cached build as a writer range");
    Require(after.writerWaits == before.writerWaits && after.presyncs == before.presyncs, what + "stage A waited for a pending image that no recorded work writes or leases");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto locked = LockedWriterWaits();
        device.Dispatch(scenario.refresh, 1u, 1u, 1u, {}, ProgramAddress(RefreshImageCode), std::move(prepared));
        Require(LockedWriterWaits() == locked, what + "the storage refresh waited under the GPU mutex although no recorded work writes or leases the image's memory");
    }
    RequireStored(device, scenario, what, false);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->CanWaitUnlocked()) {
            std::puts("skipped, the device has no timeline semaphores: the pre-sync is not tested");
            return VulkanTestSkipped;
        }
        const bool watched = AgcDriver::GuestMemory::WriteWatched();
        GuestBlock lockedBlock(watched);
        GuestBlock openBlock(watched);
        GuestBlock inFlightBlock(watched);
        GuestBlock idleBlock(watched);
        RunLocked(*device, Prepare(*device, lockedBlock));
        RunPresync(*device, Prepare(*device, openBlock), false, "writer in the open batch");
        if (!watched) std::puts("guest memory has no write watch: the writer-in-flight case is skipped");
        else RunPresync(*device, Prepare(*device, inFlightBlock), true, "writer in flight");
        RunNoWriter(*device, Prepare(*device, idleBlock));
        std::puts("storage pre-sync writer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
