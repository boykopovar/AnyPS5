#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

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

alignas(256) constexpr std::array<std::uint32_t, 26> ArrayLoadCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf0001f38, 0x00011004,
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
    {-1.0f, -1.0f, 0.5f, 1.0f}, {0.83f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.31f, 0.5f, 1.0f}
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t pipeBankXor = 0) {
    AgcDriver::Registers cx;
    const auto address = block.Address() | pipeBankXor;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Float16x4Info;
    cx[0x31d] = MultisampledAttrib;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R16G16B16A16_SFLOAT && color.samples == Samples && color.extent.width == Side && color.extent.height == Side, "the target did not decode as a 16x16 RGBA16F surface of 4 samples");
    Require(color.address == block.Address() && color.pipeBankXor == pipeBankXor, "the color target lost its base address or pipe/bank XOR");
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
    state.samples = color.samples;
    return state;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, std::uint64_t indexAddress = 0, std::uint32_t indexSize = 2u) {
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
    const AgcDriver::Pm4::DrawParameters draw{indexAddress, static_cast<std::uint32_t>(Triangle.size()), indexAddress != 0 ? indexSize : 0u, 1, 0, indexAddress != 0};
    device.Draw(state, draw, shaders);
}

std::array<std::uint32_t, 8> TargetDescriptor(const Block& block, std::uint32_t sampleShift, std::uint32_t pipeBankXor = 0) {
    const auto address = block.Address() | pipeBankXor;
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

ShaderRecompiler::RecompileResult CompileLoad(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, std::vector<std::uint32_t>& userData, bool arrayed = false) {
    return CompileProgram(device, arrayed ? ArrayLoadCode : LoadCode, texture, userData);
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

std::vector<std::array<float, 4>> RunLoad(AgcDriver::VulkanDevice& device, const Block& block, bool arrayed, std::uint32_t fragments, std::uint32_t pipeBankXor) {
    std::vector<std::uint32_t> userData;
    const auto program = CompileLoad(device, TargetDescriptor(block, static_cast<std::uint32_t>(std::countr_zero(fragments)), pipeBankXor), userData, arrayed);
    std::vector<std::array<float, 4>> loaded(Side * Side * fragments);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>((arrayed ? ArrayLoadCode : LoadCode).data());
    for (std::uint32_t first = 0; first < loaded.size(); first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto index = first + tid;
            const auto pixel = index / fragments;
            const std::array<std::uint32_t, 6> words = arrayed
                ? std::array<std::uint32_t, 6>{0u, 0u, pixel % Side, pixel / Side, 0u, index % fragments}
                : std::array<std::uint32_t, 6>{0u, 0u, 0u, pixel % Side, pixel / Side, index % fragments};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, codeAddress);
        device.WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t component = 0; component < 4u; ++component) loaded[first + tid][component] = std::bit_cast<float>(Buffer[tid * Words + LoadResult + component]);
        }
    }
    return loaded;
}

