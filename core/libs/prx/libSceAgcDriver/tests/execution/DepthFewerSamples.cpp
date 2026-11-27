#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
    std::vector<std::pair<std::uint32_t, std::uint32_t>> extra{};
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
    for (const auto& [offset, value] : options.extra) Append(commands, 0x69, {offset, value});
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

constexpr std::uint8_t CoverStencil = 0x80;
constexpr std::uint32_t Equal = 2u << 8u;
constexpr std::uint32_t Rgba8Blended = 0x8028u;
constexpr std::uint32_t CompressedRgba8 = 0xe028u;
constexpr std::uint32_t FourFragments = 0x12000u;
constexpr std::uint32_t TwoFragments = 0xa000u;
constexpr std::uint32_t RenderTargetTiling = 0x4dc6c000u;
constexpr std::uint32_t AlphaBlend = 0x40000504u;
constexpr std::size_t ColorBlockBytes = 65536;
constexpr std::size_t ColorBytes = ColorBlockBytes * 8;
std::array<std::byte, ColorBytes * 2 + ColorBlockBytes * 4> ColorStorage{};
std::byte* const PlainColor = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(ColorStorage.data()) + ColorBlockBytes - 1) & ~(ColorBlockBytes - 1));
std::byte* const EqaaColor = PlainColor + ColorBytes;
std::byte* const EqaaFmask = EqaaColor + ColorBytes;
std::byte* const EqaaCmask = EqaaFmask + ColorBlockBytes;
std::byte* const Resolved = EqaaCmask + ColorBlockBytes;

using Color = std::array<float, 4>;
alignas(256) std::array<std::array<std::uint32_t, 64>, 4> ConstantCode{};
alignas(8) std::array<Header, 4> ConstantHeaders{};
constexpr std::array<Color, 4> Constants{{{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.2f, 0.4f, 0.6f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.5f}}};
enum ConstantProgram : std::size_t { Black, White, Base, HalfRed };

void RegisterConstantPrograms() {
    const std::array<ShaderRegister, 3> pixelContext{{{0x203, 0x800}, {0x1c4, 9}, {0x1b6, 0}}};
    for (std::size_t program = 0; program < Constants.size(); ++program) {
        auto& code = ConstantCode[program];
        std::size_t at = 0;
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
            code[at++] = std::bit_cast<std::uint32_t>(Constants[program][channel]);
        }
        code[at++] = 0xf800180fu;
        code[at++] = 0x07060504u;
        code[at++] = 0xbf810000u;
        Registered(ConstantHeaders[program], code.data(), at * sizeof(std::uint32_t), 1);
        AgcDriverResolveShaderAbi_nid_postfix(&ConstantHeaders[program].shader, pixelContext, {});
    }
}

struct AnchorTarget {
    std::byte* color;
    std::uint32_t info;
    std::uint32_t attrib;
    std::byte* fmask;
    std::byte* cmask;
    const char* what;
};

std::vector<std::pair<std::uint32_t, std::uint32_t>> TargetRegisters(const AnchorTarget& target) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> registers{{0x31d, target.attrib}, {0x3b8, RenderTargetTiling}};
    if (target.fmask != nullptr) {
        const auto fmask = reinterpret_cast<std::uintptr_t>(target.fmask);
        registers.insert(registers.end(), {{0x321, static_cast<std::uint32_t>(fmask >> 8u)}, {0x3a0, static_cast<std::uint32_t>(fmask >> 40u)}});
    }
    if (target.cmask != nullptr) {
        const auto cmask = reinterpret_cast<std::uintptr_t>(target.cmask);
        registers.insert(registers.end(), {{0x31f, static_cast<std::uint32_t>(cmask >> 8u)}, {0x398, static_cast<std::uint32_t>(cmask >> 40u)}});
    }
    return registers;
}

std::uint32_t Anchor(std::uint32_t locations, std::uint32_t sample) {
    if (sample < 2u) return sample;
    const auto at = Location(locations, sample);
    const auto distance = [&](std::uint32_t stored) {
        const auto anchor = Location(locations, stored);
        return (at[0] - anchor[0]) * (at[0] - anchor[0]) + (at[1] - anchor[1]) * (at[1] - anchor[1]);
    };
    return distance(1) < distance(0) ? 1u : 0u;
}

std::array<std::uint8_t, Width * Height> CopiedStencil(std::uint32_t sample, const std::string& what) {
    CopyTarget.fill(Untouched);
    std::vector<std::uint32_t> commands;
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(CopyTarget.data()), .colorInfo = R8Uint, .targetMask = 1, .renderControl = 0x08u | (sample << 8u), .coverAll = true, .aaConfig = TwoSamples, .msaaEnable = false});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, what + ": the DB stencil copy of sample " + std::to_string(sample) + " was not submitted");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(CopyTarget.data()), sizeof(CopyTarget));
    std::array<std::uint8_t, Width * Height> copied{};
    for (std::size_t index = 0; index < copied.size(); ++index) copied[index] = CopyTarget[(index / Width) * CopyPitch + index % Width];
    return copied;
}

