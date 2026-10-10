#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::PixelInput;
using ShaderRecompiler::PixelInputBit;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ResultDwords = 12;
constexpr std::uint32_t Fp16Input = 0x03080000u;
constexpr std::uint32_t Fp16DefaultHighInput = 0x01580000u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height * ResultDwords> Results{};

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000u, 0x80000005u, 0xe0382010u, 0x80000605u, 0xbf8c3f70u, 0xf800020fu, 0x09080706u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 47> PixelCode{
    0x7e080f02u, 0x7e0a0f03u, 0xd5430006u, 0x0411ff05u, 0x000000c0u, 0x7e300300u, 0x7e320301u, 0xd742001au,
    0x00020000u, 0x7e3602ffu, 0xabcd0000u, 0xd75a001bu, 0x046a0200u, 0xd7420008u, 0x00020100u, 0x7e3802ffu,
    0x12340000u, 0xd75a001cu, 0x04220300u, 0x7e1e02ffu, 0x4500bc00u, 0xd743001du, 0x043e0140u, 0x7e3c0280u,
    0xd75a001eu, 0x04760340u, 0x7e3e0280u, 0xd75a801fu, 0x046a0200u, 0xd7420020u, 0x00020181u, 0xd7420021u,
    0x000201c1u, 0xd7420022u, 0x40020000u, 0xd7430023u, 0x043e0000u, 0xe0782000u, 0x80011806u, 0xe0782010u,
    0x80011c06u, 0xe0782020u, 0x80012006u, 0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 3> LowX{0x3400u, 0x3f00u, 0xb800u};
constexpr std::array<std::uint32_t, 3> HighX{0x4200u, 0xc100u, 0x4700u};
constexpr std::array<std::uint32_t, 3> LowY{0x3800u, 0x3a00u, 0x3000u};
constexpr std::array<std::uint32_t, 3> HighY{0x6c00u, 0x3c00u, 0xc800u};

struct Vertex {
    std::array<float, 4> position;
    std::array<std::uint32_t, 4> parameter;
};

float HalfToFloat(std::uint32_t half) {
    const auto sign = (half & 0x8000u) != 0u ? -1.0f : 1.0f;
    const auto exponent = static_cast<int>((half >> 10u) & 0x1fu);
    const auto mantissa = static_cast<float>(half & 0x3ffu);
    if (exponent == 0) return sign * std::ldexp(mantissa, -24);
    if (exponent == 31) return mantissa == 0.0f ? sign * INFINITY : NAN;
    return sign * std::ldexp(1024.0f + mantissa, exponent - 25);
}

std::uint32_t FloatToHalf(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto sign = (bits >> 16u) & 0x8000u;
    const auto magnitude = bits & 0x7fffffffu;
    if (magnitude > 0x7f800000u) return sign | 0x7e00u;
    if (magnitude >= 0x477ff000u) return sign | 0x7c00u;
    if (magnitude < 0x38800000u) {
        const auto scaled = std::fabs(value) * 16777216.0f;
        return sign | static_cast<std::uint32_t>(std::nearbyint(scaled));
    }
    const auto rounded = magnitude + 0xfffu + ((magnitude >> 13u) & 1u);
    return sign | ((rounded - 0x38000000u) >> 13u);
}

float RoundToHalf(float value) {
    return HalfToFloat(FloatToHalf(value));
}

std::uint32_t Pack(std::uint32_t low, std::uint32_t high) {
    return low | (high << 16u);
}

std::vector<Vertex> Triangles() {
    std::vector<Vertex> vertices;
    constexpr std::uint32_t columns = 6;
    constexpr std::uint32_t rows = 4;
    for (std::uint32_t cell = 0; cell < columns * rows; ++cell) {
        const float width = 2.0f / columns;
        const float height = 2.0f / rows;
        const float x = -1.0f + width * static_cast<float>(cell % columns);
        const float y = -1.0f + height * static_cast<float>(cell / columns);
        const float skew = 0.07f * static_cast<float>(cell % 5u);
        const std::array<std::array<float, 2>, 3> corners{{
            {x + (0.05f + skew) * width, y + 0.05f * height},
            {x + 0.93f * width, y + (0.11f + skew) * height},
            {x + (0.37f - skew * 0.5f) * width, y + 0.91f * height},
        }};
        for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) {
            vertices.push_back({{corners[vertex][0], corners[vertex][1], 0.5f, 1.0f}, {Pack(LowX[vertex], HighX[vertex]), Pack(LowY[vertex], HighY[vertex]), 0x3c003c00u, 0x3c003c00u}});
        }
    }
    return vertices;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::ShaderPixelStageInfo PixelStage(std::uint32_t waveSize, std::uint32_t input0) {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.interpolatorCount = 2;
    pixel.interpolatorSettings[0] = input0;
    pixel.interpolatorSettings[1] = Fp16DefaultHighInput;
    pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter) | PixelInputBit(PixelInput::PositionX) | PixelInputBit(PixelInput::PositionY);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    return pixel;
}