std::vector<std::array<float, 4>> LoadSamples(AgcDriver::VulkanDevice& device, const Block& block, std::uint32_t fragments = Samples, std::uint32_t pipeBankXor = 0) {
    auto loaded = RunLoad(device, block, false, fragments, pipeBankXor);
    const auto loadedArray = RunLoad(device, block, true, fragments, pipeBankXor);
    Require(loadedArray == loaded, "Dim2DMsaaArray image_load disagreed with Dim2DMsaa image_load on the multisampled color target");
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

void CheckSamples(const AgcDriver::Graphics::State& state, const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& background, std::span<const std::array<float, 4>> supplied = {}) {
    const auto locations = AgcDriver::Graphics::SampleLocations(state);
    Require(loaded.size() == Side * Side * state.samples, "the multisample result has the wrong element count");
    Require(supplied.empty() || supplied.size() == loaded.size(), "the initial sample count does not match the rendered target");
    std::vector<std::uint32_t> pixelsByCoverage(state.samples + 1u);
    std::vector<std::uint32_t> coveredBySample(state.samples);
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            std::uint32_t covered = 0;
            for (std::uint32_t sample = 0; sample < state.samples; ++sample) {
                const auto index = (y * Side + x) * state.samples + sample;
                const auto& texel = loaded[index];
                const bool drawn = texel == Drawn;
                const bool empty = texel == (supplied.empty() ? background : supplied[index]);
                char where[160];
                std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u) loaded (%g, %g, %g, %g)", sample, x, y, texel[0], texel[1], texel[2], texel[3]);
                Require(drawn || empty, std::string(where) + ", neither the drawn color nor the initial target sample");
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
    Require(checked >= Side * Side * state.samples * 9u / 10u, "fewer than nine in ten samples are clear of the triangle's edges");
    Require(pixelsByCoverage[0] != 0 && pixelsByCoverage[state.samples] != 0, "the triangle covers every pixel or none");
    Require(std::any_of(pixelsByCoverage.begin() + 1, pixelsByCoverage.end() - 1, [](auto count) { return count != 0; }), "no pixel is partly covered, so the samples of a pixel were not told apart");
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

void CheckNativeIndices(AgcDriver::VulkanDevice& device, const Block& source, const Block& destination) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (context.hostImportAlignment == 0) {
        std::puts("native index alias checks not run: host imports unavailable");
        return;
    }
    const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(DecodeTarget(source));
    Require(colorBytes <= source.bytes && source.bytes - colorBytes >= BlockBytes, "the native-index fixture has no neighboring block");
    const auto neighbor = source.Address() + colorBytes;
    AgcDriver::Graphics::RequireNoMultisampleTarget(neighbor, 12u, "an index neighbor control");
    std::string guarded;
    try {
        AgcDriver::Graphics::RequireNoMultisampleTarget(source.Address(), 48u, "a draw-input control");
    } catch (const std::exception& error) {
        guarded = error.what();
    }
    Require(guarded.find("whose samples are only on the host") != std::string::npos, "the draw-input source is not guarded as a native target");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* native = AgcDriver::Graphics::HostImportFor(context, source.Address(), 12u);
        const auto* adjacent = AgcDriver::Graphics::HostImportFor(context, neighbor, 12u);
        Require(native != nullptr && native == adjacent, "the native indices and their neighbor do not share a host import");
    }
    const auto state = TargetState(DecodeTarget(destination));
    AgcDriver::Graphics::NoteColorMetadataFill(source.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
    std::uint32_t missed = 0;
    for (const std::uint32_t indexSize : {2u, 4u}) {
        if (indexSize == 2u) {
            const std::array<std::uint16_t, 3> indices{0u, 1u, 2u};
            std::memcpy(source.data + colorBytes, indices.data(), sizeof(indices));
        } else {
            const std::array<std::uint32_t, 3> indices{0u, 1u, 2u};
            std::memcpy(source.data + colorBytes, indices.data(), sizeof(indices));
        }
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::NoteColorMetadataFill(destination.Address(), static_cast<std::size_t>(AgcDriver::Graphics::MultisampledColorBytes(state.color)), 0u);
            AgcDriver::Graphics::MaterializeMultisampleTargetClear(context, state.color);
        }
        CheckUniform(LoadSamples(device, destination), {}, "before the neighboring indexed draw");
        Draw(device, state, neighbor, indexSize);
        device.WaitIdle();
        CheckSamples(state, LoadSamples(device, destination), {});
        for (const bool edge : {false, true}) {
            const auto address = edge ? neighbor - indexSize : source.Address();
            std::string refusal;
            try {
                Draw(device, state, address, indexSize);
                device.WaitIdle();
            } catch (const std::exception& error) {
                refusal = error.what();
            }
            const bool refused = refusal.find("whose samples are only on the host") != std::string::npos;
            std::printf("native index %u %s: refused=%u neighbor=passed detail=%s\n", indexSize * 8u, edge ? "edge" : "base", refused ? 1u : 0u, refusal.c_str());
            Require(refusal.empty() || refused, "the native index input failed for another reason: " + refusal);
            missed += refused ? 0u : 1u;
        }
    }
    CheckUniform(LoadSamples(device, source), Ones, "after native draw-input alias attempts");
    std::puts("native index controls: shared-import=1 source-samples=1024 neighbor-triangle=passed");
    Require(missed == 0, "native indices missed " + std::to_string(missed) + " resident-image refusals");
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

std::vector<std::array<float, 4>> ReadHalfSamples(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color) {
    const auto bytes = AgcDriver::Graphics::ReadMultisampleTargetSamples_nid_no_patch(context, color);
    Require(bytes.size() == Side * Side * color.samples * 8u, "native color readback has the wrong byte count");
    std::vector<std::array<float, 4>> result(Side * Side * color.samples);
    constexpr std::array<std::array<std::uint16_t, 4>, 4> bits{{{}, {0x3400u, 0x3800u, 0x3000u, 0x3c00u}, {0x3c00u, 0x3c00u, 0x3c00u, 0x3c00u}, {0x3a00u, 0x3400u, 0x3800u, 0x3c00u}}};
    constexpr std::array<std::array<float, 4>, 4> values{{{}, Drawn, Ones, Written}};
    for (std::size_t index = 0; index < result.size(); ++index) {
        std::array<std::uint16_t, 4> sample{};
        std::memcpy(sample.data(), bytes.data() + index * sizeof(sample), sizeof(sample));
        const auto found = std::find(bits.begin(), bits.end(), sample);
        Require(found != bits.end(), "native color readback changed a half-float sample at element " + std::to_string(index));
        result[index] = values[static_cast<std::size_t>(found - bits.begin())];
    }
    return result;
}

void RequireReadbackRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        static_cast<void>(AgcDriver::Graphics::ReadMultisampleTargetSamples_nid_no_patch(context, color));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "native color readback was not refused: " + refusal);
}

void RequireInitialRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::MultisampleTargetView(context, color);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "native color initialization was not refused: " + refusal);
}

bool InitialCountSupported(const AgcDriver::Graphics::Context& context, VkFormat format, std::uint32_t samples) {
    VkImageFormatProperties supported{};
    const auto result = context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0u, &supported);
    if (result != VK_ERROR_FORMAT_NOT_SUPPORTED) AgcDriver::Graphics::Check(result, "native color initialization fixture format query");
    if (result == VK_SUCCESS && (supported.sampleCounts & samples) != 0 && (context.limits.framebufferColorSampleCounts & samples) != 0) return true;
    std::printf("skipped native color initialization subcase: format=%d samples=%u is unavailable\n", static_cast<int>(format), samples);
    return false;
}

