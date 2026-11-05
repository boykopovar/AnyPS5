#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 64;
constexpr std::uint32_t Operations = 11;
constexpr std::uint32_t Format32_32UInt = 62;
constexpr std::uint32_t Format32_32SInt = 63;
constexpr std::uint32_t Format32_32Float = 64;
constexpr std::uint32_t Type2D = 9;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};
alignas(4096) std::array<std::uint32_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 219> Code{
    0x34020085, 0x34060088, 0xe0381000, 0x80000401, 0xe0341010, 0x80001001, 0xbf8c3f70, 0x7e100300,
    0x7e120280, 0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307, 0xf03c2308,
    0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70, 0xe0741000, 0x80010c03,
    0xe0741008, 0x80011203, 0x7e100300, 0x7e120281, 0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08,
    0x7e180306, 0x7e1a0307, 0xf0442308, 0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208,
    0xbf8c3f70, 0xe0741010, 0x80010c03, 0xe0741018, 0x80011203, 0x7e100300, 0x7e120282, 0x7e140304,
    0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307, 0xf0482308, 0x00020c08, 0x7e240280,
    0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70, 0xe0741020, 0x80010c03, 0xe0741028, 0x80011203,
    0x7e100300, 0x7e120283, 0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307,
    0xf0502308, 0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70, 0xe0741030,
    0x80010c03, 0xe0741038, 0x80011203, 0x7e100300, 0x7e120284, 0x7e140304, 0x7e160305, 0xf03c0308,
    0x00020a08, 0x7e180306, 0x7e1a0307, 0xf0542308, 0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308,
    0x00021208, 0xbf8c3f70, 0xe0741040, 0x80010c03, 0xe0741048, 0x80011203, 0x7e100300, 0x7e120285,
    0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307, 0xf0582308, 0x00020c08,
    0x7e240280, 0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70, 0xe0741050, 0x80010c03, 0xe0741058,
    0x80011203, 0x7e100300, 0x7e120286, 0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306,
    0x7e1a0307, 0xf05c2308, 0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70,
    0xe0741060, 0x80010c03, 0xe0741068, 0x80011203, 0x7e100300, 0x7e120287, 0x7e140304, 0x7e160305,
    0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307, 0xf0602308, 0x00020c08, 0x7e240280, 0x7e260280,
    0xf0442308, 0x00021208, 0xbf8c3f70, 0xe0741070, 0x80010c03, 0xe0741078, 0x80011203, 0x7e100300,
    0x7e120288, 0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307, 0xf0642308,
    0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70, 0xe0741080, 0x80010c03,
    0xe0741088, 0x80011203, 0x7e100300, 0x7e120289, 0x7e140304, 0x7e160305, 0xf03c0308, 0x00020a08,
    0x7e180306, 0x7e1a0307, 0xf0682308, 0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208,
    0xbf8c3f70, 0xe0741090, 0x80010c03, 0xe0741098, 0x80011203, 0x7e100300, 0x7e12028a, 0x7e140304,
    0x7e160305, 0xf03c0308, 0x00020a08, 0x7e180306, 0x7e1a0307, 0x7e1c0310, 0x7e1e0311, 0xf0402f08,
    0x00020c08, 0x7e240280, 0x7e260280, 0xf0442308, 0x00021208, 0xbf8c3f70, 0xe07410a0, 0x80010c03,
    0xe07410a8, 0x80011203, 0xbf810000,
};

constexpr std::array<std::array<std::uint64_t, 2>, 12> Edges{{
    {0u, 0u}, {0u, 5u}, {5u, 0u}, {0x00000000ffffffffull, 1u},
    {0x0000000100000000ull, 1u}, {0x0000000100000000ull, 0x00000000ffffffffull}, {0x8000000000000000ull, 0x7fffffffffffffffull}, {0x7fffffffffffffffull, 0x8000000000000000ull},
    {0xffffffffffffffffull, 1u}, {1u, 0xffffffffffffffffull}, {0xffffffff00000000ull, 0x00000000ffffffffull}, {0x7fffffff00000000ull, 0x80000000ffffffffull},
}};

std::uint64_t Word(const std::uint32_t* words) {
    return words[0] | static_cast<std::uint64_t>(words[1]) << 32u;
}

void SetWord(std::uint32_t* words, std::uint64_t value) {
    words[0] = static_cast<std::uint32_t>(value);
    words[1] = static_cast<std::uint32_t>(value >> 32u);
}

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            SetWord(words + 2u * j, state ^ (state << 29u));
        }
        if (tid < Edges.size()) {
            SetWord(words, Edges[tid][0]);
            SetWord(words + 2, Edges[tid][1]);
        }
        if ((tid & 1u) != 0u) {
            SetWord(words + 4, Word(words));
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t width, std::uint32_t height, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t format) {
    Output.fill(0xdeadbeefu);
    Texels.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto texture = TextureDescriptor(Texels.data(), Threads, Operations, format);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
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

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

void Check(std::uint32_t format) {
    constexpr std::array<const char*, Operations> names{"image_atomic_swap", "image_atomic_add", "image_atomic_sub", "image_atomic_smin", "image_atomic_umin", "image_atomic_smax", "image_atomic_umax", "image_atomic_and", "image_atomic_or", "image_atomic_xor", "image_atomic_cmpswap"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const std::uint64_t a = Word(in);
        const std::uint64_t b = Word(in + 2);
        const std::uint64_t c = Word(in + 4);
        const auto sa = static_cast<std::int64_t>(a);
        const auto sb = static_cast<std::int64_t>(b);
        const std::array<std::uint64_t, Operations> expected{
            b,
            a + b,
            a - b,
            static_cast<std::uint64_t>(std::min(sa, sb)),
            std::min(a, b),
            static_cast<std::uint64_t>(std::max(sa, sb)),
            std::max(a, b),
            a & b,
            a | b,
            a ^ b,
            a == c ? b : a,
        };
        for (std::uint32_t j = 0; j < Operations; ++j) {
            const std::uint64_t returned = Word(&Output[tid * Results + 4u * j]);
            const std::uint64_t stored = Word(&Output[tid * Results + 4u * j + 2u]);
            Require(returned == a, std::string(names[j]) + ", format " + std::to_string(format) + ": thread " + std::to_string(tid) + " returned " + Hex(returned) + ", expected " + Hex(a));
            Require(stored == expected[j], std::string(names[j]) + ", format " + std::to_string(format) + ": thread " + std::to_string(tid) + " stored " + Hex(stored) + ", expected " + Hex(expected[j]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityInt64ImageEXT)) {
            std::puts("skipped, the device has no shaderImageInt64Atomics");
            return VulkanTestSkipped;
        }
        FillInput();
        for (const auto format : {Format32_32UInt, Format32_32SInt, Format32_32Float}) {
            Run(*device, format);
            Check(format);
        }
        std::puts("64-bit image atomics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
