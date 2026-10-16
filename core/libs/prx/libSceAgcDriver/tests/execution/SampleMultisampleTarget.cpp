#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 16;
constexpr std::uint32_t Samples = 4;
constexpr std::uint32_t SampleShift = 2;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Float16x4Info = 0x0730u;
constexpr std::uint32_t MultisampledAttrib = (SampleShift << 12u) | (SampleShift << 15u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Format16161616Float = 71;
constexpr std::uint32_t Type2DMsaa = 14;
constexpr std::uint32_t TileRenderTarget = 0x1b;
constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t LoadResult = 12;
constexpr std::array<float, 4> Drawn{0.25f, 0.5f, 0.125f, 1.0f};
constexpr double EdgeMargin = 1.0 / 32.0;
constexpr std::uint32_t FillOnes = 0x3c003c00u;
constexpr std::array<float, 4> Ones{1.0f, 1.0f, 1.0f, 1.0f};
constexpr std::array<float, 4> Written{0.75f, 0.25f, 0.5f, 1.0f};
constexpr std::uint32_t StoredSample = 1;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelCode{
    0x7e0802ff, 0x3e800000, 0x7e0a02ff, 0x3f000000, 0x7e0c02ff, 0x3e000000, 0x7e0e02ff, 0x3f800000, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 26> LoadCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf0001f30, 0x00011005,
    0xbf8c3f70, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c,
    0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 19> StoreCode{
    0x34020087, 0xe030100c, 0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xe0301018,
    0x80001001, 0xe030101c, 0x80001101, 0xe0301020, 0x80001201, 0xe0301024, 0x80001301, 0xbf8c3f70,
    0xf0201f30, 0x00011005, 0xbf810000,
};

struct SampleWrite {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t sample;
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {0.83f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.71f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
#endif
        Require(data != nullptr, "cannot allocate the multisampled color target");
        std::memset(data, 0, bytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Float16x4Info;
    cx[0x31d] = MultisampledAttrib;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R16G16B16A16_SFLOAT && color.samples == Samples && color.extent.width == Side && color.extent.height == Side, "the target did not decode as a 16x16 RGBA16F surface of 4 samples");
    return color;
}

AgcDriver::Graphics::State TargetState(const AgcDriver::Graphics::ColorTarget& color) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Side, Side};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Side), static_cast<float>(Side), -static_cast<float>(Side), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Side, Side}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = Samples;
    return state;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
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
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const std::span<const std::uint32_t> pixelCode(PixelCode);
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
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
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, fragment);
    const auto pixelResult = invocation.Materialize(*invocation.Capture(runtime));
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::array<std::uint32_t, 8> TargetDescriptor(const Block& block, std::uint32_t sampleShift) {
    const auto address = block.Address();
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format16161616Float << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (sampleShift << 16u) | (TileRenderTarget << 20u) | (Type2DMsaa << 28u),
        0u,
        sampleShift << 4u,
        0u,
        0u,
    };
}

ShaderRecompiler::RecompileResult CompileProgram(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, std::vector<std::uint32_t>& userData) {
    const auto address = reinterpret_cast<std::uintptr_t>(Buffer.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(Buffer.size() * 4u), 0x31016facu};
    userData.assign(16, 0u);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

ShaderRecompiler::RecompileResult CompileLoad(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, std::vector<std::uint32_t>& userData) {
    return CompileProgram(device, LoadCode, texture, userData);
}

void StoreSamples(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::vector<SampleWrite>& writes) {
    std::vector<std::uint32_t> userData;
    const auto program = CompileProgram(device, StoreCode, texture, userData);
    for (std::size_t first = 0; first < writes.size(); first += Threads) {
        Buffer.fill(0u);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& write = writes[std::min(first + tid, writes.size() - 1)];
            const std::array<std::uint32_t, 10> words{0u, 0u, 0u, write.x, write.y, write.sample, std::bit_cast<std::uint32_t>(Written[0]), std::bit_cast<std::uint32_t>(Written[1]), std::bit_cast<std::uint32_t>(Written[2]), std::bit_cast<std::uint32_t>(Written[3])};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
        device.WaitIdle();
    }
}

std::vector<SampleWrite> SampleWrites(bool (*selected)(std::uint32_t x, std::uint32_t y, std::uint32_t sample)) {
    std::vector<SampleWrite> writes;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            for (std::uint32_t sample = 0; sample < Samples; ++sample) {
                if (selected(x, y, sample)) writes.push_back({x, y, sample});
            }
        }
    }
    return writes;
}

