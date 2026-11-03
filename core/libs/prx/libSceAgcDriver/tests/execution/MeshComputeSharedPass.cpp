#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "../MeshGeometryFixture.hpp"
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
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::size_t TargetBytes = Width * Height * 4;
alignas(256) constexpr std::array<std::array<float, 8>, 3> LeftVertices{{
    {-0.9f, -0.8f, 0, 1, 1, 1, 1, 1}, {-0.1f, -0.8f, 0, 1, 1, 1, 1, 1}, {-0.5f, 0.8f, 0, 1, 1, 1, 1, 1}
}};
alignas(256) constexpr std::array<std::array<float, 8>, 3> RightVertices{{
    {0.1f, -0.8f, 0, 1, 1, 1, 1, 1}, {0.9f, -0.8f, 0, 1, 1, 1, 1, 1}, {0.5f, 0.8f, 0, 1, 1, 1, 1, 1}
}};
alignas(256) constexpr std::array<std::array<float, 8>, 3> OffscreenVertices{{
    {3.0f, 3.0f, 0, 1, 1, 1, 1, 1}, {3.5f, 3.0f, 0, 1, 1, 1, 1, 1}, {3.0f, 3.5f, 0, 1, 1, 1, 1, 1}
}};
alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};

constexpr ShaderRecompiler::MeshConfiguration Subgroup{4u, 4u, 12u, 12u, 4u, 64u, 1024u, 0u, 4u};

constexpr std::size_t BlockBytes = 65536;

class GuestTarget {
public:
    GuestTarget() : watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
        } else {
            data = static_cast<std::byte*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, BlockBytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes * 2) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::byte*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
        } else {
            data = static_cast<std::byte*>(std::aligned_alloc(BlockBytes, BlockBytes));
        }
#endif
        Require(data != nullptr, "cannot allocate the color target");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, BlockBytes, true, true, true);
    }

    ~GuestTarget() {
        AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(data), BlockBytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, BlockBytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
        } else {
            std::free(data);
        }
#endif
    }

    GuestTarget(const GuestTarget&) = delete;
    GuestTarget& operator=(const GuestTarget&) = delete;
    std::span<std::byte> Pixels() const { return {data, TargetBytes}; }
    bool Watched() const { return watched && AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(data), BlockBytes); }

private:
    bool watched;
    std::byte* data = nullptr;
};

struct Program {
    ShaderRecompiler::RecompileResult geometry;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t geometryPush = 0;
};

Program Compile(const AgcDriver::VulkanDevice& device, const std::array<std::array<float, 8>, 3>& vertices, const AgcDriver::Pm4::DrawParameters& draw) {
    const auto target = device.Target();
    const auto address = reinterpret_cast<std::uintptr_t>(vertices.data());
    std::vector<std::uint32_t> userData(12u, 0u);
    userData[8] = static_cast<std::uint32_t>(address);
    userData[9] = static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (32u << 16u);
    userData[10] = static_cast<std::uint32_t>(vertices.size());
    userData[11] = 0x01016facu;
    const auto& code = AgcDriver::Test::MeshGeometryCode;
    const std::array<ShaderRecompiler::MemoryRegion, 1> geometryMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    ShaderRecompiler::RecompileRequest geometry{
        {ShaderStage::Mesh, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, geometryMemory},
        target,
        {0, 0, 0, ShaderRecompiler::MeshDrawPushOffsetBytes}
    };
    geometry.graphics = ShaderRecompiler::GraphicsCompileContext{0, {}, Subgroup, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}};
    geometry.useCache = false;
    Program program{ShaderRecompiler::Recompile(geometry), {}, 0};
    program.geometryPush = static_cast<std::uint32_t>(program.geometry.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(program.geometryPush, target)
    };
    fragment.useCache = false;
    program.pixel = ShaderRecompiler::Recompile(fragment);
    return program;
}

AgcDriver::Graphics::State Target(std::span<std::byte> pixels) {
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Geometry, 0x20u, 64, 64, Subgroup, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.color.surfaceAddress = state.color.address;
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
    return state;
}

