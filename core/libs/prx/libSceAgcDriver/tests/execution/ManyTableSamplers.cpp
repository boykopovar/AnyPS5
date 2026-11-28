#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Samplers = 128;
constexpr std::uint32_t Direct = 4;
constexpr std::uint32_t TableSamplers = Samplers - Direct;
constexpr std::uint32_t TableOutside = 64;
constexpr std::uint32_t DirectOutside = Samplers - 1u;
constexpr std::uint32_t StageSamplers = 32;
constexpr std::uint32_t SamplerWord = 32;
constexpr std::uint32_t TableSamplerByte = 2048;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> OpaqueBlackBorder{0x1b6u, (4u * 256u) << 12u, 0u, 1u << 30u};
constexpr std::array<std::uint32_t, 4> PointWrap{0x0u, (4u * 256u) << 12u, 0u, 0u};

alignas(256) std::array<float, Width * Height> FloatTexels{};
alignas(4096) std::array<std::uint32_t, 1024> Record{};
alignas(256) std::array<float, Lanes * 16u> Output{};
alignas(256) std::array<std::uint32_t, 128> Srt{};

std::vector<std::uint32_t> SamplerCode() {
    std::vector<std::uint32_t> code{0xf4080100u, 0xfa000040u, 0xf4080700u, 0xfa000020u, 0x7e200500u, 0x8f108c10u, 0xf4280902u, 0x20000000u};
    code.insert(code.end(), {0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0x061402f4u, 0x7e160302u, 0x7e220280u});
    bool first = true;
    for (std::uint32_t sampler = 0; sampler < Samplers; ++sampler) {
        if (sampler < TableSamplers) code.insert(code.end(), {0xf4280802u, 0x20000000u | (TableSamplerByte + sampler * 16u)});
        else code.insert(code.end(), {0xf4080800u, 0xfa000000u | ((SamplerWord + (sampler - TableSamplers) * 4u) * 4u)});
        if (sampler == TableOutside) {
            code.insert(code.end(), {0xf09c8f08u, 0x01090c0au});
            continue;
        }
        if (sampler == DirectOutside) {
            code.insert(code.end(), {0xf09c8f08u, 0x0109140au});
            continue;
        }
        code.insert(code.end(), {0xf09c8f08u, 0x01090401u, first ? 0x7e100304u : 0x06100908u});
        first = false;
    }
    code.insert(code.end(), {0x7e240308u, 0x7e120202u, 0x34201286u, 0x4a202100u, 0x34202084u, 0xe0781000u, 0x80070c10u, 0xe0781400u, 0x80071410u, 0xe0741800u, 0x80071110u, 0xbf810000u});
    return code;
}

const std::vector<std::uint32_t> Code = SamplerCode();

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t stride, std::uint32_t records) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u)};
}

struct Outcome {
    std::vector<float> table;
    std::vector<float> direct;
    std::vector<float> sums;
    std::size_t samplerElements = 0;
    std::vector<std::uint64_t> imageSamplers;
};

Outcome Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 4>& tableOutside, const std::array<std::uint32_t, 4>& directOutside) {
    Srt = {};
    Record = {};
    const auto record = Buffer(Record.data(), static_cast<std::uint32_t>(sizeof(Record)), 1u);
    const auto output = Buffer(Output.data(), 0u, static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    std::copy(record.begin(), record.end(), Srt.begin() + 16);
    const auto image = Image(FloatTexels.data(), Format32Float);
    std::copy(image.begin(), image.end(), Record.begin());
    for (std::uint32_t sampler = 0; sampler < TableSamplers; ++sampler) {
        const auto& words = sampler == TableOutside ? tableOutside : PointClamp;
        std::copy(words.begin(), words.end(), Record.begin() + TableSamplerByte / 4u + sampler * 4u);
    }
    for (std::uint32_t sampler = TableSamplers; sampler < Samplers; ++sampler) {
        const auto& words = sampler == DirectOutside ? directOutside : PointClamp;
        std::copy(words.begin(), words.end(), Srt.begin() + SamplerWord + (sampler - TableSamplers) * 4u);
    }
    const auto rootAddress = reinterpret_cast<std::uintptr_t>(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u)};
    const std::span<const std::uint32_t> span(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 3> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Record.data()), std::as_bytes(std::span(Record))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    Outcome outcome;
    const auto result = ShaderRecompiler::Recompile(request);
    for (const auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers) outcome.samplerElements += binding.guestDescriptor.size() / 4u;
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable) outcome.imageSamplers.insert(outcome.imageSamplers.end(), binding.imageSamplers.begin(), binding.imageSamplers.end());
    }
    Output.fill(-1.0f);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(span.data()));
    device.WaitIdle();
    outcome.table.assign(Output.begin(), Output.begin() + Lanes * 4u);
    outcome.direct.assign(Output.begin() + Lanes * 4u, Output.begin() + Lanes * 8u);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) outcome.sums.push_back(Output[513u + lane * 4u]);
    return outcome;
}

