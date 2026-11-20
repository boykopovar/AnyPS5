#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Words = 16;
constexpr std::uint32_t ResultWord = 4;
constexpr std::int32_t Width = 4;
constexpr std::int32_t Height = 3;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Entry = 2;

alignas(256) constexpr std::array<std::uint32_t, 18> SampleCode{
    0x34020086, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xbf8c3f70, 0xf09c0f08, 0x00610802,
    0xbf8c3f70, 0xe0701010, 0x80000801, 0xe0701014, 0x80000901, 0xe0701018, 0x80000a01, 0xe070101c,
    0x80000b01, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> BorderPixelCode{
    0x7e0402f1u, 0x7e0602f1u, 0xf09c0f08u, 0x00610802u, 0xbf8c3f70u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 4096> Texels{};
alignas(256) std::array<std::uint32_t, 192> Tables{};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

using Color = std::array<float, 4>;

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void Submit(std::vector<std::uint32_t> commands, const char* what) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, std::string(what) + ": the submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

std::uint64_t Table(std::uint32_t index) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Tables.data() + index * 64u));
}

void SetEntry(std::uint32_t table, std::uint32_t entry, Color color) {
    for (std::uint32_t component = 0; component < 4u; ++component) Tables[table * 64u + entry * 4u + component] = std::bit_cast<std::uint32_t>(color[component]);
}

void SetBase(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::uint32_t low, std::uint64_t base) {
    Append(commands, opcode, {low, static_cast<std::uint32_t>(base >> 8u)});
    Append(commands, opcode, {low + 1u, static_cast<std::uint32_t>(base >> 40u) & 0xffu});
}

void FillTexture() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Width, Height, 1);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "border color table registers: the texture does not fit its storage");
    std::fill(Texels.begin(), Texels.end(), std::uint8_t{0xeeu});
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | ((static_cast<std::uint32_t>(Width - 1) & 3u) << 30u),
        (static_cast<std::uint32_t>(Width - 1) >> 2u) | (static_cast<std::uint32_t>(Height - 1) << 14u),
        0xfacu | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 16> UserData() {
    const auto buffer = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Buffer.data()));
    const std::array<std::uint32_t, 4> sampler{0x000080b6u, 0u, 0u, (3u << 30u) | Entry};
    const auto texture = TextureDescriptor();
    std::array<std::uint32_t, 16> words{static_cast<std::uint32_t>(buffer), static_cast<std::uint32_t>((buffer >> 32u) & 0xffffu), static_cast<std::uint32_t>(Buffer.size() * 4u), 0x31016facu};
    std::copy(texture.begin(), texture.end(), words.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), words.begin() + 12);
    return words;
}

void Expect(const char* what, std::array<float, 4> sample, Color expected, double tolerance) {
    for (std::uint32_t component = 0; component < 4u; ++component) {
        Require(std::fabs(static_cast<double>(sample[component]) - expected[component]) <= tolerance, std::string(what) + ": component " + std::to_string(component) + " is " + std::to_string(sample[component]) + ", expected " + std::to_string(expected[component]));
    }
}

void Dispatch(const char* what, std::initializer_list<std::pair<std::uint32_t, std::uint64_t>> bases, Color expected) {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Buffer[lane * Words] = std::bit_cast<std::uint32_t>(-0.5f);
        Buffer[lane * Words + 1u] = std::bit_cast<std::uint32_t>(-0.5f);
    }
    const auto kernel = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    std::vector<std::uint32_t> commands;
    for (const auto& [opcode, base] : bases) SetBase(commands, opcode, opcode == 0x79u ? 0x380u : 0x20u, base);
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(kernel >> 8u)});
    Append(commands, 0x76, {0x20d, static_cast<std::uint32_t>(kernel >> 40u) & 0xffu});
    Append(commands, 0x76, {0x207, Threads});
    Append(commands, 0x76, {0x208, 1});
    Append(commands, 0x76, {0x209, 1});
    Append(commands, 0x76, {0x213, 16u << 1u});
    const auto words = UserData();
    for (std::uint32_t k = 0; k < words.size(); ++k) Append(commands, 0x76, {0x240 + k, words[k]});
    Append(commands, 0x15, {1, 1, 1, 0x41});
    Submit(commands, what);
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Buffer.data()), Buffer.size() * 4u);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        std::array<float, 4> sample{};
        for (std::uint32_t component = 0; component < 4u; ++component) sample[component] = std::bit_cast<float>(Buffer[lane * Words + ResultWord + component]);
        Expect((std::string(what) + ", lane " + std::to_string(lane)).c_str(), sample, expected, 1.0 / 255.0);
    }
}

