#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::PixelInput;
using ShaderRecompiler::PixelInputBit;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Slots = 4;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t PositionAtSample = 0x20000u;
constexpr std::uint32_t PositionUpperLeft = 0x100000u;
constexpr std::uint32_t Positions = PixelInputBit(PixelInput::PositionX) | PixelInputBit(PixelInput::PositionY);
constexpr std::uint32_t Ancillary = PixelInputBit(PixelInput::Ancillary);
constexpr std::uint32_t PerspectiveSample = PixelInputBit(PixelInput::PerspectiveSample);
constexpr std::uint32_t PerspectiveCenter = PixelInputBit(PixelInput::PerspectiveCenter);
constexpr std::uint32_t LinearSample = PixelInputBit(PixelInput::LinearSample);
constexpr std::uint32_t Village = PerspectiveSample | Positions | Ancillary;

struct Record {
    std::uint32_t invocations;
    float x;
    float y;
    float i;
    float j;
    std::uint32_t ancillary;
    std::array<std::uint32_t, 2> unused;
};
static_assert(sizeof(Record) == 32);
alignas(256) std::array<Record, Width * Height * Slots> Records{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 26> AncillaryStoreCode{
    0x7e0a0f02, 0x7e0c0f03, 0x340c0c86, 0x4a0a0d05, 0x340a0a82, 0xd5480007, 0x02111104, 0x4a0a0f05,
    0x340a0a85, 0x7e100281, 0xe0c81000, 0x80000805, 0xe0701004, 0x80000205, 0xe0701008, 0x80000305,
    0xe070100c, 0x80000005, 0xe0701010, 0x80000105, 0xe0701014, 0x80000405, 0xbf8c3f70, 0xf8001890,
    0x00000000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 20> PixelStoreCode{
    0x7e0a0f02, 0x7e0c0f03, 0x340c0c86, 0x4a0a0d05, 0x340a0a87, 0x7e100281, 0xe0c81000, 0x80000805,
    0xe0701004, 0x80000205, 0xe0701008, 0x80000305, 0xe070100c, 0x80000005, 0xe0701010, 0x80000105,
    0xbf8c3f70, 0xf8001890, 0x00000000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> ExportCode{0xf800180f, 0x04020100, 0xbf810000};

struct Corner {
    float x;
    float y;
    float w;
};

constexpr std::array<Corner, 3> Corners{{{-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 2.0f}, {-1.0f, 1.0f, 0.5f}}};

std::array<std::array<float, 4>, 3> ClipPositions() {
    std::array<std::array<float, 4>, 3> positions{};
    for (std::size_t i = 0; i < Corners.size(); ++i) positions[i] = {Corners[i].x * Corners[i].w, Corners[i].y * Corners[i].w, 0.5f * Corners[i].w, Corners[i].w};
    return positions;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, stride == 0u ? 0x31016facu : 0x01016facu};
}

std::array<std::array<double, 2>, 4> StandardSamples(std::uint32_t samples) {
    if (samples == 2) return {{{0.25, 0.25}, {-0.25, -0.25}, {}, {}}};
    if (samples == 4) return {{{-0.125, -0.375}, {0.375, -0.125}, {-0.375, 0.125}, {0.125, 0.375}}};
    return {};
}

struct Pass {
    std::uint32_t samples = 4;
    std::uint32_t inputs = Village;
    bool iterated = false;
    std::uint32_t barycentricControl = 0;
    std::array<std::uint16_t, 4> quadSampleMask{0xffffu, 0xffffu, 0xffffu, 0xffffu};
    bool customLocations = false;
};

std::uint8_t CustomLocation(std::uint32_t pixel, std::uint32_t sample) {
    return static_cast<std::uint8_t>(sample | (pixel << 4u));
}

AgcDriver::Graphics::State PassState(const Pass& pass) {
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.hasColorTarget = false;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blendConstants = {};
    state.samples = pass.samples;
    state.perSampleShading = pass.iterated;
    state.quadSampleMask = pass.quadSampleMask;
    state.customSampleLocations = pass.customLocations;
    state.sampleLocationGrid = pass.customLocations;
    for (std::uint32_t pixel = 0; pass.customLocations && pixel < 4u; ++pixel) {
        for (std::uint32_t sample = 0; sample < pass.samples; ++sample) state.sampleLocations[pixel * 8u + sample] = CustomLocation(pixel, sample);
    }
    return state;
}

ShaderRecompiler::ShaderPixelStageInfo PassPixel(const Pass& pass, const AgcDriver::Graphics::State& state) {
    const AgcDriver::Registers context{{0x1b3, pass.inputs}, {0x1b4, pass.inputs}, {0x1b6, 0u}, {0x1b8, pass.barycentricControl}, {0x1c5, 0u}, {0x203, 0u}};
    auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    pixel.quadPixelMask = AgcDriver::Graphics::QuadPixelMask(state);
    pixel.quadSampleMask = AgcDriver::Graphics::QuadSampleMasks(state);
    pixel.iteratesSamples = state.perSampleShading;
    pixel.sampleLocations = AgcDriver::Graphics::PixelSampleLocations(state, pixel);
    return pixel;
}

void Draw(AgcDriver::VulkanDevice& device, const Pass& pass) {
    Records.fill({});
    const auto target = device.Target();
    static const auto positions = ClipPositions();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(positions.data(), 16u, static_cast<std::uint32_t>(positions.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    const auto state = PassState(pass);
    const auto pixel = PassPixel(pass, state);
    Require(ShaderRecompiler::PixelInputVgpr(pass.inputs, PixelInput::PositionX) == 2u && ShaderRecompiler::PixelInputVgpr(pass.inputs, PixelInput::PositionY) == 3u, "the test inputs moved the positions");
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const auto records = BufferDescriptor(Records.data(), 0u, static_cast<std::uint32_t>(sizeof(Records)));
    std::copy(records.begin(), records.end(), pixelUserData.begin());
    const std::span<const std::uint32_t> code = (pass.inputs & Ancillary) != 0u ? std::span<const std::uint32_t>(AncillaryStoreCode) : std::span<const std::uint32_t>(PixelStoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    std::vector<AgcDriver::Graphics::CompiledShader> shaders{{ShaderStage::Vertex, &vertexResult, 0}};
    ShaderRecompiler::RecompileResult geometry;
    if (pixelResult.barycentricEmulation.active) {
        geometry = ShaderRecompiler::BuildBarycentricGeometryShader(vertexResult, pixelResult, target, device.GeometryLimits());
        shaders.push_back({ShaderStage::Geometry, &geometry, 0});
    }
    shaders.push_back({ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)});
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(positions.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::array<double, 2> Barycentrics(double x, double y, bool perspective) {
    const double b1 = x / Width;
    const double b2 = (Height - y) / Height;
    const std::array<double, 3> linear{1.0 - b1 - b2, b1, b2};
    if (!perspective) return {linear[1], linear[2]};
    double sum = 0.0;
    for (std::size_t i = 0; i < Corners.size(); ++i) sum += linear[i] / Corners[i].w;
    return {linear[1] / Corners[1].w / sum, linear[2] / Corners[2].w / sum};
}

std::string Point(double x, double y) {
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

void Check(const Pass& pass, const std::string& what) {
    const bool ancillary = (pass.inputs & Ancillary) != 0u;
    const bool samplePair = (pass.inputs & (PerspectiveSample | LinearSample)) != 0u;
    const bool perspective = (pass.inputs & (PerspectiveSample | PerspectiveCenter)) != 0u;
    const auto samples = StandardSamples(pass.samples);
    const auto full = (1u << pass.samples) - 1u;
    const auto location = (pass.barycentricControl & PositionUpperLeft) != 0u ? 3u : (pass.barycentricControl >> 16u) & 3u;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto mask = pass.quadSampleMask[(x & 1u) | ((y & 1u) << 1u)] & full;
            std::uint32_t inside = 0;
            std::uint32_t outside = 0;
            for (std::uint32_t sample = 0; sample < pass.samples; ++sample) {
                const auto weights = Barycentrics(x + 0.5 + samples[sample][0], y + 0.5 + samples[sample][1], false);
                const double first = 1.0 - weights[0] - weights[1];
                inside += first > 1e-3 ? 1u : 0u;
                outside += first < -1e-3 ? 1u : 0u;
            }
            if (inside != pass.samples && outside != pass.samples) continue;
            for (std::uint32_t slot = 0; slot < Slots; ++slot) {
                const auto& record = Records[(y * Width + x) * Slots + slot];
                std::uint32_t expected = 0;
                if (inside == 0u) expected = 0;
                else if (pass.iterated) expected = ancillary ? (slot < pass.samples ? (mask >> slot) & 1u : 0u) : (slot == 0u ? static_cast<std::uint32_t>(std::popcount(mask)) : 0u);
                else expected = slot == 0u && mask != 0u ? 1u : 0u;
                const auto where = what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") slot " + std::to_string(slot);
                Require(record.invocations == expected, where + " ran " + std::to_string(record.invocations) + " time(s), expected " + std::to_string(expected));
                if (expected == 0u) continue;
                const bool known = !pass.iterated || ancillary;
                const auto sample = pass.iterated ? slot : 0u;
                if (ancillary) Require(((record.ancillary >> 8u) & 0xfu) == sample, where + " read sample number " + std::to_string((record.ancillary >> 8u) & 0xfu));
                const double centerX = x + 0.5;
                const double centerY = y + 0.5;
                double positionX = centerX;
                double positionY = centerY;
                if (location == 3u) {
                    positionX = x;
                    positionY = y;
                } else if (location == 2u) {
                    positionX = centerX + samples[sample][0];
                    positionY = centerY + samples[sample][1];
                }
                if (known || location == 0u || location == 3u) {
                    Require(record.x == static_cast<float>(positionX) && record.y == static_cast<float>(positionY), where + " read the position " + Point(record.x, record.y) + ", expected " + Point(positionX, positionY));
                }
                if (samplePair && !known) continue;
                const double evaluatedX = samplePair ? centerX + samples[sample][0] : centerX;
                const double evaluatedY = samplePair ? centerY + samples[sample][1] : centerY;
                const auto weights = Barycentrics(evaluatedX, evaluatedY, perspective);
                Require(std::fabs(record.i - weights[0]) <= 1e-5 && std::fabs(record.j - weights[1]) <= 1e-5, where + " read the barycentrics " + Point(record.i, record.j) + ", expected " + Point(weights[0], weights[1]) + " at " + Point(evaluatedX, evaluatedY));
            }
        }
    }
}

bool Run(AgcDriver::VulkanDevice& device, const Pass& pass, const std::string& what) {
    try {
        Draw(device, pass);
    } catch (const std::exception& error) {
        if (std::string(error.what()).find("device does not support rendering without attachments at the draw's sample count") == std::string::npos) throw;
        std::printf("%s: skipped, %s\n", what.c_str(), error.what());
        return false;
    }
    Check(pass, what);
    return true;
}

struct Module {
    std::vector<spv::BuiltIn> builtins;
    std::vector<spv::Capability> capabilities;
    std::vector<std::array<float, 2>> offsets;
    std::vector<std::vector<std::array<float, 2>>> tables;
    bool variableOffset = false;
};

Module PixelModule(const Pass& pass, bool interpolationFunctions) {
    const auto state = PassState(pass);
    auto pixel = PassPixel(pass, state);
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    static constexpr std::array<std::uint32_t, 1> withOffsets{spv::CapabilityInterpolationFunction};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(ExportCode.data()), ExportCode, 0, {}};
    request.context.waveSize = WaveSize;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = true;
    if (interpolationFunctions) request.target.supportedCapabilities = withOffsets;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, spv::BuiltIn> decorations;
    std::map<std::uint32_t, float> floats;
    std::map<std::uint32_t, std::array<float, 2>> vectors;
    Module module;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpCapability) module.capabilities.push_back(static_cast<spv::Capability>(words[at + 1]));
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationBuiltIn) decorations[words[at + 1]] = static_cast<spv::BuiltIn>(words[at + 3]);
        if (op == spv::OpConstant && (words[at] >> 16u) == 4u) floats[words[at + 2]] = std::bit_cast<float>(words[at + 3]);
        if (op == spv::OpConstantComposite && (words[at] >> 16u) == 5u && floats.contains(words[at + 3]) && floats.contains(words[at + 4])) vectors[words[at + 2]] = {floats[words[at + 3]], floats[words[at + 4]]};
        if (op == spv::OpConstantComposite && (words[at] >> 16u) > 5u && vectors.contains(words[at + 3])) {
            std::vector<std::array<float, 2>> table;
            for (std::size_t element = at + 3; element < at + (words[at] >> 16u) && vectors.contains(words[element]); ++element) table.push_back(vectors[words[element]]);
            module.tables.push_back(table);
        }
        if (op == spv::OpExtInst && words[at + 4] == GLSLstd450InterpolateAtOffset) {
            const auto found = vectors.find(words[at + 6]);
            if (found == vectors.end()) module.variableOffset = true;
            else module.offsets.push_back(found->second);
        }
        if (op != spv::OpLoad && op != spv::OpAccessChain && op != spv::OpExtInst) continue;
        const auto pointer = op == spv::OpExtInst ? words[at + 5] : words[at + 3];
        const auto found = decorations.find(pointer);
        if (found != decorations.end() && std::ranges::find(module.builtins, found->second) == module.builtins.end()) module.builtins.push_back(found->second);
    }
    return module;
}

bool Has(const Module& module, spv::BuiltIn builtin) {
    return std::ranges::find(module.builtins, builtin) != module.builtins.end();
}

bool Has(const Module& module, spv::Capability capability) {
    return std::ranges::find(module.capabilities, capability) != module.capabilities.end();
}

void ExpectModules() {
    for (const std::uint32_t samples : {2u, 4u}) {
        const auto name = std::to_string(samples) + "x";
        const auto first = StandardSamples(samples)[0];
        Pass pass;
        pass.samples = samples;
        auto module = PixelModule(pass, true);
        Require(!Has(module, spv::BuiltInSampleId) && !Has(module, spv::CapabilitySampleRateShading), name + ": a pixel shader the guest runs once per pixel reads SampleId");
        Require(!module.offsets.empty() && !module.variableOffset && std::ranges::all_of(module.offsets, [&](const auto& offset) { return offset[0] == static_cast<float>(first[0]) && offset[1] == static_cast<float>(first[1]); }), name + ": PERSP_SAMPLE is not interpolated at sample 0's offset");
        Require(Has(module, spv::CapabilityInterpolationFunction) && Has(module, spv::BuiltInBaryCoordKHR) && Has(module, spv::BuiltInFragCoord), name + ": the once-per-pixel module lacks its inputs");
        pass.iterated = true;
        module = PixelModule(pass, true);
        Require(Has(module, spv::BuiltInSampleId) && Has(module, spv::CapabilitySampleRateShading), name + ": an iterated pixel shader lost its per-sample inputs");
        const auto standard = StandardSamples(samples);
        const auto guestTable = [&](const std::vector<std::array<float, 2>>& table) {
            if (table.size() != 8u) return false;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                if (table[sample][0] != static_cast<float>(standard[sample][0]) || table[sample][1] != static_cast<float>(standard[sample][1])) return false;
            }
            return true;
        };
        Require(module.offsets.empty() && module.variableOffset && std::ranges::any_of(module.tables, guestTable), name + ": an iterated PERSP_SAMPLE is not interpolated at the guest's sample locations");
        pass.customLocations = true;
        module = PixelModule(pass, true);
        const auto customTable = [&](std::uint32_t carried) {
            return [&, carried](const std::vector<std::array<float, 2>>& table) {
                if (table.size() != 32u) return false;
                for (std::uint32_t index = 0; index < table.size(); ++index) {
                    const auto location = index % 8u < carried ? CustomLocation(index / 8u, index % 8u) : std::uint8_t{0u};
                    if (table[index][0] != ShaderRecompiler::SampleLocationOffset(location, 0u) || table[index][1] != ShaderRecompiler::SampleLocationOffset(location, 1u)) return false;
                }
                return true;
            };
        };
        Require(module.variableOffset && Has(module, spv::BuiltInSampleId) && Has(module, spv::BuiltInFragCoord) && std::ranges::any_of(module.tables, customTable(samples)), name + ": an iterated PERSP_SAMPLE is not interpolated at each pixel's custom sample locations");
        pass.iterated = false;
        module = PixelModule(pass, true);
        Require(module.variableOffset && !Has(module, spv::BuiltInSampleId) && Has(module, spv::BuiltInFragCoord) && std::ranges::any_of(module.tables, customTable(1u)), name + ": a once-per-pixel PERSP_SAMPLE is not interpolated at each pixel's custom sample 0");
        pass.customLocations = false;
        pass.iterated = true;
        pass.inputs = PerspectiveCenter | Positions | Ancillary;
        module = PixelModule(pass, true);
        Require(!module.offsets.empty() && !module.variableOffset && std::ranges::all_of(module.offsets, [](const auto& offset) { return offset[0] == 0.0f && offset[1] == 0.0f; }), name + ": an iterated PERSP_CENTER is not interpolated at the pixel center");
        pass.iterated = false;
        module = PixelModule(pass, true);
        Require(module.offsets.empty() && !Has(module, spv::BuiltInSampleId) && !Has(module, spv::CapabilityInterpolationFunction), name + ": a once-per-pixel PERSP_CENTER changed");
        pass.inputs = Village;
        module = PixelModule(pass, false);
        Require(module.offsets.empty() && !Has(module, spv::BuiltInSampleId) && !Has(module, spv::CapabilityInterpolationFunction), name + ": a device without sampleRateShading got offset interpolation");
    }
    Pass single;
    single.samples = 1;
    const auto module = PixelModule(single, true);
    Require(module.offsets.empty() && !Has(module, spv::BuiltInSampleId), "1x: a single-sample pixel shader changed");
}