PFN_vkGetPhysicalDeviceImageFormatProperties imageFormatProperties = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL NoReadbackImage(VkPhysicalDevice physical, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if ((flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) != 0 && usage == (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return imageFormatProperties(physical, format, type, tiling, usage, flags, properties);
}

void Readback(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    context.limits.maxStorageBufferRange = 256u;
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        Block block(BlockBytes);
        auto color = DecodeTarget(block);
        color.samples = fragments;
        RequireReadbackRefused(context, color, "requires an existing resident target");
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(context, color);
        }
        const auto state = TargetState(color);
        Draw(device, state);
        CheckSamples(state, ReadHalfSamples(context, color), {});
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            auto* recording = Recorder::Active();
            Require(recording != nullptr && context.recorder != nullptr, "the readback fixture has no owning recorder");
            Recorder other(context);
            other.Activate();
            RequireReadbackRefused(context, color, "another active recorder on the same device");
            recording->Activate();
        }
        auto otherDevice = context;
        otherDevice.device = VK_NULL_HANDLE;
        RequireReadbackRefused(otherDevice, color, "requires an existing resident target");
        auto otherMip = color;
        otherMip.mip = 1;
        RequireReadbackRefused(context, otherMip, "subresource does not match");
        NoteColorMetadataFill(block.Address(), static_cast<std::size_t>(MultisampledColorBytes(color)), FillOnes);
        CheckUniform(ReadHalfSamples(context, color), Ones, "after a native color fill");
        if (fragments == Samples) {
            CheckUniform(LoadSamples(device, block), Ones, "before storage migration");
            const auto selected = [](std::uint32_t x, std::uint32_t, std::uint32_t sample) { return x < Side / 2u && sample == StoredSample; };
            StoreSamples(device, TargetDescriptor(block, SampleShift), SampleWrites(selected));
            CheckWritten(std::vector<std::array<float, 4>>(Side * Side * Samples, Ones), ReadHalfSamples(context, color), selected, "after storage migration and per-sample writes");
        }
        std::printf("native color readback: RGBA16F samples=%u coverage/fill/refusals/chunks passed\n", fragments);
        ++exercised;
        {
            Block exact(BlockBytes);
            auto exactColor = DecodeTarget(exact, 0x8100u);
            exactColor.samples = fragments;
            const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, 8u, fragments, Side, Side, 1u, exactColor.pipeBankXor);
            constexpr std::array<std::uint16_t, 12> patterns{0u, 0x8000u, 1u, 0x8001u, 0x03ffu, 0x0400u, 0x7c00u, 0xfc00u, 0x7e01u, 0xfe55u, 0x7d01u, 0xfdffu};
            std::vector<std::uint16_t> words(static_cast<std::size_t>(layout.LinearBytes()) / sizeof(std::uint16_t));
            for (std::size_t index = 0; index < words.size(); ++index) words[index] = patterns[index % patterns.size()];
            const auto expected = std::as_bytes(std::span(words));
            auto tiled = std::as_writable_bytes(std::span(exact.data, exact.bytes));
            layout.Tile(expected, tiled);
            const std::vector<std::byte> before(tiled.begin(), tiled.end());
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                MultisampleTargetView(context, exactColor);
            }
            const auto samples = ReadMultisampleTargetSamples_nid_no_patch(context, exactColor);
            Require(std::equal(samples.begin(), samples.end(), expected.begin(), expected.end()), "native half-float readback changed an exceptional-value bit pattern");
            const auto snapshot = ReadMultisampleTargetColorSnapshot_nid_no_patch(context, exactColor);
            Require(snapshot.original == before && snapshot.current == before && std::equal(before.begin(), before.end(), tiled.begin()), "native exceptional-value snapshot changed samples, padding or guest bytes");
            std::printf("native color readback: RGBA16F samples=%u signed zeros/subnormals/infinities/NaN payload bits preserved\n", fragments);
        }
    }
    constexpr std::array<VkFormat, 5> formats{VK_FORMAT_R8_UINT, VK_FORMAT_R16_UINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32A32_UINT};
    constexpr std::array<std::uint32_t, 4> pattern{0x5au, 0x89abcdefu, 0x01234567u, 0xfedcba98u};
    for (std::size_t index = 0; index < formats.size(); ++index) {
        if (!InitialCountSupported(context, formats[index], Samples)) continue;
        Block block(BlockBytes);
        auto color = DecodeTarget(block);
        color.format = formats[index];
        color.elementBytes = 1u << index;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            VkImage image = VK_NULL_HANDLE;
            MultisampleTargetView(context, color, &image);
            Recorder::Active()->Sync();
            CommandBatch batch(context);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            VkClearColorValue clear{};
            std::copy(pattern.begin(), pattern.end(), clear.uint32);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(batch.Handle(), image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1u, &range);
            batch.SubmitAndWait();
        }
        const auto bytes = ReadMultisampleTargetSamples_nid_no_patch(context, color);
        Require(bytes.size() == Side * Side * Samples * color.elementBytes, "native integer sample count is wrong");
        for (std::size_t offset = 0; offset < bytes.size(); offset += color.elementBytes) Require(std::memcmp(bytes.data() + offset, pattern.data(), color.elementBytes) == 0, "native integer sample bytes changed at " + std::to_string(offset));
        std::printf("native color readback: elementBytes=%u integer bits/chunks passed\n", color.elementBytes);
    }
    if (InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) {
        Block block(BlockBytes);
        const auto color = DecodeTarget(block);
        auto unsupported = context;
        imageFormatProperties = context.imageFormatProperties;
        unsupported.imageFormatProperties = NoReadbackImage;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(unsupported, color);
        }
        Draw(device, TargetState(color));
        RequireReadbackRefused(unsupported, color, "multisample color readback");
        CheckSamples(TargetState(color), LoadSamples(device, block), {});
        std::puts("native color readback: unsupported snapshot format refuses readback and retains rendering");
        Block invalidBlock(BlockBytes);
        auto invalid = DecodeTarget(invalidBlock);
        invalid.elementBytes = 4u;
        RequireInitialRefused(context, invalid, "color format or element size is unsupported");
        std::puts("native color readback: mismatched source format and element size is refused before view creation");
    }
    Require(exercised != 0, "no supported native color readback sample count was exercised");
}

