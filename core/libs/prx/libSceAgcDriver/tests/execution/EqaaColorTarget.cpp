#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
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
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Color = std::array<float, 4>;
using Triangle = std::array<std::array<float, 4>, 3>;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 256;
constexpr std::uint32_t Samples = 4;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t CompressedUnorm8x4Info = 0xc028u;
constexpr std::uint32_t Unorm8x4Info = 0x8028u;
constexpr std::uint32_t EqaaAttrib = 0xa000u;
constexpr std::uint32_t MsaaAttrib = 0x12000u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint64_t EqaaBytes = 0x80000;
constexpr std::uint64_t MsaaBytes = 0x100000;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr Triangle Lower{{{-1.0f, -1.0f, 0.5f, 1.0f}, {0.9f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.7f, 0.5f, 1.0f}}};
alignas(256) constexpr Triangle Upper{{{-0.5f, 1.0f, 0.5f, 1.0f}, {1.0f, -0.6f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f}}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 4> Programs{};
std::size_t programCount = 0;

std::span<const std::uint32_t> Program(const Color& color) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

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
            Require(raw != MAP_FAILED, "cannot map a test block");
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
        Require(data != nullptr, "cannot allocate a test block");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "a test block is not write-watched");
        std::memset(data, 0, bytes);
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, const Block* fmask, std::uint32_t info, std::uint32_t attrib) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = info;
    cx[0x31d] = attrib;
    cx[0x31f] = 0;
    cx[0x321] = fmask == nullptr ? 0u : static_cast<std::uint32_t>(fmask->Address() >> 8u);
    cx[0x3a0] = fmask == nullptr ? 0u : static_cast<std::uint32_t>(fmask->Address() >> 40u);
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R8G8B8A8_UNORM && color.elementBytes == 4 && color.tileMode == ColorTileMode::RenderTarget, "the 8_8_8_8 unorm target did not decode as an RGBA8 render target");
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const Triangle& triangle, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(triangle.size()), 0x01016facu};
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
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = color.samples;

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint8_t> Resolve(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& source, const Block& destination) {
    AgcDriver::Graphics::ColorMetadataPass pass{};
    pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
    pass.source = source;
    pass.targets = {DecodeTarget(destination, nullptr, Unorm8x4Info, 0u)};
    device.ColorMetadataPass(pass);
    AgcDriver::Graphics::StorageTexture::FlushPending(destination.Address(), destination.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(destination.bytes);
    AgcDriver::GuestMemory::Read(destination.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::string GuardRefusal(std::uint64_t address, std::size_t bytes) {
    try {
        AgcDriver::Graphics::RequireNoMultisampleColorTarget(address, bytes, "a test read");
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const ColorTargetLayout resolvedLayout(Width, Height, ColorTileMode::RenderTarget, 4u);
        Block eqaaColor(static_cast<std::size_t>(MsaaBytes));
        Block eqaaFmask(BlockBytes);
        Block msaaColor(static_cast<std::size_t>(MsaaBytes));
        Block msaaFmask(BlockBytes);
        Block eqaaResolved(resolvedLayout.Bytes());
        Block msaaResolved(resolvedLayout.Bytes());

        const auto eqaa = DecodeTarget(eqaaColor, &eqaaFmask, CompressedUnorm8x4Info, EqaaAttrib);
        const auto msaa = DecodeTarget(msaaColor, &msaaFmask, CompressedUnorm8x4Info, MsaaAttrib);
        Require(eqaa.samples == Samples && msaa.samples == Samples, "the EQAA and 4x targets are not drawn at their 4 coverage samples");
        Require(AgcDriver::Graphics::MultisampledColorBytes(eqaa) == EqaaBytes && AgcDriver::Graphics::MultisampledColorBytes(msaa) == MsaaBytes, "the 4-sample 2-fragment target does not occupy the 512 KiB of its fragments, or the 4x target the 1 MiB of its samples");

        const auto red = Program({1.0f, 0.0f, 0.0f, 1.0f});
        const auto green = Program({0.0f, 1.0f, 0.0f, 1.0f});
        for (const auto* target : {&eqaa, &msaa}) {
            Draw(*device, *target, Lower, red);
            Draw(*device, *target, Upper, green);
        }
        device->WaitIdle();

        const auto fmaskRefusal = GuardRefusal(eqaaFmask.Address(), 256u);
        Require(fmaskRefusal.find("the FMASK of EQAA color target") != std::string::npos && fmaskRefusal.find("whose samples are only on the host") != std::string::npos, "a read of the EQAA target's FMASK was not refused by name: " + fmaskRefusal);
        Require(!GuardRefusal(eqaaColor.Address() + EqaaBytes - 1u, 1u).empty(), "the last byte of the EQAA target's fragments is not guarded");
        Require(GuardRefusal(eqaaColor.Address() + EqaaBytes, static_cast<std::size_t>(MsaaBytes - EqaaBytes)).empty(), "memory past the EQAA target's two fragments is guarded as if it held four samples");
        Require(GuardRefusal(msaaFmask.Address(), 256u).empty(), "the FMASK of a 4-sample 4-fragment target was guarded");

        const auto eqaaTexels = Resolve(*device, eqaa, eqaaResolved);
        const auto msaaTexels = Resolve(*device, msaa, msaaResolved);
        Require(eqaaTexels == msaaTexels, "the CB resolve of the 4-sample 2-fragment target differs from the 4-sample 4-fragment one drawn the same way");
        const auto quarter = std::count_if(eqaaTexels.begin(), eqaaTexels.end(), [](std::uint8_t value) { return (value >= 60u && value <= 68u) || (value >= 187u && value <= 195u); });
        Require(quarter > 0, "no edge pixel of the EQAA resolve holds a quarter or three-quarter coverage value, so its 4 coverage samples were not kept");
        const auto covered = std::count(eqaaTexels.begin(), eqaaTexels.end(), std::uint8_t{255});
        Require(covered > 0 && covered < static_cast<std::ptrdiff_t>(eqaaTexels.size()), "the EQAA resolve holds no fully covered or no uncovered texels");

        Block movedFmask(BlockBytes);
        const auto moved = DecodeTarget(eqaaColor, &movedFmask, CompressedUnorm8x4Info, EqaaAttrib);
        Draw(*device, moved, Lower, red);
        device->WaitIdle();
        const auto movedRefusal = GuardRefusal(movedFmask.Address(), 256u);
        Require(movedRefusal.find("the FMASK of EQAA color target") != std::string::npos, "a read of the FMASK base the EQAA target was last drawn with was not refused by name: " + movedRefusal);
        Require(GuardRefusal(eqaaFmask.Address(), 256u).empty(), "the FMASK base the EQAA target was drawn with before is still guarded");
        std::printf("EQAA color target tests passed: 4-sample 2-fragment draws resolve like 4x MSAA (%zu bytes), %td quarter-coverage channels, FMASK guarded at the base of the last draw\n", eqaaTexels.size(), quarter);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
