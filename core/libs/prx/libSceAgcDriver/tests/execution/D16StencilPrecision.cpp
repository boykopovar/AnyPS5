#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::ClearCachedPipelines;
using AgcDriver::Graphics::ClearDepthSurfaces;
using AgcDriver::Graphics::ClearPipelineLibraries;
using AgcDriver::Graphics::CompiledShader;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DepthTarget;
using AgcDriver::Graphics::HostDepthFormat;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::State;
using AgcDriver::Graphics::WaitForOptimizedPipelines;
using DepthFastClearHarness::BufferDescriptor;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Pixels;
using DepthFastClearHarness::PixelCode;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::VertexCode;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

VkPhysicalDevice RealPhysical = VK_NULL_HANDLE;
PFN_vkGetPhysicalDeviceFormatProperties RealFormatProperties = nullptr;
PFN_vkGetPhysicalDeviceImageFormatProperties RealImageFormatProperties = nullptr;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties RealSparseImageFormatProperties = nullptr;

constexpr VkFormatFeatureFlags HostDepthFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

bool FormatHasHostDepthFeatures(VkFormat format) {
    if (RealFormatProperties == nullptr) return false;
    VkFormatProperties properties{};
    RealFormatProperties(RealPhysical, format, &properties);
    return (properties.optimalTilingFeatures & HostDepthFeatures) == HostDepthFeatures;
}

bool DepthAttachmentSupported(VkFormat format) {
    if (RealFormatProperties == nullptr) return false;
    VkFormatProperties properties{};
    RealFormatProperties(RealPhysical, format, &properties);
    return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u;
}

VkFormat ExpectedHostFormat() {
    for (const auto candidate : {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT}) {
        if (FormatHasHostDepthFeatures(candidate)) return candidate;
    }
    return VK_FORMAT_D16_UNORM_S8_UINT;
}

constexpr float Inv65535 = 1.0f / 65535.0f;

constexpr float BucketCenter(std::uint32_t bucket) {
    return static_cast<float>(bucket) * Inv65535;
}

constexpr float BucketOffset(std::uint32_t bucket, float delta) {
    const float raw = (static_cast<float>(bucket) + delta) * Inv65535;
    if (raw < 0.0f) return 0.0f;
    if (raw > 1.0f) return 1.0f;
    return raw;
}

alignas(4096) std::array<std::byte, 128 * 128 * 4> DepthStorage{};
alignas(4096) std::array<std::byte, 256 * 256> StencilStorage{};

struct TriangleSpec {
    float z0 = 0.5f;
    float z1 = 0.5f;
    float z2 = 0.5f;
};

constexpr TriangleSpec FlatTriangle(float depth) {
    return {depth, depth, depth};
}

alignas(256) std::vector<std::array<float, 4>> VertexPool;

std::uint32_t RegisterTriangle(TriangleSpec spec) {
    for (std::size_t index = 0; index + 2 < VertexPool.size(); index += 3) {
        if (VertexPool[index][2] == spec.z0 && VertexPool[index + 1][2] == spec.z1 && VertexPool[index + 2][2] == spec.z2) {
            return static_cast<std::uint32_t>(index);
        }
    }
    const auto first = static_cast<std::uint32_t>(VertexPool.size());
    VertexPool.push_back({-1.0f, -1.0f, spec.z0, 1.0f});
    VertexPool.push_back({3.0f, -1.0f, spec.z1, 1.0f});
    VertexPool.push_back({-1.0f, 3.0f, spec.z2, 1.0f});
    return first;
}

struct FragExportSpec {
    float exportedDepth = 0.5f;
    float clampMin = 0.0f;
    float clampMax = 1.0f;
};

struct FragExportShader {
    FragExportSpec spec{};
    alignas(256) std::array<std::uint32_t, 8> code{};
    ShaderRecompiler::RecompileResult pixel{};
};