std::array<std::uint8_t, 4> ResolvedTexel(std::size_t index) {
    static const AgcDriver::Graphics::ColorTargetLayout layout(Width, Height, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4u);
    const auto* texel = Resolved + layout.Offset(static_cast<std::uint32_t>(index % Width), static_cast<std::uint32_t>(index / Width));
    return {std::to_integer<std::uint8_t>(texel[0]), std::to_integer<std::uint8_t>(texel[1]), std::to_integer<std::uint8_t>(texel[2]), std::to_integer<std::uint8_t>(texel[3])};
}

std::uint8_t Unorm(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

void CoverFollowsAnchoredStencil(std::uint32_t locations, const std::string& what) {
    std::fill(PlaneStorage.begin(), PlaneStorage.end(), std::byte{0});
    {
        std::vector<std::uint32_t> commands;
        AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(Unused.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = BaseStencil | 0xff00u | 0xff0000u, .coverAll = true, .aaConfig = TwoSamples, .locations = StandardTwoSampleLocations});
        AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(Unused.data()), .targetMask = 0, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = CoverStencil | 0xff00u | 0xff0000u, .shiftX = TestShift, .aaConfig = FourSamples, .eqaa = EqaaAnchors, .locations = locations});
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, what + ": the stencil fill and the four-sample stencil draw were not submitted");
        AgcDriverWaitIdle_nid_postfix();
    }
    std::array<std::array<bool, 2>, Width * Height> stored{};
    std::array<std::size_t, 3> kinds{};
    for (std::size_t index = 0; index < stored.size(); ++index) {
        for (std::uint32_t sample = 0; sample < 2; ++sample) {
            const auto [x, y] = Location(locations, sample);
            stored[index][sample] = InsideTestDraw(static_cast<std::uint32_t>(index % Width), static_cast<std::uint32_t>(index / Width), x, y, what);
        }
        ++kinds[stored[index][0] == stored[index][1] ? (stored[index][0] ? 0u : 1u) : 2u];
    }
    Require(kinds[0] != 0 && kinds[1] != 0 && kinds[2] != 0, what + ": the stencil draw left " + std::to_string(kinds[0]) + " texels with both stored samples drawn, " + std::to_string(kinds[1]) + " with neither and " + std::to_string(kinds[2]) + " with one, expected some of each");
    std::array<std::array<std::uint8_t, Width * Height>, 2> before{};
    for (std::uint32_t sample = 0; sample < 2; ++sample) {
        before[sample] = CopiedStencil(sample, what);
        for (std::size_t index = 0; index < stored.size(); ++index) Require(before[sample][index] == (stored[index][sample] ? CoverStencil : BaseStencil), what + ": stored stencil sample " + std::to_string(sample) + " holds " + std::to_string(before[sample][index]) + " at texel " + std::to_string(index) + " before the color draws");
    }
    std::array<std::uint32_t, 4> anchors{};
    for (std::uint32_t sample = 0; sample < 4; ++sample) anchors[sample] = Anchor(locations, sample);
    const std::array<AnchorTarget, 2> targets{{{PlainColor, Rgba8Blended, FourFragments, nullptr, nullptr, "a four-sample color target"}, {EqaaColor, CompressedRgba8, TwoFragments, EqaaFmask, EqaaCmask, "a four-sample two-fragment color target with FMASK and CMASK"}}};
    for (const auto& target : targets) {
        for (const bool blended : {false, true}) {
            const auto base = blended ? Base : Black;
            const auto& baseColor = Constants[base];
            const auto& source = Constants[blended ? HalfRed : White];
            std::array<std::uint8_t, 4> unchanged{};
            std::array<std::uint8_t, 4> covered{};
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                unchanged[channel] = Unorm(baseColor[channel]);
                const auto destination = unchanged[channel] / 255.0f;
                covered[channel] = blended ? Unorm(source[channel] * source[3] + destination * (1.0f - source[3])) : Unorm(source[channel]);
            }
            for (std::uint32_t only = 0; only <= 4; ++only) {
                const auto mask = only == 4 ? 0xfu : 1u << only;
                const auto label = what + ", " + target.what + (blended ? ", alpha-blended" : ", constant color") + (only == 4 ? ", all samples" : ", sample " + std::to_string(only) + " alone");
                auto registers = TargetRegisters(target);
                std::vector<std::uint32_t> commands;
                AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(target.color), .colorInfo = target.info, .coverAll = true, .aaConfig = FourSamples, .locations = locations, .pixel = ConstantCode[base].data(), .extra = registers});
                auto cover = registers;
                cover.insert(cover.end(), {{0x1e0, blended ? AlphaBlend : 0u}, {0x30e, mask | (mask << 16u)}, {0x30f, mask | (mask << 16u)}});
                AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(target.color), .colorInfo = target.info, .depthControl = StencilEnable | Equal, .stencilRefMask = CoverStencil | 0xff00u | 0xff0000u, .coverAll = true, .aaConfig = FourSamples, .eqaa = EqaaAnchors, .locations = locations, .pixel = ConstantCode[blended ? HalfRed : White].data(), .extra = cover});
                auto resolve = registers;
                const auto destination = reinterpret_cast<std::uintptr_t>(Resolved);
                resolve.insert(resolve.end(), {{0x202, 0xcc0030u}, {0x327, static_cast<std::uint32_t>(destination >> 8u)}, {0x391, static_cast<std::uint32_t>(destination >> 40u)}, {0x32a, 0u}, {0x32b, Rgba8Blended}, {0x32c, 0u}, {0x3b1, ((Width - 1u) << 14u) | (Height - 1u)}, {0x3b9, RenderTargetTiling}});
                AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(target.color), .colorInfo = target.info, .targetMask = 0xff, .coverAll = true, .aaConfig = FourSamples, .locations = locations, .extra = resolve});
                std::fill(Resolved, Resolved + ColorBlockBytes, std::byte{Untouched});
                Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
                Require(sceAgcDriverSubmitDcb(&packet) == 0, label + ": the base fill, the stencil-tested color draw and the CB resolve were not submitted");
                AgcDriverWaitIdle_nid_postfix();
                AgcDriver::GuestMemory::FlushGpuWrites(destination, ColorBlockBytes);
                for (std::size_t index = 0; index < stored.size(); ++index) {
                    std::uint32_t passed = 0;
                    std::string passing;
                    for (std::uint32_t sample = 0; sample < 4; ++sample) {
                        if (((mask >> sample) & 1u) == 0 || !stored[index][anchors[sample]]) continue;
                        ++passed;
                        passing += std::to_string(sample);
                    }
                    const auto texel = ResolvedTexel(index);
                    for (std::uint32_t channel = 0; channel < 4; ++channel) {
                        const auto expected = (static_cast<float>(4u - passed) * unchanged[channel] + static_cast<float>(passed) * covered[channel]) / 4.0f;
                        Require(std::fabs(texel[channel] - expected) <= 2.0f, label + ": the resolve holds " + std::to_string(texel[channel]) + " in channel " + std::to_string(channel) + " of texel " + std::to_string(index) + ", expected " + std::to_string(expected) + " from coverage samples {" + passing + "} passing the stencil test (stored samples " + (stored[index][0] ? "0 drawn" : "0 not drawn") + ", " + (stored[index][1] ? "1 drawn" : "1 not drawn") + ")");
                    }
                }
            }
        }
    }
    for (std::uint32_t sample = 0; sample < 2; ++sample) Require(CopiedStencil(sample, what) == before[sample], what + ": stored stencil sample " + std::to_string(sample) + " changed under the color draws that only test it");
}