ShaderRecompiler::RecompileResult RecompilePixel(std::span<const std::uint32_t> code, std::uint32_t waveSize, std::uint32_t input0, const ShaderRecompiler::SpirvTarget& target, std::uint32_t pushOffset) {
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto results = BufferDescriptor(Results.data(), ResultDwords * 4u, Width * Height);
    std::copy(results.begin(), results.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, PixelStage(waveSize, input0), std::nullopt, pixelMemory},
        target,
        {0, 0, pushOffset, 128 - pushOffset}
    };
    fragment.useCache = false;
    fragment.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false};
    return ShaderRecompiler::Recompile(fragment);
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    static const auto triangles = Triangles();
    const auto target = device.Target();
    Pixels.fill(std::byte{0});
    Results.fill(Sentinel);

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), sizeof(Vertex), static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const auto pixelResult = RecompilePixel(PixelCode, waveSize, Fp16Input, target, vertexPush);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, vertexPush}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

float Delta(const std::array<std::uint32_t, 3>& values, std::uint32_t vertex) {
    return RoundToHalf(HalfToFloat(values[vertex]) - HalfToFloat(values[0]));
}

float FirstStep(float delta, float coordinate, float origin) {
    return std::fma(delta, coordinate, origin);
}

double SecondStep(float delta, float coordinate, float firstStep) {
    const double product = static_cast<double>(delta) * static_cast<double>(coordinate);
    const double sum = product + firstStep;
    const double productPart = sum - firstStep;
    const double error = (product - productPart) + (firstStep - (sum - productPart));
    if (error == 0.0 || (std::bit_cast<std::uint64_t>(sum) & 1u) != 0u) return sum;
    return std::nextafter(sum, error > 0.0 ? INFINITY : -INFINITY);
}

double Saturate(double value) {
    return value > 0.0 ? std::min(value, 1.0) : 0.0;
}

std::uint32_t HalfOf(double value) {
    if (value == 0.0) return std::signbit(value) ? 0x8000u : 0u;
    const int step = std::max(std::ilogb(value) - 10, -24);
    const double rounded = std::ldexp(std::nearbyint(std::ldexp(value, -step)), step);
    if (std::fabs(rounded) > 65504.0) return std::signbit(value) ? 0xfc00u : 0x7c00u;
    if (rounded == 0.0) return std::signbit(value) ? 0x8000u : 0u;
    return FloatToHalf(static_cast<float>(rounded));
}

void Check(std::uint32_t waveSize) {
    const auto what = "16-bit interpolation wave" + std::to_string(waveSize);
    std::uint32_t covered = 0;
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto* result = &Results[pixel * ResultDwords];
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) != 255u) {
            Require(result[0] == Sentinel, what + ": uncovered pixel " + std::to_string(pixel) + " stored a result");
            continue;
        }
        ++covered;
        const float i = std::bit_cast<float>(result[0]);
        const float j = std::bit_cast<float>(result[1]);
        const auto lowX = FirstStep(Delta(LowX, 1), i, HalfToFloat(LowX[0]));
        const auto highX = FirstStep(Delta(HighX, 1), i, HalfToFloat(HighX[0]));
        const auto highY = FirstStep(Delta(HighY, 1), i, HalfToFloat(0x4500u));
        const std::array<std::uint32_t, ResultDwords> expected{
            result[0],
            result[1],
            std::bit_cast<std::uint32_t>(lowX),
            0xabcd0000u | HalfOf(SecondStep(Delta(LowX, 2), j, lowX)),
            0x12340000u | HalfOf(SecondStep(Delta(HighX, 2), j, highX)),
            std::bit_cast<std::uint32_t>(highY),
            HalfOf(SecondStep(Delta(HighY, 2), j, highY)),
            HalfOf(Saturate(SecondStep(Delta(LowX, 2), j, lowX))),
            std::bit_cast<std::uint32_t>(FirstStep(0.0f, i, 1.0f)),
            std::bit_cast<std::uint32_t>(FirstStep(0.0f, i, 0.0f)),
            std::bit_cast<std::uint32_t>(FirstStep(Delta(LowX, 1), -i, HalfToFloat(LowX[0]))),
            std::bit_cast<std::uint32_t>(FirstStep(Delta(LowX, 1), i, -1.0f)),
        };
        for (std::uint32_t dword = 2; dword < ResultDwords; ++dword) {
            char text[160];
            std::snprintf(text, sizeof(text), ": pixel %u dword %u is 0x%08x, expected 0x%08x (I %a, J %a)", pixel, dword, result[dword], expected[dword], static_cast<double>(i), static_cast<double>(j));
            Require(result[dword] == expected[dword], what + text);
        }
    }
    Require(covered > 1000, what + ": the triangles covered only " + std::to_string(covered) + " pixels");
}

