#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t ScissorWidth = 128;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Unorm8888Info = 10u << 2u;
constexpr std::uint32_t Snorm8888Info = (10u << 2u) | (1u << 8u);
constexpr std::uint32_t Unorm88Info = 3u << 2u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t PositionInputs = 0x300u;
constexpr std::uint32_t DepthInput = 0x400u;


alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 64> Programs{};
std::size_t programCount = 0;

std::span<const std::uint32_t> Float32Program(std::array<float, 4> rgba) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t vgpr = 0; vgpr < 4u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(rgba[vgpr]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::span<const std::uint32_t> DepthAsXProgram(std::array<float, 4> rgba) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    code[at++] = 0x7e080300u;
    for (std::uint32_t vgpr = 1; vgpr < 4u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(rgba[vgpr]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

std::span<const std::uint32_t> TwoTargetProgram(std::array<float, 4> first, std::array<float, 4> second) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t vgpr = 0; vgpr < 8u; ++vgpr) {
        code[at++] = 0x7e0002ffu | ((4u + vgpr) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(vgpr < 4u ? first[vgpr] : second[vgpr - 4u]);
    }
    code[at++] = 0xf800000fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xf800181fu;
    code[at++] = 0x0b0a0908u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

const std::array<std::array<float, 4>, 3>& TriangleAt(float z) {
    static std::map<float, std::array<std::array<float, 4>, 3>> triangles;
    return triangles.try_emplace(z, std::array<std::array<float, 4>, 3>{{{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}}}).first->second;
}

struct DrawSetup {
    std::uint32_t inputs = 0;
    std::uint32_t shaderControl = 0;
    std::span<const std::array<float, 4>> vertices = Triangle;
    std::uint64_t depth = 0;
    bool depthWrite = false;
    bool withoutInterlock = false;
};

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate the color target");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

struct BlendCase {
    VkBlendFactor colorSource;
    VkBlendFactor colorDestination;
    VkBlendOp colorOperation;
    VkBlendFactor alphaSource;
    VkBlendFactor alphaDestination;
    VkBlendOp alphaOperation;
    VkColorComponentFlags writeMask;
    std::array<float, 4> constants;
    bool snorm;
};


struct Target {
    AgcDriver::Graphics::ColorTarget color;
    BlendCase blend;
};

void Draw(AgcDriver::VulkanDevice& device, std::span<const Target> targets, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode, const DrawSetup& setup = {}) {
    auto target = device.Target();
    std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
    if (setup.withoutInterlock) std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityFragmentShaderPixelInterlockEXT));
    target.supportedCapabilities = capabilities;
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(setup.vertices.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(setup.vertices.size()), 0x01016facu};
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

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    for (const auto& entry : targets) {
        auto color = entry.color;
        color.partialWriteMask = AgcDriver::Graphics::UnclampedBlendMask;
        state.colors.push_back(color);
        VkPipelineColorBlendAttachmentState blend{};
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = entry.blend.colorSource;
        blend.dstColorBlendFactor = entry.blend.colorDestination;
        blend.colorBlendOp = entry.blend.colorOperation;
        blend.srcAlphaBlendFactor = entry.blend.alphaSource;
        blend.dstAlphaBlendFactor = entry.blend.alphaDestination;
        blend.alphaBlendOp = entry.blend.alphaOperation;
        blend.colorWriteMask = entry.blend.writeMask;
        state.blends.resize(std::max<std::size_t>(state.blends.size(), color.exportIndex + 1u));
        state.blends[color.exportIndex] = blend;
    }
    state.color = state.colors.front();
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {ScissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend = state.blends.front();
    state.blendConstants = targets.front().blend.constants;
    if (setup.depth != 0) {
        state.depth = AgcDriver::Graphics::DepthTarget{setup.depth, 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0, false};
        state.depthTest = true;
        state.depthWrite = setup.depthWrite;
        state.depthCompare = VK_COMPARE_OP_LESS;
    }

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, setup.inputs}, {0x1b4, setup.inputs}, {0x203, setup.shaderControl}, {0x1c5, exportFormat}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    fragment.context.pixel->targetExportPacking = AgcDriver::Graphics::ExportPackings(state);
    fragment.context.pixel->targetMerges = AgcDriver::Graphics::ExportTargetMerges(state);
    Require(!fragment.context.pixel->pixelKillEnable && !fragment.context.pixel->earlyZ, "the test pixel shader does not kill");
    const auto& merges = fragment.context.pixel->targetMerges;
    Require(merges.size() == targets.size() && std::ranges::all_of(merges, [&](const ShaderRecompiler::TargetMerge& merge) {
        const auto& entry = targets[static_cast<std::size_t>(&merge - merges.data())];
        return merge.blend.enabled && merge.mask == AgcDriver::Graphics::UnclampedBlendMask && merge.target == entry.color.exportIndex && merge.blend.snorm == entry.blend.snorm;
    }), "the draw's target merges do not describe one unclamped blend per target");
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, fragment);
    const auto pixelResult = invocation.Materialize(*invocation.Capture(runtime));
    const bool registered = pixelResult->variantId == ShaderRecompiler::GetPreparedArtifact(*handle).variantId;
    Require(!registered, "the blending draw reused the registered pixel shader");
    const auto words = pixelResult->spirv.Copy();
    std::size_t begins = 0, inside = 0, outside = 0;
    bool interlocked = false;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpBeginInvocationInterlockEXT) ++begins;
        if (op == spv::OpBeginInvocationInterlockEXT || op == spv::OpEndInvocationInterlockEXT) interlocked = op == spv::OpBeginInvocationInterlockEXT;
        if (op == spv::OpImageWrite) ++(interlocked ? inside : outside);
    }
    Require(begins == 1u && inside >= targets.size() && outside == 0u, "the blending draw does not store every merged target inside its one pixel interlock region");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(setup.vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const BlendCase& blendCase, std::uint32_t exportFormat, std::span<const std::uint32_t> pixelCode, const DrawSetup& setup = {}) {
    const std::array<Target, 1> target{{{color, blendCase}}};
    Draw(device, target, exportFormat, pixelCode, setup);
}
struct Case {
    const char* name;
    BlendCase blend;
    std::array<float, 4> rgba;
    bool distinguishesClamping;
};

