#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
namespace GuestMemory = AgcDriver::GuestMemory;

constexpr std::uint32_t TexelWidth = 64;
constexpr std::uint32_t TexelHeight = 4;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t LastSlice = 63;
constexpr std::size_t BlockBytes = 0x10000;
constexpr std::uint32_t TargetWidth = 64;
constexpr std::uint32_t TargetHeight = 32;
constexpr std::uint32_t Images = 8;
constexpr std::uint32_t Key = 3;
constexpr std::uint32_t Other = 5;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 16> PixelTableCode{
    0x7e040f00u, 0x2c04049fu, 0x4a040404u, 0x7e080502u, 0x8f058404u, 0xf4280300u, 0x0a000000u, 0x7e140280u,
    0x7e160280u, 0xbf8cc07fu, 0xf0008f08u, 0x0003100au, 0xbf8c3f70u, 0xf800180fu, 0x13121110u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelArrayCode{
    0xf42c0300u, 0xfa000060u, 0x7e140280u, 0x7e160280u, 0xbf8cc07fu, 0xf0000f08u, 0x0003100au, 0xbf8c3f70u, 0xf800180fu, 0x13121110u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300u, 0x7e060280u, 0x7e0202ffu, 0x12345678u, 0xf0200108u, 0x00020102u, 0xbf810000u,
};

std::size_t TargetBytes = 0;
bool OutcomeOnly = false;
const std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};
const AgcDriver::Pm4::DrawParameters TriangleDraw{0, 3, 0, 1, 0, false};

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> VertexBuffer(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 4> Image(const void* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((TexelWidth - 1u) & 3u) << 30u), ((TexelWidth - 1u) >> 2u) | ((TexelHeight - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u)};
}

std::array<std::uint32_t, 8> ArrayImage(const void* texels) {
    const auto words = Image(texels);
    return {words[0], words[1], words[2], (words[3] & 0x0fffffffu) | (Type2DArray << 28u), LastSlice, 0u, 0u, 0u};
}

class GuestBlocks {
public:
    explicit GuestBlocks(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
        if (base != nullptr) GuestArena::GuestArenaCommit_nid_postfix(base, bytes, PAGE_READWRITE, bytes);
#else
        void* mapped = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            base = reinterpret_cast<std::uint8_t*>(aligned);
            if (GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, bytes);
        }
#endif
        Require(base != nullptr, "table index: cannot allocate guest memory");
        GuestAllocations::Mutation mutation;
        mutation.Add(base, bytes, true, true, true);
    }

    ~GuestBlocks() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(base);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(base, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(base, bytes);
#else
        munmap(base, bytes);
        if (GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, bytes);
#endif
    }

    GuestBlocks(const GuestBlocks&) = delete;
    GuestBlocks& operator=(const GuestBlocks&) = delete;

    std::uint8_t* base = nullptr;
    std::size_t bytes = 0;
};

GuestBlocks* Target = nullptr;

struct Scene {
    GuestBlocks blocks;
    GuestBlocks table;
    GuestBlocks textures;
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{};
    AgcDriver::Graphics::State state{};

    Scene() : blocks(2u * BlockBytes), table(BlockBytes), textures(static_cast<std::size_t>(Images) * BlockBytes) {}

    float* Texels(std::uint32_t image) const { return reinterpret_cast<float*>(textures.base + static_cast<std::size_t>(image) * BlockBytes); }
    std::uint32_t* Table() const { return reinterpret_cast<std::uint32_t*>(table.base); }
    std::uint32_t* Constants() const { return reinterpret_cast<std::uint32_t*>(blocks.base + BlockBytes); }
};

std::vector<std::unique_ptr<Scene>> Scenes;

Scene& NewScene() {
    return *Scenes.emplace_back(std::make_unique<Scene>());
}