Shaders CompileBaseShaders(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    Shaders shaders;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(VertexPool.data(), 16u, static_cast<std::uint32_t>(VertexPool.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    shaders.vertex = ShaderRecompiler::Recompile(vertex);
    shaders.vertexPush = static_cast<std::uint32_t>(shaders.vertex.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(shaders.vertexPush, target)
    };
    fragment.useCache = false;
    shaders.pixel = ShaderRecompiler::Recompile(fragment);
    return shaders;
}

void CompileFragExportShader(AgcDriver::VulkanDevice& device, const Shaders& base, FragExportShader& out) {
    out.code = {
        0x7e0002f2u,
        0xf800100fu,
        0x00000000u,
        0x7e0202ffu,
        std::bit_cast<std::uint32_t>(out.spec.exportedDepth),
        0xf8001881u,
        0x00000001u,
        0xbf810000u,
    };
    const auto target = device.Target();
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    pixel.depthExportEnable = true;
    pixel.depthExportMin = out.spec.clampMin;
    pixel.depthExportMax = out.spec.clampMax;
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(out.code.data()), std::as_bytes(std::span(out.code))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(out.code.data()), out.code, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(base.vertexPush, target)
    };
    fragment.useCache = false;
    out.pixel = ShaderRecompiler::Recompile(fragment);
}

struct StepSpec {
    TriangleSpec triangle = FlatTriangle(0.5f);
    std::uint32_t firstVertex = 0u;
    int fragExportIndex = -1;
    bool depthTest = true;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;
    bool depthBoundsTest = false;
    float minDepthBounds = 0.0f;
    float maxDepthBounds = 1.0f;
    bool depthBias = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    float depthBiasClamp = 0.0f;
    bool stencilTest = false;
    VkStencilOpState stencil{};
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
    bool negativeOneToOne = false;
};

struct CaseSpec {
    std::string name;
    VkFormat format = VK_FORMAT_D16_UNORM_S8_UINT;
    float clearDepth = 1.0f;
    std::uint8_t clearStencil = 0u;
    std::vector<StepSpec> steps;
    std::uint32_t expectedCovered = 0u;
    bool skipOnD32Fallback = false;
    bool matchNativeD16 = false;
};

VkStencilOpState MakeStencil(VkCompareOp compare, VkStencilOp passOp, VkStencilOp depthFailOp, VkStencilOp failOp, std::uint32_t reference) {
    return {failOp, passOp, depthFailOp, compare, 0xffu, 0xffu, reference};
}

std::vector<CaseSpec> BuildCases(std::vector<FragExportSpec>& fragSpecs, const Context& context) {
    std::vector<CaseSpec> cases;

    const auto addFragSpec = [&](float exportedDepth, float clampMin, float clampMax) {
        const int index = static_cast<int>(fragSpecs.size());
        fragSpecs.push_back({exportedDepth, clampMin, clampMax});
        return index;
    };

    const auto addTwoDrawCase = [&](std::string name, float firstDepth, float secondDepth, VkCompareOp compare, std::uint32_t expected, float clearDepth = 1.0f, VkFormat format = VK_FORMAT_D16_UNORM_S8_UINT) {
        CaseSpec spec;
        spec.name = std::move(name);
        spec.format = format;
        spec.clearDepth = clearDepth;
        StepSpec writeStep;
        writeStep.triangle = FlatTriangle(firstDepth);
        writeStep.depthTest = true;
        writeStep.depthWrite = true;
        writeStep.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec testStep;
        testStep.triangle = FlatTriangle(secondDepth);
        testStep.depthTest = true;
        testStep.depthWrite = false;
        testStep.depthCompare = compare;
        spec.steps = {writeStep, testStep};
        spec.expectedCovered = expected;
        cases.push_back(std::move(spec));
    };

    struct BucketPair {
        std::string_view label;
        float low;
        float high;
    };

    const std::array<BucketPair, 7> sameBucketPairs{{
        {"bucket_0_near", BucketOffset(0u, 0.05f), BucketOffset(0u, 0.35f)},
        {"bucket_1", BucketOffset(1u, -0.35f), BucketOffset(1u, 0.35f)},
        {"bucket_16384_q1", BucketOffset(16384u, -0.35f), BucketOffset(16384u, 0.35f)},
        {"bucket_32768_mid_probe", 0.500002f, 0.500004f},
        {"bucket_49152_q3", BucketOffset(49152u, -0.35f), BucketOffset(49152u, 0.35f)},
        {"bucket_65534", BucketOffset(65534u, -0.35f), BucketOffset(65534u, 0.35f)},
        {"bucket_65535_far", BucketOffset(65535u, -0.35f), BucketOffset(65535u, -0.05f)},
    }};

    for (const auto& pair : sameBucketPairs) {
        addTwoDrawCase("same_" + std::string(pair.label) + "_LESS", pair.high, pair.low, VK_COMPARE_OP_LESS, 0u, 0.5f);
    }
    for (const auto& pair : sameBucketPairs) {
        addTwoDrawCase("same_" + std::string(pair.label) + "_GREATER", pair.low, pair.high, VK_COMPARE_OP_GREATER, 0u, 0.5f);
    }
    for (const auto& pair : sameBucketPairs) {
        addTwoDrawCase("same_" + std::string(pair.label) + "_EQUAL", pair.high, pair.low, VK_COMPARE_OP_EQUAL, Covered, 0.5f);
    }

    addTwoDrawCase("same_bucket_0_NOT_EQUAL", sameBucketPairs[0].high, sameBucketPairs[0].low, VK_COMPARE_OP_NOT_EQUAL, 0u, 0.5f);
    addTwoDrawCase("same_bucket_32768_NOT_EQUAL", sameBucketPairs[3].high, sameBucketPairs[3].low, VK_COMPARE_OP_NOT_EQUAL, 0u, 0.5f);
    addTwoDrawCase("same_bucket_65534_NOT_EQUAL", sameBucketPairs[5].high, sameBucketPairs[5].low, VK_COMPARE_OP_NOT_EQUAL, 0u, 0.5f);

    addTwoDrawCase("same_bucket_1_LESS_OR_EQUAL_higher_second", sameBucketPairs[1].low, sameBucketPairs[1].high, VK_COMPARE_OP_LESS_OR_EQUAL, Covered, 0.5f);
    addTwoDrawCase("same_bucket_32768_LESS_OR_EQUAL_higher_second", sameBucketPairs[3].low, sameBucketPairs[3].high, VK_COMPARE_OP_LESS_OR_EQUAL, Covered, 0.5f);
    addTwoDrawCase("same_bucket_32768_GREATER_OR_EQUAL_lower_second", sameBucketPairs[3].high, sameBucketPairs[3].low, VK_COMPARE_OP_GREATER_OR_EQUAL, Covered, 0.5f);
    addTwoDrawCase("same_bucket_32768_ALWAYS_control", sameBucketPairs[3].high, sameBucketPairs[3].low, VK_COMPARE_OP_ALWAYS, Covered, 0.5f);

    const std::array<std::uint32_t, 5> boundaryBuckets{0u, 16383u, 32767u, 49151u, 65534u};
    for (const auto bucket : boundaryBuckets) {
        const float below = BucketOffset(bucket, 0.35f);
        const float above = BucketOffset(bucket, 0.65f);
        addTwoDrawCase("adjacent_" + std::to_string(bucket) + "_" + std::to_string(bucket + 1u) + "_LESS_closer", above, below, VK_COMPARE_OP_LESS, Covered, 0.5f);
    }
    for (const auto bucket : std::array<std::uint32_t, 3>{0u, 32767u, 65534u}) {
        const float below = BucketOffset(bucket, 0.35f);
        const float above = BucketOffset(bucket, 0.65f);
        addTwoDrawCase("adjacent_" + std::to_string(bucket) + "_" + std::to_string(bucket + 1u) + "_LESS_farther", below, above, VK_COMPARE_OP_LESS, 0u, 0.5f);
    }
    for (const auto bucket : std::array<std::uint32_t, 2>{0u, 32767u}) {
        const float below = BucketOffset(bucket, 0.35f);
        const float above = BucketOffset(bucket, 0.65f);
        addTwoDrawCase("adjacent_" + std::to_string(bucket) + "_" + std::to_string(bucket + 1u) + "_EQUAL", above, below, VK_COMPARE_OP_EQUAL, 0u, 0.5f);
    }
    for (const auto bucket : std::array<std::uint32_t, 2>{32767u, 65534u}) {
        const float below = BucketOffset(bucket, 0.35f);
        const float above = BucketOffset(bucket, 0.65f);
        addTwoDrawCase("adjacent_" + std::to_string(bucket) + "_" + std::to_string(bucket + 1u) + "_NOT_EQUAL", above, below, VK_COMPARE_OP_NOT_EQUAL, Covered, 0.5f);
    }

    addTwoDrawCase("rounding_tie_half_stored_as_bucket_32768_EQUAL", 0.5f, BucketCenter(32768u), VK_COMPARE_OP_EQUAL, Covered, 1.0f);
    addTwoDrawCase("rounding_tie_half_not_stored_as_bucket_32767_EQUAL", 0.5f, BucketCenter(32767u), VK_COMPARE_OP_EQUAL, 0u, 1.0f);
    addTwoDrawCase("rounding_tie_half_above_bucket_32767_GREATER", BucketCenter(32767u), 0.5f, VK_COMPARE_OP_GREATER, Covered, 1.0f);

    {
        CaseSpec chain;
        chain.name = "chain_3draw_same_bucket_overwrite_then_LESS";
        StepSpec s1;
        s1.triangle = FlatTriangle(BucketOffset(32768u, -0.40f));
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(BucketOffset(32768u, 0.40f));
        s2.depthWrite = true;
        s2.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
        StepSpec s3;
        s3.triangle = FlatTriangle(BucketOffset(32768u, -0.20f));
        s3.depthWrite = false;
        s3.depthCompare = VK_COMPARE_OP_LESS;
        chain.steps = {s1, s2, s3};
        chain.expectedCovered = 0u;
        cases.push_back(std::move(chain));
    }

    {
        CaseSpec preserve;
        preserve.name = "chain_3draw_depthWrite_disabled_preserves_quantized";
        StepSpec s1;
        s1.triangle = FlatTriangle(BucketOffset(16384u, 0.35f));
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(BucketCenter(8000u));
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s3;
        s3.triangle = FlatTriangle(BucketOffset(16384u, -0.35f));
        s3.depthWrite = false;
        s3.depthCompare = VK_COMPARE_OP_EQUAL;
        preserve.steps = {s1, s2, s3};
        preserve.expectedCovered = Covered;
        cases.push_back(std::move(preserve));
    }

    {
        CaseSpec clearLess;
        clearLess.name = "clearDepth_subquantum_bucket_32768_LESS";
        clearLess.clearDepth = 0.500004f;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500002f);
        s1.depthWrite = false;
        s1.depthCompare = VK_COMPARE_OP_LESS;
        clearLess.steps = {s1};
        clearLess.expectedCovered = 0u;
        cases.push_back(std::move(clearLess));
    }

    {
        CaseSpec clearEqual;
        clearEqual.name = "clearDepth_subquantum_bucket_32768_EQUAL";
        clearEqual.clearDepth = 0.500004f;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500002f);
        s1.depthWrite = false;
        s1.depthCompare = VK_COMPARE_OP_EQUAL;
        clearEqual.steps = {s1};
        clearEqual.expectedCovered = Covered;
        cases.push_back(std::move(clearEqual));
    }

    {
        CaseSpec stDepthFail;
        stDepthFail.name = "stencil_depthFailOp_increment_on_subquantum_LESS";
        stDepthFail.clearStencil = 0u;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500004f);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(0.500002f);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        s2.stencilTest = true;
        s2.stencil = MakeStencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_KEEP, 1u);
        StepSpec s3;
        s3.triangle = FlatTriangle(0.5f);
        s3.depthTest = false;
        s3.stencilTest = true;
        s3.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 1u);
        stDepthFail.steps = {s1, s2, s3};
        stDepthFail.expectedCovered = Covered;
        cases.push_back(std::move(stDepthFail));
    }

    {
        CaseSpec stPassSuppressed;
        stPassSuppressed.name = "stencil_passOp_suppressed_on_subquantum_LESS";
        stPassSuppressed.clearStencil = 0u;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500004f);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(0.500002f);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        s2.stencilTest = true;
        s2.stencil = MakeStencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 7u);
        StepSpec s3;
        s3.triangle = FlatTriangle(0.5f);
        s3.depthTest = false;
        s3.stencilTest = true;
        s3.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 7u);
        stPassSuppressed.steps = {s1, s2, s3};
        stPassSuppressed.expectedCovered = 0u;
        cases.push_back(std::move(stPassSuppressed));
    }

    {
        CaseSpec stEqualPass;
        stEqualPass.name = "stencil_passOp_replace_on_subquantum_EQUAL";
        stEqualPass.clearStencil = 0u;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500004f);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(0.500002f);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        s2.stencilTest = true;
        s2.stencil = MakeStencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 5u);
        StepSpec s3;
        s3.triangle = FlatTriangle(0.5f);
        s3.depthTest = false;
        s3.stencilTest = true;
        s3.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 5u);
        stEqualPass.steps = {s1, s2, s3};
        stEqualPass.expectedCovered = Covered;
        cases.push_back(std::move(stEqualPass));
    }

    {
        CaseSpec stEqualDepthFailSuppressed;
        stEqualDepthFailSuppressed.name = "stencil_depthFailOp_suppressed_on_subquantum_EQUAL";
        stEqualDepthFailSuppressed.clearStencil = 0u;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500004f);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(0.500002f);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        s2.stencilTest = true;
        s2.stencil = MakeStencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, 9u);
        StepSpec s3;
        s3.triangle = FlatTriangle(0.5f);
        s3.depthTest = false;
        s3.stencilTest = true;
        s3.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 9u);
        stEqualDepthFailSuppressed.steps = {s1, s2, s3};
        stEqualDepthFailSuppressed.expectedCovered = 0u;
        cases.push_back(std::move(stEqualDepthFailSuppressed));
    }

    {
        CaseSpec stFailOp;
        stFailOp.name = "stencil_failOp_replace_before_depth_test";
        stFailOp.clearStencil = 3u;
        StepSpec s1;
        s1.triangle = FlatTriangle(0.500004f);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(0.500002f);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        s2.stencilTest = true;
        s2.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_REPLACE, 2u);
        StepSpec s3;
        s3.triangle = FlatTriangle(0.5f);
        s3.depthTest = false;
        s3.stencilTest = true;
        s3.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 2u);
        stFailOp.steps = {s1, s2, s3};
        stFailOp.expectedCovered = Covered;
        cases.push_back(std::move(stFailOp));
    }

    {
        CaseSpec stAdjacentPass;
        stAdjacentPass.name = "stencil_passOp_replace_on_adjacent_bucket_LESS";
        stAdjacentPass.clearStencil = 0u;
        StepSpec s1;
        s1.triangle = FlatTriangle(BucketOffset(32767u, 0.65f));
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = FlatTriangle(BucketOffset(32767u, 0.35f));
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        s2.stencilTest = true;
        s2.stencil = MakeStencil(VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 11u);
        StepSpec s3;
        s3.triangle = FlatTriangle(0.5f);
        s3.depthTest = false;
        s3.stencilTest = true;
        s3.stencil = MakeStencil(VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 11u);
        stAdjacentPass.steps = {s1, s2, s3};
        stAdjacentPass.expectedCovered = Covered;
        cases.push_back(std::move(stAdjacentPass));
    }

    {
        const float zHigh = BucketOffset(32768u, 0.35f);
        const float zLow = BucketOffset(32768u, -0.35f);
        const float ndcHigh = (zHigh - 0.25f) * 2.0f;
        const float ndcLow = (zLow - 0.25f) * 2.0f;

        CaseSpec vpLess;
        vpLess.name = "viewport_subrange_025_075_subquantum_LESS";
        StepSpec s1;
        s1.triangle = FlatTriangle(ndcHigh);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        s1.minDepth = 0.25f;
        s1.maxDepth = 0.75f;
        StepSpec s2;
        s2.triangle = FlatTriangle(ndcLow);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        s2.minDepth = 0.25f;
        s2.maxDepth = 0.75f;
        vpLess.steps = {s1, s2};
        vpLess.expectedCovered = 0u;
        cases.push_back(std::move(vpLess));

        CaseSpec vpEqual;
        vpEqual.name = "viewport_subrange_025_075_subquantum_EQUAL";
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        vpEqual.steps = {s1, s2};
        vpEqual.expectedCovered = Covered;
        cases.push_back(std::move(vpEqual));
    }

    {
        const float zHigh = BucketOffset(32768u, 0.35f);
        const float zLow = BucketOffset(32768u, -0.35f);
        const float ndcForLow = 1.0f - zLow;
        const float ndcForHigh = 1.0f - zHigh;

        CaseSpec revGreater;
        revGreater.name = "viewport_reversed_z_1_to_0_subquantum_GREATER";
        revGreater.clearDepth = 0.0f;
        StepSpec s1;
        s1.triangle = FlatTriangle(ndcForLow);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        s1.minDepth = 1.0f;
        s1.maxDepth = 0.0f;
        StepSpec s2;
        s2.triangle = FlatTriangle(ndcForHigh);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_GREATER;
        s2.minDepth = 1.0f;
        s2.maxDepth = 0.0f;
        revGreater.steps = {s1, s2};
        revGreater.expectedCovered = 0u;
        cases.push_back(std::move(revGreater));

        CaseSpec revEqual;
        revEqual.name = "viewport_reversed_z_1_to_0_subquantum_EQUAL";
        revEqual.clearDepth = 0.0f;
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        revEqual.steps = {s1, s2};
        revEqual.expectedCovered = Covered;
        cases.push_back(std::move(revEqual));
    }

    if (context.depthClipControl) {
        const float zHigh = BucketOffset(16384u, 0.35f);
        const float zLow = BucketOffset(16384u, -0.35f);
        const float ndcHigh = zHigh * 2.0f - 1.0f;
        const float ndcLow = zLow * 2.0f - 1.0f;

        CaseSpec negClip;
        negClip.name = "viewport_negativeOneToOne_subquantum_LESS";
        StepSpec s1;
        s1.triangle = FlatTriangle(ndcHigh);
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        s1.negativeOneToOne = true;
        StepSpec s2;
        s2.triangle = FlatTriangle(ndcLow);
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        s2.negativeOneToOne = true;
        negClip.steps = {s1, s2};
        negClip.expectedCovered = 0u;
        cases.push_back(std::move(negClip));
    }

    {
        CaseSpec sloped;
        sloped.name = "sloped_interpolated_triangle_within_bucket_32768_LESS";
        StepSpec s1;
        s1.triangle = {BucketOffset(32768u, 0.10f), BucketOffset(32768u, 0.40f), BucketOffset(32768u, 0.40f)};
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.triangle = {BucketOffset(32768u, -0.40f), BucketOffset(32768u, -0.10f), BucketOffset(32768u, -0.10f)};
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        sloped.steps = {s1, s2};
        sloped.expectedCovered = 0u;
        cases.push_back(std::move(sloped));
    }

    {
        const int fragHigh = addFragSpec(0.500004f, 0.0f, 1.0f);
        const int fragLow = addFragSpec(0.500002f, 0.0f, 1.0f);
        const int fragClampMax = addFragSpec(1.5f, 0.25f, BucketOffset(49152u, 0.35f));
        const int fragClampMin = addFragSpec(-0.5f, BucketOffset(16384u, 0.35f), 0.75f);

        CaseSpec fdLess;
        fdLess.name = "fragDepth_export_subquantum_LESS";
        StepSpec s1;
        s1.fragExportIndex = fragHigh;
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec s2;
        s2.fragExportIndex = fragLow;
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        fdLess.steps = {s1, s2};
        fdLess.expectedCovered = 0u;
        cases.push_back(std::move(fdLess));

        CaseSpec fdEqual;
        fdEqual.name = "fragDepth_export_subquantum_EQUAL";
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        fdEqual.steps = {s1, s2};
        fdEqual.expectedCovered = Covered;
        cases.push_back(std::move(fdEqual));

        CaseSpec fdVsMix;
        fdVsMix.name = "fragDepth_write_then_vertexDepth_subquantum_LESS";
        StepSpec sVs;
        sVs.triangle = FlatTriangle(0.500002f);
        sVs.depthWrite = false;
        sVs.depthCompare = VK_COMPARE_OP_LESS;
        fdVsMix.steps = {s1, sVs};
        fdVsMix.expectedCovered = 0u;
        cases.push_back(std::move(fdVsMix));

        CaseSpec fdMaxClamp;
        fdMaxClamp.name = "fragDepth_clamped_to_ZMAX_subquantum_EQUAL";
        StepSpec sClampMax;
        sClampMax.fragExportIndex = fragClampMax;
        sClampMax.depthWrite = true;
        sClampMax.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec sTestMax;
        sTestMax.triangle = FlatTriangle(BucketOffset(49152u, -0.35f));
        sTestMax.depthWrite = false;
        sTestMax.depthCompare = VK_COMPARE_OP_EQUAL;
        fdMaxClamp.steps = {sClampMax, sTestMax};
        fdMaxClamp.expectedCovered = Covered;
        cases.push_back(std::move(fdMaxClamp));

        CaseSpec fdMinClamp;
        fdMinClamp.name = "fragDepth_clamped_to_ZMIN_subquantum_LESS";
        StepSpec sClampMin;
        sClampMin.fragExportIndex = fragClampMin;
        sClampMin.depthWrite = true;
        sClampMin.depthCompare = VK_COMPARE_OP_ALWAYS;
        StepSpec sTestMin;
        sTestMin.triangle = FlatTriangle(BucketOffset(16384u, -0.35f));
        sTestMin.depthWrite = false;
        sTestMin.depthCompare = VK_COMPARE_OP_LESS;
        fdMinClamp.steps = {sClampMin, sTestMin};
        fdMinClamp.expectedCovered = 0u;
        cases.push_back(std::move(fdMinClamp));
    }

    if (context.depthBounds) {
        const auto addDepthBoundsCase = [&](std::string name, float clearOrWriteDepth, bool useClear, float minB, float maxB, std::uint32_t expected) {
            CaseSpec spec;
            spec.name = std::move(name);
            StepSpec testStep;
            testStep.triangle = FlatTriangle(0.5f);
            testStep.depthTest = false;
            testStep.depthBoundsTest = true;
            testStep.minDepthBounds = minB;
            testStep.maxDepthBounds = maxB;
            if (useClear) {
                spec.clearDepth = clearOrWriteDepth;
                spec.steps = {testStep};
            } else {
                StepSpec writeStep;
                writeStep.triangle = FlatTriangle(clearOrWriteDepth);
                writeStep.depthWrite = true;
                writeStep.depthCompare = VK_COMPARE_OP_ALWAYS;
                spec.steps = {writeStep, testStep};
            }
            spec.expectedCovered = expected;
            cases.push_back(std::move(spec));
        };

        addDepthBoundsCase("depthBounds_round_up_into_bucket_32768_inside", BucketOffset(32768u, -0.40f), false, BucketOffset(32768u, -0.15f), BucketOffset(32768u, 0.15f), Covered);
        addDepthBoundsCase("depthBounds_round_down_into_bucket_32768_inside", BucketOffset(32768u, 0.40f), false, BucketOffset(32768u, -0.15f), BucketOffset(32768u, 0.15f), Covered);
        addDepthBoundsCase("depthBounds_subquantum_bounds_round_to_32768_inside", BucketOffset(32768u, 0.40f), false, BucketOffset(32768u, 0.20f), BucketOffset(32768u, 0.49f), Covered);
        addDepthBoundsCase("depthBounds_subquantum_bounds_round_to_32769_outside", BucketCenter(32768u), false, BucketOffset(32768u, 0.51f), BucketOffset(32768u, 0.90f), 0u);
        addDepthBoundsCase("depthBounds_subquantum_min_rounds_down_to_32768_inside", BucketCenter(32768u), false, BucketOffset(32768u, 0.49f), 1.0f, Covered);
        addDepthBoundsCase("depthBounds_subquantum_max_rounds_up_to_32768_inside", BucketCenter(32768u), false, 0.0f, BucketOffset(32768u, -0.49f), Covered);
        addDepthBoundsCase("depthBounds_subquantum_max_rounds_down_to_32767_outside", BucketCenter(32768u), false, 0.0f, BucketOffset(32768u, -0.51f), 0u);
        addDepthBoundsCase("depthBounds_half_tie_05_matches_bucket_32768_inside", BucketCenter(32768u), false, 0.5f, 0.5f, Covered);
        addDepthBoundsCase("depthBounds_half_tie_05_excludes_bucket_32767_outside", BucketCenter(32767u), false, 0.5f, 0.5f, 0u);
        if (context.depthRangeUnrestricted) {
            addDepthBoundsCase("depthBounds_unrestricted_negative_clamped_zero_inside", 0.0f, true, -0.5f, -0.1f, Covered);
            addDepthBoundsCase("depthBounds_unrestricted_above_one_clamped_one_inside", 1.0f, true, 1.1f, 1.5f, Covered);
        }
        addDepthBoundsCase("depthBounds_clearDepth_subquantum_quantized_inside", 0.500004f, true, BucketOffset(32768u, -0.10f), BucketOffset(32768u, 0.10f), Covered);
    }

    {
        CaseSpec clearRecipEqual;
        clearRecipEqual.name = "clearDepth_bucket_385_reciprocal_exact_EQUAL";
        clearRecipEqual.clearDepth = BucketOffset(385u, 0.30f);
        StepSpec s1;
        s1.triangle = FlatTriangle(BucketOffset(385u, -0.30f));
        s1.depthWrite = false;
        s1.depthCompare = VK_COMPARE_OP_EQUAL;
        clearRecipEqual.steps = {s1};
        clearRecipEqual.expectedCovered = Covered;
        cases.push_back(std::move(clearRecipEqual));
    }

    {
        CaseSpec slopeBiasLess;
        slopeBiasLess.name = "depthBias_slope_subquantum_within_bucket_32768_LESS";
        StepSpec s1;
        s1.triangle = {BucketCenter(32768u), BucketOffset(32768u, 0.10f), BucketOffset(32768u, 0.10f)};
        s1.depthWrite = true;
        s1.depthCompare = VK_COMPARE_OP_ALWAYS;
        s1.depthBias = true;
        s1.depthBiasSlope = 0.5f;
        StepSpec s2;
        s2.triangle = FlatTriangle(BucketOffset(32768u, -0.35f));
        s2.depthWrite = false;
        s2.depthCompare = VK_COMPARE_OP_LESS;
        slopeBiasLess.steps = {s1, s2};
        slopeBiasLess.expectedCovered = 0u;
        cases.push_back(std::move(slopeBiasLess));

        CaseSpec slopeBiasEqual;
        slopeBiasEqual.name = "depthBias_slope_subquantum_within_bucket_32768_EQUAL";
        s2.depthCompare = VK_COMPARE_OP_EQUAL;
        slopeBiasEqual.steps = {s1, s2};
        slopeBiasEqual.expectedCovered = Covered;
        cases.push_back(std::move(slopeBiasEqual));

        CaseSpec slopeBiasMultiStepLess;
        slopeBiasMultiStepLess.name = "depthBias_slope_multistep_bucket_32768_to_32772_LESS";
        StepSpec ms1;
        ms1.triangle = {BucketCenter(32768u), BucketOffset(32768u, 0.25f), BucketCenter(32768u)};
        ms1.depthWrite = true;
        ms1.depthCompare = VK_COMPARE_OP_ALWAYS;
        ms1.depthBias = true;
        ms1.depthBiasSlope = 2048.0f;
        StepSpec ms2;
        ms2.triangle = FlatTriangle(BucketCenter(32771u));
        ms2.depthWrite = false;
        ms2.depthCompare = VK_COMPARE_OP_LESS;
        slopeBiasMultiStepLess.steps = {ms1, ms2};
        slopeBiasMultiStepLess.expectedCovered = Covered;
        cases.push_back(std::move(slopeBiasMultiStepLess));

        CaseSpec slopeBiasMultiStepEqual;
        slopeBiasMultiStepEqual.name = "depthBias_slope_multistep_bucket_32768_to_32772_EQUAL";
        ms2.triangle = FlatTriangle(BucketCenter(32772u));
        ms2.depthCompare = VK_COMPARE_OP_EQUAL;
        slopeBiasMultiStepEqual.steps = {ms1, ms2};
        slopeBiasMultiStepEqual.expectedCovered = Covered;
        cases.push_back(std::move(slopeBiasMultiStepEqual));

        CaseSpec slopeBiasMultiStepUnbiasedEqual;
        slopeBiasMultiStepUnbiasedEqual.name = "depthBias_slope_multistep_not_unbiased_32768_EQUAL";
        ms2.triangle = FlatTriangle(BucketCenter(32768u));
        ms2.depthCompare = VK_COMPARE_OP_EQUAL;
        slopeBiasMultiStepUnbiasedEqual.steps = {ms1, ms2};
        slopeBiasMultiStepUnbiasedEqual.expectedCovered = 0u;
        cases.push_back(std::move(slopeBiasMultiStepUnbiasedEqual));

        StepSpec cs1;
        cs1.triangle = FlatTriangle(BucketCenter(32768u));
        cs1.depthWrite = true;
        cs1.depthCompare = VK_COMPARE_OP_ALWAYS;
        cs1.depthBias = true;
        cs1.depthBiasConstant = 4.0f;

        CaseSpec constBiasMultiStepLess;
        constBiasMultiStepLess.name = "depthBias_constant_multistep_bucket_32768_LESS";
        StepSpec cs2;
        cs2.triangle = FlatTriangle(BucketCenter(32771u));
        cs2.depthWrite = false;
        cs2.depthCompare = VK_COMPARE_OP_LESS;
        constBiasMultiStepLess.steps = {cs1, cs2};
        constBiasMultiStepLess.expectedCovered = Covered;
        constBiasMultiStepLess.skipOnD32Fallback = true;
        cases.push_back(std::move(constBiasMultiStepLess));

        CaseSpec constBiasMultiStepEqual32772;
        constBiasMultiStepEqual32772.name = "depthBias_constant_multistep_native_D16_32772_EQUAL";
        cs2.triangle = FlatTriangle(BucketCenter(32772u));
        cs2.depthCompare = VK_COMPARE_OP_EQUAL;
        constBiasMultiStepEqual32772.steps = {cs1, cs2};
        constBiasMultiStepEqual32772.skipOnD32Fallback = true;
        constBiasMultiStepEqual32772.matchNativeD16 = true;
        cases.push_back(std::move(constBiasMultiStepEqual32772));

        CaseSpec constBiasMultiStepEqual32776;
        constBiasMultiStepEqual32776.name = "depthBias_constant_multistep_native_D16_32776_EQUAL";
        cs2.triangle = FlatTriangle(BucketCenter(32776u));
        cs2.depthCompare = VK_COMPARE_OP_EQUAL;
        constBiasMultiStepEqual32776.steps = {cs1, cs2};
        constBiasMultiStepEqual32776.skipOnD32Fallback = true;
        constBiasMultiStepEqual32776.matchNativeD16 = true;
        cases.push_back(std::move(constBiasMultiStepEqual32776));

        CaseSpec constBiasMultiStepUnbiasedEqual;
        constBiasMultiStepUnbiasedEqual.name = "depthBias_constant_multistep_not_unbiased_32768_EQUAL";
        cs2.triangle = FlatTriangle(BucketCenter(32768u));
        cs2.depthCompare = VK_COMPARE_OP_EQUAL;
        constBiasMultiStepUnbiasedEqual.steps = {cs1, cs2};
        constBiasMultiStepUnbiasedEqual.expectedCovered = 0u;
        constBiasMultiStepUnbiasedEqual.skipOnD32Fallback = true;
        cases.push_back(std::move(constBiasMultiStepUnbiasedEqual));
    }

    addTwoDrawCase("control_D16_UNORM_no_stencil_subquantum_LESS", 0.500004f, 0.500002f, VK_COMPARE_OP_LESS, 0u, 1.0f, VK_FORMAT_D16_UNORM);
    addTwoDrawCase("control_D16_UNORM_no_stencil_subquantum_EQUAL", 0.500004f, 0.500002f, VK_COMPARE_OP_EQUAL, Covered, 1.0f, VK_FORMAT_D16_UNORM);
    addTwoDrawCase("control_D32_SFLOAT_S8_UINT_subquantum_LESS_passes", 0.500004f, 0.500002f, VK_COMPARE_OP_LESS, Covered, 1.0f, VK_FORMAT_D32_SFLOAT_S8_UINT);
    if (DepthAttachmentSupported(VK_FORMAT_D24_UNORM_S8_UINT)) {
        addTwoDrawCase("control_D24_UNORM_S8_UINT_guest_not_quantized_LESS", 0.500004f, 0.500002f, VK_COMPARE_OP_LESS, Covered, 1.0f, VK_FORMAT_D24_UNORM_S8_UINT);
        addTwoDrawCase("control_D24_UNORM_S8_UINT_guest_not_quantized_EQUAL", 0.500004f, 0.500002f, VK_COMPARE_OP_EQUAL, 0u, 1.0f, VK_FORMAT_D24_UNORM_S8_UINT);
    }

    for (auto& testCase : cases) {
        for (auto& step : testCase.steps) {
            step.firstVertex = RegisterTriangle(step.triangle);
        }
    }

    return cases;
}