std::vector<std::array<float, 4>> StoreInitial(const Block& block, std::uint32_t fragments, std::uint32_t pipeBankXor, bool uniform) {
    using namespace AgcDriver::Graphics;
    const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, 8u, fragments, Side, Side, 1u, pipeBankXor);
    Require(layout.Bytes() <= block.bytes, "the initial color footprint exceeds its allocation");
    StoreWords(block, block.bytes, 0x38003800u);
    std::vector<std::byte> linear(static_cast<std::size_t>(layout.LinearBytes()));
    std::vector<std::array<float, 4>> expected(Side * Side * fragments);
    for (std::uint32_t index = 0; index < expected.size(); ++index) {
        const auto sample = index % fragments;
        const auto pixel = index / fragments;
        const bool ones = uniform || ((pixel % Side) * 3u + (pixel / Side) * 5u + sample * 7u) % 2u == 0u;
        const std::array<std::uint16_t, 4> bits = ones ? std::array<std::uint16_t, 4>{0x3c00u, 0x3c00u, 0x3c00u, 0x3c00u} : std::array<std::uint16_t, 4>{0x3a00u, 0x3400u, 0x3800u, 0x3c00u};
        std::memcpy(linear.data() + index * sizeof(bits), bits.data(), sizeof(bits));
        expected[index] = ones ? Ones : Written;
    }
    layout.Tile(linear, std::as_writable_bytes(std::span(block.data, static_cast<std::size_t>(layout.Bytes()))));
    return expected;
}

VKAPI_ATTR VkResult VKAPI_CALL NoInitialStorageImage(VkPhysicalDevice physical, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if ((flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) != 0 && (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return imageFormatProperties(physical, format, type, tiling, usage, flags, properties);
}

std::vector<std::byte> HalfSamples(std::span<const std::array<float, 4>> samples) {
    constexpr std::array<std::array<std::uint16_t, 4>, 4> bits{{{}, {0x3400u, 0x3800u, 0x3000u, 0x3c00u}, {0x3c00u, 0x3c00u, 0x3c00u, 0x3c00u}, {0x3a00u, 0x3400u, 0x3800u, 0x3c00u}}};
    constexpr std::array<std::array<float, 4>, 4> values{{{}, Drawn, Ones, Written}};
    std::vector<std::byte> result(samples.size() * sizeof(bits[0]));
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto found = std::find(values.begin(), values.end(), samples[index]);
        Require(found != values.end(), "guest MIMG returned an unexpected sample color");
        std::memcpy(result.data() + index * sizeof(bits[0]), bits[static_cast<std::size_t>(found - values.begin())].data(), sizeof(bits[0]));
    }
    return result;
}

void RequireColorSnapshotRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        static_cast<void>(AgcDriver::Graphics::ReadMultisampleTargetColorSnapshot_nid_no_patch(context, color));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "guest color snapshot was not refused: " + refusal);
}