void Build(AgcDriver::VulkanDevice& device, Scene& scene, bool arrays = false) {
    const auto target = device.Target();
    const std::uint32_t stride = arrays ? 8u : 4u;
    for (std::uint32_t image = 0; image < Images; ++image) {
        std::fill_n(scene.Texels(image), TexelWidth * TexelHeight, image == Key ? 1.0f : 0.0f);
        if (arrays) {
            const auto words = ArrayImage(scene.Texels(image));
            std::copy(words.begin(), words.end(), scene.Table() + image * stride);
        } else {
            const auto words = Image(scene.Texels(image));
            std::copy(words.begin(), words.end(), scene.Table() + image * stride);
        }
    }
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    auto* triangle = reinterpret_cast<std::array<float, 4>*>(scene.Constants() + 64);
    std::copy(Triangle.begin(), Triangle.end(), triangle);
    const auto vertices = VertexBuffer(triangle, 16u, static_cast<std::uint32_t>(Triangle.size()));
    std::copy(vertices.begin(), vertices.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    scene.vertex = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(scene.vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = false;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(12, 0u);
    const std::span<const std::uint32_t> code = arrays ? std::span<const std::uint32_t>(PixelArrayCode) : std::span<const std::uint32_t>(PixelTableCode);
    const auto tableWords = Buffer(scene.Table(), Images * stride * 4u);
    std::copy(tableWords.begin(), tableWords.end(), pixelUserData.begin());
    pixelUserData[4] = Key;
    const std::array<ShaderRecompiler::MemoryRegion, 2> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}, {reinterpret_cast<std::uintptr_t>(scene.Table()), std::as_bytes(std::span(scene.Table(), Images * stride))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    scene.pixel = ShaderRecompiler::Recompile(fragment);
    scene.shaders = {{{ShaderStage::Vertex, &scene.vertex, 0}, {ShaderStage::Fragment, &scene.pixel, PixelPushOffset(vertexPush, target)}}};
    auto& state = scene.state;
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Target->base), {TargetWidth, TargetHeight}, VK_FORMAT_R8G8B8A8_UNORM, TargetBytes, 0xe4u};
    state.color.surfaceAddress = state.color.address;
    state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    state.color.elementBytes = 4;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {TargetWidth, TargetHeight};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(TargetHeight), static_cast<float>(TargetWidth), -static_cast<float>(TargetHeight), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {TargetWidth, TargetHeight}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
}

unsigned Brightest() {
    GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Target->base), TargetBytes);
    unsigned brightest = 0;
    for (std::size_t pixel = 0; pixel < TargetBytes; pixel += 4) brightest = std::max(brightest, static_cast<unsigned>(Target->base[pixel]));
    return brightest;
}

struct Outcome {
    unsigned brightest = 0;
    double accepted = 0;
    double partial = 0;
    double fallback = 0;
    double bumped = 0;
    double lookedUp = 0;
    double rebound = 0;
    double compared = 0;
    double builds = 0;

    std::string Describe() const {
        char text[256];
        std::snprintf(text, sizeof(text), "brightest %u, accepted %.0f, partial %.0f, fallback %.0f, bumped %.0f, looked up %.0f, rebound %.0f, compared %.0f, builds %.0f", brightest, accepted, partial, fallback, bumped, lookedUp, rebound, compared, builds);
        return text;
    }
};

Outcome Draw(AgcDriver::VulkanDevice& device, Scene& scene) {
    GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::WriteIndexOutcomesCounted();
    device.Draw(scene.state, TriangleDraw, scene.shaders);
    device.WaitIdle();
    const auto after = AgcDriver::Graphics::WriteIndexOutcomesCounted();
    Outcome outcome;
    outcome.brightest = Brightest();
    outcome.accepted = static_cast<double>(after.accepted - before.accepted);
    outcome.partial = static_cast<double>(after.partial - before.partial);
    outcome.fallback = static_cast<double>(after.fallback - before.fallback);
    outcome.bumped = static_cast<double>(after.bumped - before.bumped);
    outcome.lookedUp = static_cast<double>(after.lookedUp - before.lookedUp);
    outcome.rebound = static_cast<double>(after.rebound - before.rebound);
    outcome.compared = static_cast<double>(after.compared - before.compared);
    outcome.builds = static_cast<double>(after.builds - before.builds);
    return outcome;
}