std::uint32_t ExecuteStep(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& baseShaders, std::span<const FragExportShader> fragShaders, const CaseSpec& testCase, const StepSpec& step) {
    std::fill(Pixels.begin(), Pixels.end(), std::byte{0});
    const ShaderRecompiler::RecompileResult* pixelProgram = &baseShaders.pixel;
    if (step.fragExportIndex >= 0) {
        pixelProgram = &fragShaders[static_cast<std::size_t>(step.fragExportIndex)].pixel;
    }
    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &baseShaders.vertex, 0},
        {ShaderStage::Fragment, pixelProgram, PixelPushOffset(baseShaders.vertexPush, device.Target())}
    }};
    constexpr VkExtent2D extent{Width, Height};
    const std::uint64_t stencilAddr = (testCase.format == VK_FORMAT_D16_UNORM) ? 0u : reinterpret_cast<std::uintptr_t>(StencilStorage.data());
    State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), extent, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = DepthTarget{reinterpret_cast<std::uintptr_t>(DepthStorage.data()), stencilAddr, extent, testCase.format, testCase.clearDepth, testCase.clearStencil, 0u, false};
    state.depthTest = step.depthTest;
    state.depthWrite = step.depthWrite;
    state.depthCompare = step.depthCompare;
    state.depthBoundsTest = step.depthBoundsTest;
    state.minDepthBounds = step.minDepthBounds;
    state.maxDepthBounds = step.maxDepthBounds;
    state.depthBias = step.depthBias;
    state.depthBiasConstant = step.depthBiasConstant;
    state.depthBiasSlope = step.depthBiasSlope;
    state.depthBiasClamp = step.depthBiasClamp;
    state.stencilTest = step.stencilTest;
    state.stencilFront = step.stencil;
    state.stencilBack = step.stencil;
    state.renderExtent = extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), step.minDepth, step.maxDepth};
    state.negativeOneToOne = step.negativeOneToOne;
    state.scissor = {{0, 0}, extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false, step.firstVertex};
    AgcDriver::Graphics::Draw(context, state, draw, shaders);
    device.WaitIdle();
    std::uint32_t covered = 0u;
    for (std::size_t pixel = 0; pixel < Pixels.size() / 4u; ++pixel) {
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) == 255u) ++covered;
    }
    return covered;
}

