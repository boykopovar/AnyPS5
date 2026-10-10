#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Operations = 6;
constexpr std::uint32_t TextureHeight = 8;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};
alignas(4096) std::array<std::uint32_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 102> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xbf8c3f70, 0x7e100300, 0x7e120280, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0482108,
    0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701000, 0x80010b03, 0xe0701004,
    0x80010c03, 0x7e120281, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0502108, 0x00020b08,
    0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701008, 0x80010b03, 0xe070100c, 0x80010c03,
    0x7e120282, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0582108, 0x00020b08, 0x7e180280,
    0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701010, 0x80010b03, 0xe0701014, 0x80010c03, 0x7e120283,
    0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf06c2108, 0x00020b08, 0x7e180280, 0xf0442108,
    0x00020c08, 0xbf8c3f70, 0xe0701018, 0x80010b03, 0xe070101c, 0x80010c03, 0x7e120284, 0x7e140304,
    0xf03c0108, 0x00020a08, 0x7e160305, 0xf0702108, 0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08,
    0xbf8c3f70, 0xe0701020, 0x80010b03, 0xe0701024, 0x80010c03, 0x7e120285, 0x7e140304, 0xf03c0108,
    0x00020a08, 0x7e160305, 0x7e180306, 0xf0402308, 0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08,
    0xbf8c3f70, 0xe0701028, 0x80010b03, 0xe070102c, 0x80010c03, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 2>, 12> Edges{{
    {0u, 0u}, {0u, 5u}, {5u, 0u}, {5u, 5u}, {4u, 5u}, {6u, 5u},
    {0x7fffffffu, 0x80000000u}, {0x80000000u, 0x7fffffffu}, {0xffffffffu, 1u}, {1u, 0xffffffffu},
    {0xffffffffu, 0xffffffffu}, {0xfffffffeu, 0xffffffffu},
}};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = static_cast<std::uint32_t>(state >> 32u);
        }
        if (tid < Edges.size()) {
            words[0] = Edges[tid][0];
            words[1] = Edges[tid][1];
        }
        if ((tid & 1u) != 0u) {
            words[2] = words[0];
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
    const auto texture = TextureDescriptor(Texels.data(), Threads, TextureHeight, format);
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

void Check(std::uint32_t format) {
    constexpr std::array<const char*, Operations> names{"image_atomic_sub", "image_atomic_smin", "image_atomic_smax", "image_atomic_inc", "image_atomic_dec", "image_atomic_cmpswap"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const std::uint32_t a = in[0];
        const std::uint32_t b = in[1];
        const std::uint32_t c = in[2];
        const auto sa = static_cast<std::int32_t>(a);
        const auto sb = static_cast<std::int32_t>(b);
        const std::array<std::uint32_t, Operations> expected{
            a - b,
            static_cast<std::uint32_t>(std::min(sa, sb)),
            static_cast<std::uint32_t>(std::max(sa, sb)),
            a >= b ? 0u : a + 1u,
            (a == 0u || a > b) ? b : a - 1u,
            a == c ? b : a,
        };
        for (std::uint32_t j = 0; j < Operations; ++j) {
            const std::uint32_t returned = Output[tid * Results + 2u * j];
            const std::uint32_t stored = Output[tid * Results + 2u * j + 1u];
            Require(returned == a, std::string(names[j]) + ", format " + std::to_string(format) + ": thread " + std::to_string(tid) + " returned " + std::to_string(returned) + ", expected " + std::to_string(a));
            Require(stored == expected[j], std::string(names[j]) + ", format " + std::to_string(format) + ": thread " + std::to_string(tid) + " stored " + std::to_string(stored) + ", expected " + std::to_string(expected[j]));
        }
    }
}

const Testing::Case uintTexels{"ImageAtomics_R32UintTexels_ReturnOriginalAndStoreResult", [] {
    auto& device = SharedVulkanTestDevice();
    FillInput();
    Run(device, Format32UInt);
    Check(Format32UInt);
}};

const Testing::Case sintTexels{"ImageAtomics_R32SintTexels_ReturnOriginalAndStoreResult", [] {
    auto& device = SharedVulkanTestDevice();
    FillInput();
    Run(device, Format32SInt);
    Check(Format32SInt);
}};

const Testing::Case floatTexels{"ImageAtomics_R32FloatTexels_ReturnOriginalAndStoreResult", [] {
    auto& device = SharedVulkanTestDevice();
    FillInput();
    Run(device, Format32Float);
    Check(Format32Float);
}};

} // namespace