std::vector<std::uint32_t> Draw(std::initializer_list<std::pair<std::uint32_t, std::uint64_t>> bases) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(TriangleVertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(BorderPixelCode.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 16u << 1u},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [opcode, base] : bases) SetBase(commands, opcode, opcode == 0x79u ? 0x380u : 0x20u, base);
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    const auto words = UserData();
    for (std::uint32_t k = 0; k < words.size(); ++k) Append(commands, 0x76, {0x00c + k, words[k]});
    Append(commands, 0x2d, {3, 2});
    return commands;
}

void DrawExpecting(const char* what, std::initializer_list<std::pair<std::uint32_t, std::uint64_t>> bases, Color expected) {
    ColorMemory.fill(std::byte{0});
    Submit(Draw(bases), what);
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
    std::size_t covered = 0;
    for (std::size_t pixel = 0; pixel < ColorMemory.size() / 4u; ++pixel) {
        const auto* bytes = ColorMemory.data() + pixel * 4u;
        if (std::all_of(bytes, bytes + 4, [](std::byte value) { return value == std::byte{0}; })) continue;
        ++covered;
        std::array<float, 4> sample{};
        for (std::uint32_t component = 0; component < 4u; ++component) sample[component] = static_cast<float>(std::to_integer<unsigned>(bytes[component])) / 255.0f;
        Expect((std::string(what) + ", pixel " + std::to_string(pixel)).c_str(), sample, expected, 1.5 / 255.0);
    }
    Require(covered != 0, std::string(what) + ": the draw wrote no pixels");
}

void ExpectDrawFailure(const char* what, std::string_view reason) {
    std::vector<std::uint32_t> commands{0xc0011000u | (0x1au << 2u), 0u, 0u};
    const auto draw = Draw({});
    commands.insert(commands.end(), draw.begin(), draw.end());
    try {
        Submit(commands, what);
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string(what) + ": unexpected error: " + error.what());
        return;
    }
    throw std::runtime_error(std::string(what) + " was accepted");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        FillTexture();
        Registered(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
        Registered(PixelHeader, BorderPixelCode.data(), sizeof(BorderPixelCode), 1);
        const Color first{0.15f, 0.5f, 0.05f, 0.0f};
        const Color rewritten{0.25f, 0.75f, 1.0f, 0.5f};
        const Color other{1.0f, 0.0f, 0.0f, 1.0f};
        SetEntry(0, Entry, first);
        SetEntry(1, Entry, other);
        try {
            Dispatch("dispatch through TA_CS_BC_BASE_ADDR", {{0x79u, Table(0)}}, first);
        } catch (const std::exception& error) {
            if (std::string_view(error.what()).find("VK_EXT_custom_border_color") == std::string_view::npos) throw;
            std::puts("skipped, the device has no VK_EXT_custom_border_color with customBorderColorWithoutFormat");
            AgcDriverShutdown_nid_postfix();
            return VulkanTestSkipped;
        }
        Dispatch("dispatch with TA_BC_BASE_ADDR naming another table", {{0x79u, Table(0)}, {0x69u, Table(1)}}, first);
        SetEntry(0, Entry, rewritten);
        Dispatch("dispatch after the entry was rewritten, base kept from the last submission", {}, rewritten);
        DrawExpecting("draw through TA_BC_BASE_ADDR", {{0x69u, Table(1)}}, other);
        DrawExpecting("draw with TA_CS_BC_BASE_ADDR naming another table", {{0x69u, Table(0)}, {0x79u, Table(1)}}, rewritten);
        SetEntry(0, Entry, first);
        DrawExpecting("draw after the entry was rewritten", {{0x69u, Table(0)}}, first);
        ExpectDrawFailure("draw after a context clear dropped TA_BC_BASE_ADDR", "table base register was never written");
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& error) {
            Require(std::string_view(error.what()).find("table base register was never written") != std::string_view::npos, std::string("shutdown reported another failure: ") + error.what());
            std::puts("border color table register tests passed");
            return 0;
        }
        throw std::runtime_error("shutdown did not report the draw without a table base");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
