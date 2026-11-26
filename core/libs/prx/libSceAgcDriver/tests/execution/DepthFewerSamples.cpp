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
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint8_t DrawnStencil = 0x5a;
constexpr std::uint8_t BaseStencil = 0x25;
constexpr std::uint8_t Untouched = 0xc3;
constexpr float TestShift = -8.0f;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 4> WhitePixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 11> StorePixelCode{};
alignas(256) std::array<std::uint32_t, 4> StoreTarget{};
constexpr std::size_t CopyPitch = 256;
alignas(256) std::array<std::uint8_t, CopyPitch * Height> CopyTarget{};
alignas(256) std::array<std::uint8_t, Width * Height * 4> Unused{};
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
alignas(8) Header StorePixelHeader{};

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
constexpr std::uint32_t TwoSamples = 0x00100001u;
constexpr std::uint32_t FourSamples = 0x0020c002u;
constexpr std::uint32_t StandardTwoSampleLocations = 0xcc44u;
constexpr std::uint32_t EqaaAnchors = 0x00052202u;

struct DrawOptions {
    std::uintptr_t color = 0;
    std::uint32_t colorInfo = Rgba8Unorm;
    std::uint32_t targetMask = 0xf;
    std::uint32_t renderControl = 0;
    std::uint32_t depthControl = 0;
    std::uint32_t stencilOperations = 0;
    std::uint32_t stencilRefMask = 0;
    float shiftX = 0.0f;
    bool coverAll = false;
    std::uint32_t aaConfig = 0;
    std::uint32_t eqaa = 0;
    std::uint32_t locations = 0;
    bool msaaEnable = true;
    const std::uint32_t* pixel = WhitePixelCode.data();
    std::uint32_t shaderControl = 0x800;
};

void AppendDraw(std::vector<std::uint32_t>& commands, const DrawOptions& options) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(options.pixel);
    const auto depth = reinterpret_cast<std::uintptr_t>(DepthMemory);
    const auto stencil = reinterpret_cast<std::uintptr_t>(StencilMemory);
    const bool multisampled = options.aaConfig != 0 && options.msaaEnable;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, 2}, {0x1b4, 2}, {0x1b6, 0}, {0x207, 0}, {0x200, options.depthControl}, {0x201, options.eqaa}, {0x203, options.shaderControl},
        {0x2dc, 0xaa00}, {0x2f8, options.aaConfig}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffffu}, {0x30f, 0xffffffffu},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, options.targetMask}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 9}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, options.colorInfo},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, (Height << 16u) | Width},
        {0x81, 0x80000000}, {0x82, (Height << 16u) | Width},
        {0x90, 0x80000000}, {0x91, (Height << 16u) | Width},
        {0x94, 0x80000000}, {0x95, (Height << 16u) | Width},
        {0x10f, std::bit_cast<std::uint32_t>(options.coverAll ? static_cast<float>(Width) : Width / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>((options.coverAll ? static_cast<float>(Width) : Width / 2.0f) + options.shiftX)},
        {0x111, std::bit_cast<std::uint32_t>(options.coverAll ? -64.0f : -(Height / 2.0f))}, {0x112, std::bit_cast<std::uint32_t>(options.coverAll ? Height - 64.0f : Height / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, options.locations}, {0x302, options.locations}, {0x306, options.locations}, {0x30a, options.locations},
        {0x318, static_cast<std::uint32_t>(options.color >> 8u)}, {0x390, static_cast<std::uint32_t>(options.color >> 40u)}, {0x292, multisampled ? 3u : 2u},
        {0x000, options.renderControl}, {0x002, 0}, {0x007, ((Height - 1u) << 16u) | (Width - 1u)},
        {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
        {0x010, 0x80000187u}, {0x011, 0x20000181u},
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

std::array<int, 2> Location(std::uint32_t locations, std::uint32_t sample) {
    const auto bits = (locations >> (8u * sample)) & 0xffu;
    return {static_cast<int>(bits & 7u) - static_cast<int>(bits & 8u), static_cast<int>((bits >> 4u) & 7u) - static_cast<int>((bits >> 4u) & 8u)};
}

bool InsideTestDraw(std::uint32_t column, std::uint32_t row, int x, int y, const std::string& what) {
    const auto sampleX = 32 * static_cast<int>(column) + 16 + 2 * x;
    const auto sampleY = 32 * static_cast<int>(row) + 16 + 2 * y;
    const auto left = 32 * static_cast<int>(TestShift);
    const auto bottom = 32 * static_cast<int>(Height);
    const auto edge = sampleX - left - 16 * sampleY;
    Require(sampleX != left && sampleY != bottom && edge != 0, what + ": coverage sample (" + std::to_string(x) + ", " + std::to_string(y) + ")/16 of texel " + std::to_string(row * Width + column) + " lies on an edge of the four-sample draw");
    return sampleX > left && sampleY < bottom && edge < 0;
}

void StoredSamplesFollowLeadingCoverage(std::uint32_t locations, const std::string& what) {
    std::array<std::size_t, 2> drawn{};
    for (std::uint32_t sample = 0; sample < 2; ++sample) {
        const auto [x, y] = Location(locations, sample);
        std::fill(PlaneStorage.begin(), PlaneStorage.end(), std::byte{0});
        CopyTarget.fill(Untouched);
        std::vector<std::uint32_t> commands;
        AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(Unused.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = BaseStencil | 0xff00u | 0xff0000u, .coverAll = true, .aaConfig = TwoSamples, .locations = StandardTwoSampleLocations});
        AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(Unused.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = DrawnStencil | 0xff00u | 0xff0000u, .shiftX = TestShift, .aaConfig = FourSamples, .eqaa = EqaaAnchors, .locations = locations});
        AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(CopyTarget.data()), .colorInfo = R8Uint, .targetMask = 1, .renderControl = 0x08u | (sample << 8u), .coverAll = true, .aaConfig = TwoSamples, .msaaEnable = false});
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, what + ": the stencil fill, the four-sample stencil draw and the DB stencil copy of sample " + std::to_string(sample) + " were not submitted");
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(CopyTarget.data()), sizeof(CopyTarget));
        std::size_t base = 0;
        for (std::size_t index = 0; index < Width * Height; ++index) {
            const bool covered = InsideTestDraw(static_cast<std::uint32_t>(index % Width), static_cast<std::uint32_t>(index / Width), x, y, what);
            const auto expected = covered ? DrawnStencil : BaseStencil;
            const auto copy = CopyTarget[(index / Width) * CopyPitch + index % Width];
            Require(copy == expected, what + ": stored stencil sample " + std::to_string(sample) + " holds " + std::to_string(copy) + " at texel " + std::to_string(index) + ", expected " + std::to_string(expected) + (covered ? " (coverage sample " + std::to_string(sample) + " at (" + std::to_string(x) + ", " + std::to_string(y) + ")/16 is inside the four-sample draw)" : " (coverage sample " + std::to_string(sample) + " is outside the four-sample draw)"));
            drawn[sample] += covered;
            base += !covered;
        }
        Require(drawn[sample] != 0 && base != 0, what + ": stored stencil sample " + std::to_string(sample) + " holds the drawn value in " + std::to_string(drawn[sample]) + " texels and the fill value in " + std::to_string(base) + ", expected some of each");
    }
    Require(drawn[0] != drawn[1], what + ": both stored stencil samples were drawn in " + std::to_string(drawn[0]) + " texels, expected the two coverage samples to differ");
}

