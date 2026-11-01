#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{0x7e000208u, 0x7e020280u, 0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 11> PixelCode{0xf4080100u, 0xfa000000u, 0xbf8cc07fu, 0xe0300000u, 0x80010000u, 0xbf8c3f70u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 10> ArithmeticPixelCode{0xf4000100u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0x06000002u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> DirectBufferPixelCode{0xe0300000u, 0x80000000u, 0xbf8c3f70u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<float, 4> PixelConstant{};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

constexpr std::size_t BlockBytes = 65536;

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) Header ArithmeticPixelHeader{};
alignas(8) Header DirectBufferPixelHeader{};

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

std::uint32_t* GuestBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    Require(block != nullptr, "data-only hit: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void PixelUserData(std::vector<std::uint32_t>& commands, const std::uint32_t* pointer, std::uint32_t unread) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(address)});
    Append(commands, 0x76, {0x00d, static_cast<std::uint32_t>(address >> 32u)});
    Append(commands, 0x76, {0x00e, unread});
}

std::vector<std::uint32_t> Setup(bool triangle = false, const std::uint32_t* pixelCode = nullptr, std::uint32_t pixelUserSgprs = 3) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(triangle ? TriangleVertexCode.data() : VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode ? pixelCode : triangle ? ArithmeticPixelCode.data() : PixelCode.data());
    std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
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
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, triangle ? 0u : 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, pixelUserSgprs << 1u},
    };
    if (triangle) context.insert(context.end(), {{0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0}});
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    return commands;
}

AgcDriver::DriverDetail::DrawEntryCounters Submit(std::vector<std::uint32_t> commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
}

std::string Counts(const AgcDriver::DriverDetail::DrawEntryCounters& counters) {
    return std::to_string(counters.lookups) + " lookups, " + std::to_string(counters.dataHits) + " data-only hits, stages " + std::to_string(counters.dataStagesReused) + " reused / " + std::to_string(counters.dataStagesCompiled) + " compiled / " + std::to_string(counters.dataVerified) + " verified";
}

void UnreadUserWordsKeepTheStage(bool verify) {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    auto* first = GuestBlock();
    auto* second = GuestBlock();
    const auto describe = [](std::uint32_t* table, std::uint32_t* data, float value) {
        const auto address = reinterpret_cast<std::uintptr_t>(data);
        data[0] = std::bit_cast<std::uint32_t>(value);
        table[0] = static_cast<std::uint32_t>(address);
        table[1] = static_cast<std::uint32_t>(address >> 32u) & 0xffffu;
        table[2] = 16;
        table[3] = 0x4bfacu;
    };
    describe(first, first + 1024, 1.0f);
    describe(second, second + 1024, 0.5f);

    auto commands = Setup();
    PixelUserData(commands, first, 0x11);
    Append(commands, 0x2d, {3, 2});
    auto counters = Submit(std::move(commands));
    Require(counters.lookups == 1 && counters.dataHits == 0, "the first draw: " + Counts(counters));
    commands.clear();
    PixelUserData(commands, first, 0x11);
    Append(commands, 0x2d, {3, 2});
    counters = Submit(std::move(commands));
    Require(counters.inserts != 0, "the repeated register key was not admitted: " + Counts(counters));

    commands.clear();
    PixelUserData(commands, first, 0x22);
    Append(commands, 0x2d, {3, 2});
    const auto before = counters;
    counters = Submit(std::move(commands));
    const auto reused = counters.dataStagesReused - before.dataStagesReused;
    const auto compiled = counters.dataStagesCompiled - before.dataStagesCompiled;
    Require(counters.dataHits == before.dataHits + 1, "a draw that changed only a user word no stage reads was not a data-only hit: " + Counts(counters));
    if (verify) Require(compiled == 2 && counters.dataVerified - before.dataVerified == 2, "verify mode: a draw that changed only an unread user word did not build and compare both stages: " + Counts(counters));
    else Require(reused == 2 && compiled == 0, "a draw that changed only a user word no stage reads did not reuse both stages: " + Counts(counters));

    commands.clear();
    PixelUserData(commands, second, 0x22);
    Append(commands, 0x2d, {3, 2});
    const auto moved = counters;
    counters = Submit(std::move(commands));
    Require(counters.dataHits == moved.dataHits + 1, "a draw that pointed the pixel stage at another buffer descriptor was not a data-only hit: " + Counts(counters));
    Require(counters.dataStagesCompiled - moved.dataStagesCompiled >= 1, "a draw that pointed the pixel stage at another buffer descriptor reused the pixel stage: " + Counts(counters));
    if (!verify) Require(counters.dataStagesReused - moved.dataStagesReused == 1, "a draw that changed only the pixel stage's descriptor pointer did not reuse the vertex stage: " + Counts(counters));

    const bool refresh = std::getenv("APS5_DRAW_PIPELINE") != nullptr;
    for (const std::uint32_t unread : {0x33u, 0x44u}) {
        commands.clear();
        PixelUserData(commands, second, unread);
        Append(commands, 0x2d, {3, 2});
        const auto last = counters;
        counters = Submit(std::move(commands));
        Require(counters.dataHits == last.dataHits + 1 && counters.inserts == last.inserts, "a draw at the moved pointer with a new unread word: " + Counts(counters));
        if (verify) continue;
        const auto compiled = counters.dataStagesCompiled - last.dataStagesCompiled;
        const bool expectReuse = refresh && unread == 0x44u;
        Require(compiled == (expectReuse ? 0u : 1u), std::string(expectReuse ? "after a refresh, the draw at the moved pointer compiled its pixel stage again: " : "the draw at the moved pointer did not compile its pixel stage: ") + Counts(counters));
        Require(counters.shapeUpdates - last.shapeUpdates == (refresh && unread == 0x33u ? 1u : 0u), "shape refreshes: " + std::to_string(counters.shapeUpdates - last.shapeUpdates) + " (" + Counts(counters) + ")");
    }
}