void Draw(const AgcDriver::Graphics::Context& context, const AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, const Program& program, const AgcDriver::Pm4::DrawParameters& draw) {
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Mesh, &program.geometry, 0},
        {ShaderStage::Fragment, &program.pixel, PixelPushOffset(program.geometryPush, device.Target())}
    }};
    AgcDriver::Graphics::Draw(context, state, draw, shaders);
}

AgcDriver::Graphics::DrawPassCounts Passes() {
    AgcDriver::Graphics::DrawPassCounts sum{};
    for (const auto& counts : AgcDriver::Graphics::DrawPassTotals()) {
        sum.draws += counts.draws;
        sum.passesBegun += counts.passesBegun;
        sum.passesContinued += counts.passesContinued;
    }
    return sum;
}

std::string Describe(const AgcDriver::Graphics::DrawPassCounts& before, const AgcDriver::Graphics::DrawPassCounts& after) {
    return std::to_string(after.draws - before.draws) + " recorded, " + std::to_string(after.passesBegun - before.passesBegun) + " begun, " + std::to_string(after.passesContinued - before.passesContinued) + " continued";
}

bool Lit(std::span<const std::byte> pixels, std::uint32_t x) {
    return std::to_integer<unsigned>(pixels[(24u * Width + x) * 4u]) > 200u;
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
        _putenv_s("APS5_NO_MESH_SHADER", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
        setenv("APS5_NO_MESH_SHADER", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->Target().meshAsCompute) {
            std::puts("skipped, the device runs mesh-path draws in its mesh stage");
            return VulkanTestSkipped;
        }
        const AgcDriver::Pm4::DrawParameters draw{0, 3, 0, 1, 0, false};
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const auto left = Compile(*device, LeftVertices, draw);
        const auto right = Compile(*device, RightVertices, draw);
        const GuestTarget pixels;
        const GuestTarget otherPixels;
        if (!pixels.Watched() || !otherPixels.Watched()) {
            std::puts("skipped, guest memory is not write-watched here, so a linear color target is never resident");
            return VulkanTestSkipped;
        }
        const auto state = Target(pixels.Pixels());
        const auto other = Target(otherPixels.Pixels());
        const auto offscreen = Compile(*device, OffscreenVertices, draw);
        for (const auto* target : {&state, &other}) {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            Draw(context, *device, *target, offscreen, draw);
        }
        device->WaitIdle();
        AgcDriver::Graphics::DrawPassCounts first{}, afterLeft{}, afterRight{}, afterOther{};
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::Recorder recorder(context);
            recorder.Activate();
            first = Passes();
            Draw(context, *device, state, left, draw);
            afterLeft = Passes();
            Draw(context, *device, state, right, draw);
            afterRight = Passes();
            Draw(context, *device, other, left, draw);
            afterOther = Passes();
            recorder.Submit();
            recorder.Sync();
        }
        device->WaitIdle();
        AgcDriver::Graphics::StorageTexture::FlushAllPending("mesh compute shared pass test");
        device->WaitIdle();
        Require(afterLeft.draws - first.draws == 1 && afterRight.draws - afterLeft.draws == 1 && afterOther.draws - afterRight.draws == 1, "the mesh-path draws were not recorded: " + Describe(first, afterOther));
        Require(afterRight.passesBegun == afterLeft.passesBegun && afterRight.passesContinued - afterLeft.passesContinued == 1, "a mesh-path draw into the same target did not continue the pass: " + Describe(afterLeft, afterRight));
        Require(afterOther.passesBegun - afterRight.passesBegun == 1, "a mesh-path draw into another target did not begin a pass: " + Describe(afterRight, afterOther));
        Require(Lit(pixels.Pixels(), 16) && Lit(pixels.Pixels(), 48) && !Lit(pixels.Pixels(), 32), "the mesh-path draws sharing a pass drew the wrong pixels: left=" + std::to_string(Lit(pixels.Pixels(), 16)) + " gap=" + std::to_string(Lit(pixels.Pixels(), 32)) + " right=" + std::to_string(Lit(pixels.Pixels(), 48)));
        Require(Lit(otherPixels.Pixels(), 16) && !Lit(otherPixels.Pixels(), 48), "the mesh-path draw into the second target drew the wrong pixels");
        std::cout << "Consecutive mesh-path draws run as compute shared one render pass\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
