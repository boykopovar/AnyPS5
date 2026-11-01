#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
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
#include <string>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e040285u, 0xf8000082u, 0x00000200u, 0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 4> WhitePixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> ColorMemory{};
alignas(256) std::array<std::byte, 65536> DepthMemory{};
alignas(256) std::array<std::byte, 65536> StencilMemory{};

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

constexpr std::array<ShaderRegister, 3> PixelContext{{{0x203, 0x802}, {0x1c4, 9}, {0x1b6, 0}}};

struct DrawOptions {
    bool depth = false;
    std::uint32_t depthControl = 0x701u;
    std::uint32_t operations = 0;
    std::uint32_t front = 0x00ffff25u;
    std::uint32_t back = 0x00ffff25u;
    std::uint32_t shaderControl = 0x802u;
    std::uint32_t pixelInputs = 0;
};

std::vector<std::uint32_t> Draw(std::span<const std::uint32_t> pixelCode = PixelCode, const DrawOptions& options = {}) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, options.pixelInputs}, {0x1b4, options.pixelInputs}, {0x1b6, 0}, {0x207, 0}, {0x200, options.depth ? options.depthControl : 0}, {0x203, options.shaderControl},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 9}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)}, {0x292, 2},
    };
    if (options.depth) {
        const auto depth = reinterpret_cast<std::uintptr_t>(DepthMemory.data());
        const auto stencil = reinterpret_cast<std::uintptr_t>(StencilMemory.data());
        const std::vector<std::pair<std::uint32_t, std::uint32_t>> depthContext{
            {0x000, 0}, {0x002, 0}, {0x007, (3u << 16u) | 63u},
            {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
            {0x010, 3}, {0x011, 1},
            {0x012, static_cast<std::uint32_t>(depth >> 8u)}, {0x014, static_cast<std::uint32_t>(depth >> 8u)},
            {0x013, static_cast<std::uint32_t>(stencil >> 8u)}, {0x015, static_cast<std::uint32_t>(stencil >> 8u)},
            {0x01a, static_cast<std::uint32_t>(depth >> 40u)}, {0x01c, static_cast<std::uint32_t>(depth >> 40u)},
            {0x01b, static_cast<std::uint32_t>(stencil >> 40u)}, {0x01d, static_cast<std::uint32_t>(stencil >> 40u)},
            {0x10b, options.operations}, {0x10c, options.front}, {0x10d, options.back},
        };
        context.insert(context.end(), depthContext.begin(), depthContext.end());
    }
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
    return commands;
}

void UnusedStencilRefExport() {
    ColorMemory.fill(Kept);
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    AgcDriverResolveShaderAbi_nid_postfix(&PixelHeader.shader, PixelContext, {});
    auto commands = Draw();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto kept = static_cast<std::size_t>(std::count(ColorMemory.begin(), ColorMemory.end(), Kept));
    Require(kept != ColorMemory.size(), "the draw whose pixel shader exports an unused stencil test value wrote no pixels");
}