void Settle(AgcDriver::VulkanDevice& device, Scene& scene, const std::string& what, unsigned expected = 255u) {
    Outcome outcome;
    for (int round = 0; round < 6; ++round) {
        outcome = Draw(device, scene);
        if (outcome.accepted == 1.0) break;
    }
    Require(outcome.accepted == 1.0 && outcome.brightest == expected, what + ": the unchanged draw did not settle on the index's O(1) proof (" + outcome.Describe() + ")");
}

std::uint64_t Bumps(GuestMemory::WriteBumpSource source) {
    return GuestMemory::WriteIndexCounted().bySource[static_cast<std::size_t>(source)];
}

void Fill(float* texels, float value) {
    std::fill_n(texels, TexelWidth * TexelHeight, value);
}

void CpuWrite(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "cpu write");
    Fill(scene.Texels(Key), 0.0f);
    auto outcome = Draw(device, scene);
    Require(outcome.brightest == 0u, "cpu write: the draw after a CPU write to the sampled texture still sampled its old texels (" + outcome.Describe() + ")");
    Require(outcome.accepted == 0.0 && outcome.bumped == 1.0 && outcome.builds == 0.0, "cpu write: expected one bumped element re-resolved without a rebuild (" + outcome.Describe() + ")");
    outcome = Draw(device, scene);
    Require(outcome.accepted == 1.0 && outcome.brightest == 0u, "cpu write: the next unchanged draw did not return to the O(1) proof (" + outcome.Describe() + ")");
    Fill(scene.Texels(Other), 0.5f);
    outcome = Draw(device, scene);
    Require(outcome.bumped == 1.0 && outcome.brightest == 0u && outcome.builds == 0.0, "cpu write: a write to another element should bump that element only (" + outcome.Describe() + ")");
    Fill(scene.Texels(Key), 1.0f);
    outcome = Draw(device, scene);
    Require(outcome.brightest == 255u && outcome.bumped == 1.0, "cpu write: restoring the texels was missed (" + outcome.Describe() + ")");
}

void TableWord(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "table word");
    const auto bytes = static_cast<std::size_t>(Images) * 16u;
    const bool watched = GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(scene.Table()), bytes);
    std::printf("table word: the T# table is %s\n", watched ? "write-watched (index bump)" : "compared (per-span compare)");
    const auto saved = scene.Table()[Other * 4u + 3u];
    scene.Table()[Other * 4u + 3u] = saved ^ 0x00000100u;
    auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.fallback == 1.0, "table word: a T# word change in the table did not invalidate the O(1) proof (" + outcome.Describe() + ")");
    scene.Table()[Other * 4u + 3u] = saved;
    outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0, "table word: restoring the T# word did not invalidate the O(1) proof (" + outcome.Describe() + ")");
    Settle(device, scene, "table word (after)");
}

void PendingWrite(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "pending write");
    std::vector<std::uint32_t> userData(16, 0u);
    const auto image = Image(scene.Texels(Key));
    std::copy(image.begin(), image.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{8, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto store = ShaderRecompiler::Recompile(request);
    const auto pendingBefore = Bumps(GuestMemory::WriteBumpSource::Pending);
    device.Dispatch(store, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto surface = reinterpret_cast<std::uintptr_t>(scene.Texels(Key));
    Require(AgcDriver::Graphics::StorageTexture::FindPending(surface, TexelWidth * TexelHeight * 4u) != nullptr, "pending write: the store left no pending storage image over the sampled texture");
    Require(OutcomeOnly || Bumps(GuestMemory::WriteBumpSource::Pending) > pendingBefore, "pending write: the recorded storage write did not bump the subscription over the texture");
    auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.brightest == 0u, "pending write: the draw after a pending GPU write over the sampled texture took the O(1) proof or missed the stored texel (" + outcome.Describe() + ")");
    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        AgcDriver::Graphics::FlushCachedTextures(device.Device());
    }
    device.WaitIdle();
    Require(AgcDriver::Graphics::StorageTexture::FindPending(surface, TexelWidth * TexelHeight * 4u) == nullptr, "pending write: the flush left the storage image pending");
    std::uint32_t stored = 0;
    std::memcpy(&stored, scene.Texels(Key), sizeof(stored));
    Require(stored == 0x12345678u, "pending write: the flush did not publish the stored texel to guest memory");
    outcome = Draw(device, scene);
    Require(outcome.brightest == 0u, "pending write: the draw after the write landed sampled the old texels (" + outcome.Describe() + ")");
    Settle(device, scene, "pending write (landed)", 0u);
}

