#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint8_t DrawnStencil = 0x5a;
constexpr std::uint8_t BaseStencil = 0x25;
constexpr std::uint8_t Untouched = 0xc3;
constexpr std::uint8_t ReferenceKept = 0x40;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 4> WhitePixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
constexpr std::size_t CopyPitch = 256;
alignas(256) std::array<std::uint8_t, CopyPitch * Height> CopyTarget{};
alignas(256) std::array<std::uint8_t, Width * Height * 4> StencilReference{};
alignas(256) std::array<std::uint8_t, Width * Height * 4> CopyReference{};
constexpr float StencilShift = Width / 4.0f;
constexpr std::size_t PlaneBytes = 65536;
std::array<std::byte, PlaneBytes * 3> PlaneStorage{};
std::byte* const DepthMemory = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(PlaneStorage.data()) + PlaneBytes - 1) & ~(PlaneBytes - 1));
std::byte* const StencilMemory = DepthMemory + PlaneBytes;

struct Header {
    Shader shader{};
    ShaderUserData userData{};
    std::array<ShaderRegister, 4> registers{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    if (type == 1) {
        const auto address = reinterpret_cast<std::uintptr_t>(code);
        header.registers = {{{0x008, static_cast<std::uint32_t>(address >> 8u)}, {0x009, static_cast<std::uint32_t>(address >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)}}};
        header.shader.sh_registers = header.registers.data();
        header.shader.num_sh_registers = header.registers.size();
    }
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

constexpr std::uint32_t StencilEnable = 1u;
constexpr std::uint32_t Always = 7u << 8u;
constexpr std::uint32_t ReplaceWithTest = 0x30u;
constexpr std::uint32_t Rgba8Unorm = 0x28028u;
constexpr std::uint32_t R8Uint = 0x8404u;
constexpr std::uint32_t R8UintAlternate = 0x50c04u;

struct DrawOptions {
    std::uintptr_t color = 0;
    std::uint32_t colorInfo = Rgba8Unorm;
    std::uint32_t targetMask = 0xf;
    std::uint32_t renderControl = 0;
    std::uint32_t depthControl = 0;
    std::uint32_t stencilOperations = 0;
    std::uint32_t stencilRefMask = 0;
    float shift = 0.0f;
    std::uint32_t samples = 1;
    bool depthTarget = true;
    std::uint32_t sampleMask = 0xffffffffu;
};

void AppendDraw(std::vector<std::uint32_t>& commands, const DrawOptions& options) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(WhitePixelCode.data());
    const auto depth = reinterpret_cast<std::uintptr_t>(DepthMemory);
    const auto stencil = reinterpret_cast<std::uintptr_t>(StencilMemory);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, 0}, {0x1b4, 0}, {0x1b6, 0}, {0x207, 0}, {0x200, options.depthControl}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, options.samples == 2 ? 0x00100001u : 0u}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, options.sampleMask}, {0x30f, options.sampleMask},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, options.targetMask}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 9}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, options.colorInfo},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, (Height << 16u) | Width},
        {0x81, 0x80000000}, {0x82, (Height << 16u) | Width},
        {0x90, 0x80000000}, {0x91, (Height << 16u) | Width},
        {0x94, 0x80000000}, {0x95, (Height << 16u) | Width},
        {0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(Width / 2.0f + options.shift)},
        {0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f))}, {0x112, std::bit_cast<std::uint32_t>(Height / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(options.color >> 8u)}, {0x390, static_cast<std::uint32_t>(options.color >> 40u)}, {0x292, 2},
        {0x000, options.renderControl}, {0x002, 0}, {0x007, ((Height - 1u) << 16u) | (Width - 1u)},
        {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
        {0x010, !options.depthTarget ? 0u : options.samples == 2 ? 0x80000187u : 3u}, {0x011, !options.depthTarget ? 0u : options.samples == 2 ? 0x20000181u : 1u},
        {0x012, static_cast<std::uint32_t>(depth >> 8u)}, {0x014, static_cast<std::uint32_t>(depth >> 8u)},
        {0x013, static_cast<std::uint32_t>(stencil >> 8u)}, {0x015, static_cast<std::uint32_t>(stencil >> 8u)},
        {0x01a, static_cast<std::uint32_t>(depth >> 40u)}, {0x01c, static_cast<std::uint32_t>(depth >> 40u)},
        {0x01b, static_cast<std::uint32_t>(stencil >> 40u)}, {0x01d, static_cast<std::uint32_t>(stencil >> 40u)},
        {0x10b, options.stencilOperations}, {0x10c, options.stencilRefMask}, {0x10d, options.stencilRefMask},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
}

bool Covered(const std::array<std::uint8_t, Width * Height * 4>& reference, std::size_t index, const std::string& what) {
    const auto value = reference[index * 4u];
    Require(value == 0xffu || value == ReferenceKept, what + ": a reference draw wrote " + std::to_string(value) + " at texel " + std::to_string(index));
    return value == 0xffu;
}

void CopyHoldsTheDrawnStencil(std::uint32_t renderControl, std::uint32_t samples, const std::string& what, std::uint32_t copyInfo = R8Uint, std::uint32_t copyMask = 1) {
    const auto copiedSample = (renderControl & 0x80u) != 0 ? 0u : (renderControl >> 8u) & 0xfu;
    const bool drawnInCopiedSample = samples == 1 || copiedSample == 1;
    CopyTarget.fill(Untouched);
    StencilReference.fill(ReferenceKept);
    CopyReference.fill(ReferenceKept);
    std::vector<std::uint32_t> commands;
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(CopyReference.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = BaseStencil | 0xff00u | 0xff0000u, .samples = samples});
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(StencilReference.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = DrawnStencil | 0xff00u | 0xff0000u, .shift = StencilShift, .samples = samples, .sampleMask = samples == 1 ? 0xffffffffu : 0x00020002u});
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(CopyTarget.data()), .colorInfo = copyInfo, .targetMask = copyMask, .renderControl = renderControl, .samples = samples});
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(StencilReference.data()), .shift = StencilShift, .depthTarget = false});
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(CopyReference.data()), .depthTarget = false});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, what + ": the stencil write, DB stencil copy and reference draws were not submitted");
    AgcDriverWaitIdle_nid_postfix();
    for (const auto* target : {CopyTarget.data(), StencilReference.data(), CopyReference.data()}) AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(target), target == CopyTarget.data() ? sizeof(CopyTarget) : sizeof(CopyReference));
    std::array<std::size_t, 3> counts{};
    for (std::size_t index = 0; index < Width * Height; ++index) {
        const auto copy = CopyTarget[(index / Width) * CopyPitch + index % Width];
        const bool copied = Covered(CopyReference, index, what);
        const bool stenciled = Covered(StencilReference, index, what);
        const auto expected = !copied ? Untouched : stenciled && drawnInCopiedSample ? DrawnStencil : BaseStencil;
        ++counts[!copied ? 0 : stenciled && drawnInCopiedSample ? 1 : 2];
        Require(copy == expected, what + ": the DB stencil copy holds " + std::to_string(copy) + " at texel " + std::to_string(index) + ", expected " + std::to_string(expected) + (!copied ? " (outside the copy draw)" : stenciled && drawnInCopiedSample ? " (the shifted stencil draw covers it in the copied sample)" : stenciled ? " (the shifted stencil draw wrote only sample 1, the copy reads sample 0)" : " (inside the copy draw, outside the shifted stencil draw)"));
    }
    for (std::size_t row = 0; row < Height; ++row) {
        for (std::size_t x = Width; x < CopyPitch; ++x) Require(CopyTarget[row * CopyPitch + x] == Untouched, what + ": the DB stencil copy wrote past the width of row " + std::to_string(row));
    }
    Require(counts[0] != 0 && (counts[1] != 0) == drawnInCopiedSample && counts[2] != 0, what + ": " + std::to_string(counts[1]) + " copied texels hold drawn stencil, " + std::to_string(counts[2]) + " hold the first stencil draw's value and " + std::to_string(counts[0]) + " lie outside the copy" + (drawnInCopiedSample ? ", expected some of each" : ", expected no drawn stencil in sample 0"));
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, WhitePixelCode.data(), sizeof(WhitePixelCode), 1);
        const std::array<ShaderRegister, 3> pixelContext{{{0x203, 0x800}, {0x1c4, 9}, {0x1b6, 0}}};
        AgcDriverResolveShaderAbi_nid_postfix(&PixelHeader.shader, pixelContext, {});
        CopyHoldsTheDrawnStencil(0x88u, 1, "a centroid stencil copy of a one-sample plane");
        CopyHoldsTheDrawnStencil(0x08u, 1, "a sample 0 stencil copy of a one-sample plane");
        CopyHoldsTheDrawnStencil(0x88u, 2, "a centroid stencil copy of a two-sample plane");
        CopyHoldsTheDrawnStencil(0x108u, 2, "a sample 1 stencil copy of a two-sample plane");
        CopyHoldsTheDrawnStencil(0x88u, 2, "a centroid stencil copy of a two-sample plane into an alternate-swap target written through green", R8UintAlternate, 2);
        CopyHoldsTheDrawnStencil(0x108u, 2, "a sample 1 stencil copy of a two-sample plane into an alternate-swap target written through green", R8UintAlternate, 2);
        AgcDriverShutdown_nid_postfix();
        std::puts("the DB stencil copy wrote both drawn stencil values exactly where its draw rasterized");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