constexpr std::string_view WritingPixelShaderRefusal = "rasterized at the leading samples of a target with fewer samples than the rasterizer is unsupported when its pixel shader writes memory";

bool Refusal(const std::exception& error) {
    return std::string_view(error.what()).find(WritingPixelShaderRefusal) != std::string_view::npos;
}

void MemoryWritingPixelShaderRefusedThenShutdown() {
    std::fill(PlaneStorage.begin(), PlaneStorage.end(), std::byte{0});
    std::vector<std::uint32_t> commands;
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(Unused.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = DrawnStencil | 0xff00u | 0xff0000u, .shiftX = TestShift, .aaConfig = FourSamples, .eqaa = EqaaAnchors, .locations = 0xa66acc44u, .pixel = StorePixelCode.data(), .shaderControl = 0xc00});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    bool refused = false;
    try {
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the four-sample stencil draw with a storing pixel shader was not submitted");
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        if (!Refusal(error)) throw;
        refused = true;
    }
    Require(refused, "a four-sample stencil draw into a two-sample stencil target whose pixel shader stores to memory with EXEC_ON_NOOP was not refused");
    try {
        AgcDriverShutdown_nid_postfix();
    } catch (const std::exception& error) {
        if (!Refusal(error)) throw;
    }
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
        const auto store = reinterpret_cast<std::uintptr_t>(StoreTarget.data());
        StorePixelCode = {0x7e0002ffu, static_cast<std::uint32_t>(store), 0x7e0202ffu, static_cast<std::uint32_t>(store >> 32u), 0x7e0402ffu, 0x5au, 0xdc708000u, 0x007d0200u, 0xf800180fu, 0x02020202u, 0xbf810000u};
        Registered(StorePixelHeader, StorePixelCode.data(), sizeof(StorePixelCode), 1);
        AgcDriverResolveShaderAbi_nid_postfix(&StorePixelHeader.shader, pixelContext, {});
        StoredSamplesFollowLeadingCoverage(0xa66acc44u, "four coverage samples whose first two sit at the standard two-sample locations");
        try {
            StoredSamplesFollowLeadingCoverage(0x22eea66au, "four coverage samples at (-6,6), (6,-6), (-2,-2) and (2,2)");
        } catch (const std::exception& error) {
            if (std::string_view(error.what()).find("custom sample locations require VK_EXT_sample_locations support") == std::string_view::npos) throw;
            std::puts("the custom two-sample locations case is skipped, the device lacks VK_EXT_sample_locations for two samples");
        }
        MemoryWritingPixelShaderRefusedThenShutdown();
        std::puts("a two-sample stencil target under a four-sample rasterizer kept the coverage of the first two samples and refused a pixel shader that stores to memory");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