void VerifyEarlyFragmentTestsMemoryWriteRefusal(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& baseShaders) {
    if (HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT) == VK_FORMAT_D16_UNORM_S8_UINT) return;
    ShaderRecompiler::RecompileResult earlyWritePixel = baseShaders.pixel;
    const auto words = earlyWritePixel.spirv.Words();
    std::uint32_t entryPointId = 0u;
    bool hasEarly = false;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = words[cursor] & 0xffffu;
        if (op == 15u && count >= 3u) entryPointId = words[cursor + 2];
        if (op == 16u && count >= 3u && words[cursor + 2] == 9u) hasEarly = true;
        if (op == 54u) break;
        cursor += count;
    }
    if (!hasEarly && entryPointId != 0u) {
        std::vector<std::uint32_t> patched;
        patched.reserve(words.size() + 3u);
        for (std::size_t cursor = 0; cursor < words.size();) {
            if (cursor < 5u) {
                patched.push_back(words[cursor++]);
                continue;
            }
            const auto count = words[cursor] >> 16u;
            const auto op = words[cursor] & 0xffffu;
            patched.insert(patched.end(), words.begin() + static_cast<std::ptrdiff_t>(cursor), words.begin() + static_cast<std::ptrdiff_t>(cursor + count));
            if (op == 15u) {
                patched.insert(patched.end(), {(3u << 16u) | 16u, entryPointId, 9u});
            }
            cursor += count;
        }
        earlyWritePixel.spirv = std::move(patched);
    }
    ShaderRecompiler::DescriptorBinding storageBinding{};
    storageBinding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    storageBinding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    storageBinding.count = 1u;
    storageBinding.bufferWritten = {true};
    earlyWritePixel.bindings.push_back(std::move(storageBinding));

    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &baseShaders.vertex, 0},
        {ShaderStage::Fragment, &earlyWritePixel, PixelPushOffset(baseShaders.vertexPush, device.Target())}
    }};
    constexpr VkExtent2D extent{Width, Height};
    State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), extent, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = DepthTarget{reinterpret_cast<std::uintptr_t>(DepthStorage.data()), reinterpret_cast<std::uintptr_t>(StencilStorage.data()), extent, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u, 0u, false};
    state.depthTest = true;
    state.depthWrite = true;
    state.depthCompare = VK_COMPARE_OP_LESS;
    state.renderExtent = extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0.0f, 1.0f};
    state.scissor = {{0, 0}, extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    const AgcDriver::Graphics::VertexInputLayout vertexInput{};
    const AgcDriver::Graphics::PipelineSetLayouts setLayouts{};
    bool threw = false;
    try {
        const AgcDriver::Graphics::Pipeline pipeline(context, state, vertexInput, setLayouts, shaders);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find("early fragment tests with memory writes on a 16-bit depth target with stencil") != std::string_view::npos;
    }
    Require(threw, "early fragment tests with memory writes and depthTest enabled on a fallback D16_UNORM_S8_UINT target must throw");
}

