#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t Draws = 4352;
using ShaderInput = std::array<float, 4>;

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{0x7e000208u, 0x7e020280u, 0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0xf4000100u, 0xfa000000u, 0x7e000204u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> ColorMemory{};
alignas(16) std::array<ShaderInput, Draws> Sources{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
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
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> EveryDraw(std::span<const ShaderInput> inputs = Sources, std::uint32_t scissorRight = 64) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
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
        {0x3b8, 0x9000000u}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40000u | scissorRight},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 2u << 1u},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    for (const auto& source : inputs) {
        const auto address = reinterpret_cast<std::uintptr_t>(source.data());
        Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(address)});
        Append(commands, 0x76, {0x00d, static_cast<std::uint32_t>(address >> 32u)});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

AgcDriver::DriverDetail::DrawEntryCounters Pass(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
}

void RegisterShaders() {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
}

void CacheDistinctDraws(bool testMetadataBudget) {
    RegisterShaders();
    for (std::size_t i = 0; i < Sources.size(); ++i) Sources[i] = {static_cast<float>(i + 1) / static_cast<float>(Draws + 1), 0.0f, 0.0f, 1.0f};
    auto commands = EveryDraw();
    const auto start = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto seen = Pass(commands);
    const auto inserted = Pass(commands);
    const auto footprint = AgcDriver::DriverDetail::Driver::Get().DrawCacheSize();
    std::cout << footprint.entries << " distinct draws held " << footprint.variantBytes << " accounted bytes after insertion\n";
    if (testMetadataBudget) {
        Require(inserted.byteEvictions > seen.byteEvictions, "retained shader metadata did not trigger a draw-cache byte eviction");
        Require(footprint.variantBytes <= AgcDriver::DriverDetail::Driver::DrawCacheBytes(), "the draw cache exceeds its byte budget");
        return;
    }
    const auto reused = Pass(commands);
    Require(reused.lookups - inserted.lookups == Draws, "the third pass made " + std::to_string(reused.lookups - inserted.lookups) + " draw-cache lookups, expected " + std::to_string(Draws));
    Require(reused.hits - inserted.hits == Draws, "the third pass over " + std::to_string(Draws) + " distinct draws made " + std::to_string(reused.hits - inserted.hits) + " draw-cache hits and " + std::to_string(reused.evictions - inserted.evictions) + " evictions; the default entry cap does not hold them");
    Require(inserted.inserts - start.inserts >= Draws - 1, "the first two passes inserted " + std::to_string(inserted.inserts - start.inserts) + " entries for " + std::to_string(Draws) + " distinct draws (" + std::to_string(seen.inserts - start.inserts) + " in the first)");
    Require(AgcDriver::DriverDetail::Driver::Get().DrawCacheSize().entries >= Draws, "the draw cache holds " + std::to_string(AgcDriver::DriverDetail::Driver::Get().DrawCacheSize().entries) + " entries after " + std::to_string(Draws) + " distinct draws");
}

void SharedResultsAreChargedOnce() {
    RegisterShaders();
    Sources.front() = {1.0f, 0.0f, 0.0f, 1.0f};
    const auto input = std::span<const ShaderInput>{Sources}.first(1);
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    const auto populate = [&](std::uint32_t scissorRight) {
        auto commands = EveryDraw(input, scissorRight);
        Pass(commands);
        Pass(commands);
        return driver.DrawCacheSize();
    };
    const auto one = populate(64);
    const auto two = populate(48);
    const auto three = populate(32);
    Require(one.entries == 1 && two.entries == 2 && three.entries == 2, "expected two cached draws and one eviction");
    std::cout << "Draw-cache bytes with one, two and two entries after eviction: " << one.variantBytes << ", " << two.variantBytes << ", " << three.variantBytes << '\n';
    Require(two.variantBytes > one.variantBytes && two.variantBytes < 2 * one.variantBytes, "retained shader metadata must be charged once for shared draw results");
    Require(three.variantBytes == two.variantBytes, "evicting one shared result reference changed the shader metadata charge");
}

}

int main(int argc, char** argv) {
    try {
        const auto test = argc == 2 ? std::string_view(argv[1]) : std::string_view{};
        Require(argc == 1 || test == "--metadata-budget" || test == "--shared-result", "unknown draw cache capacity test argument");
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (test == "--shared-result") SharedResultsAreChargedOnce();
        else CacheDistinctDraws(test == "--metadata-budget");
        AgcDriverShutdown_nid_postfix();
        if (test.empty()) std::cout << Draws << " distinct draws stayed in the draw cache under the default entry cap\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