void CheckColorSnapshot(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const Block& block, const std::vector<std::array<float, 4>>& loaded, std::optional<std::uint32_t> padding = std::nullopt) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto* begin = reinterpret_cast<const std::byte*>(block.data);
    const std::vector<std::byte> before(begin, begin + block.bytes);
    const auto snapshot = ReadMultisampleTargetColorSnapshot_nid_no_patch(context, color);
    Require(snapshot.original == before, "guest snapshot baseline differs from the mapped original bytes");
    auto expected = before;
    if (padding) {
        for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(*padding)) std::memcpy(expected.data() + offset, &*padding, sizeof(*padding));
    }
    const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, color.elementBytes, color.samples, Side, Side, 1u, color.pipeBankXor);
    layout.Tile(HalfSamples(loaded), expected);
    Require(snapshot.current == expected, "guest color snapshot differs from the MIMG samples or preserved padding");
    Require(std::equal(before.begin(), before.end(), begin), "capturing the guest color snapshot modified guest memory");
    Require(ReadHalfSamples(context, color) == loaded, "native snapshot readback disagrees with guest MIMG");
    Require(LoadSamples(device, block, color.samples, color.pipeBankXor) == loaded, "sampling after the native snapshot changed the image contents");
    std::string guard;
    try {
        RequireNoMultisampleTarget(block.Address(), 1u, "snapshot validation");
    } catch (const std::exception& error) {
        guard = error.what();
    }
    Require(guard.find("overlaps multisampled color target") != std::string::npos, "capturing a snapshot removed the native target's guest-access guard");
    Draw(device, TargetState(color));
    Require(LoadSamples(device, block, color.samples, color.pipeBankXor) == loaded, "attachment reuse after the native snapshot changed untouched samples");
}

void InitialContents(AgcDriver::VulkanDevice& device, bool snapshots = false) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            for (const bool uniform : {false, true}) {
                Block block(BlockBytes);
                auto color = DecodeTarget(block, pipeBankXor);
                color.samples = fragments;
                const auto expected = StoreInitial(block, fragments, pipeBankXor, uniform);
                const std::vector<std::uint8_t> before(block.data, block.data + block.bytes);
                const auto state = TargetState(color);
                Draw(device, state);
                const auto loaded = LoadSamples(device, block, fragments, pipeBankXor);
                CheckSamples(state, loaded, {}, expected);
                if (snapshots) CheckColorSnapshot(device, color, block, loaded);
                Require(std::equal(before.begin(), before.end(), block.data), "initialization or the draw changed guest color bytes or padding");
                std::printf("native color initial samples: fragments=%u xor=0x%x %s background survives first partial draw\n", fragments, pipeBankXor, uniform ? "uniform" : "varying pixel/fragment");
                if (snapshots && !uniform && pipeBankXor == 0x8100u) {
                    for (const auto pattern : {FillOnes, 0u}) {
                        NoteColorMetadataFill(block.Address(), block.bytes, pattern);
                        Draw(device, state);
                        const auto filled = LoadSamples(device, block, fragments, pipeBankXor);
                        CheckSamples(state, filled, pattern == FillOnes ? Ones : std::array<float, 4>{});
                        CheckColorSnapshot(device, color, block, filled, pattern);
                    }
                    StoreWords(block, block.bytes, FillOnes);
                    NoteColorMetadataFill(block.Address(), block.bytes, FillOnes);
                    Draw(device, state);
                    const auto filled = LoadSamples(device, block, fragments, pipeBankXor);
                    CheckSamples(state, filled, Ones);
                    CheckColorSnapshot(device, color, block, filled, FillOnes);
                    const auto previous = block.data[block.bytes - 1u];
                    block.data[block.bytes - 1u] ^= 0x5au;
                    RequireColorSnapshotRefused(context, color, "no longer matches its initial contents or a known full fill");
                    block.data[block.bytes - 1u] = previous;
                    auto wrong = color;
                    wrong.pipeBankXor = 0u;
                    RequireColorSnapshotRefused(context, wrong, "subresource does not match the resident target");
                    std::printf("native color snapshot: fragments=%u full fills preserve padding; changed guest bytes and mismatched XOR refused\n", fragments);
                }
                ++exercised;
            }
        }
        {
            Block block(BlockBytes);
            StoreWords(block, block.bytes, FillOnes);
            auto color = DecodeTarget(block);
            color.samples = fragments;
            auto noStorage = context;
            noStorage.storageImageMultisample = false;
            noStorage.storageImageWriteWithoutFormat = false;
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                MultisampleTargetView(noStorage, color);
            }
            const auto state = TargetState(color);
            Draw(device, state);
            const auto loaded = LoadSamples(device, block, fragments);
            CheckSamples(state, loaded, Ones);
            if (snapshots) CheckColorSnapshot(device, color, block, loaded);
            std::printf("native color initial samples: fragments=%u exact uniform clear without storage writes passed\n", fragments);
            ++exercised;
        }
        {
            Block block(BlockBytes);
            auto color = DecodeTarget(block, 0x8100u);
            color.samples = fragments;
            const auto expected = StoreInitial(block, fragments, 0x8100u, false);
            auto linear = color;
            linear.tileMode = ColorTileMode::Linear;
            RequireInitialRefused(context, linear, "initial multisample color footprint is not implemented");
            for (const auto profile : {0u, 1u, 2u}) {
                auto unsupported = context;
                if (profile == 0) unsupported.storageImageMultisample = false;
                if (profile == 1) unsupported.storageImageWriteWithoutFormat = false;
                if (profile == 2) unsupported.detiler = nullptr;
                RequireInitialRefused(unsupported, color, "require enabled formatless multisampled storage image writes");
            }
            auto noRange = context;
            noRange.limits.maxStorageBufferRange = 0u;
            RequireInitialRefused(noRange, color, "storage range cannot hold an aligned element");
            auto noImage = context;
            imageFormatProperties = context.imageFormatProperties;
            noImage.imageFormatProperties = NoInitialStorageImage;
            RequireInitialRefused(noImage, color, "cannot initialize the multisampled color target through its unsigned storage view");
            auto chunked = context;
            chunked.limits.maxStorageBufferRange = 256u;
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                MultisampleTargetView(chunked, color);
            }
            const auto state = TargetState(color);
            Draw(device, state);
            const auto loaded = LoadSamples(device, block, fragments, 0x8100u);
            CheckSamples(state, loaded, {}, expected);
            if (snapshots) CheckColorSnapshot(device, color, block, loaded);
            std::printf("native color initial samples: fragments=%u five unsupported capability profiles and linear layout refused, restored 256-byte chunks passed\n", fragments);
            ++exercised;
        }
    }
    if (snapshots) {
        Block block(BlockBytes);
        Block metadata(BlockBytes);
        auto color = DecodeTarget(block);
        color.cmaskAddress = metadata.Address();
        RequireInitialRefused(context, color, "CMASK require a modeled metadata import");
        std::puts("native color snapshot: unmodeled CMASK import refused before target creation");
        if (InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) {
            auto completed = std::make_shared<Block>(BlockBytes);
            auto completedColor = DecodeTarget(*completed);
            StoreInitial(*completed, Samples, 0u, false);
            Draw(device, TargetState(completedColor));
            auto completionRefusals = std::make_shared<std::array<std::string, 2>>();
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                auto* recorder = context.recorder != nullptr ? context.recorder : Recorder::Active();
                Require(recorder != nullptr, "the snapshot fixture has no owning recorder");
                recorder->OnComplete([completed, completedColor, context, completionRefusals] {
                    try {
                        static_cast<void>(ReadMultisampleTargetSamples_nid_no_patch(context, completedColor));
                    } catch (const std::exception& error) {
                        (*completionRefusals)[0] = error.what();
                    }
                    try {
                        static_cast<void>(ReadMultisampleTargetColorSnapshot_nid_no_patch(context, completedColor));
                    } catch (const std::exception& error) {
                        (*completionRefusals)[1] = error.what();
                    }
                    StoreWords(*completed, completed->bytes, FillOnes);
                    NoteColorMetadataFill(completed->Address(), completed->bytes, FillOnes);
                });
                auto* recording = Recorder::Active();
                Require(recording != nullptr, "the snapshot fixture has no active recorder");
                Recorder other(context);
                other.Activate();
                RequireColorSnapshotRefused(context, completedColor, "another active recorder on the same device");
                recording->Activate();
            }
            const auto snapshot = ReadMultisampleTargetColorSnapshot_nid_no_patch(context, completedColor);
            Require(std::all_of(completionRefusals->begin(), completionRefusals->end(), [](const auto& error) { return error.find("during a recorder completion") != std::string::npos; }), "a native color snapshot recursively completed its producer");
            std::vector<std::byte> expected(completed->bytes);
            for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(FillOnes)) std::memcpy(expected.data() + offset, &FillOnes, sizeof(FillOnes));
            Require(snapshot.original == expected && snapshot.current == expected, "the native snapshot captured its baseline before producer completion");
            std::puts("native color snapshot: producer completion precedes baseline capture; another active recorder remains refused");
        }
    }
    Require(exercised != 0, "no supported native color initial-content case was exercised");
}