constexpr int Tolerance = 1;

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t info, std::uint32_t slot = 0) {
    AgcDriver::Registers cx;
    const auto stride = slot * 0xfu;
    cx[0x318 + stride] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b + stride] = 0;
    cx[0x31c + stride] = info;
    cx[0x31d + stride] = 0;
    cx[0x3b0 + slot] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8 + slot] = TiledAttrib3;
    cx[0x390 + slot] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, slot);
    color.exportIndex = slot;
    const auto format = info == Unorm88Info ? VK_FORMAT_R8G8_UNORM : info == Snorm8888Info ? VK_FORMAT_R8G8B8A8_SNORM : VK_FORMAT_R8G8B8A8_UNORM;
    Require(color.format == format && color.componentMapping == 0xe4u && color.packing == ShaderRecompiler::ColorExportPacking::None && color.tileMode == ColorTileMode::RenderTarget && color.bytes == block.bytes, "the color target did not decode as expected");
    return color;
}

float Factor(VkBlendFactor kind, std::uint32_t channel, bool alpha, const std::array<float, 4>& source, const std::array<float, 4>& destination, const std::array<float, 4>& constants) {
    const std::uint32_t index = alpha ? 3u : channel;
    switch (kind) {
        case VK_BLEND_FACTOR_ZERO: return 0.0f;
        case VK_BLEND_FACTOR_ONE: return 1.0f;
        case VK_BLEND_FACTOR_SRC_COLOR: return source[index];
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return 1.0f - source[index];
        case VK_BLEND_FACTOR_DST_COLOR: return destination[index];
        case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return 1.0f - destination[index];
        case VK_BLEND_FACTOR_SRC_ALPHA: return source[3];
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return 1.0f - source[3];
        case VK_BLEND_FACTOR_DST_ALPHA: return destination[3];
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return 1.0f - destination[3];
        case VK_BLEND_FACTOR_CONSTANT_COLOR: return constants[index];
        case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return 1.0f - constants[index];
        case VK_BLEND_FACTOR_CONSTANT_ALPHA: return constants[3];
        case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return 1.0f - constants[3];
        case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return alpha ? 1.0f : std::fmin(source[3], 1.0f - destination[3]);
        default: throw std::runtime_error("the oracle has no blend factor " + std::to_string(static_cast<int>(kind)));
    }
}