void SubmitDraw(std::span<const std::uint32_t> code, const DrawOptions& options) {
    ColorMemory.fill(Kept);
    Registered(PixelHeader, code.data(), code.size_bytes(), 1);
    const std::array<ShaderRegister, 5> context{{{0x203, options.shaderControl}, {0x1c4, 9}, {0x1b6, 0}, {0x1b3, options.pixelInputs}, {0x1b4, options.pixelInputs}}};
    AgcDriverResolveShaderAbi_nid_postfix(&PixelHeader.shader, context, {});
    auto commands = Draw(code, options);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the stencil reference regression submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

std::array<std::byte, 1024> InitializeStencil() {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    SubmitDraw(PixelCode, {.depth = true, .operations = 0x30u, .shaderControl = 0x800u});
    const auto coverage = ColorMemory;
    Require(std::count(coverage.begin(), coverage.end(), Kept) != coverage.size(), "the stencil fixture has no covered pixels");
    return coverage;
}

template<class TPredicate>
void CheckWhitePixels(const std::array<std::byte, 1024>& coverage, TPredicate selected, const char* message) {
    std::size_t selectedPixels = 0;
    for (std::uint32_t y = 0; y < 4u; ++y) {
        for (std::uint32_t x = 0; x < 64u; ++x) {
            const auto pixel = (y * 64u + x) * 4u;
            const bool drawn = coverage[pixel] != Kept && selected(x, y);
            selectedPixels += drawn;
            for (std::uint32_t channel = 0; channel < 4u; ++channel) Require(ColorMemory[pixel + channel] == (drawn ? std::byte{255} : Kept), message);
        }
    }
    Require(selectedPixels != 0, "the stencil readback probe selected no covered pixels");
}

void ProbeStencil(std::uint32_t reference) {
    SubmitDraw(WhitePixelCode, {.depth = true, .depthControl = 0x201u, .front = 0x0000ff00u | reference, .back = 0x0000ff00u | reference, .shaderControl = 0x800u});
}

void IndependentReference(bool invert) {
    const auto coverage = InitializeStencil();
    const auto operations = invert ? 0x00070070u : 0u;
    SubmitDraw(PixelCode, {.depth = true, .depthControl = 0x007007e7u, .operations = operations, .shaderControl = 0x800u});
    const auto expected = ColorMemory;
    Require(expected == coverage, "the reference-independent baseline changed color coverage");
    static_cast<void>(InitializeStencil());
    SubmitDraw(PixelCode, {.depth = true, .depthControl = 0x007007e7u, .operations = operations});
    Require(ColorMemory == expected, "dropping an unused reference changed the full color readback");
    ProbeStencil(invert ? (37u ^ 255u) : 37u);
    CheckWhitePixels(coverage, [](auto, auto) { return true; }, "dropping an unused reference changed the stencil readback");
}

void DependentReference(std::string_view name) {
    static_cast<void>(InitializeStencil());
    DrawOptions options{.depth = true};
    if (name == "compare") options.depthControl = 0x201u;
    else if (name == "back-compare") options.depthControl = 0x200781u;
    else if (name == "replace") options.operations = 0x30u;
    else if (name == "fail-replace") { options.depthControl = 1u; options.operations = 3u; }
    else if (name == "depth-fail-replace") { options.depthControl = 0x703u; options.operations = 0x300u; }
    else if (name == "partial-replace") { options.operations = 0x30u; options.front = options.back = 0x0080ff25u; }
    else throw std::runtime_error("unknown dependent stencil reference case");
    const auto stencil = StencilMemory;
    bool refused = false;
    try { SubmitDraw(PixelCode, options); }
    catch (const std::exception& error) {
        refused = std::string_view(error.what()).find("a pixel shader that exports the stencil reference needs VK_EXT_shader_stencil_export, which the device lacks") != std::string_view::npos;
        if (!refused) throw;
    }
    Require(refused, "a dependent reference without native stencil export was admitted");
    Require(std::all_of(ColorMemory.begin(), ColorMemory.end(), [](auto value) { return value == Kept; }), "a refused stencil draw wrote color");
    Require(StencilMemory == stencil, "a refused stencil draw changed its guest stencil memory");
}

}

int main(int argc, char** argv) {
    try {
        bool nativeStencilExport = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            nativeStencilExport = TargetHasCapability(device->Target(), spv::CapabilityStencilExportEXT);
        }
        bool expectedRefusal = false;
        if (argc == 3 && std::string_view(argv[1]) == "--case") {
            const std::string_view name = argv[2];
            if (name == "keep" || name == "invert") IndependentReference(name == "invert");
            else {
                if (nativeStencilExport) {
                    std::puts("skipped: a dependent reference refusal requires a device without native stencil export");
                    return VulkanTestSkipped;
                }
                DependentReference(name);
                expectedRefusal = true;
            }
        } else {
            Require(argc == 1, "expected --case keep, invert, compare, back-compare, replace, fail-replace, depth-fail-replace, or partial-replace");
            UnusedStencilRefExport();
            IndependentReference(false);
            IndependentReference(true);
        }
        try { AgcDriverShutdown_nid_postfix(); }
        catch (const std::exception& error) {
            if (!expectedRefusal || std::string_view(error.what()).find("a pixel shader that exports the stencil reference needs VK_EXT_shader_stencil_export, which the device lacks") == std::string_view::npos) throw;
        }
        std::puts(expectedRefusal ? "a dependent stencil reference retained the native extension refusal" : "unused stencil reference draws and full color/stencil readbacks passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
