#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Size = 8;
constexpr std::uint32_t Faces = 6;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t Result = 8;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 65536> Cube{};

alignas(256) constexpr std::array<std::uint32_t, 30> Code{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xbf8c3f70, 0xf09c0108, 0x00610c02, 0xf0900108, 0x00610d02,
    0xf0800108, 0x00610e02, 0xf0001108, 0x00010f05, 0xbf8c3f70, 0xe0701020, 0x80000c01, 0xe0701024,
    0x80000d01, 0xe0701028, 0x80000e01, 0xe070102c, 0x80000f01, 0xbf810000,
};

struct Lane {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t loadX;
    std::uint32_t loadY;
};

Lane LaneOf(std::uint32_t tid) {
    return {tid % Size, (tid * 3u) % Size, (tid * 5u) % Size, (tid * 7u + 1u) % Size};
}

float TexelValue(std::uint32_t face, std::uint32_t x, std::uint32_t y) {
    return static_cast<float>(face * 1000u + y * 16u + x);
}

std::array<std::uint32_t, 8> CubeDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Cube.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (TypeCube << 28u),
        Faces - 1u,
        0u,
        0u,
        0u,
    };
}

void FillCube() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(CubeDescriptor()));
    Require(geometry.guestBytes <= Cube.size(), "image plane address on a cube: the texture does not fit the texel storage");
    Require(geometry.layers == Faces, "image plane address on a cube: the cube does not have six faces");
    const auto& mip = geometry.mips.at(0);
    std::fill(Cube.begin(), Cube.end(), 0u);
    for (std::uint32_t face = 0; face < Faces; ++face) {
        for (std::uint32_t y = 0; y < Size; ++y) {
            for (std::uint32_t x = 0; x < Size; ++x) {
                const float value = TexelValue(face, x, y);
                std::memcpy(&Cube[geometry.GuestLayerOffset(face) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u], &value, 4u);
            }
        }
    }
}

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid);
        const float u = 1.0f + (static_cast<float>(lane.x) + 0.5f) / static_cast<float>(Size);
        const float v = 1.0f + (static_cast<float>(lane.y) + 0.5f) / static_cast<float>(Size);
        const std::array<std::uint32_t, 5> words{std::bit_cast<std::uint32_t>(u), std::bit_cast<std::uint32_t>(v), 0u, lane.loadX, lane.loadY};
        std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = CubeDescriptor();
    const std::array<std::uint32_t, 4> sampler{0u, 0xfffu << 12u, 0u, 0u};
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Check() {
    constexpr std::array<const char*, 4> names{"image_sample_lz", "image_sample_l", "image_sample", "image_load"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto lane = LaneOf(tid);
        const std::array<float, 4> expected{TexelValue(0u, lane.x, lane.y), TexelValue(0u, lane.x, lane.y), TexelValue(0u, lane.x, lane.y), TexelValue(0u, lane.loadX, lane.loadY)};
        for (std::uint32_t op = 0; op < expected.size(); ++op) {
            const float value = std::bit_cast<float>(Buffer[tid * Words + Result + op]);
            Require(value == expected[op], std::string("image plane address on a cube: ") + names[op] + " 2d on a cube texture, thread " + std::to_string(tid) + " is " + std::to_string(value) + ", expected " + std::to_string(expected[op]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillCube();
        FillInput();
        Run(*device);
        Check();
        std::puts("image plane address on cube tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