alignas(256) std::array<std::uint32_t, 12> VertexValues{};

void DrawVertexInput(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, std::uint64_t address, std::uint32_t stride) {
    alignas(256) static constexpr std::array<std::uint32_t, 7> code{
        0xbe822100u, 0x34020a84u, 0xe0781000u, 0x80010801u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u,
    };
    VertexValues.fill(0xdeadbeefu);
    const auto output = reinterpret_cast<std::uintptr_t>(VertexValues.data());
    std::vector<std::uint32_t> userData(12u, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 1u;
    userData[4] = static_cast<std::uint32_t>(output);
    userData[5] = static_cast<std::uint32_t>((output >> 32u) & 0xffffu);
    userData[6] = static_cast<std::uint32_t>(sizeof(VertexValues));
    userData[7] = 0x31016facu;
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1u;
    info.resources[0].fields = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), stride == 0u ? 16u : 3u, 0x3104dfacu};
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8u;
    info.fetchBufferReg = 10u;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device.Target(), {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertex = ShaderRecompiler::Recompile(vertexRequest);
    Require(vertex.vertexAttributes.size() == 1u, "the native vertex probe did not create a fixed-function attribute");
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{pixelAddress, std::as_bytes(std::span(PixelCode))}}};
    const AgcDriver::Registers registers{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    ShaderRecompiler::RecompileRequest pixelRequest{
        {ShaderStage::Fragment, pixelAddress, PixelCode, 0, {}},
        {64u, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(registers, AgcDriver::Graphics::ExportMappings(state)), std::nullopt, pixelMemory},
        device.Target(), PixelPushLayout(vertexPush, device.Target())
    };
    pixelRequest.useCache = false;
    const auto pixel = ShaderRecompiler::Recompile(pixelRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(vertexPush, device.Target())}}};
    const AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void CheckVertexValues(std::uint64_t address, std::uint32_t stride) {
    std::array<std::uint32_t, 4> expected{};
    for (std::size_t vertex = 0; vertex < 3u; ++vertex) {
        std::memcpy(expected.data(), reinterpret_cast<const void*>(address + vertex * stride), sizeof(expected));
        for (std::size_t component = 0; component < expected.size(); ++component) {
            Require(VertexValues[vertex * 4u + component] == expected[component], "the fixed-function vertex probe did not observe the original guest bytes");
        }
    }
}