float Equation(VkBlendOp operation, float source, float sourceFactor, float destination, float destinationFactor) {
    volatile float weightedSource = source * sourceFactor;
    volatile float weightedDestination = destination * destinationFactor;
    switch (operation) {
        case VK_BLEND_OP_ADD: return weightedSource + weightedDestination;
        case VK_BLEND_OP_SUBTRACT: return weightedSource - weightedDestination;
        case VK_BLEND_OP_REVERSE_SUBTRACT: return weightedDestination - weightedSource;
        case VK_BLEND_OP_MIN: return std::fmin(source, destination);
        case VK_BLEND_OP_MAX: return std::fmax(source, destination);
        default: throw std::runtime_error("the oracle has no blend operation " + std::to_string(static_cast<int>(operation)));
    }
}

float Decode(std::uint8_t byte, bool snorm) {
    return snorm ? std::fmax(static_cast<float>(static_cast<std::int8_t>(byte)) / 127.0f, -1.0f) : static_cast<float>(byte) / 255.0f;
}

std::uint8_t Encode(float value, bool snorm) {
    if (std::isnan(value)) return 0;
    if (snorm) return static_cast<std::uint8_t>(static_cast<std::int8_t>(std::nearbyint(static_cast<double>(std::fmin(std::fmax(value, -1.0f), 1.0f)) * 127.0)));
    return static_cast<std::uint8_t>(std::nearbyint(static_cast<double>(std::fmin(std::fmax(value, 0.0f), 1.0f)) * 255.0));
}

std::array<std::uint8_t, 4> Blended(const BlendCase& blend, const std::array<float, 4>& rgba, const std::array<std::uint8_t, 4>& stored, bool clampInputs) {
    std::array<float, 4> source = rgba;
    std::array<float, 4> destination{};
    std::array<float, 4> constants = blend.constants;
    const float low = blend.snorm ? -1.0f : 0.0f;
    for (std::uint32_t channel = 0; channel < 4u; ++channel) destination[channel] = Decode(stored[channel], blend.snorm);
    if (clampInputs) {
        for (auto& value : source) value = std::fmin(std::fmax(value, low), 1.0f);
        for (auto& value : constants) value = std::fmin(std::fmax(value, low), 1.0f);
    }
    std::array<std::uint8_t, 4> result = stored;
    for (std::uint32_t channel = 0; channel < 4u; ++channel) {
        if ((blend.writeMask & (1u << channel)) == 0u) continue;
        const bool alpha = channel == 3u;
        const auto sourceFactor = Factor(alpha ? blend.alphaSource : blend.colorSource, channel, alpha, source, destination, constants);
        const auto destinationFactor = Factor(alpha ? blend.alphaDestination : blend.colorDestination, channel, alpha, source, destination, constants);
        const float clampedFactorSource = clampInputs ? std::fmin(std::fmax(sourceFactor, 0.0f), 1.0f) : sourceFactor;
        const float clampedFactorDestination = clampInputs ? std::fmin(std::fmax(destinationFactor, 0.0f), 1.0f) : destinationFactor;
        result[channel] = Encode(Equation(alpha ? blend.alphaOperation : blend.colorOperation, source[channel], clampedFactorSource, destination[channel], clampedFactorDestination), blend.snorm);
    }
    return result;
}

std::vector<std::uint8_t> ReadBack(AgcDriver::VulkanDevice& device, const Block& block) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(block.bytes);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::vector<std::uint8_t> Fill(Block& block, std::uint32_t seed) {
    std::vector<std::uint8_t> bytes(block.bytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] = static_cast<std::uint8_t>(((index + seed) * 0x9e3779b9u) >> 23u);
    std::memcpy(block.data, bytes.data(), block.bytes);
    return bytes;
}

std::array<std::uint8_t, 4> Texel(const std::vector<std::uint8_t>& bytes, const ColorTargetLayout& layout, std::uint32_t x, std::uint32_t y, std::uint32_t components) {
    const auto offset = layout.Offset(x, y);
    std::array<std::uint8_t, 4> texel{0u, 0u, 0u, 0xffu};
    for (std::uint32_t channel = 0; channel < components; ++channel) texel[channel] = bytes[offset + channel];
    return texel;
}

struct Outcome {
    bool distinguished = false;
};

int Difference(std::uint8_t left, std::uint8_t right, bool snorm) {
    return snorm ? std::abs(static_cast<int>(static_cast<std::int8_t>(left)) - static_cast<int>(static_cast<std::int8_t>(right))) : std::abs(static_cast<int>(left) - static_cast<int>(right));
}