void Remap(AgcDriver::VulkanDevice& device) {
#ifdef _WIN32
    static_cast<void>(device);
    std::puts("remap: skipped on Windows");
#else
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "remap");
    auto* block = reinterpret_cast<std::uint8_t*>(scene.Texels(Key));
    void* fresh = mmap(nullptr, BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(fresh != MAP_FAILED, "remap: cannot map the replacement block");
    Fill(static_cast<float*>(fresh), 0.0f);
    const auto forgetBefore = Bumps(GuestMemory::WriteBumpSource::Forget);
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(scene.textures.base);
    }
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
    Require(mremap(fresh, BlockBytes, BlockBytes, MREMAP_MAYMOVE | MREMAP_FIXED, block) == block, "remap: cannot move the replacement block over the texture");
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(scene.textures.base, scene.textures.bytes, true, true, true);
    }
    Require(OutcomeOnly || Bumps(GuestMemory::WriteBumpSource::Forget) > forgetBefore, "remap: the free and remap of the texture's range did not bump its subscription");
    const auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.brightest == 0u, "remap: the draw after a remap of the sampled texture took the O(1) proof or sampled the old mapping (" + outcome.Describe() + ")");
    Settle(device, scene, "remap (after)", 0u);
#endif
}

void Compared(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    GuestMemory::Unwatch(reinterpret_cast<std::uintptr_t>(scene.Texels(Key)), BlockBytes);
    Require(!GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(scene.Texels(Key)), BlockBytes), "compared: the texture's range is still watched");
    Build(device, scene);
    Settle(device, scene, "compared");
    auto outcome = Draw(device, scene);
    Require(OutcomeOnly || (outcome.accepted == 1.0 && outcome.compared == 1.0), "compared: expected the unwatched element on its per-element proof inside the O(1) path (" + outcome.Describe() + ")");
    Fill(scene.Texels(Key), 0.0f);
    outcome = Draw(device, scene);
    Require(outcome.brightest == 0u && outcome.builds == 0.0, "compared: a CPU write to the compared texture was missed or rebuilt the object (" + outcome.Describe() + ")");
    Fill(scene.Texels(Key), 1.0f);
    outcome = Draw(device, scene);
    Require(outcome.brightest == 255u, "compared: restoring the compared texture was missed (" + outcome.Describe() + ")");
}

void DriverStore(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "driver store");
    const auto driverBefore = Bumps(GuestMemory::WriteBumpSource::Driver);
    GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(scene.Texels(Other)), TexelWidth * TexelHeight * 4u);
    Require(OutcomeOnly || Bumps(GuestMemory::WriteBumpSource::Driver) > driverBefore, "driver store: a driver store over a texture did not bump its subscription");
    auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.brightest == 255u && outcome.builds == 0.0, "driver store: the draw after a driver store over a sampled texture took the O(1) proof or rebuilt the object (" + outcome.Describe() + ")");
    Settle(device, scene, "driver store (after)");
}

void UnwatchRange(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "unwatch");
    const auto unwatchBefore = Bumps(GuestMemory::WriteBumpSource::Unwatch);
    GuestMemory::Unwatch(reinterpret_cast<std::uintptr_t>(scene.Texels(Other)), BlockBytes);
    Require(OutcomeOnly || Bumps(GuestMemory::WriteBumpSource::Unwatch) > unwatchBefore, "unwatch: leaving the write watch did not bump the texture's subscription");
    const auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.brightest == 255u, "unwatch: the draw after a texture left the write watch took the O(1) proof (" + outcome.Describe() + ")");
}