void ExpectSampleLocations() {
    Pass pass;
    for (const std::uint32_t samples : {1u, 2u, 4u, 8u}) {
        pass.samples = samples;
        const auto state = PassState(pass);
        const auto locations = AgcDriver::Graphics::PixelSampleLocations(state, PassPixel(pass, state));
        const std::uint8_t expected = samples == 1u ? 0x00u : samples == 2u ? 0x44u : samples == 4u ? 0xaeu : 0xd1u;
        for (std::uint32_t index = 0; index < locations.size(); ++index) Require(locations[index] == (index % 8u == 0u ? expected : 0u), std::to_string(samples) + "x: the once-per-pixel locations are not sample 0's standard location");
    }
    pass.samples = 4;
    auto state = PassState(pass);
    state.customSampleLocations = true;
    state.sampleLocationGrid = true;
    for (std::uint32_t pixel = 0; pixel < 4u; ++pixel) state.sampleLocations[pixel * 8u] = static_cast<std::uint8_t>(0x10u * pixel + 1u);
    auto pixel = PassPixel(pass, state);
    for (std::uint32_t index = 0; index < 4u; ++index) Require(pixel.sampleLocations[index * 8u] == 0x10u * index + 1u, "custom per-pixel sample 0 locations were not carried");
    pass.iterated = true;
    state = PassState(pass);
    pixel = PassPixel(pass, state);
    for (std::uint32_t index = 0; index < pixel.sampleLocations.size(); ++index) {
        const std::array<std::uint8_t, 4> standard{0xaeu, 0xe6u, 0x2au, 0x62u};
        Require(pixel.sampleLocations[index] == (index % 8u < 4u ? standard[index % 8u] : 0u), "an iterated draw did not carry every sample's location");
    }
    pass.iterated = false;
    pass.inputs = PerspectiveCenter | Positions | Ancillary;
    state = PassState(pass);
    Require(std::ranges::all_of(PassPixel(pass, state).sampleLocations, [](std::uint8_t location) { return location == 0u; }), "a draw without sample inputs carried sample locations");
    pass.barycentricControl = PositionAtSample;
    state = PassState(pass);
    Require(PassPixel(pass, state).sampleLocations[0] == 0xaeu, "positions at the sample did not carry sample 0's location");
}

}

