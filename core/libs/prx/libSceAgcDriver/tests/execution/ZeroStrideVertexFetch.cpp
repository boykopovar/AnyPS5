#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Vertices = 3;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, 20u * 4u> Output{};
constexpr std::size_t ArgumentBytes = 65536;
alignas(4096) std::array<std::uint32_t, 32768> ArgumentStorage{};
auto* const Arguments = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(ArgumentStorage.data()) + ArgumentBytes - 1u) & ~static_cast<std::uintptr_t>(ArgumentBytes - 1u));
alignas(256) std::array<std::array<float, 8>, 20> StridedElements{};

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<float, 4> Element{0.25f, -2.0f, 7.5f, 1.0f};

struct Case {
    std::uint32_t select;
    std::uint32_t records;
    bool inRange;
    const char* what;
    std::uint32_t selectors = 0xfacu;
    std::uint32_t stride = 0;
    std::uint32_t format = 77;
    std::uint32_t firstVertex = 0;
    bool nullBase = false;
    bool indirect = false;
    std::uint32_t fetchIndex = 0;
    bool unalignedBase = false;
};

std::array<std::uint32_t, 4> VertexBufferDescriptor(const Case& value) {
    const auto address = value.nullBase ? 0u : value.unalignedBase ? 1u : reinterpret_cast<std::uintptr_t>(value.stride == 0 ? Element.data() : StridedElements.front().data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (value.stride << 16u), value.records, (0x0104dfacu & ~0x7ffffu) | (value.format << 12u) | value.selectors | (value.select << 28u)};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

void Draw(const std::shared_ptr<AgcDriver::VulkanDevice>& device, const Case& value) {
    Pixels.fill(std::byte{0x40});
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = VertexBufferDescriptor(value);
    info.resourcesDst[0] = {8, 4, 0, value.fetchIndex};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device->Target(),
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    auto vertexResult = ShaderRecompiler::Recompile(vertexRequest);
    vertexResult.variantId = 14695981039346656037ull;
    for (const auto word : vertexResult.spirv) vertexResult.variantId = (vertexResult.variantId ^ word) * 1099511628211ull;
    for (const auto shape : {value.format, value.stride, value.select, value.selectors, value.fetchIndex}) vertexResult.variantId = (vertexResult.variantId ^ shape) * 1099511628211ull;
    Require(vertexResult.vertexAttributes.size() == 1u, std::string(value.what) + ": the fetch did not become a vertex attribute");
    if (device->Target().registrationModules) Require(!vertexResult.registrationModule, std::string(value.what) + ": registration modules: the vertex module was emitted in registration form");
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        device->Target(),
        PixelPushLayout(vertexPush, device->Target())
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, device->Target())}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    AgcDriver::Pm4::DrawParameters draw{0, Vertices, 0, 1, 0, false, value.firstVertex, value.fetchIndex == 0 ? 0u : 17u};
    if (value.indirect) {
        Arguments[0] = Vertices;
        Arguments[1] = 1;
        Arguments[2] = value.firstVertex;
        Arguments[3] = 0;
        draw.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{reinterpret_cast<std::uintptr_t>(Arguments), 0x24u, 16u, 16u, 1u, false, 0u, 0x280u, 0x280u, 0x280u, false, 0u};
        AgcDriver::DriverDetail::DrawProgram program{};
        program.firstUserSgpr = 8;
        program.userData = userData;
        AgcDriver::DriverDetail::Driver::ClassifyIndirectDraw(vertexResult, state, program, device, draw, false);
    }
    device->Draw(state, draw, shaders);
    device->WaitIdle();
}

void DescriptorDiagnostics(const std::shared_ptr<AgcDriver::VulkanDevice>& device) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        for (const auto selector : {2u, 3u}) {
            const auto shift = component * 3u;
            const Case value{3u, 16u, true, "reserved selector", (0xfacu & ~(7u << shift)) | (selector << shift)};
            const auto fields = VertexBufferDescriptor(value);
            char expected[192];
            std::snprintf(expected, sizeof(expected), "reserved prepared vertex component selector %u at location 0 component %u (V#=[%08x %08x %08x %08x])",
                selector, component, fields[0], fields[1], fields[2], fields[3]);
            bool rejected = false;
            try {
                Draw(device, value);
            } catch (const std::runtime_error& error) {
                const std::string message(error.what());
                Require(message.find(expected) != std::string::npos, "reserved selector error did not identify the component and descriptor: " + message);
                rejected = true;
            }
            Require(rejected, "a reserved vertex component selector was accepted");
        }
    }
}