void Released(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "release");
    const auto releaseBefore = Bumps(GuestMemory::WriteBumpSource::Release);
    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        AgcDriver::Graphics::ClearCachedTextures(device.Device());
    }
    Require(OutcomeOnly || Bumps(GuestMemory::WriteBumpSource::Release) > releaseBefore, "release: dropping the sampled textures from the cache did not bump their subscription");
    auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.brightest == 255u, "release: the draw after its textures left the sampled cache took the O(1) proof (" + outcome.Describe() + ")");
    Require(OutcomeOnly || outcome.lookedUp == static_cast<double>(Images), "release: expected every released element looked up again (" + outcome.Describe() + ")");
    Settle(device, scene, "release (after)");
}

void Imported(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene);
    Settle(device, scene, "import");
    const auto importBefore = Bumps(GuestMemory::WriteBumpSource::Import);
    Require(GuestMemory::ImportWatched(reinterpret_cast<std::uintptr_t>(scene.Texels(Other)), BlockBytes, [] { return true; }, true), "import: the import of the texture's range was refused");
    Require(OutcomeOnly || Bumps(GuestMemory::WriteBumpSource::Import) > importBefore, "import: an import over a texture did not bump its subscription");
    auto outcome = Draw(device, scene);
    Require(outcome.accepted == 0.0 && outcome.lookedUp == 1.0 && outcome.brightest == 255u && outcome.builds == 0.0, "import: expected the imported element looked up again without a rebuild (" + outcome.Describe() + ")");
    Settle(device, scene, "import (after)");
}

void ArraySlices(AgcDriver::VulkanDevice& device) {
    auto& scene = NewScene();
    Build(device, scene, true);
    Settle(device, scene, "array");
    Fill(scene.Texels(Key), 0.0f);
    auto outcome = Draw(device, scene);
    Require(outcome.brightest == 0u && outcome.bumped == 1.0 && outcome.fallback == 0.0 && outcome.builds == 0.0, "array: a write to the base slice of a 2D array read as 2D was missed or rebuilt the object (" + outcome.Describe() + ")");
    std::fill_n(scene.Texels(Key) + 8u * TexelWidth * TexelHeight, TexelWidth * TexelHeight, 1.0f);
    outcome = Draw(device, scene);
    Require(outcome.brightest == 0u && outcome.rebound == 0.0 && outcome.fallback == 0.0 && outcome.builds == 0.0, "array: a write to another slice of a 2D array read as 2D re-bound the element the draw reads from its base slice (" + outcome.Describe() + ")");
    Settle(device, scene, "array (after)", 0u);
}

}

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        _putenv_s("APS5_TABLE_INDEX", "1");
#else
        setenv("APS5_TABLE_INDEX", "1", 1);
#endif
        std::vector<std::string> args(argv + 1, argv + argc);
        if (const auto flag = std::find(args.begin(), args.end(), "outcome"); flag != args.end()) {
            OutcomeOnly = true;
            args.erase(flag);
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(GuestMemory::WriteIndexEnabled(), "the write index is not enabled");
        if (!GuestMemory::WriteWatched()) {
            std::puts("table index validation skipped: no write watch");
            return 0;
        }
        TargetBytes = AgcDriver::Graphics::ColorTargetLayout(TargetWidth, TargetHeight, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4).Bytes();
        GuestBlocks target((TargetBytes + BlockBytes - 1) / BlockBytes * BlockBytes);
        Target = &target;
        const std::array<std::pair<const char*, std::function<void(AgcDriver::VulkanDevice&)>>, 10> cases{{{"cpu", &CpuWrite}, {"table", &TableWord}, {"pending", &PendingWrite}, {"remap", &Remap}, {"compared", &Compared}, {"driver", &DriverStore}, {"unwatch", &UnwatchRange}, {"release", &Released}, {"import", &Imported}, {"array", &ArraySlices}}};
        for (const auto& [name, run] : cases) {
            if (!args.empty() && std::find(args.begin(), args.end(), name) == args.end()) continue;
            run(*device);
            std::printf("table index validation: %s passed\n", name);
        }
        device->WaitIdle();
        Scenes.clear();
        std::puts("table index validation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