int main() {
    try {
        ExpectSampleLocations();
        ExpectModules();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(TargetHasCapability(device->Target(), spv::CapabilityInterpolationFunction), "the test device lacks sampleRateShading");
        std::uint32_t ran = 0;
        for (const std::uint32_t samples : {4u, 2u}) {
            const auto name = std::to_string(samples) + "x ";
            Pass pass;
            pass.samples = samples;
            ran += Run(*device, pass, name + "PERSP_SAMPLE, POS and ANCILLARY run once per pixel") ? 1u : 0u;
            pass.inputs = PerspectiveSample | Positions;
            ran += Run(*device, pass, name + "PERSP_SAMPLE without ANCILLARY at sample 0") ? 1u : 0u;
            pass.inputs = LinearSample | Positions | Ancillary;
            ran += Run(*device, pass, name + "LINEAR_SAMPLE at sample 0") ? 1u : 0u;
            pass.inputs = PerspectiveCenter | Positions | Ancillary;
            ran += Run(*device, pass, name + "PERSP_CENTER once per pixel") ? 1u : 0u;
            pass.inputs = Village;
            pass.barycentricControl = PositionUpperLeft;
            ran += Run(*device, pass, name + "positions at the upper-left corner") ? 1u : 0u;
            pass.inputs = PerspectiveSample | Positions;
            pass.barycentricControl = PositionAtSample;
            ran += Run(*device, pass, name + "positions at sample 0") ? 1u : 0u;
            pass.inputs = Village;
            pass.barycentricControl = 0;
            pass.quadSampleMask = {0x1u, 0x2u, 0x0u, 0x3u};
            ran += Run(*device, pass, name + "a partial quad sample mask once per pixel") ? 1u : 0u;
            pass.quadSampleMask = {0xffffu, 0xffffu, 0xffffu, 0xffffu};
            pass.iterated = true;
            pass.barycentricControl = PositionAtSample;
            ran += Run(*device, pass, name + "an iterated shader per sample") ? 1u : 0u;
            pass.inputs = PerspectiveCenter | Positions | Ancillary;
            pass.barycentricControl = 0;
            ran += Run(*device, pass, name + "an iterated shader with PERSP_CENTER and positions at the center") ? 1u : 0u;
            pass.inputs = PerspectiveSample | Positions;
            ran += Run(*device, pass, name + "an iterated shader without ANCILLARY") ? 1u : 0u;
        }
        Pass single;
        single.samples = 1;
        ran += Run(*device, single, "1x PERSP_SAMPLE, POS and ANCILLARY") ? 1u : 0u;
        Require(ran >= 11u, "fewer than eleven draws ran");
        std::printf("pixel sample iteration tests passed (%u draws)\n", ran);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