void CheckWritten(const std::vector<std::array<float, 4>>& before, const std::vector<std::array<float, 4>>& after, bool (*selected)(std::uint32_t x, std::uint32_t y, std::uint32_t sample), const std::string& what) {
    std::uint32_t written = 0;
    for (std::uint32_t index = 0; index < after.size(); ++index) {
        const auto pixel = index / Samples;
        const auto x = pixel % Side;
        const auto y = pixel / Side;
        const auto sample = index % Samples;
        const bool chosen = selected(x, y, sample);
        char where[96];
        std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u)", sample, x, y);
        Require(after[index] == (chosen ? Written : before[index]), std::string(where) + (chosen ? " does not hold the stored color " : " changed although it was not stored to ") + what);
        written += chosen ? 1u : 0u;
    }
    Require(written != 0 && written != after.size(), "the stores covered every sample or none " + what);
}

void RequireStoreRefused(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::string& expected, const std::string& what) {
    std::string refusal;
    try {
        StoreSamples(device, texture, {{0u, 0u, 0u}});
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, what + " was not refused: " + refusal);
}

std::vector<std::array<float, 4>> LoadSamples(AgcDriver::VulkanDevice& device, const Block& block) {
    std::vector<std::uint32_t> userData;
    const auto program = CompileLoad(device, TargetDescriptor(block, SampleShift), userData);
    std::vector<std::array<float, 4>> loaded(Side * Side * Samples);
    for (std::uint32_t first = 0; first < loaded.size(); first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto index = first + tid;
            const auto pixel = index / Samples;
            const std::array<std::uint32_t, 6> words{0u, 0u, 0u, pixel % Side, pixel / Side, index % Samples};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadCode.data()));
        device.WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t component = 0; component < 4u; ++component) loaded[first + tid][component] = std::bit_cast<float>(Buffer[tid * Words + LoadResult + component]);
        }
    }
    return loaded;
}

double EdgeDistance(double x, double y, std::size_t from) {
    const auto pixels = [](const std::array<float, 4>& vertex) {
        return std::array<double, 2>{(static_cast<double>(vertex[0]) + 1.0) * 0.5 * Side, Side - (static_cast<double>(vertex[1]) + 1.0) * 0.5 * Side};
    };
    const auto a = pixels(Triangle[from]);
    const auto b = pixels(Triangle[(from + 1u) % Triangle.size()]);
    const auto c = pixels(Triangle[(from + 2u) % Triangle.size()]);
    const auto length = std::hypot(b[0] - a[0], b[1] - a[1]);
    const auto side = [&](double px, double py) { return ((b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0])) / length; };
    return side(c[0], c[1]) < 0 ? -side(x, y) : side(x, y);
}

void CheckSamples(const AgcDriver::Graphics::State& state, const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& background) {
    const auto locations = AgcDriver::Graphics::SampleLocations(state);
    std::array<std::uint32_t, Samples + 1u> pixelsByCoverage{};
    std::array<std::uint32_t, Samples> coveredBySample{};
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            std::uint32_t covered = 0;
            for (std::uint32_t sample = 0; sample < Samples; ++sample) {
                const auto& texel = loaded[(y * Side + x) * Samples + sample];
                const bool drawn = texel == Drawn;
                const bool empty = texel == background;
                char where[160];
                std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u) loaded (%g, %g, %g, %g)", sample, x, y, texel[0], texel[1], texel[2], texel[3]);
                Require(drawn || empty, std::string(where) + ", neither the drawn color nor the cleared target");
                covered += drawn ? 1u : 0u;
                coveredBySample[sample] += drawn ? 1u : 0u;
                const auto px = x + static_cast<double>(locations[sample].x);
                const auto py = y + static_cast<double>(locations[sample].y);
                const auto distance = std::min({EdgeDistance(px, py, 0), EdgeDistance(px, py, 1), EdgeDistance(px, py, 2)});
                if (std::fabs(distance) < EdgeMargin) continue;
                ++checked;
                Require(drawn == (distance > 0), std::string(where) + (distance > 0 ? ", which the triangle covers" : ", which the triangle does not cover"));
            }
            ++pixelsByCoverage[covered];
        }
    }
    Require(checked >= Side * Side * Samples * 9u / 10u, "fewer than nine in ten samples are clear of the triangle's edges");
    Require(pixelsByCoverage[0] != 0 && pixelsByCoverage[Samples] != 0, "the triangle covers every pixel or none");
    Require(pixelsByCoverage[1] + pixelsByCoverage[2] + pixelsByCoverage[3] != 0, "no pixel is partly covered, so the samples of a pixel were not told apart");
    Require(std::adjacent_find(coveredBySample.begin(), coveredBySample.end(), std::not_equal_to<>()) != coveredBySample.end(), "every sample index has the same coverage, so the sample index was not told apart");
}