void Fill() {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) FloatTexels[y * Width + x] = static_cast<float>(x) + 100.0f * static_cast<float>(y) + 0.25f;
    }
}

float InsideSum(float texel) {
    float sum = texel;
    for (std::uint32_t sampler = 1; sampler + 2u < Samplers; ++sampler) sum += texel;
    return sum;
}

void Check(AgcDriver::VulkanDevice& device) {
    const auto edge = FloatTexels[Width - 1u];
    const auto first = Run(device, PointWrap, OpaqueBlackBorder);
    const auto second = Run(device, PointClamp, PointClamp);
    Require(first.samplerElements == Direct * 2u, "many table samplers: the direct sampler binding holds " + std::to_string(first.samplerElements) + " elements, expected " + std::to_string(Direct * 2u));
    Require(first.imageSamplers.size() == 1u && first.imageSamplers[0] == (std::uint64_t{1} << (Direct * 2u)) - 1u, "many table samplers: the image table element is not paired with the elements of direct samplers " + std::to_string(TableSamplers) + " to " + std::to_string(Samplers - 1u));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto inside = InsideSum(FloatTexels[lane % Width]);
        for (const auto* outcome : {&first, &second}) Require(std::bit_cast<std::uint32_t>(outcome->sums[lane]) == std::bit_cast<std::uint32_t>(inside), "many table samplers: lane " + std::to_string(lane) + " summed " + std::to_string(outcome->sums[lane]) + " through the point-clamp samplers, expected " + std::to_string(inside));
        Require(first.table[lane * 4u] == FloatTexels[lane % Width], "many table samplers: lane " + std::to_string(lane) + " read " + std::to_string(first.table[lane * 4u]) + " two widths outside the image through table sampler 64, expected its wrapped texel " + std::to_string(FloatTexels[lane % Width]));
        Require(first.direct[lane * 4u] == 0.0f && first.direct[lane * 4u + 3u] == 1.0f, "many table samplers: lane " + std::to_string(lane) + " read " + std::to_string(first.direct[lane * 4u]) + ", alpha " + std::to_string(first.direct[lane * 4u + 3u]) + " outside the image through direct sampler 127, expected its opaque black border");
        Require(second.table[lane * 4u] == edge && second.direct[lane * 4u] == edge, "many table samplers: the clamped control of lane " + std::to_string(lane) + " read " + std::to_string(second.table[lane * 4u]) + " through table sampler 64 and " + std::to_string(second.direct[lane * 4u]) + " through direct sampler 127, expected the edge texel " + std::to_string(edge));
    }
    std::printf("many table samplers: %u samplers (%u through the record table, %u direct as %zu sampler elements); outside the image, table sampler 64 wraps to the lane's texel and direct sampler 127 reads its opaque black border, and clamping S#s there read the edge texel\n", Samplers, TableSamplers, Direct, first.samplerElements);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (SkipOnLlvmpipe(*device, "124 table samplers and 4 direct: lane 8 sums 0 through the point-clamp samplers, expected 1039.5")) return VulkanTestSkipped;
        const auto limits = device->graphicsContext().limits;
        if (limits.maxPerStageDescriptorSamplers < StageSamplers || limits.maxDescriptorSetSamplers < StageSamplers) {
            std::printf("skipped, the device binds fewer than %u samplers per stage or set\n", StageSamplers);
            return VulkanTestSkipped;
        }
        Fill();
        Check(*device);
        std::puts("many table samplers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
