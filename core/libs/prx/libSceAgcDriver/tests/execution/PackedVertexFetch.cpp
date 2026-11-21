#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include "PackedFetch_Cover_vert.h"
#include "PackedFetch_PackedVertexProducer_frag.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace AgcDriver::Graphics;
using ShaderRecompiler::ShaderStage;
constexpr std::uint32_t Width = 64u;
constexpr std::uint32_t Records = Width * Width;
alignas(256) std::array<std::uint32_t, Records> vertices;
alignas(256) std::array<std::array<std::uint32_t, 4>, Records> output;
alignas(256) std::array<std::uint32_t, Records> pixels;

std::uint32_t word(std::uint32_t index) {
    return (index & 1023u) | (((index * 17u + 511u) & 1023u) << 10u) |
        (((index * 73u + 512u) & 1023u) << 20u) | ((index >> 10u) << 30u);
}

float component(std::uint32_t value, unsigned bits, unsigned format) {
    const auto sign = 1u << (bits - 1u);
    const auto mask = 2u * sign - 1u;
    value &= mask;
    const int signedValue = (value & sign) != 0u ? static_cast<int>(value) - static_cast<int>(mask + 1u) : static_cast<int>(value);
    if (format == 50u) return static_cast<float>(value) / static_cast<float>(mask);
    if (format == 51u) return std::max(-1.0f, static_cast<float>(signedValue) / static_cast<float>(sign - 1u));
    return static_cast<float>(format == 53u || format == 55u ? signedValue : static_cast<int>(value));
}

ShaderRecompiler::RecompileResult shader(std::span<const std::uint32_t> words, std::uint64_t id) {
    ShaderRecompiler::RecompileResult result;
    result.spirv = std::vector<std::uint32_t>(words.begin(), words.end());
    result.variantId = id;
    return result;
}

unsigned run(AgcDriver::VulkanDevice& device) {
    auto producerVertex = shader(PACKED_FETCH_COVER_VERT_SPV, 1u);
    auto producerPixel = shader(PACKED_FETCH_PACKEDVERTEXPRODUCER_FRAG_SPV, 2u);
    const std::array producerShaders{CompiledShader{ShaderStage::Vertex, &producerVertex, 0}, CompiledShader{ShaderStage::Fragment, &producerPixel, 0}};
    const auto address = reinterpret_cast<std::uintptr_t>(vertices.data());
    State consumer{};
    consumer.stages.path = ShaderPath::Vertex;
    consumer.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Width, Width}, VK_FORMAT_R8G8B8A8_UNORM, sizeof(pixels), 0xe4u, ColorTileMode::Linear, 4};
    consumer.colors = {consumer.color};
    consumer.hasColorTarget = true;
    consumer.renderExtent = {Width, Width};
    consumer.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    consumer.vertexPoints = true;
    consumer.viewport = {0, 0, Width, Width, 0, 1};
    consumer.scissor = {{0, 0}, {Width, Width}};
    consumer.blend.colorWriteMask = 15u;
    consumer.blends = {consumer.blend};
    auto producer = consumer;
    producer.color = {address, {Width, Width}, VK_FORMAT_R8G8B8A8_UNORM, sizeof(vertices), 0xe4u, ColorTileMode::Linear, 4};
    producer.colors = {producer.color};
    producer.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    producer.vertexPoints = false;

    alignas(256) static constexpr std::array<std::uint32_t, 7> vertexCode{
        0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 4> pixelCode{0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000};
    const auto captured = reinterpret_cast<std::uintptr_t>(output.data());
    std::array<std::uint32_t, 12> userData{0x10000000u, 1u, 0u, 0u, static_cast<std::uint32_t>(captured),
        static_cast<std::uint32_t>(captured >> 32u), sizeof(output), 0x31016facu};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(std::span(vertexCode))}}};
    constexpr std::array formats{VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_SNORM_PACK32,
        VK_FORMAT_A2B10G10R10_USCALED_PACK32, VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
        VK_FORMAT_A2B10G10R10_UINT_PACK32, VK_FORMAT_A2B10G10R10_SINT_PACK32};
    unsigned completed = 0;
    for (unsigned format = 50u; format < 50u + formats.size(); ++format) {
        if (!device.SupportsVertexBufferFormat(formats[format - 50u])) {
            std::cout << "skipped packed vertex format " << format << ": vertex fetch unsupported\n";
            continue;
        }
        for (const bool gpuProducer : {false, true}) {
            ShaderRecompiler::ShaderVertexStageInfo info{};
            info.resourcesNum = 1;
            info.resources[0].fields = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (4u << 16u),
                Records, (1u << 24u) | (format << 12u) | (gpuProducer ? 0x977u : 0xfacu)};
            info.resourcesDst[0] = {8, 4, 0, 0};
            info.fetchAttribReg = 8;
            info.fetchBufferReg = 10;
            info.fetchEmbedded = true;
            ShaderRecompiler::RecompileRequest request{{ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
                {64u, 0, userData, std::nullopt, std::nullopt, info, memory}, device.Target(), {0, 0, 0, 64}};
            const auto vertex = ShaderRecompiler::Recompile(request);
            Require(vertex.vertexAttributes.size() == 1, "packed fetch did not use a native vertex attribute");
            ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
            pixelInfo.targetOutputMode[0] = 9;
            pixelInfo.targetExportMapping.fill(0xe4u);
            ShaderRecompiler::RecompileRequest fragment{{ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
                {64u, 0, {}, std::nullopt, pixelInfo, std::nullopt, {}}, device.Target(), PixelPushLayout(vertex.pushConstants.size(), device.Target())};
            const auto pixel = ShaderRecompiler::Recompile(fragment);
            const std::array shaders{CompiledShader{ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderStage::Fragment, &pixel, PixelPushOffset(vertex.pushConstants.size(), device.Target())}};
            for (std::uint32_t index = 0; index < Records; ++index) vertices[index] = ~word(index);
            output.fill({0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu});
            if (gpuProducer) device.Draw(producer, {0, 3, 0, 1, 0, false}, producerShaders);
            device.Draw(consumer, {0, Records, 0, 1, 0, false}, shaders);
            device.WaitIdle();
            for (std::uint32_t index = 0; index < Records; ++index) {
                const auto packed = gpuProducer ? word(index) : ~word(index);
                for (unsigned channel = 0; channel < 4u; ++channel) {
                    const unsigned source = gpuProducer ? 3u - channel : channel;
                    const float expected = component(packed >> (10u * source), source == 3u ? 2u : 10u, format);
                    const auto bits = output[index][channel];
                    const float actual = format == 54u ? static_cast<float>(bits) : format == 55u ? static_cast<float>(std::bit_cast<std::int32_t>(bits)) : std::bit_cast<float>(bits);
                    if (!(std::abs(expected - actual) <= (format <= 51u ? 0.000001f : 0.0f))) {
                        throw std::runtime_error("packed vertex format " + std::to_string(format) + (gpuProducer ? " GPU" : " CPU") +
                            ", index " + std::to_string(index) + ", channel " + std::to_string(channel) + ": expected " +
                            std::to_string(expected) + ", saw " + std::to_string(actual));
                    }
                }
            }
        }
        ++completed;
    }
    return completed;
}
}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (SkipWithoutSharedMemoryStores()) return VulkanTestSkipped;
        const auto completed = run(*device);
        if (completed == 0u) return VulkanTestSkipped;
        std::cout << "packed vertex fetch tests passed for " << completed << " of 6 formats\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