int NativeVertexInput(std::unique_ptr<AgcDriver::VulkanDevice>& device) {
    Block source(4u * BlockBytes);
    Block destination(BlockBytes);
    struct ReleaseDevice {
        std::unique_ptr<AgcDriver::VulkanDevice>& value;
        ~ReleaseDevice() { value.reset(); }
    } releaseDevice{device};
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    if (context.hostImportAlignment == 0u) {
        std::puts("native vertex input not tested: host imports unavailable");
        return VulkanTestSkipped;
    }
    const auto color = DecodeTarget(source);
    const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
    Require(colorBytes >= 48u && colorBytes + 48u <= source.bytes, "the native vertex probe has no neighboring range");
    constexpr std::array<float, 4> element{0.25f, -2.0f, 7.5f, 1.0f};
    for (std::size_t vertex = 0; vertex < 3u; ++vertex) {
        std::memcpy(source.data + vertex * sizeof(element), element.data(), sizeof(element));
        std::memcpy(source.data + colorBytes + vertex * sizeof(element), element.data(), sizeof(element));
    }
    std::memcpy(source.data + colorBytes - 8u, element.data(), 8u);
    Draw(*device, TargetState(color));
    AgcDriver::Graphics::NoteColorMetadataFill(source.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
    CheckUniform(LoadSamples(*device, source), Ones, "before the fixed-function vertex probe");
    const auto neighbor = source.Address() + colorBytes;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* native = AgcDriver::Graphics::HostImportFor(context, source.Address(), 48u);
        const auto* adjacent = AgcDriver::Graphics::HostImportFor(context, neighbor, 48u);
        Require(native != nullptr && native == adjacent, "the native and neighboring vertices do not share an import");
    }
    AgcDriver::Graphics::RequireNoMultisampleTarget(neighbor, 48u, "the vertex neighbor control");
    const auto state = TargetState(DecodeTarget(destination));
    std::uint32_t missed = 0u;
    for (const auto stride : {0u, 16u}) {
        for (const bool edge : {false, true}) {
            DrawVertexInput(*device, state, neighbor, stride);
            CheckVertexValues(neighbor, stride);
            const auto address = edge ? neighbor - 8u : source.Address();
            std::string refusal;
            try {
                DrawVertexInput(*device, state, address, stride);
                CheckVertexValues(address, stride);
            } catch (const std::exception& error) {
                refusal = error.what();
            }
            const bool refused = refusal.find("whose samples are only on the host") != std::string::npos;
            Require(refusal.empty() || refused, "the native vertex input failed for another reason: " + refusal);
            std::printf("native vertex stride %u %s: refused=%u neighbor=passed original-guest-values=%u detail=%s\n", stride, edge ? "edge" : "base", refused ? 1u : 0u, refused ? 0u : 12u, refusal.c_str());
            missed += refused ? 0u : 1u;
        }
    }
    CheckUniform(LoadSamples(*device, source), Ones, "after fixed-function vertex inputs");
    std::puts("native vertex controls: shared-import=1 source-samples=1024 neighbor-values=passed");
    Require(missed == 0u, "native vertex inputs missed " + std::to_string(missed) + " resident-image refusals");
    return 0;
}

void RequirePublicationRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        AgcDriver::Graphics::PublishMultisampleTargetColor_nid_no_patch(context, color);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "native color publication was not refused: " + refusal);
}

