#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::PixelInput;
using ShaderRecompiler::PixelInputBit;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 48;
constexpr std::uint32_t Slices = 2;
constexpr std::size_t SliceBytes = 65536;
constexpr std::size_t SurfaceBytes = SliceBytes * Slices;
constexpr std::uint32_t Color32x4FloatInfo = (14u << 2u) | (7u << 8u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;
constexpr float Unwritten = -7.0f;

alignas(256) constexpr std::array<std::uint32_t, 8> ParameterVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> LayerVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0x7e080281, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d4, 0x04040404, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> VertexBufferLayerVertexCode{
    0xe0382000, 0x80000005, 0xe030201c, 0x80000405, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d4, 0x04040404, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> SecondParameterVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf800021f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> ClipVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> ClipLayerVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0x7e080281, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80000d4, 0x04040404, 0xf80008e7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> FourParameterClipVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf800021f, 0x03020100, 0xf800022f, 0x03020100, 0xf800023f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 19> FourParameterClipLayerVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0x7e080281, 0xf800020f, 0x03020100, 0xf800021f, 0x03020100, 0xf800022f, 0x03020100, 0xf800023f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80000d4, 0x04040404, 0xf80008e7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexValuePixelCode{
    0xc80a0000, 0xc80e0001, 0xc8120002, 0xf800180f, 0x04040302, 0xbf810000,
};

constexpr std::uint32_t MiscVector = 1u << 21u;
constexpr std::uint32_t ClipVector = 1u << 22u;
constexpr std::uint32_t ThreeClipDistances = 0x7u;
constexpr std::uint32_t VertexLayer = 1u << 18u;
constexpr std::uint32_t LayerExport = VertexLayer | MiscVector;

struct VertexProgram {
    const char* name;
    std::span<const std::uint32_t> code;
    std::uint32_t paClVsOutCntl;
    std::uint32_t layer;
};

const VertexProgram Parameter{"param0 and position", ParameterVertexCode, 0u, 0u};
const VertexProgram Layer{"param0, position and layer 1", LayerVertexCode, MiscVector | VertexLayer, 1u};
const VertexProgram SecondParameter{"param0, param1 and position", SecondParameterVertexCode, 0u, 0u};
const VertexProgram Clip{"param0, position and three clip distances", ClipVertexCode, ClipVector | ThreeClipDistances, 0u};
const VertexProgram ClipLayer{"param0, position, three clip distances and layer 1", ClipLayerVertexCode, MiscVector | VertexLayer | ClipVector | ThreeClipDistances, 1u};
const VertexProgram FourParameterClip{"param0 to param3, position and three clip distances", FourParameterClipVertexCode, ClipVector | ThreeClipDistances, 0u};
const VertexProgram FourParameterClipLayer{"param0 to param3, position, three clip distances and layer 1", FourParameterClipLayerVertexCode, MiscVector | VertexLayer | ClipVector | ThreeClipDistances, 1u};
const VertexProgram VertexBufferLayer{"param0, position and the layer of each vertex", VertexBufferLayerVertexCode, MiscVector | VertexLayer, 1u};

struct Corner {
    float x;
    float y;
    float w;
};

constexpr std::array<Corner, 3> Corners{{{-0.8f, -0.8f, 1.0f}, {0.9f, -0.6f, 2.5f}, {-0.5f, 0.85f, 0.5f}}};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> pad;
    std::uint32_t layer;
};

alignas(256) std::array<Vertex, 3> Vertices{};

std::array<std::array<float, 4>, 3> ClipPositions() {
    std::array<std::array<float, 4>, 3> positions{};
    for (std::size_t i = 0; i < Corners.size(); ++i) positions[i] = {Corners[i].x * Corners[i].w, Corners[i].y * Corners[i].w, 0.5f * Corners[i].w, Corners[i].w};
    return positions;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SliceBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + SliceBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + SliceBytes - 1) & ~(static_cast<std::uintptr_t>(SliceBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + SliceBytes) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + SliceBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "barycentric patch vertex layer: cannot allocate the color array");
        GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }

