#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::size_t ImageBytes = Width * Height * 4u;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Initial = 0x3f800000u;
constexpr std::uint32_t Loaded = 0x5a5a1234u;
constexpr std::uint32_t Stored = 0x22222222u;
constexpr std::size_t SourceOffset = BlockBytes / 2u;
constexpr std::size_t LoadedOffset = SourceOffset + 0x40u;
constexpr std::size_t QueryOffset = SourceOffset + 0x10000u;

alignas(256) constexpr std::array<std::uint32_t, 10> LoadToImageCode{0x7e000200u, 0x7e020201u, 0xdc308040u, 0x027d0000u, 0xbf8c3f70u, 0x7e000280u, 0x7e020280u, 0xf0200108u, 0x00010200u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> StoreGlobalCode{0x7e000208u, 0x7e020209u, 0x7e0402ffu, Stored, 0xdc708100u, 0x007d0200u, 0xbf810000u};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "read-only lease writer: cannot allocate the guest block");
        std::fill(Words(), Words() + BlockBytes / 4u, Initial);
        Words()[LoadedOffset / 4u] = Loaded;
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

    std::uint32_t* Words() const { return reinterpret_cast<std::uint32_t*>(block); }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::vector<std::uint32_t> LoadUserData(std::uint64_t image, std::uint64_t source) {
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = static_cast<std::uint32_t>(source);
    userData[1] = static_cast<std::uint32_t>(source >> 32u);
    userData[4] = static_cast<std::uint32_t>(image >> 8u);
    userData[5] = static_cast<std::uint32_t>((image >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Width - 1u) & 3u) << 30u);
    userData[6] = ((Width - 1u) >> 2u) | ((Height - 1u) << 14u);
    userData[7] = 0xfacu | (Type2D << 28u);
    return userData;
}

std::vector<std::uint32_t> StoreUserData(std::uint64_t base) {
    std::vector<std::uint32_t> userData(10, 0u);
    userData[8] = static_cast<std::uint32_t>(base);
    userData[9] = static_cast<std::uint32_t>(base >> 32u);
    return userData;
}

struct Pending {
    bool read;
    std::uint64_t writer;
    std::uint64_t open;
};

Pending PendingAt(std::uint64_t address) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "read-only lease writer: no active recorder");
    return {recorder->DescribePendingRead(address, 64u).has_value(), recorder->PendingWriterSerial(address, 64u), recorder->OpenSerial()};
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool counted = std::getenv("APS5_NO_READ_ONLY_LEASE_SKIP") != nullptr;
        GuestBlock guest;
        const auto target = device->Target();
        const auto query = guest.Address() + QueryOffset;
        device->WaitIdle();

        const auto reader = Compile(LoadToImageCode, LoadUserData(guest.Address(), guest.Address() + SourceOffset), target);
        device->Dispatch(reader, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(LoadToImageCode.data()));
        const auto afterRead = PendingAt(query);
        Require(afterRead.read, "read-only lease writer: the build that only loads through BDA noted no lease over the guest block");
        if (counted) Require(afterRead.writer == afterRead.open, "read-only lease writer: APS5_NO_READ_ONLY_LEASE_SKIP did not count the read-only lease as a writer");
        else Require(afterRead.writer == 0, "read-only lease writer: the lease of a build that never stores through BDA counts as a writer (serial " + std::to_string(afterRead.writer) + ")");
        Require(AgcDriver::Graphics::StorageTexture::FlushPending(guest.Address(), ImageBytes, nullptr, "test"), "read-only lease writer: the read-only build's image store left no results pending");
        device->WaitIdle();
        Require(guest.Words()[0] == Loaded, "read-only lease writer: the read-only build did not store the loaded word into its image");

        const auto storer = Compile(StoreGlobalCode, StoreUserData(guest.Address() + SourceOffset), target);
        device->Dispatch(storer, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(StoreGlobalCode.data()));
        const auto afterStore = PendingAt(query);
        Require(afterStore.writer == afterStore.open, "read-only lease writer: the lease of a build that stores through BDA does not count as a writer");
        device->WaitIdle();
        Require(guest.Words()[(SourceOffset + 0x100u) / 4u] == Stored, "read-only lease writer: the storing build's word did not reach guest memory");

        std::puts("read-only lease writer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
