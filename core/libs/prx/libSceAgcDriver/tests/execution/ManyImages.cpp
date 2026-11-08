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

constexpr std::uint32_t Width = 4;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t DirectImages = 64;
constexpr std::uint32_t Images = DirectImages + 1u;
constexpr std::uint32_t RecordBytes = 8192;
constexpr std::uint32_t DirectWord = 16;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;
constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};

alignas(256) std::array<std::array<float, 64u * Height>, Images> Texels{};
alignas(256) std::array<std::uint32_t, RecordBytes / 4u> Table{};
alignas(256) std::array<std::uint32_t, Images + 4u> Output{};
alignas(256) std::array<std::uint32_t, DirectWord + DirectImages * 8u> Srt{};

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t stride, std::uint32_t records) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

std::array<std::uint32_t, 8> Image(const void* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

float Texel(std::uint32_t image) {
    return 1.0f + static_cast<float>(image) * 0.5f;
}

std::uint32_t TableImageIndex(bool tableFirst) {
    return tableFirst ? 0u : DirectImages;
}

std::uint32_t DirectImageIndex(bool tableFirst, std::uint32_t direct) {
    return tableFirst ? direct + 1u : direct;
}

std::vector<std::uint32_t> ImagesCode(bool tableFirst) {
    std::vector<std::uint32_t> code{0xf4080100u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080200u, 0xfa000020u, 0x8f108d02u, 0x7e0202f0u, 0x7e0402f0u};
    const auto table = [&] { code.insert(code.end(), {0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450401u, 0xe0700000u | (TableImageIndex(tableFirst) * 4u), 0x80070400u}); };
    if (tableFirst) table();
    for (std::uint32_t direct = 0; direct < DirectImages; ++direct) code.insert(code.end(), {0xf40c0b00u, 0xfa000000u | ((DirectWord + direct * 8u) * 4u), 0xf09c0f08u, 0x004b0401u, 0xe0700000u | (DirectImageIndex(tableFirst, direct) * 4u), 0x80070400u});
    if (!tableFirst) table();
    code.push_back(0xbf810000u);
    return code;
}

struct Outcome {
    std::vector<std::uint32_t> words;
    std::uint32_t imageTableCount = 0;
    std::uint32_t heapImages = 0;
};

Outcome Run(AgcDriver::VulkanDevice& device, bool tableFirst) {
    Srt = {};
    Table = {};
    const auto table = Buffer(Table.data(), RecordBytes, 1u);
    const auto output = Buffer(Output.data(), 0u, static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(output.begin(), output.end(), Srt.begin() + 4);
    std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 8);
    const auto tableImage = Image(Texels[TableImageIndex(tableFirst)].data());
    std::copy(tableImage.begin(), tableImage.end(), Table.begin());
    for (std::uint32_t direct = 0; direct < DirectImages; ++direct) {
        const auto words = Image(Texels[DirectImageIndex(tableFirst, direct)].data());
        std::copy(words.begin(), words.end(), Srt.begin() + DirectWord + direct * 8u);
    }
    const auto code = ImagesCode(tableFirst);
    const auto rootAddress = reinterpret_cast<std::uintptr_t>(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u)};
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 3> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Outcome outcome;
    for (const auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable) outcome.imageTableCount = binding.count;
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestImages && binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) outcome.heapImages += binding.count;
    }
    Output.fill(Unwritten);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(span.data()));
    device.WaitIdle();
    outcome.words.assign(Output.begin(), Output.end());
    return outcome;
}

void Check(AgcDriver::VulkanDevice& device, bool tableFirst) {
    const auto outcome = Run(device, tableFirst);
    const auto order = std::string(tableFirst ? "a table image then 64 direct images" : "64 direct images then a table image");
    Require(outcome.imageTableCount == 1u && outcome.heapImages == DirectImages, "many images: " + order + " were bound as " + std::to_string(outcome.imageTableCount) + " image table and " + std::to_string(outcome.heapImages) + " heap elements");
    for (std::uint32_t image = 0; image < Images; ++image) {
        const auto kind = image == TableImageIndex(tableFirst) ? " (table)" : " (direct)";
        Require(outcome.words[image] == std::bit_cast<std::uint32_t>(Texel(image)), "many images: with " + order + ", image " + std::to_string(image) + kind + " sampled " + std::to_string(std::bit_cast<float>(outcome.words[image])) + ", expected " + std::to_string(Texel(image)));
    }
    Require(std::all_of(outcome.words.begin() + Images, outcome.words.end(), [](std::uint32_t word) { return word == Unwritten; }), "many images: the program wrote past its " + std::to_string(Images) + " output words");
    std::printf("many images: %s, 65 images, each sampled its own texels; image %u is the table image\n", order.c_str(), TableImageIndex(tableFirst));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t image = 0; image < Images; ++image) Texels[image].fill(Texel(image));
        Check(*device, true);
        Check(*device, false);
        std::puts("many images tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