void CheckUniform(const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& expected, const std::string& what) {
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        const auto& texel = loaded[index];
        char where[160];
        std::snprintf(where, sizeof(where), "sample %zu of pixel (%zu, %zu) loaded (%g, %g, %g, %g)", index % Samples, index / Samples % Side, index / Samples / Side, texel[0], texel[1], texel[2], texel[3]);
        Require(texel == expected, std::string(where) + " " + what);
    }
}

void StoreWords(const Block& block, std::uint64_t bytes, std::uint32_t word) {
    for (std::uint64_t offset = 0; offset < bytes; offset += sizeof(word)) std::memcpy(block.data + offset, &word, sizeof(word));
}

void RequireRefused(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::string& expected, const std::string& what) {
    std::string refusal;
    try {
        std::vector<std::uint32_t> userData;
        const auto program = CompileLoad(device, texture, userData);
        Buffer.fill(0u);
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadCode.data()));
        device.WaitIdle();
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, what + " was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Block block(4u * BlockBytes);
        Block other(BlockBytes);
        const auto color = DecodeTarget(block);
        const auto state = TargetState(color);
        Draw(*device, state);
        CheckSamples(state, LoadSamples(*device, block), {});
        const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
        Require(colorBytes != 0 && colorBytes <= block.bytes, "the 4x target's color memory does not fit its block");
        AgcDriver::Graphics::NoteColorMetadataFill(block.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
        CheckUniform(LoadSamples(*device, block), Ones, "after a fill of the target's color memory with half-float ones");
        RequireRefused(*device, TargetDescriptor(block, 3u), "texture of 8 samples", "loading a 4x target as an 8x texture");
        Require(colorBytes <= other.bytes, "the 4x target's color memory does not fit one block");
        CheckUniform(LoadSamples(*device, other), {}, "from zeroed color memory no draw rendered to");
        Block stored(BlockBytes);
        StoreWords(stored, colorBytes, FillOnes);
        CheckUniform(LoadSamples(*device, stored), Ones, "from color memory of half-float ones no draw rendered to");
        const auto storedState = TargetState(DecodeTarget(stored));
        Draw(*device, storedState);
        CheckSamples(storedState, LoadSamples(*device, stored), Ones);
        Block mixed(BlockBytes);
        StoreWords(mixed, colorBytes, FillOnes);
        const std::uint32_t odd = 0x3c000000u;
        std::memcpy(mixed.data + colorBytes - sizeof(odd), &odd, sizeof(odd));
        RequireRefused(*device, TargetDescriptor(mixed, SampleShift), "does not hold one repeated 32-bit value", "loading a multisampled texture no draw rendered to whose color memory is not uniform");
        const auto oneSample = [](std::uint32_t, std::uint32_t, std::uint32_t sample) { return sample == StoredSample; };
        const auto drawnBefore = LoadSamples(*device, block);
        StoreSamples(*device, TargetDescriptor(block, SampleShift), SampleWrites(oneSample));
        CheckWritten(drawnBefore, LoadSamples(*device, block), oneSample, "into one sample of each pixel of the drawn target");
        const auto leftHalf = [](std::uint32_t x, std::uint32_t, std::uint32_t sample) { return x < Side / 2u && sample != 0u; };
        Block undrawn(BlockBytes);
        StoreSamples(*device, TargetDescriptor(undrawn, SampleShift), SampleWrites(leftHalf));
        CheckWritten(std::vector<std::array<float, 4>>(Side * Side * Samples), LoadSamples(*device, undrawn), leftHalf, "into zeroed color memory no draw rendered to");
        Block mixedStore(BlockBytes);
        StoreWords(mixedStore, colorBytes, FillOnes);
        std::memcpy(mixedStore.data + colorBytes - sizeof(odd), &odd, sizeof(odd));
        RequireStoreRefused(*device, TargetDescriptor(mixedStore, SampleShift), "writing the 16x16 multisampled texture", "storing to a multisampled texture no draw rendered to whose color memory is not uniform");
        std::puts("stores to one sample of each pixel of a drawn 4x target and to samples of zeroed color memory no draw rendered to reach exactly those samples, and a store to non-uniform color memory no draw rendered to is refused");
        std::puts("every sample of a 4x color target loaded through its multisampled texture descriptor matches the triangle drawn into it, then the fill of its color memory; a surface no draw rendered to loads the value its color memory repeats and keeps it under a later draw");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