void Publication(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            for (const auto profile : {0u, 1u, 2u}) {
                Block block(2u * BlockBytes);
                auto color = DecodeTarget(block, pipeBankXor);
                color.samples = fragments;
                const auto initial = StoreInitial(block, fragments, pipeBankXor, false);
                RequirePublicationRefused(context, color, "requires an existing resident target");
                const auto state = TargetState(color);
                Draw(device, state);
                const auto colorBytes = static_cast<std::size_t>(MultisampledColorBytes(color));
                if (profile != 0u) {
                    NoteColorMetadataFill(block.Address(), colorBytes, profile == 1u ? FillOnes : 0u);
                    Draw(device, state);
                }
                const auto loaded = LoadSamples(device, block, fragments, pipeBankXor);
                if (profile == 0u) CheckSamples(state, loaded, {}, initial);
                else CheckSamples(state, loaded, profile == 1u ? Ones : std::array<float, 4>{});
                const auto* begin = reinterpret_cast<const std::byte*>(block.data);
                const std::vector<std::byte> before(begin, begin + block.bytes);
                auto expected = before;
                if (profile != 0u) {
                    const auto pattern = profile == 1u ? FillOnes : 0u;
                    for (std::size_t offset = 0; offset < colorBytes; offset += sizeof(pattern)) std::memcpy(expected.data() + offset, &pattern, sizeof(pattern));
                }
                const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, color.elementBytes, fragments, Side, Side, 1u, pipeBankXor);
                layout.Tile(HalfSamples(loaded), std::span(expected).first(colorBytes));
                if (fragments == Samples && pipeBankXor == 0u && profile == 0u) {
                    {
                        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                        auto* recording = Recorder::Active();
                        Require(recording != nullptr && context.recorder != nullptr, "the publication fixture has no owning recorder");
                        Recorder other(context);
                        other.Activate();
                        RequireReadbackRefused(context, color, "another active recorder on the same device");
                        RequirePublicationRefused(context, color, "another active recorder on the same device");
                        recording->Activate();
                    }
                    auto wrong = color;
                    wrong.pipeBankXor = 0x8100u;
                    RequirePublicationRefused(context, wrong, "subresource does not match");
                    {
                        GuestAllocations::Mutation mutation;
                        mutation.Protect(block.data + 4096u, 4096u, true, false, true, [] {});
                    }
                    RequirePublicationRefused(context, color, "requires writable registered guest memory");
                    {
                        GuestAllocations::Mutation mutation;
                        mutation.Protect(block.data + 4096u, 4096u, true, true, true, [] {});
                    }
                    block.data[colorBytes - 1u] ^= 0x5au;
                    RequirePublicationRefused(context, color, "no longer matches its initial contents or a known full fill");
                    block.data[colorBytes - 1u] ^= 0x5au;
                    Require(std::equal(before.begin(), before.end(), begin), "refused publication changed guest bytes");
                    std::string guard;
                    try {
                        RequireNoMultisampleTarget(block.Address(), 1u, "publication refusal validation");
                    } catch (const std::exception& error) {
                        guard = error.what();
                    }
                    Require(guard.find("overlaps multisampled color target") != std::string::npos, "refused publication lost its resident guard");
                }
                std::shared_ptr<void> retained;
                VkImage oldImage = VK_NULL_HANDLE;
                {
                    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                    MultisampleTargetView(context, color, retained, &oldImage);
                }
                std::vector<std::byte> read(block.bytes);
                PublishMultisampleTargetColor_nid_no_patch(context, color);
                AgcDriver::GuestMemory::Read(block.Address(), read);
                RequireNoMultisampleTarget(block.Address(), colorBytes, "after explicit publication");
                RequireReadbackRefused(context, color, "requires an existing resident target");
                Require(read == expected, "published guest color differs from MIMG samples, padding or neighboring bytes");
                Draw(device, state);
                Require(LoadSamples(device, block, fragments, pipeBankXor) == loaded, "a fresh target did not import the published samples");
                VkImage newImage = VK_NULL_HANDLE;
                {
                    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                    MultisampleTargetView(context, color, &newImage);
                }
                Require(newImage != oldImage, "publication reused the retired native image");
                std::printf("native color publication: fragments=%u xor=0x%x profile=%u complete bytes, bounds, retirement and reimport passed\n", fragments, pipeBankXor, profile);
                ++exercised;
            }
        }
    }
    Require(exercised != 0u, "no supported native color publication case was exercised");
    if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) return;
    auto completed = std::make_shared<Block>(BlockBytes);
    auto color = DecodeTarget(*completed);
    StoreInitial(*completed, Samples, 0u, false);
    Draw(device, TargetState(color));
    const auto completionRefusal = std::make_shared<std::string>();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        auto* recorder = context.recorder != nullptr ? context.recorder : Recorder::Active();
        Require(recorder != nullptr, "the publication fixture has no owning recorder");
        recorder->OnComplete([completed, color, context, completionRefusal] {
            try {
                PublishMultisampleTargetColor_nid_no_patch(context, color);
            } catch (const std::exception& error) {
                *completionRefusal = error.what();
            }
            StoreWords(*completed, completed->bytes, FillOnes);
            NoteColorMetadataFill(completed->Address(), completed->bytes, FillOnes);
        });
    }
    PublishMultisampleTargetColor_nid_no_patch(context, color);
    std::vector<std::uint32_t> read(completed->bytes / sizeof(std::uint32_t));
    AgcDriver::GuestMemory::Read(completed->Address(), std::as_writable_bytes(std::span(read)));
    Require(completionRefusal->find("during a recorder completion") != std::string::npos, "recursive publication from a completion was not refused: " + *completionRefusal);
    Require(std::all_of(read.begin(), read.end(), [](const auto word) { return word == FillOnes; }), "publication captured its guest baseline before producer completions");
    std::puts("native color publication: known full fill from a producer completion precedes the publication baseline; recursive publication stays refused");
}

}

int main(int argc, char** argv) {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::string_view(argv[1]) == "--publication") {
            Publication(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--readback") {
            Readback(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--snapshot") {
            InitialContents(*device, true);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--initial") {
            InitialContents(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--native-vertex-input") return NativeVertexInput(device);
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
        Block destination(BlockBytes);
        struct ReleaseIndexDevice {
            std::unique_ptr<AgcDriver::VulkanDevice>& device;
            ~ReleaseIndexDevice() { device.reset(); }
        } releaseIndexDevice{device};
        CheckNativeIndices(*device, block, destination);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