private:
    std::uint8_t* block = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeColor(std::uint64_t address) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = (Slices - 1u) << 13u;
    cx[0x31c] = Color32x4FloatInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = (TiledAttrib3 & ~((0x1fu << 14u) | 0x1fffu)) | (static_cast<std::uint32_t>(ColorTileMode::RenderTarget) << 14u) | (Slices - 1u);
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    cx[0x207] = LayerExport;
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.layers == Slices && color.arraySlice == 0u && color.arrayLastSlice == Slices - 1u && color.arrayAddress == address && color.format == VK_FORMAT_R32G32B32A32_SFLOAT && color.elementBytes == 16u && color.componentMapping == 0xe4u, "the two-slice RGBA32F color view did not decode");
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::ArraySurfaceForTarget(color));
    Require(geometry.guestBytes <= SurfaceBytes && geometry.GuestLayerOffset(1u) == SliceBytes, "the two-slice RGBA32F color array is not two 64 KiB slices");
    return color;
}

class Surface {
public:
    explicit Surface(const AgcDriver::Graphics::ColorTarget& color) : geometry(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::ArraySurfaceForTarget(color))) {
        const auto tileMode = AgcDriver::Graphics::ColorTextureTileMode(color.tileMode);
        thin = AgcDriver::Graphics::ThinBlockLayout(tileMode, 16u);
        equation = AgcDriver::Graphics::FindTextureSwizzleEquation(static_cast<std::uint32_t>(color.tileMode), 16u);
        Require(equation != nullptr, "missing the color array's swizzle equation");
    }

    const float* Texel(const std::uint8_t* memory, std::uint32_t x, std::uint32_t y, std::uint32_t slice) const {
        const auto& mip = geometry.mips.at(0);
        std::uint64_t swizzled = 0;
        for (std::uint32_t bit = 0; bit < equation->bits.size(); ++bit) {
            const auto mask = equation->bits[bit];
            const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12u) & (mask & 0xfff000u)) ^ ((slice << 24u) & (mask & 0xff000000u));
            swizzled |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
        }
        const auto offset = geometry.GuestLayerOffset(slice) + (static_cast<std::uint64_t>(y / thin[2]) * mip.blocksPerRow + x / thin[1]) * thin[0] + swizzled;
        return reinterpret_cast<const float*>(memory + offset);
    }

private:
    AgcDriver::Graphics::SurfaceGeometry geometry;
    std::array<std::uint32_t, 3> thin{};
    const AgcDriver::Graphics::TextureSwizzleEquation* equation = nullptr;
};

struct Provoking {
    std::array<std::uint32_t, 3> layers;
    bool lastControl;
    bool lastMode;
};

void Draw(AgcDriver::VulkanDevice& device, std::uint8_t* memory, const VertexProgram& program, const Provoking& provoking) {
    const auto surface = reinterpret_cast<std::uint64_t>(memory);
    std::fill_n(reinterpret_cast<float*>(memory), SurfaceBytes / sizeof(float), Unwritten);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto color = DecodeColor(surface);
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = false;
    target.fragmentBarycentricBuiltins = false;
    const auto positions = ClipPositions();
    for (std::size_t i = 0; i < Vertices.size(); ++i) Vertices[i] = {positions[i], {}, provoking.layers[i]};
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), static_cast<std::uint32_t>(sizeof(Vertex)), static_cast<std::uint32_t>(Vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(program.code.data()), std::as_bytes(program.code)}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = program.paClVsOutCntl;
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(program.code.data()), program.code, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 1;
    pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData;
    const std::span<const std::uint32_t> pixelCode(VertexValuePixelCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(pixelResult.barycentricEmulation.active && pixelResult.fragmentParameters.size() == 1 && pixelResult.fragmentParameters[0].perVertex, std::string(program.name) + ": v_interp_mov of P10 and P20 did not emulate per-vertex parameters");
    const auto patches = ShaderRecompiler::BuildBarycentricTessellationShaders(vertexResult, pixelResult, target, device.MaxFragmentInputComponents(), provoking.lastControl ? 2u : 0u);
    const std::array<AgcDriver::Graphics::CompiledShader, 4> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::TessellationControl, &patches.control, 0},
        {ShaderStage::TessellationEvaluation, &patches.evaluation, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)},
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = color;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.renderLayers = Slices;
    state.layerExports = LayerExport;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.provokingVertexMode = provoking.lastMode ? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    device.Draw(state, AgcDriver::Pm4::DrawParameters{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false}, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "barycentric patch vertex layer test");
    device.WaitIdle();
}

