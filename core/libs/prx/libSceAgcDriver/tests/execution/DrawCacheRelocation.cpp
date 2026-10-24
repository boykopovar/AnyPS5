#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "Optimization/SrtWalker.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DrawMiss;

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{0x7e000208u, 0x7e020280u, 0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> SecondVertexCode = VertexCode;
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0xf4000100u, 0xfa000000u, 0x7e000204u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 16> ForwardedPixelCode{
    0xf4040080u, 0xfa000000u, 0xbf8cc07fu, 0xf4000101u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0xd5480001u,
    0x02050800u, 0x7e020d01u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(16) std::array<float, 4> Other{0.5f, 0.0f, 0.0f, 0.0f};
alignas(16) std::array<float, 4> ForwardedData{};
alignas(32) std::array<std::array<std::uint64_t, 4>, 6> AlignedTables{};
alignas(32) std::array<std::uint64_t, 4> OffsetTable{};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) Header SecondVertexHeader{};
alignas(8) Header TriangleVertexHeader{};
alignas(8) Header ForwardedPixelHeader{};

const Shader* Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    return &header.shader;
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> DrawFrom(std::initializer_list<const void*> sources, const std::uint32_t* vertexCode = VertexCode.data(), const std::uint32_t* pixelCode = PixelCode.data()) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(vertexCode);
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
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
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 2u << 1u},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    for (const auto* source : sources) {
        const auto address = reinterpret_cast<std::uintptr_t>(source);
        Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(address)});
        Append(commands, 0x76, {0x00d, static_cast<std::uint32_t>(address >> 32u)});
        Append(commands, 0x2d, {3, 2});
    }
    return commands;
}

void MovedDataHitsTheCache(bool libraries) {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const auto draw = [&](std::initializer_list<const void*> sources) {
        auto commands = DrawFrom(sources);
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
        AgcDriverWaitIdle_nid_postfix();
        return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    };
    static std::array<std::array<float, 4>, 8> same{};
    for (auto& entry : same) entry = {1.0f, 0.0f, 0.0f, 0.0f};
    AgcDriver::DriverDetail::DrawEntryCounters counters{};
    for (std::size_t i = 0; i < 4; ++i) counters = draw({same[i].data()});
    Require(counters.lookups == 4 && counters.hits == 0 && counters.inserts == 2, mode + ": the draws that teach the moved pointer and its contents made " + std::to_string(counters.hits) + " hits and " + std::to_string(counters.inserts) + " inserts, expected 0 and 2");
    for (std::size_t i = 4; i < same.size(); ++i) counters = draw({same[i].data()});
    Require(counters.lookups == 8 && counters.hits == 4 && counters.inserts == 2, mode + ": draws whose data moved again did not reuse the cached draw (" + std::to_string(counters.hits) + " hits in " + std::to_string(counters.lookups) + " lookups)");
    const auto other = draw({Other.data()});
    Require(other.lookups == 9 && other.hits == 4, mode + ": a draw whose moved data holds another value reused the cached draw");
    const auto again = draw({same[0].data()});
    Require(again.lookups == 10 && again.hits == 5, mode + ": a draw at an address seen before did not reuse the cached draw (" + std::to_string(again.hits) + " hits)");
}

void SplitRunsNameTheirSharedWords() {
    alignas(16) static std::array<std::uint32_t, 4> data{1u, 2u, 3u, 4u};
    auto table = std::make_shared<ShaderRecompiler::SrtGuestWords>();
    table->words = {5u, 6u, 7u, 8u};
    const auto dataAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data.data()));
    const auto tableAddress = dataAddress + 0x1000u;
    const std::array<ShaderRecompiler::MemoryRegion, 2> regions{{{dataAddress, std::as_bytes(std::span(data))}, {tableAddress, std::as_bytes(std::span(table->words))}}};
    ShaderRecompiler::SrtUserDataUse use;
    use.asBase = 0x3u;
    use.reads = {{dataAddress, 0}, {dataAddress + 8u, 1}};
    const std::array<std::shared_ptr<const ShaderRecompiler::SrtGuestWords>, 1> supplied{table};
    AgcDriver::DriverDetail::DispatchVariant variant;
    AgcDriver::DriverDetail::Driver::tagVariantRuns(variant, regions, use, true, 0, supplied);
    Require(variant.runs.size() > regions.size(), "two user-data bases into one region did not split it into several runs");
    Require(variant.shared.size() == 1u && variant.shared.front().regions == 1u, "the table region's words were not recorded as one shared run");
    const auto& shared = variant.shared.front();
    std::size_t first = 0;
    for (std::size_t run = 0; run < shared.region && run < variant.runs.size(); ++run) first += static_cast<std::size_t>((variant.runs[run].second - variant.runs[run].first) / sizeof(std::uint32_t));
    Require(shared.region < variant.runs.size() && variant.runs[shared.region].first == tableAddress && first == shared.first, "the shared words of a variant whose runs were split for relocation name run " + std::to_string(shared.region) + " of " + std::to_string(variant.runs.size()) + ", not the table's run " + std::to_string(variant.runs.size() - 1u));
}