Outcome Expect(AgcDriver::VulkanDevice& device, const Block& block, const ColorTargetLayout& layout, const std::vector<std::uint8_t>& before, bool snorm, std::uint32_t writeMask, const std::string& what, const std::function<std::array<std::uint8_t, 4>(const std::array<std::uint8_t, 4>&)>& expected, const std::function<std::array<std::uint8_t, 4>(const std::array<std::uint8_t, 4>&)>& clamped, std::uint32_t components = 4u) {
    const auto stored = ReadBack(device, block);
    Outcome outcome;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto was = Texel(before, layout, x, y, components);
            const bool inside = x < ScissorWidth;
            const auto want = inside ? expected(was) : was;
            const auto got = Texel(stored, layout, x, y, components);
            if (inside && !std::equal(want.begin(), want.begin() + components, clamped(was).begin())) outcome.distinguished = true;
            for (std::uint32_t channel = 0; channel < components; ++channel) {
                const bool written = inside && (writeMask & (1u << channel)) != 0u;
                if (Difference(got[channel], want[channel], snorm) <= (written ? Tolerance : 0)) continue;
                char detail[256];
                std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) %s the scissor holds (%u, %u, %u, %u), expected (%u, %u, %u, %u) from (%u, %u, %u, %u)", x, y, inside ? "inside" : "outside", got[0], got[1], got[2], got[3], want[0], want[1], want[2], want[3], was[0], was[1], was[2], was[3]);
                throw std::runtime_error(what + detail);
            }
        }
    }
    return outcome;
}

void RunCase(AgcDriver::VulkanDevice& device, Block& block, const ColorTargetLayout& layout, const AgcDriver::Graphics::ColorTarget& color, const Case& testCase, std::uint32_t seed) {
    const auto before = Fill(block, seed);
    Draw(device, color, testCase.blend, Float32Export, Float32Program(testCase.rgba));
    const auto outcome = Expect(device, block, layout, before, testCase.blend.snorm, testCase.blend.writeMask, std::string("unclamped blend ") + testCase.name,
        [&](const std::array<std::uint8_t, 4>& was) { return Blended(testCase.blend, testCase.rgba, was, false); },
        [&](const std::array<std::uint8_t, 4>& was) { return Blended(testCase.blend, testCase.rgba, was, true); });
    Require(outcome.distinguished == testCase.distinguishesClamping, std::string("unclamped blend ") + testCase.name + (testCase.distinguishesClamping ? " does not differ from Vulkan's clamped blend, so it cannot catch a clamping approximation" : " differs from Vulkan's clamped blend, which the case declares equal"));
}

void RunTwoTargets(AgcDriver::VulkanDevice& device, const std::string& name, std::array<Block*, 2> blocks, std::array<const ColorTargetLayout*, 2> layouts, std::array<AgcDriver::Graphics::ColorTarget, 2> colors, std::array<Case, 2> cases, std::uint32_t seed) {
    std::array<std::vector<std::uint8_t>, 2> before;
    for (std::size_t index = 0; index < 2u; ++index) before[index] = Fill(*blocks[index], seed + static_cast<std::uint32_t>(index));
    const std::array<Target, 2> targets{{{colors[0], cases[0].blend}, {colors[1], cases[1].blend}}};
    Draw(device, targets, Float32Export | (Float32Export << 4u), TwoTargetProgram(cases[0].rgba, cases[1].rgba));
    for (std::size_t index = 0; index < 2u; ++index) {
        const auto& testCase = cases[index];
        const auto what = name + ", MRT" + std::to_string(index) + " " + testCase.name;
        const auto outcome = Expect(device, *blocks[index], *layouts[index], before[index], testCase.blend.snorm, testCase.blend.writeMask, what,
            [&](const std::array<std::uint8_t, 4>& was) { return Blended(testCase.blend, testCase.rgba, was, false); },
            [&](const std::array<std::uint8_t, 4>& was) { return Blended(testCase.blend, testCase.rgba, was, true); },
            colors[index].format == VK_FORMAT_R8G8_UNORM ? 2u : 4u);
        Require(outcome.distinguished == testCase.distinguishesClamping, what + (testCase.distinguishesClamping ? " does not differ from Vulkan's clamped blend" : " differs from Vulkan's clamped blend"));
    }
}