double Edge(const std::array<double, 2>& a, const std::array<double, 2>& b, double x, double y) {
    return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
}

std::array<double, 3> Weights(std::uint32_t x, std::uint32_t y) {
    std::array<std::array<double, 2>, 3> window{};
    for (std::size_t i = 0; i < Corners.size(); ++i) window[i] = {(Corners[i].x + 1.0) * Width / 2.0, (1.0 - Corners[i].y) * Height / 2.0};
    const double px = x + 0.5;
    const double py = y + 0.5;
    const double area = Edge(window[0], window[1], window[2][0], window[2][1]);
    return {Edge(window[1], window[2], px, py) / area, Edge(window[2], window[0], px, py) / area, Edge(window[0], window[1], px, py) / area};
}

std::string Check(const std::uint8_t* memory, std::uint32_t layer, const std::string& what) {
    const Surface surface(DecodeColor(reinterpret_cast<std::uint64_t>(memory)));
    const auto positions = ClipPositions();
    const auto p0 = positions[0][0];
    const std::array<float, 3> expected{positions[1][0] - p0, positions[2][0] - p0, p0};
    std::uint32_t inside = 0;
    std::uint32_t wrong = 0;
    std::uint32_t missing = 0;
    std::uint32_t outside = 0;
    std::uint32_t otherSlice = 0;
    std::string first;
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto* pixel = surface.Texel(memory, x, y, slice);
                const bool written = std::any_of(pixel, pixel + 4, [](float value) { return value != Unwritten; });
                if (slice != layer) {
                    if (!written) continue;
                    ++otherSlice;
                    if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") of slice " + std::to_string(slice) + ", which the draw does not select, is written";
                    continue;
                }
                const auto weights = Weights(x, y);
                const auto low = std::min({weights[0], weights[1], weights[2]});
                if (low > 0.02) {
                    ++inside;
                    if (!written) {
                        ++missing;
                        if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") of slice " + std::to_string(slice) + " inside the triangle is unwritten";
                        continue;
                    }
                    for (std::uint32_t channel = 0; channel < 3; ++channel) {
                        if (pixel[channel] == expected[channel]) continue;
                        ++wrong;
                        if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(pixel[channel]) + ", expected " + std::to_string(expected[channel]);
                        break;
                    }
                } else if (low < -0.02 && written) {
                    ++outside;
                    if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") outside the triangle is written";
                }
            }
        }
    }
    Require(inside > 200u, what + ": too few interior pixels were checked");
    if (missing == 0u && wrong == 0u && outside == 0u && otherSlice == 0u) return {};
    return what + ": in slice " + std::to_string(layer) + " " + std::to_string(missing) + " of " + std::to_string(inside) + " interior pixels unwritten, " + std::to_string(wrong) + " with wrong per-vertex values, " + std::to_string(outside) + " written outside the triangle; " + std::to_string(otherSlice) + " pixels written in the other slice; first: " + first;
}

std::unique_ptr<AgcDriver::VulkanDevice> OpenDevice() {
    auto device = OpenVulkanTestDevice();
    Require(device != nullptr, "the Vulkan device closed after the first open");
    return device;
}