VKAPI_ATTR void VKAPI_CALL FormatPropertiesWithoutD16S8(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    RealFormatProperties(RealPhysical, format, properties);
    if (format == VK_FORMAT_D16_UNORM_S8_UINT) properties->optimalTilingFeatures = 0u;
}

VKAPI_ATTR void VKAPI_CALL FormatPropertiesOnlyD32S8(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    RealFormatProperties(RealPhysical, format, properties);
    if (format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) properties->optimalTilingFeatures = 0u;
}

VKAPI_ATTR VkResult VKAPI_CALL ForwardImageFormatProperties(VkPhysicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    return RealImageFormatProperties(RealPhysical, format, type, tiling, usage, flags, properties);
}

VKAPI_ATTR void VKAPI_CALL ForwardSparseImageFormatProperties(VkPhysicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, std::uint32_t* count, VkSparseImageFormatProperties* properties) {
    RealSparseImageFormatProperties(RealPhysical, format, type, samples, usage, tiling, count, properties);
}

const char* FormatName(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM_S8_UINT: return "VK_FORMAT_D16_UNORM_S8_UINT";
    case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
    default: return "OTHER";
    }
}

struct ModeResult {
    std::size_t passed = 0;
    std::size_t failed = 0;
};

ModeResult RunMode(AgcDriver::VulkanDevice& device, const Context& context, std::string_view modeName, VkFormat expectedHostFormat, const Shaders& baseShaders, std::span<const FragExportShader> fragShaders, std::span<const CaseSpec> cases) {
    device.WaitIdle();
    WaitForOptimizedPipelines(context.device);
    ClearCachedPipelines(context.device);
    ClearPipelineLibraries(context.device);
    ClearDepthSurfaces(context.device);

    const VkFormat hostFormat = HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT);
    Require(hostFormat == expectedHostFormat, std::string("mode ") + std::string(modeName) + " selected " + FormatName(hostFormat) + ", expected " + FormatName(expectedHostFormat));
    VerifyEarlyFragmentTestsMemoryWriteRefusal(device, context, baseShaders);
    std::printf("=== Mode: %.*s (HostDepthFormat(D16_UNORM_S8_UINT) = %s) ===\n", static_cast<int>(modeName.size()), modeName.data(), FormatName(hostFormat));
    std::fflush(stdout);

    ModeResult summary{};
    std::size_t ran = 0u;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto& testCase = cases[index];
        if (testCase.skipOnD32Fallback && expectedHostFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) continue;
        ++ran;
        std::uint32_t expected = testCase.expectedCovered;
        if (testCase.matchNativeD16) {
            CaseSpec nativeCase = testCase;
            nativeCase.format = VK_FORMAT_D16_UNORM;
            ClearDepthSurfaces(context.device);
            for (const auto& step : nativeCase.steps) {
                expected = ExecuteStep(device, context, baseShaders, fragShaders, nativeCase, step);
            }
        }
        ClearDepthSurfaces(context.device);
        std::uint32_t actual = 0u;
        for (const auto& step : testCase.steps) {
            actual = ExecuteStep(device, context, baseShaders, fragShaders, testCase, step);
        }
        const bool ok = (actual == expected);
        if (ok) {
            ++summary.passed;
        } else {
            ++summary.failed;
        }
        std::printf("  [%02zu/%02zu] %-54s expected=%4u actual=%4u [%s]\n",
                    ran,
                    cases.size(),
                    testCase.name.c_str(),
                    expected,
                    actual,
                    ok ? "PASS" : "FAIL");
    }
    std::printf("--- Mode %.*s summary: %zu/%zu passed, %zu failed ---\n\n",
                static_cast<int>(modeName.size()),
                modeName.data(),
                summary.passed,
                ran,
                summary.failed);
    std::fflush(stdout);
    return summary;
}

} // namespace

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;

        const Context baseContext = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        RealPhysical = baseContext.physical;
        RealFormatProperties = baseContext.formatProperties;
        RealImageFormatProperties = baseContext.imageFormatProperties;
        RealSparseImageFormatProperties = baseContext.sparseImageFormatProperties;
        Require(RealFormatProperties != nullptr, "the test device exposes no format properties query");

        std::vector<FragExportSpec> fragSpecs;
        const auto cases = BuildCases(fragSpecs, baseContext);
        const auto baseShaders = CompileBaseShaders(*device);
        std::vector<FragExportShader> fragShaders(fragSpecs.size());
        for (std::size_t i = 0; i < fragSpecs.size(); ++i) {
            fragShaders[i].spec = fragSpecs[i];
            CompileFragExportShader(*device, baseShaders, fragShaders[i]);
        }

        std::printf("Device: %s\n", device->DeviceName().c_str());

        std::size_t totalFailed = 0u;

        const auto defaultSummary = RunMode(*device, baseContext, "DefaultHostFormat", ExpectedHostFormat(), baseShaders, fragShaders, cases);
        totalFailed += defaultSummary.failed;

        VkFormatProperties d24Props{};
        RealFormatProperties(RealPhysical, VK_FORMAT_D24_UNORM_S8_UINT, &d24Props);
        if ((d24Props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u &&
            HostDepthFormat(baseContext, VK_FORMAT_D16_UNORM_S8_UINT) != VK_FORMAT_D24_UNORM_S8_UINT) {
            Context d24Context = baseContext;
            d24Context.physical = reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(0xD2408u));
            d24Context.memoryProperties2 = nullptr;
            d24Context.formatProperties = FormatPropertiesWithoutD16S8;
            d24Context.imageFormatProperties = ForwardImageFormatProperties;
            d24Context.sparseImageFormatProperties = ForwardSparseImageFormatProperties;
            const auto d24Summary = RunMode(*device, d24Context, "ForcedD24S8Fallback", VK_FORMAT_D24_UNORM_S8_UINT, baseShaders, fragShaders, cases);
            totalFailed += d24Summary.failed;
        }

        VkFormatProperties d32Props{};
        RealFormatProperties(RealPhysical, VK_FORMAT_D32_SFLOAT_S8_UINT, &d32Props);
        if ((d32Props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u &&
            HostDepthFormat(baseContext, VK_FORMAT_D16_UNORM_S8_UINT) != VK_FORMAT_D32_SFLOAT_S8_UINT) {
            Context d32Context = baseContext;
            d32Context.physical = reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(0xD3208u));
            d32Context.memoryProperties2 = nullptr;
            d32Context.formatProperties = FormatPropertiesOnlyD32S8;
            d32Context.imageFormatProperties = ForwardImageFormatProperties;
            d32Context.sparseImageFormatProperties = ForwardSparseImageFormatProperties;
            static_cast<void>(HostDepthFormat(d32Context, VK_FORMAT_D16_UNORM_S8_UINT));
            d32Context.formatProperties = FormatPropertiesWithoutD16S8;
            const auto d32Summary = RunMode(*device, d32Context, "ForcedD32S8Fallback", VK_FORMAT_D32_SFLOAT_S8_UINT, baseShaders, fragShaders, cases);
            totalFailed += d32Summary.failed;
        }

        device->WaitIdle();
        WaitForOptimizedPipelines(baseContext.device);
        ClearCachedPipelines(baseContext.device);
        ClearPipelineLibraries(baseContext.device);
        ClearDepthSurfaces(baseContext.device);

        Require(totalFailed == 0u, "D16_UNORM_S8_UINT depth precision mismatches detected");
        std::puts("d16 stencil precision tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
