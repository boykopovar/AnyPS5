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
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 6;
constexpr std::uint32_t QueryThreads = 32;
constexpr std::uint32_t Words = 8;
constexpr std::uint32_t ResultWord = 4;
constexpr std::uint32_t Size = 64;
constexpr std::uint32_t Slices = 8;
constexpr std::uint32_t Faces = 6;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TypeCube = 11;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint32_t, QueryThreads * 4> Query{};
alignas(256) std::array<std::uint8_t, Size * Size * 4 * Slices> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> SampleCode{
    0x34020085, 0xe03c1000, 0x80000201, 0xbf8c3f70, 0xf09c0f18, 0x00610502, 0xbf8c3f70, 0xe0781010, 0x80000501, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> LoadCode{
    0x7e02020d, 0x7e04020c, 0xf0001118, 0x00010300, 0xbf8c3f70, 0x34080082, 0xe0701000, 0x80000304, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> MipLoadCode{
    0x7e02020d, 0x7e04020c, 0xf0041118, 0x00010500, 0xbf8c3f70, 0x34080082, 0xe0701000, 0x80000504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> ResinfoCode{
    0x34060084, 0x7e020280, 0xf0380f18, 0x00010a01, 0xbf8c3f70, 0xe0781000, 0x80000a03, 0xbf810000,
};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t baseArray, std::uint32_t depth) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (TypeCube << 28u),
        depth | (baseArray << 16u),
        0u,
        0u,
        0u,
    };
}

void FillTexture() {
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(0u, Slices - 1u)));
    Require(surface.layers == Slices && surface.guestBytes <= Texels.size(), "image sample short cube: the slices do not fit the texel storage");
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        const auto value = Bits(static_cast<float>(slice + 1u));
        for (std::uint64_t offset = 0; offset < surface.layerBytes; offset += 4u) {
            std::copy_n(reinterpret_cast<const std::uint8_t*>(&value), 4u, &Texels[surface.GuestLayerOffset(slice) + offset]);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> BilinearSampler() {
    return {0u, 0xfffu << 12u, (1u << 20u) | (1u << 22u) | (1u << 26u), 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t baseArray, std::uint32_t depth, std::uint32_t* output, std::uint32_t outputBytes, const std::array<std::uint32_t, 4>& tail, std::uint32_t threads) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(output, outputBytes);
    const auto texture = TextureDescriptor(baseArray, depth);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(tail.begin(), tail.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::string Row(std::uint32_t baseArray, std::uint32_t depth) {
    return " with BASE_ARRAY " + std::to_string(baseArray) + " and DEPTH " + std::to_string(depth);
}

void CheckSamples(AgcDriver::VulkanDevice& device, std::uint32_t baseArray, std::uint32_t depth) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Buffer[tid * Words] = Bits(1.5f);
        Buffer[tid * Words + 1u] = Bits(1.5f);
        Buffer[tid * Words + 2u] = Bits(static_cast<float>(tid));
    }
    Run(device, SampleCode, baseArray, depth, Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u), BilinearSampler(), Threads);
    for (std::uint32_t face = 0; face < Threads; ++face) {
        const float value = std::bit_cast<float>(Buffer[face * Words + ResultWord]);
        const float expected = static_cast<float>(baseArray + face + 1u);
        Require(value == expected, "image_sample_lz cube" + Row(baseArray, depth) + ": face " + std::to_string(face) + " read " + std::to_string(value) + ", expected slice " + std::to_string(baseArray + face) + " (" + std::to_string(expected) + ")");
    }
}

void CheckLoads(AgcDriver::VulkanDevice& device, std::uint32_t baseArray, std::uint32_t depth, std::uint32_t y) {
    for (std::uint32_t face = 0; face < Faces; ++face) {
        Buffer.fill(0xdeadbeefu);
        Run(device, LoadCode, baseArray, depth, Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u), {face, y, 0u, 0u}, Threads);
        const auto slice = baseArray + face;
        const float expected = slice <= depth ? static_cast<float>(slice + 1u) : 0.0f;
        const std::string where = "image_load cube" + Row(baseArray, depth) + " at y " + std::to_string(y) + ", face " + std::to_string(face) + ": ";
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const float value = std::bit_cast<float>(Buffer[tid]);
            Require(value == expected, where + "lane " + std::to_string(tid) + " read " + std::to_string(value) + ", expected " + std::to_string(expected) + (slice <= depth ? " (slice " + std::to_string(slice) + ")" : " (slice " + std::to_string(slice) + " is past DEPTH)"));
        }
    }
}

void CheckResinfo(AgcDriver::VulkanDevice& device, std::uint32_t baseArray, std::uint32_t depth) {
    Query.fill(0xdeadbeefu);
    Run(device, ResinfoCode, baseArray, depth, Query.data(), static_cast<std::uint32_t>(Query.size() * 4u), {0u, 0u, 0u, 0u}, QueryThreads);
    const std::string where = "image_get_resinfo cube" + Row(baseArray, depth) + ": ";
    Require(Query[0] == Size && Query[1] == Size, where + "extent " + std::to_string(Query[0]) + "x" + std::to_string(Query[1]) + ", expected " + std::to_string(Size) + "x" + std::to_string(Size));
    const auto expected = depth - baseArray + 1u;
    Require(Query[2] == expected, where + "depth component " + std::to_string(Query[2]) + ", expected " + std::to_string(expected));
    Require(Query[3] == 1u, where + "level count " + std::to_string(Query[3]) + ", expected 1");
}

void CheckMipLoadRejected(AgcDriver::VulkanDevice& device) {
    Buffer.fill(0u);
    try {
        Run(device, MipLoadCode, 0u, 4u, Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u), {0u, 0u, 0u, 0u}, Threads);
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find("fewer than six slices") != std::string_view::npos, std::string("image_load_mip through a cube with five slices failed for another reason: ") + error.what());
        return;
    }
    throw std::runtime_error("image_load_mip through a cube with five slices was accepted, but hardware loads stop at DEPTH");
}

constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 6> ShortRows{{{0u, 4u}, {0u, 3u}, {0u, 1u}, {1u, 3u}, {2u, 5u}, {2u, 6u}}};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
        for (const auto [baseArray, depth] : {std::pair{0u, 5u}, std::pair{0u, 0u}, std::pair{0u, 4u}, std::pair{0u, 3u}, std::pair{0u, 1u}, std::pair{2u, 6u}, std::pair{2u, 5u}}) CheckSamples(*device, baseArray, depth);
        for (const auto [baseArray, depth] : ShortRows) {
            for (const auto y : {0u, Size - 1u}) CheckLoads(*device, baseArray, depth, y);
            CheckResinfo(*device, baseArray, depth);
        }
        CheckMipLoadRejected(*device);
        std::puts("image sample short cube tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
