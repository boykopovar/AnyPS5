#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t RecordBytes = 32;
constexpr std::uint32_t CheckedBytes = 64;
constexpr std::uint8_t Fill = 0xa5;

alignas(256) constexpr std::array<std::uint32_t, 22> StraddlingShortStoreCode{
    0x7da40080, 0x7e0202ff, 0x00001122, 0xe0684001, 0x80000100, 0x7e0202ff, 0x00003344, 0xe0684006, 0x80000100, 0x7e0202ff, 0x00005566,
    0xe068400b, 0x80000100, 0x7e0202ff, 0x00007788, 0xe0680013, 0x80000100, 0x7e0202ff, 0x00000099, 0xe0604017, 0x80000100, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "straddling short store: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }
    ~GuestBlock() { GuestAllocations::Mutation().Remove(block); }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Bytes() { return block; }

private:
    std::uint8_t* block = nullptr;
};

bool Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t misalignment, bool atomics) {
    auto* bytes = guest.Bytes();
    std::memset(bytes, Fill, CheckedBytes);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes)) + misalignment;
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, RecordBytes, 0x31016facu};
    const std::span<const std::uint32_t> code(StraddlingShortStoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.target.coherentBufferAtomics = atomics;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::array<std::uint8_t, CheckedBytes> expected{};
    expected.fill(Fill);
    for (const auto [offset, value] : std::array<std::pair<std::uint32_t, std::uint8_t>, 9>{{{1, 0x22}, {2, 0x11}, {6, 0x44}, {7, 0x33}, {11, 0x66}, {12, 0x55}, {19, 0x88}, {20, 0x77}, {23, 0x99}}}) expected[misalignment + offset] = value;
    bool passed = true;
    for (std::uint32_t index = 0; index < CheckedBytes; ++index) {
        if (bytes[index] == expected[index]) continue;
        std::printf("base misalignment %u, coherent buffer atomics %s: byte %u of the V# is %02x, expected %02x\n", misalignment, atomics ? "on" : "off", index - misalignment, bytes[index], expected[index]);
        passed = false;
    }
    return passed;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        bool passed = true;
        for (const bool atomics : {false, true}) {
            for (std::uint32_t misalignment = 0; misalignment < 4u; ++misalignment) passed = Run(*device, guest, misalignment, atomics) && passed;
        }
        Require(passed, "buffer_store_short at a V# offset whose low two bits are 3 did not write both of its bytes, or a short or byte store wrote a byte it does not cover");
        std::puts("straddling short store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