constexpr std::string_view ReplacingCoverRefusal = "a depth target with fewer samples than the rasterizer is unsupported for color-writing draws that write depth or stencil";

void ReplacingColorDrawRefusedThenShutdown() {
    std::vector<std::uint32_t> commands;
    const AnchorTarget target{PlainColor, Rgba8Blended, FourFragments, nullptr, nullptr, "a four-sample color target"};
    AppendDraw(commands, {.color = reinterpret_cast<std::uintptr_t>(target.color), .colorInfo = target.info, .depthControl = StencilEnable | Always, .stencilOperations = ReplaceWithTest, .stencilRefMask = CoverStencil | 0xff00u | 0xff0000u, .coverAll = true, .aaConfig = FourSamples, .eqaa = EqaaAnchors, .locations = 0x22eea66au, .pixel = ConstantCode[White].data(), .extra = TargetRegisters(target)});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    std::string refusal;
    try {
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the color draw that replaces the stencil of a two-sample target under a four-sample rasterizer was not submitted");
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(ReplacingCoverRefusal) != std::string::npos, "a color draw that replaces the stencil of a two-sample target under a four-sample rasterizer was not refused by name: " + refusal);
    try {
        AgcDriverShutdown_nid_postfix();
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find(ReplacingCoverRefusal) == std::string_view::npos) throw;
    }
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

int main(int argc, char** argv) {
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
        RegisterConstantPrograms();
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(PlainColor, ColorBytes * 2 + ColorBlockBytes * 3, true, true, true);
        }
        if (argc == 2 && std::string_view(argv[1]) == "--stencil-writing-color") {
            ReplacingColorDrawRefusedThenShutdown();
            std::puts("a color draw that replaces the stencil of a two-sample target under a four-sample rasterizer was refused by name");
            return 0;
        }
        std::fill(EqaaCmask, EqaaCmask + ColorBlockBytes, std::byte{0xff});
        CoverFollowsAnchoredStencil(0x622ae6aeu, "the standard four-sample locations");
        CoverFollowsAnchoredStencil(0x4cc4a66au, "four coverage samples at (-6,6), (6,-6), (4,-4) and (-4,4), each nearer one stored sample");
        CoverFollowsAnchoredStencil(0x22eea66au, "four coverage samples at (-6,6), (6,-6), (-2,-2) and (2,2) (PPSA01317), whose samples 2 and 3 are assumed to take the lower stored sample of a tie");
        MemoryWritingPixelShaderRefusedThenShutdown();
        std::puts("a two-sample stencil target under a four-sample rasterizer kept the coverage of the first two samples for draws that write no color, gave color-writing draws each coverage sample's nearest stored sample, and refused a pixel shader that stores to memory");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