ShaderRecompiler::SpirvTarget OfflineTarget(bool barycentric) {
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00401000u;
    target.spirvVersion = 0x00010300u;
    target.subgroupSize = 64;
    target.fragmentShaderBarycentricEnabled = barycentric;
    return target;
}

void ExpectFailure(std::span<const std::uint32_t> code, std::uint32_t input0, bool barycentric, const char* expected, const char* message) {
    try {
        static_cast<void>(RecompilePixel(code, 64u, input0, OfflineTarget(barycentric), 0u));
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, std::string(message) + ": " + error.what());
        return;
    }
    Testing::Fail(message);
}

void ExpectRejected(std::uint32_t word0, std::uint32_t word1, const char* expected, const char* message) {
    alignas(256) const std::array<std::uint32_t, 3> code{word0, word1, 0xbf810000u};
    ExpectFailure(code, Fp16Input, true, expected, message);
}


void RequireBarycentrics(const AgcDriver::VulkanDevice& device) {
    if (!device.Target().fragmentShaderBarycentricEnabled) Testing::Skip("the device has no fragmentShaderBarycentric");
}

const Testing::Case withoutBarycentrics{"InterpolationF16_TargetWithoutBarycentrics_IsRejected", [] {
    ExpectFailure(PixelCode, Fp16Input, false, "requires fragmentShaderBarycentric", "16-bit interpolation was accepted without barycentrics");
}};

const Testing::Case thirtyTwoBitInput{"InterpolationF16_ThirtyTwoBitInput_IsRejected", [] {
    ExpectFailure(PixelCode, 0x03000000u, true, "without FP16_INTERP_MODE", "16-bit interpolation of a 32-bit input was accepted");
}};

const Testing::Case passThroughInput{"InterpolationF16_PassThroughInput_IsRejected", [] {
    ExpectFailure(PixelCode, 0x03080420u, true, "passes its vertices through unchanged", "16-bit interpolation of a pass-through input was accepted");
}};

const Testing::Case modifiersRejected{"InterpolationF16_InterpolationModifiers_AreRejected", [] {
    ExpectRejected(0xd75a001bu, 0x0c6a0200u, "interpolation modifiers", "v_interp_p2_f16 accepted an output modifier");
    ExpectRejected(0xd75a081bu, 0x046a0200u, "interpolation modifiers", "v_interp_p2_f16 accepted op_sel");
    ExpectRejected(0xd742001au, 0x20020000u, "interpolation modifiers", "v_interp_p1ll_f16 accepted a negated attribute");
}};

const Testing::Case scalarOperandsRejected{"InterpolationF16_ScalarOperands_AreRejected", [] {
    ExpectRejected(0xd742001au, 0x00000000u, "not a vector register", "v_interp_p1ll_f16 accepted a scalar I coordinate");
    ExpectRejected(0xd743001du, 0x003e0140u, "not a vector register", "v_interp_p1lv_f16 accepted a scalar P0");
}};

const Testing::Case wave64Draw{"InterpolationF16_Wave64Draw_MatchesReferenceInterpolation", [] {
    auto& device = SharedVulkanTestDevice();
    RequireBarycentrics(device);
    Draw(device, 64u);
    Check(64u);
}};

const Testing::Case wave32Draw{"InterpolationF16_Wave32Draw_MatchesReferenceInterpolation", [] {
    auto& device = SharedVulkanTestDevice();
    RequireBarycentrics(device);
    Draw(device, 32u);
    Check(32u);
}};

} // namespace