unsigned BrightestColorByte() {
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
    unsigned brightest = 0;
    for (const auto value : ColorMemory) brightest = std::max(brightest, static_cast<unsigned>(std::to_integer<std::uint8_t>(value)));
    return brightest;
}

void ReadUserWordsChangeTheOutput() {
    Registered(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Registered(ArithmeticPixelHeader, ArithmeticPixelCode.data(), sizeof(ArithmeticPixelCode), 1);
    PixelConstant[0] = 0.25f;
    auto* table = reinterpret_cast<std::uint32_t*>(PixelConstant.data());
    const auto draw = [&](float word, bool setup) {
        auto commands = setup ? Setup(true) : std::vector<std::uint32_t>{};
        PixelUserData(commands, table, std::bit_cast<std::uint32_t>(word));
        Append(commands, 0x2d, {3, 2});
        Submit(std::move(commands));
        return BrightestColorByte();
    };
    draw(0.25f, true);
    const auto first = draw(0.25f, false);
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto second = draw(0.5f, false);
    const auto after = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto closeTo = [](unsigned value, unsigned expected) { return value + 2 >= expected && value <= expected + 2; };
    Require(closeTo(first, 128), "a pixel stage adding user word 2 (0.25) to a loaded 0.25 wrote " + std::to_string(first) + ", expected 128");
    Require(closeTo(second, 191), "a pixel stage adding user word 2 changed to 0.5 wrote " + std::to_string(second) + ", expected 191 (" + Counts(after) + ", before: " + Counts(before) + ")");
}

void DirectBufferBaseMovesWithItsPayload() {
    Registered(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Registered(DirectBufferPixelHeader, DirectBufferPixelCode.data(), sizeof(DirectBufferPixelCode), 1);
    auto* ring = GuestBlock();
    ColorMemory.fill(std::byte{0});
    const auto slot = [&](std::size_t index, float value) {
        auto* data = ring + 1024 + index * 64;
        data[0] = std::bit_cast<std::uint32_t>(value);
        return data;
    };
    const auto draw = [&](const std::uint32_t* data, bool setup) {
        auto commands = setup ? Setup(true, DirectBufferPixelCode.data(), 4) : std::vector<std::uint32_t>{};
        const auto address = reinterpret_cast<std::uintptr_t>(data);
        Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(address)});
        Append(commands, 0x76, {0x00d, static_cast<std::uint32_t>(address >> 32u) & 0xffffu});
        Append(commands, 0x76, {0x00e, 16});
        Append(commands, 0x76, {0x00f, 0x3004bfacu});
        Append(commands, 0x2d, {3, 2});
        Submit(std::move(commands));
        return BrightestColorByte();
    };
    const auto first = slot(0, 0.25f);
    const auto second = slot(1, 1.0f);
    draw(first, true);
    const auto repeated = draw(first, false);
    const auto before = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    const auto moved = draw(second, false);
    const auto after = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    Require(moved > repeated, "a draw whose descriptor base and payload both moved read the old payload: " + std::to_string(repeated) + " then " + std::to_string(moved) + " (" + Counts(after) + ", before: " + Counts(before) + ")");
    Require(after.dataHits == before.dataHits + 1, "a draw whose descriptor base and payload moved was not a data-only hit (" + Counts(after) + ", before: " + Counts(before) + ")");
    Require(after.dataStagesCompiled - before.dataStagesCompiled >= 1, "a draw whose descriptor base moved reused the pixel stage built for the old base: the rebuild this test documents is gone, flip it to a reuse check (" + Counts(after) + ", before: " + Counts(before) + ")");
}

}

int main() {
    try {
        bool libraries = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            libraries = device->GraphicsPipelineLibraries();
        }
        if (!libraries && std::getenv("APS5_NO_GPL") == nullptr) return VulkanTestSkipped;
        const bool verify = std::getenv("APS5_VERIFY_DRAW_DATA_HITS") != nullptr;
        UnreadUserWordsKeepTheStage(verify);
        ReadUserWordsChangeTheOutput();
        DirectBufferBaseMovesWithItsPayload();
        AgcDriverShutdown_nid_postfix();
        std::cout << "Data-only hits " << (verify ? "verified" : "reused") << " the stages whose read user words did not change\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