void Overdraw(AgcDriver::VulkanDevice& device, Block& block, const ColorTargetLayout& layout, const AgcDriver::Graphics::ColorTarget& color, float first, float last, std::uint32_t seed) {
    constexpr std::uint32_t Layers = 16;
    static std::map<float, std::vector<std::array<float, 4>>> stacks;
    auto& vertices = stacks[last];
    std::vector<float> depths;
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        const float z = layer % 2u == Layers % 2u ? first : last;
        depths.push_back(z);
        const auto& triangle = TriangleAt(z);
        vertices.insert(vertices.end(), triangle.begin(), triangle.end());
    }
    BlendCase blend{VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xf, {}, false};
    const auto before = Fill(block, seed);
    Draw(device, color, blend, Float32Export, DepthAsXProgram({0.0f, 0.4f, 0.6f, 0.5f}), {DepthInput, 0u, vertices});
    Expect(device, block, layout, before, false, blend.writeMask, "one draw of 16 full-screen triangles alternating z " + std::to_string(first) + " and " + std::to_string(last) + " blended into a UNORM target",
        [&](const std::array<std::uint8_t, 4>& was) {
            auto state = was;
            for (const auto z : depths) state = Blended(blend, {z, 0.4f, 0.6f, 0.5f}, state, false);
            return state;
        },
        [&](const std::array<std::uint8_t, 4>& was) { return was; });
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFragmentShaderPixelInterlockEXT)) {
            std::puts("skipped, the device lacks fragmentShaderPixelInterlock");
            return VulkanTestSkipped;
        }
        const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 4);
        Block block(layout.Bytes());
        const auto unorm = DecodeTarget(block, Unorm8888Info);
        const auto snorm = DecodeTarget(block, Snorm8888Info);
        constexpr VkColorComponentFlags All = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        constexpr auto Zero = VK_BLEND_FACTOR_ZERO;
        constexpr auto One = VK_BLEND_FACTOR_ONE;
        constexpr auto Add = VK_BLEND_OP_ADD;
        const std::array<float, 4> constants{1.5f, -0.25f, 0.5f, 2.0f};
        const std::array<Case, 14> unormCases{{
            {"SRC_ALPHA, ONE_MINUS_SRC_ALPHA (the Ghost of Yotei title scene setup) with an out-of-range source", {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, One, Zero, Add, All, {}, false}, {0.75f, -0.5f, 2.0f, 1.5f}, true},
            {"SRC_COLOR, ONE_MINUS_SRC_COLOR", {VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, Add, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, All, {}, false}, {1.5f, -0.25f, 0.5f, 0.75f}, true},
            {"DST_COLOR, ZERO", {VK_BLEND_FACTOR_DST_COLOR, Zero, Add, VK_BLEND_FACTOR_DST_ALPHA, Zero, Add, All, {}, false}, {2.0f, 0.5f, 3.0f, 1.25f}, true},
            {"ONE, ONE with a negative source", {One, One, Add, One, One, Add, All, {}, false}, {-0.5f, 0.25f, -2.0f, -0.75f}, true},
            {"SUBTRACT ONE, ONE with a large source", {One, One, VK_BLEND_OP_SUBTRACT, One, One, VK_BLEND_OP_SUBTRACT, All, {}, false}, {1.5f, 0.25f, 3.0f, 2.0f}, true},
            {"REVERSE_SUBTRACT SRC_ALPHA, ONE", {VK_BLEND_FACTOR_SRC_ALPHA, One, VK_BLEND_OP_REVERSE_SUBTRACT, One, One, VK_BLEND_OP_REVERSE_SUBTRACT, All, {}, false}, {0.5f, 2.0f, 0.25f, 2.0f}, true},
            {"MIN", {One, One, VK_BLEND_OP_MIN, One, One, VK_BLEND_OP_MIN, All, {}, false}, {-0.5f, 0.5f, 2.0f, 0.25f}, false},
            {"MAX", {Zero, Zero, VK_BLEND_OP_MAX, Zero, Zero, VK_BLEND_OP_MAX, All, {}, false}, {-0.5f, 0.5f, 2.0f, 0.25f}, false},
            {"CONSTANT_COLOR, ONE_MINUS_CONSTANT_COLOR with out-of-range constants", {VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, Add, VK_BLEND_FACTOR_CONSTANT_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA, Add, All, constants, false}, {0.5f, 0.75f, 0.25f, 0.5f}, true},
            {"SRC_ALPHA_SATURATE, ONE", {VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, One, Add, One, One, Add, All, {}, false}, {1.5f, 2.0f, 0.5f, 3.0f}, true},
            {"DST_ALPHA, ONE_MINUS_DST_ALPHA", {VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, Add, One, Zero, Add, All, {}, false}, {2.0f, -1.0f, 0.5f, 0.5f}, true},
            {"separate alpha equation: color ADD SRC_ALPHA, alpha MAX", {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, One, One, VK_BLEND_OP_MAX, All, {}, false}, {1.25f, 0.5f, 0.75f, 1.75f}, true},
            {"write mask R and B", {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, One, Zero, Add, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT, {}, false}, {0.75f, 2.0f, 1.5f, 1.5f}, true},
            {"write mask none", {One, One, Add, One, One, Add, 0, {}, false}, {0.75f, 2.0f, 1.5f, 1.5f}, false},
        }};
        const std::array<Case, 4> snormCases{{
            {"SNORM SRC_ALPHA, ONE_MINUS_SRC_ALPHA", {VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, One, Zero, Add, All, {}, true}, {-0.75f, 1.5f, -2.0f, 1.5f}, true},
            {"SNORM DST_COLOR, ZERO", {VK_BLEND_FACTOR_DST_COLOR, Zero, Add, VK_BLEND_FACTOR_DST_ALPHA, Zero, Add, All, {}, true}, {-2.0f, 0.5f, 3.0f, -1.25f}, true},
            {"SNORM ONE, ONE", {One, One, Add, One, One, Add, All, {}, true}, {-1.5f, 0.25f, -0.5f, 2.0f}, true},
            {"SNORM SUBTRACT ONE, ONE", {One, One, VK_BLEND_OP_SUBTRACT, One, One, VK_BLEND_OP_SUBTRACT, All, {}, true}, {1.5f, -2.0f, 0.25f, 0.5f}, true},
        }};
        std::uint32_t seed = 1;
        for (const auto& testCase : unormCases) RunCase(*device, block, layout, unorm, testCase, seed++);
        for (const auto& testCase : snormCases) RunCase(*device, block, layout, snorm, testCase, seed++);
        std::string refusal;
        try {
            Draw(*device, unorm, unormCases[0].blend, Float32Export, Float32Program(unormCases[0].rgba), {.withoutInterlock = true});
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find("blending into an unclamped normalized color target needs the fragmentShaderPixelInterlock feature, which the device lacks") != std::string::npos, "a device without fragmentShaderPixelInterlock did not refuse the unclamped blend: " + refusal);
        Overdraw(*device, block, layout, unorm, 0.2f, 0.8f, 401u);
        Overdraw(*device, block, layout, unorm, 0.8f, 0.2f, 402u);
        const ColorTargetLayout pairLayout(Width, Height, ColorTileMode::RenderTarget, 2);
        Block firstPair(pairLayout.Bytes());
        Block secondPair(pairLayout.Bytes());
        const auto firstTarget = DecodeTarget(firstPair, Unorm88Info, 0);
        const auto secondTarget = DecodeTarget(secondPair, Unorm88Info, 1);
        const BlendCase over{VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, Add, All, {}, false};
        RunTwoTargets(*device, "two 8_8 UNORM targets blending SRC_ALPHA, ONE_MINUS_SRC_ALPHA in one draw (CB_COLOR_INFO 0x0000000c, CB_BLEND0_CONTROL and CB_BLEND1_CONTROL 0x40010504, PPSA26344)", {&firstPair, &secondPair}, {&pairLayout, &pairLayout}, {firstTarget, secondTarget},
            {{{"with an out-of-range source and alpha", over, {0.75f, -0.5f, 2.0f, 1.5f}, true}, {"with an out-of-range source", over, {1.5f, 0.25f, -1.0f, 0.5f}, true}}}, 501u);
        RunTwoTargets(*device, "an 8_8_8_8 UNORM MRT0 and an 8_8 UNORM MRT1 with different blend equations in one draw", {&block, &secondPair}, {&layout, &pairLayout}, {unorm, secondTarget},
            {{unormCases[3], unormCases[4]}}, 503u);
        std::printf("unclamped normalized color blend tests passed: %zu UNORM draws, %zu SNORM draws, 2 same-draw overdraws of 16 layers, 2 draws blending two targets each, 1 refusal without pixel interlock\n", unormCases.size(), snormCases.size());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