void Check(const Case& value) {
    for (std::uint32_t vertex = 0; vertex < Vertices; ++vertex) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto selector = (value.selectors >> (component * 3u)) & 7u;
            const auto selected = selector >= 4u ? std::bit_cast<std::uint32_t>(Element[selector - 4u]) : selector == 1u ? std::bit_cast<std::uint32_t>(1.0f) : 0u;
            const auto expected = value.inRange || selector < 4u ? selected : 0u;
            const auto actual = Output[(value.firstVertex + vertex) * 4u + component];
            Require(actual == expected, std::string(value.what) + ": vertex " + std::to_string(vertex) + " v" + std::to_string(8u + component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::shared_ptr<AgcDriver::VulkanDevice> device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::string(argv[1]) == "--ranges") {
            {
                GuestAllocations::Mutation mutation;
                mutation.Add(Arguments, ArgumentBytes, true, true, true);
            }
            for (auto& row : StridedElements) std::copy(Element.begin(), Element.end(), row.begin());
            std::uint32_t checks = 0;
            const auto check = [&](const Case& value) {
                Draw(device, value);
                Check(value);
                ++checks;
            };
            for (const auto select : {2u, 3u}) {
                Case value{select, 3u, true, "live known-zero layout transition"};
                value.stride = 32;
                value.records = select == 2u ? 3u : 16u;
                check(value);
                value.records = select == 2u ? 0u : 15u;
                value.inRange = false;
                check(value);
                value.records = select == 2u ? 3u : 16u;
                value.inRange = true;
                check(value);
            }
            for (const bool indirect : {false, true}) {
                for (const bool nullBase : {false, true}) {
                    for (const auto select : {2u, 3u}) {
                        for (const auto [format, bytes] : std::array<std::pair<std::uint32_t, std::uint32_t>, 6>{{{77u, 16u}, {74u, 12u}, {22u, 4u}, {1u, 1u}, {75u, 16u}, {76u, 16u}}}) {
                            Case value{select, select == 2u ? 0u : bytes - 1u, false, "known-zero strided typed fetch"};
                            value.stride = 32;
                            value.format = format;
                            value.firstVertex = indirect ? 0u : 17u;
                            value.nullBase = nullBase;
                            value.indirect = indirect;
                            check(value);
                            value.fetchIndex = 1;
                            value.firstVertex = 0;
                            check(value);
                        }
                    }
                }
            }
            for (const auto [select, records] : std::array<std::pair<std::uint32_t, std::uint32_t>, 3>{{{0u, 0u}, {2u, 0u}, {3u, 15u}}}) {
                Case value{select, records, false, "known-zero null zero-stride fetch"};
                value.nullBase = true;
                check(value);
            }
            for (const auto select : {2u, 3u}) {
                Case value{select, select == 2u ? 0u : 15u, false, "zero strided component selectors"};
                value.stride = 32;
                value.selectors = 7u | (6u << 3u) | (1u << 6u) | (0u << 9u);
                check(value);
                value.selectors = 0xfacu;
                value.unalignedBase = true;
                check(value);
                value.records = select == 2u ? 3u : 16u;
                value.nullBase = true;
                bool refused = false;
                try {
                    Draw(device, value);
                } catch (const std::runtime_error& error) {
                    if (std::string(error.what()).find("unaligned vertex buffer") == std::string::npos) throw;
                    refused = true;
                }
                Require(refused, "an in-range null descriptor was accepted as a zero fetch");
            }
            std::cout << "known-zero vertex range checks passed: " << checks << '\n';
            return 0;
        }
        constexpr std::uint32_t Wzyx = 7u | (6u << 3u) | (1u << 6u) | (0u << 9u);
        constexpr std::array<Case, 20> cases{{
            {0u, 0u, false, "OOB_SELECT 0, no records"},
            {0u, 1u, false, "OOB_SELECT 0, one record"},
            {0u, 16u, false, "OOB_SELECT 0, records cover the element's bytes"},
            {0u, 0xffffffffu, false, "OOB_SELECT 0, NUM_RECORDS 0xffffffff"},
            {0u, 0xffffffffu, false, "OOB_SELECT 0, NUM_RECORDS 0xffffffff, selectors W Z 1 0", Wzyx},
            {1u, 0u, false, "OOB_SELECT 1, no records"},
            {1u, 3u, true, "OOB_SELECT 1, NUM_RECORDS counts the three vertices' records"},
            {1u, 4u, true, "OOB_SELECT 1, NUM_RECORDS past the vertices"},
            {1u, 0xffffffffu, true, "OOB_SELECT 1, NUM_RECORDS 0xffffffff"},
            {1u, 3u, true, "OOB_SELECT 1, NUM_RECORDS 3, selectors W Z 1 0", Wzyx},
            {2u, 0u, false, "OOB_SELECT 2, no records"},
            {2u, 1u, true, "OOB_SELECT 2, one record"},
            {2u, 16u, true, "OOB_SELECT 2, whole element in range"},
            {3u, 0u, false, "OOB_SELECT 3, no records"},
            {3u, 8u, false, "OOB_SELECT 3, the element only partly fits"},
            {3u, 15u, false, "OOB_SELECT 3, the element misses its last byte"},
            {3u, 16u, true, "OOB_SELECT 3, whole element in range"},
            {3u, 0xffffffffu, true, "OOB_SELECT 3, NUM_RECORDS 0xffffffff"},
            {3u, 16u, true, "OOB_SELECT 3, selectors W Z 1 0", Wzyx},
            {3u, 8u, false, "OOB_SELECT 3, partly fits, selectors W Z 1 0", Wzyx},
        }};
        for (const auto& value : cases) {
            Draw(device, value);
            Check(value);
        }
        DescriptorDiagnostics(device);
        std::puts("zero-stride vertex fetch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