constexpr Provoking FirstVertex{{0u, 0u, 0u}, false, false};

std::string DrawAndCheck(AgcDriver::VulkanDevice& device, std::uint8_t* memory, const VertexProgram& program, const Provoking& provoking, const std::string& what) {
    try {
        Draw(device, memory, program, provoking);
    } catch (const std::exception& error) {
        return what + ": " + error.what();
    }
    return Check(memory, program.layer, what);
}

std::uint32_t Report(const std::string& error) {
    if (error.empty()) return 0u;
    std::cerr << error << '\n';
    return 1u;
}

std::uint32_t RunCase(std::uint8_t* memory, const VertexProgram& seed, const VertexProgram& drawn) {
    const std::string name = std::string("a pipeline for ") + drawn.name + " after one for " + seed.name;
    std::uint32_t failures = 0;
    {
        auto fresh = OpenDevice();
        failures += Report(DrawAndCheck(*fresh, memory, drawn, FirstVertex, name + ", fresh pipeline cache"));
    }
    auto device = OpenDevice();
    failures += Report(DrawAndCheck(*device, memory, seed, FirstVertex, name + ", the seeding draw"));
    failures += Report(DrawAndCheck(*device, memory, drawn, FirstVertex, name + ", pipeline cache seeded"));
    if (failures == 0u) std::printf("barycentric patch pipeline passed for %s\n", name.c_str());
    return failures;
}

std::uint32_t RunProvoking(std::uint8_t* memory, bool provokingVertexLast) {
    struct Case {
        const char* name;
        Provoking provoking;
    };
    std::vector<Case> cases{
        {"the stages built for the first provoking vertex take the layer of the first vertex", {{1u, 0u, 0u}, false, false}},
        {"the stages built for the last provoking vertex take the layer of the third vertex", {{0u, 0u, 1u}, true, false}},
    };
    if (provokingVertexLast) cases.push_back({"the last provoking vertex mode takes the layer of the third vertex", {{0u, 0u, 1u}, true, true}});
    std::uint32_t failures = 0;
    auto device = OpenDevice();
    for (const auto& item : cases) {
        const auto error = DrawAndCheck(*device, memory, VertexBufferLayer, item.provoking, std::string("a triangle whose vertices write the layers ") + std::to_string(item.provoking.layers[0]) + ", " + std::to_string(item.provoking.layers[1]) + " and " + std::to_string(item.provoking.layers[2]) + ": " + item.name);
        failures += Report(error);
        if (error.empty()) std::printf("barycentric patch layer passed: %s\n", item.name);
    }
    if (!provokingVertexLast) std::puts("the last provoking vertex mode is not checked, the device lacks it");
    return failures;
}

}

int main() {
    try {
        bool provokingVertexLast = false;
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
            const auto target = probe->Target();
            if (!target.tessellation.has_value()) {
                std::puts("skipped, the device lacks tessellationShader");
                return VulkanTestSkipped;
            }
            if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) == target.supportedCapabilities.end()) {
                std::puts("skipped, the device has no VK_EXT_shader_viewport_index_layer");
                return VulkanTestSkipped;
            }
            provokingVertexLast = probe->ProvokingVertexLast();
        }
        GuestBlock memory;
        std::uint32_t failures = 0;
        const std::array<std::array<const VertexProgram*, 2>, 4> pairs{{{&Layer, &Parameter}, {&SecondParameter, &Parameter}, {&ClipLayer, &Clip}, {&FourParameterClipLayer, &FourParameterClip}}};
        for (const auto& [extra, plain] : pairs) {
            failures += RunCase(memory.Data(), *extra, *plain);
            failures += RunCase(memory.Data(), *plain, *extra);
        }
        failures += RunProvoking(memory.Data(), provokingVertexLast);
        if (failures != 0u) {
            std::cerr << failures << " barycentric patch pipeline checks failed\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
