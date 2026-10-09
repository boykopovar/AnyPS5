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
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Sentinel = 0x05e471e1;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 24> FlatCacheBitsCode{
    0x34020084, 0x34040084, 0xd70f6a1e, 0x02020208, 0x7e3e0209, 0x503e3e80, 0xd70f6a1e, 0x02023c8c,
    0x503e3e80, 0xdc379000, 0x0a080001, 0xdc331000, 0x0d7d001e, 0xbf8c0070, 0xdc739004, 0x00080d01,
    0xdccb9008, 0x0c080a01, 0xdc731000, 0x007d0b1e, 0xbf8c0070, 0xe0781000, 0x80010a02, 0xbf810000
};

struct Row {
    std::array<std::uint32_t, 4> memory;
    std::array<std::uint32_t, Results> results;
    std::array<std::uint32_t, 4> final;
};

constexpr std::array<Row, 32> Rows{{
    {{0x00000001, 0x0000007f, 0x00000080, 0x7fffffff}, {0x00000001, 0x0000007f, 0x00000080, 0x7fffffff}, {0x00000001, 0x7fffffff, 0x00000081, 0x0000007f}},
    {{0x0000007f, 0x000000ff, 0x80000000, 0x80ff7f01}, {0x0000007f, 0x000000ff, 0x80000000, 0x80ff7f01}, {0x0000007f, 0x80ff7f01, 0x8000007f, 0x000000ff}},
    {{0x00000080, 0x80000000, 0xdeadbeef, 0x000000ff}, {0x00000080, 0x80000000, 0xdeadbeef, 0x000000ff}, {0x00000080, 0x000000ff, 0xdeadbf6f, 0x80000000}},
    {{0x000000ff, 0x12345678, 0x00000001, 0xdeadbeef}, {0x000000ff, 0x12345678, 0x00000001, 0xdeadbeef}, {0x000000ff, 0xdeadbeef, 0x00000100, 0x12345678}},
    {{0x7fffffff, 0x80ff7f01, 0x000000ff, 0x00000080}, {0x7fffffff, 0x80ff7f01, 0x000000ff, 0x00000080}, {0x7fffffff, 0x00000080, 0x800000fe, 0x80ff7f01}},
    {{0x80000000, 0x00000001, 0xffffffff, 0x12345678}, {0x80000000, 0x00000001, 0xffffffff, 0x12345678}, {0x80000000, 0x12345678, 0x7fffffff, 0x00000001}},
    {{0xffffffff, 0x00000080, 0x80ff7f01, 0x0000007f}, {0xffffffff, 0x00000080, 0x80ff7f01, 0x0000007f}, {0xffffffff, 0x0000007f, 0x80ff7f00, 0x00000080}},
    {{0x12345678, 0x7fffffff, 0x0000007f, 0xffffffff}, {0x12345678, 0x7fffffff, 0x0000007f, 0xffffffff}, {0x12345678, 0xffffffff, 0x123456f7, 0x7fffffff}},
    {{0xdeadbeef, 0xffffffff, 0x7fffffff, 0x00000001}, {0xdeadbeef, 0xffffffff, 0x7fffffff, 0x00000001}, {0xdeadbeef, 0x00000001, 0x5eadbeee, 0xffffffff}},
    {{0x80ff7f01, 0xdeadbeef, 0x12345678, 0x80000000}, {0x80ff7f01, 0xdeadbeef, 0x12345678, 0x80000000}, {0x80ff7f01, 0x80000000, 0x9333d579, 0xdeadbeef}},
    {{0x00000000, 0x00000000, 0x00000000, 0x00000000}, {0x00000000, 0x00000000, 0x00000000, 0x00000000}, {0x00000000, 0x00000000, 0x00000000, 0x00000000}},
    {{0x00000001, 0x0000007f, 0x00000080, 0x7fffffff}, {0x00000001, 0x0000007f, 0x00000080, 0x7fffffff}, {0x00000001, 0x7fffffff, 0x00000081, 0x0000007f}},
    {{0x0000007f, 0x000000ff, 0x80000000, 0x80ff7f01}, {0x0000007f, 0x000000ff, 0x80000000, 0x80ff7f01}, {0x0000007f, 0x80ff7f01, 0x8000007f, 0x000000ff}},
    {{0x00000080, 0x80000000, 0xdeadbeef, 0x000000ff}, {0x00000080, 0x80000000, 0xdeadbeef, 0x000000ff}, {0x00000080, 0x000000ff, 0xdeadbf6f, 0x80000000}},
    {{0x000000ff, 0x12345678, 0x00000001, 0xdeadbeef}, {0x000000ff, 0x12345678, 0x00000001, 0xdeadbeef}, {0x000000ff, 0xdeadbeef, 0x00000100, 0x12345678}},
    {{0x7fffffff, 0x80ff7f01, 0x000000ff, 0x00000080}, {0x7fffffff, 0x80ff7f01, 0x000000ff, 0x00000080}, {0x7fffffff, 0x00000080, 0x800000fe, 0x80ff7f01}},
    {{0x2265b1f5, 0x91b7584a, 0xd8f16adf, 0xcd613e30}, {0x2265b1f5, 0x91b7584a, 0xd8f16adf, 0xcd613e30}, {0x2265b1f5, 0xcd613e30, 0xfb571cd4, 0x91b7584a}},
    {{0xc386bbc4, 0x1027c4d1, 0x414c343c, 0x1e2feb89}, {0xc386bbc4, 0x1027c4d1, 0x414c343c, 0x1e2feb89}, {0xc386bbc4, 0x1e2feb89, 0x04d2f000, 0x1027c4d1}},
    {{0x7ed4d57b, 0xc2ce6f44, 0x7311d8a3, 0x78e51061}, {0x7ed4d57b, 0xc2ce6f44, 0x7311d8a3, 0x78e51061}, {0x7ed4d57b, 0x78e51061, 0xf1e6ae1e, 0xc2ce6f44}},
    {{0xa6cecc1b, 0x612e7696, 0xc9e9c616, 0x35bf992d}, {0xa6cecc1b, 0x612e7696, 0xc9e9c616, 0x35bf992d}, {0xa6cecc1b, 0x35bf992d, 0x70b89231, 0x612e7696}},
    {{0x18072e8c, 0x7ce42c82, 0x0741c7a8, 0xe4b06ce6}, {0x18072e8c, 0x7ce42c82, 0x0741c7a8, 0xe4b06ce6}, {0x18072e8c, 0xe4b06ce6, 0x1f48f634, 0x7ce42c82}},
    {{0xd5f4b3b2, 0x63ca828d, 0x6ec9d286, 0x9b810e76}, {0xd5f4b3b2, 0x63ca828d, 0x6ec9d286, 0x9b810e76}, {0xd5f4b3b2, 0x9b810e76, 0x44be8638, 0x63ca828d}},
    {{0xc324c985, 0xc4647159, 0x008a05a6, 0xb2221a58}, {0xc324c985, 0xc4647159, 0x008a05a6, 0xb2221a58}, {0xc324c985, 0xb2221a58, 0xc3aecf2b, 0xc4647159}},
    {{0x7204e52d, 0x442e3d43, 0xb8b6d8fe, 0xcd447e35}, {0x7204e52d, 0x442e3d43, 0xb8b6d8fe, 0xcd447e35}, {0x7204e52d, 0xcd447e35, 0x2abbbe2b, 0x442e3d43}},
    {{0x3a902931, 0x9755d4c1, 0xf1fd42a2, 0x1a2b8f1f}, {0x3a902931, 0x9755d4c1, 0xf1fd42a2, 0x1a2b8f1f}, {0x3a902931, 0x1a2b8f1f, 0x2c8d6bd3, 0x9755d4c1}},
    {{0xe6c3f339, 0x51431193, 0x07d4bedc, 0x05b6e6e3}, {0xe6c3f339, 0x51431193, 0x07d4bedc, 0x05b6e6e3}, {0xe6c3f339, 0x05b6e6e3, 0xee98b215, 0x51431193}},
    {{0x06839eb9, 0xa648a7dd, 0x8a9a021e, 0x025b413f}, {0x06839eb9, 0xa648a7dd, 0x8a9a021e, 0x025b413f}, {0x06839eb9, 0x025b413f, 0x911da0d7, 0xa648a7dd}},
    {{0xf06c144a, 0xe1988ad9, 0x619699cf, 0xafbd67f9}, {0xf06c144a, 0xe1988ad9, 0x619699cf, 0xafbd67f9}, {0xf06c144a, 0xafbd67f9, 0x5202ae19, 0xe1988ad9}},
    {{0x37730edf, 0xf8130c42, 0x6c0fd4f5, 0xb9d179e0}, {0x37730edf, 0xf8130c42, 0x6c0fd4f5, 0xb9d179e0}, {0x37730edf, 0xb9d179e0, 0xa382e3d4, 0xf8130c42}},
    {{0x076f3787, 0x8712b8bc, 0x38c0c8fd, 0xc381e88f}, {0x076f3787, 0x8712b8bc, 0x38c0c8fd, 0xc381e88f}, {0x076f3787, 0xc381e88f, 0x40300084, 0x8712b8bc}},
    {{0x701966a0, 0xf06d3fef, 0x7eed8d14, 0x8d88348a}, {0x701966a0, 0xf06d3fef, 0x7eed8d14, 0x8d88348a}, {0x701966a0, 0x8d88348a, 0xef06f3b4, 0xf06d3fef}},
    {{0x3bab6c39, 0x587fd280, 0x3b1a11df, 0xad45f23d}, {0x3bab6c39, 0x587fd280, 0x3b1a11df, 0xad45f23d}, {0x3bab6c39, 0xad45f23d, 0x76c57e18, 0x587fd280}}
}};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "flat cache bits: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", value);
    return text;
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const std::uint8_t* base) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    std::vector<std::uint32_t> userData(10, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[8] = static_cast<std::uint32_t>(address);
    userData[9] = static_cast<std::uint32_t>(address >> 32u);
    const std::span<const std::uint32_t> code(FlatCacheBitsCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize) {
    std::memset(guest.Data(), Fill, BlockBytes);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::memcpy(guest.Data() + tid * 16u, Rows[tid % Rows.size()].memory.data(), 16u);
    }
    Output.fill(Sentinel);
    Dispatch(device, waveSize, guest.Data());
    const auto wave = "flat cache bits: wave" + std::to_string(waveSize) + " lane ";
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& row = Rows[tid % Rows.size()];
        for (std::uint32_t result = 0; result < Results; ++result) {
            const auto actual = Output[tid * Results + result];
            Require(actual == row.results[result], wave + std::to_string(tid) + " v" + std::to_string(10u + result) + " is " + Hex(actual) + ", expected " + Hex(row.results[result]));
        }
        std::array<std::uint32_t, 4> memory{};
        std::memcpy(memory.data(), guest.Data() + tid * 16u, 16u);
        for (std::uint32_t dword = 0; dword < 4u; ++dword) {
            Require(memory[dword] == row.final[dword], wave + std::to_string(tid) + " memory dword " + std::to_string(dword) + " is " + Hex(memory[dword]) + ", expected " + Hex(row.final[dword]));
        }
    }
    for (std::size_t offset = Threads * 16u; offset < BlockBytes; ++offset) {
        Require(guest.Data()[offset] == Fill, wave + "store outside the rows changed byte " + std::to_string(offset));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        Run(*device, guest, 32);
        std::puts("flat cache bits tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