void UnrelocatedAliasesStayOut(bool libraries) {
    Registered(SecondVertexHeader, SecondVertexCode.data(), sizeof(SecondVertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    const auto draw = [&](const float* source) {
        auto commands = DrawFrom({source}, SecondVertexCode.data());
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
        AgcDriverWaitIdle_nid_postfix();
        return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    };
    alignas(16) static std::array<float, 4> repeated{1.0f, 0.0f, 0.0f, 0.0f};
    alignas(16) static std::array<std::array<std::array<float, 4>, 2>, 12> twice{};
    for (std::size_t i = 0; i < twice.size(); ++i) {
        for (auto& entry : twice[i]) entry = {0.25f + 0.03125f * static_cast<float>(i), 0.0f, 0.0f, 0.0f};
    }
    draw(repeated.data());
    const auto start = draw(repeated.data());
    AgcDriver::DriverDetail::DrawEntryCounters counters = start;
    for (const auto& pair : twice) {
        for (const auto& entry : pair) draw(entry.data());
        counters = draw(repeated.data());
    }
    const auto inserts = counters.inserts - start.inserts;
    Require(inserts <= AgcDriver::DriverDetail::DrawRelocationTrial::ProbationAdmissions, mode + ": draws whose alias repeats but never relocates were cached (" + std::to_string(inserts) + " inserts for " + std::to_string(twice.size()) + " aliases)");
    Require(counters.hits - start.hits == twice.size(), mode + ": the exact key of a repeated draw lost its hits next to draws whose alias never relocates (" + std::to_string(counters.hits - start.hits) + " hits in " + std::to_string(twice.size()) + " draws)");
}

void MovedForwardedWordReachesStageDataHit(bool libraries) {
    Registered(TriangleVertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Registered(ForwardedPixelHeader, ForwardedPixelCode.data(), sizeof(ForwardedPixelCode), 1);
    const auto mode = std::string(libraries ? "with" : "without") + " graphics pipeline libraries";
    constexpr std::byte Kept{0x40};
    const auto draw = [&](const void* table) {
        ColorMemory.fill(Kept);
        auto commands = DrawFrom({table}, TriangleVertexCode.data(), ForwardedPixelCode.data());
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
        return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    };
    const auto pixels = [&](std::array<std::uint8_t, 4> expected) {
        std::size_t drawn = 0;
        for (std::size_t at = 0; at < ColorMemory.size(); at += 4) {
            const std::array<std::uint8_t, 4> pixel{static_cast<std::uint8_t>(ColorMemory[at]), static_cast<std::uint8_t>(ColorMemory[at + 1]), static_cast<std::uint8_t>(ColorMemory[at + 2]), static_cast<std::uint8_t>(ColorMemory[at + 3])};
            if (pixel == std::array<std::uint8_t, 4>{0x40, 0x40, 0x40, 0x40}) continue;
            if (pixel != expected) return "pixel " + std::to_string(at / 4) + " is " + std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) + "," + std::to_string(pixel[3]) + ", expected " + std::to_string(expected[0]) + "," + std::to_string(expected[1]) + "," + std::to_string(expected[2]) + "," + std::to_string(expected[3]);
            ++drawn;
        }
        return drawn == 0 ? std::string("the draw wrote no pixel") : std::string();
    };
    const auto data = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ForwardedData.data()));
    for (auto& table : AlignedTables) table[0] = data;
    OffsetTable[2] = data;
    ForwardedData = {1.0f, 0.0f, 0.0f, 0.0f};
    const auto start = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    AgcDriver::DriverDetail::DrawEntryCounters counters{};
    for (const auto& table : AlignedTables) counters = draw(table.data());
    Require(counters.hits != start.hits, mode + ": draws whose pointer table moved to identical contents never reused the cached draw (" + std::to_string(counters.lookups - start.lookups) + " lookups, " + std::to_string(counters.inserts - start.inserts) + " inserts)");
    const auto taught = pixels({255, 0, 0, 255});
    Require(taught.empty(), mode + ": a draw through a pointer table with bit 4 of its address clear: " + taught);
    ForwardedData[0] = 0.25f;
    const auto before = counters;
    counters = draw(&OffsetTable[2]);
    Require(counters.lookups == before.lookups + 1 && counters.hits == before.hits + 1 && counters.stageDataHits == before.stageDataHits + 1, mode + ": a draw whose pointer table moved and whose data changed in place was not a relocated stage data hit (" + std::to_string(counters.hits - before.hits) + " hits, " + std::to_string(counters.stageDataHits - before.stageDataHits) + " stage data hits)");
    const auto moved = pixels({64, 255, 255, 64});
    Require(moved.empty(), mode + ": a relocated stage data hit did not draw with the moved user word: " + moved);
}

}

int main() {
    try {
        SplitRunsNameTheirSharedWords();
        bool libraries = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            libraries = device->GraphicsPipelineLibraries();
        }
        if (!libraries && std::getenv("APS5_NO_GPL") == nullptr) return VulkanTestSkipped;
        MovedDataHitsTheCache(libraries);
        UnrelocatedAliasesStayOut(libraries);
        MovedForwardedWordReachesStageDataHit(libraries);
        AgcDriverShutdown_nid_postfix();
        std::cout << "A draw whose data moved reused the cached draw " << (libraries ? "with" : "without") << " graphics pipeline libraries\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
