#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
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
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DispatchVariant;
using AgcDriver::DriverDetail::DrawEntryCounters;
using AgcDriver::DriverDetail::DrawShapeKeys;
using AgcDriver::DriverDetail::DrawShapeMiss;
using AgcDriver::DriverDetail::DrawShapeReuse;
using AgcDriver::DriverDetail::DrawShapeWord;
using AgcDriver::DriverDetail::Driver;
using AgcDriver::DriverDetail::UserWordMove;
using ShaderRecompiler::RecompileResult;

alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 11> PixelCode{0xf4080100u, 0xfa000000u, 0xbf8cc07fu, 0xe0300000u, 0x80010000u, 0xbf8c3f70u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 10> InputVertexCode{
    0xf4080504u, 0xfa000000u,
    0x02060b08u,
    0xbf8cc07fu,
    0xe00c2000u, 0x80050a03u,
    0xbf8c3f70u,
    0xf80008cfu, 0x0d0c0b0au,
    0xbf810000u,
};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t DataWord = 1024;

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

struct InputHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    std::array<ShaderSemantic, 1> semantics{};
};

alignas(8) InputHeader VertexInputHeader{};

void RegisterInputVertex() {
    auto& header = VertexInputHeader;
    header.direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 0;
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 2;
    header.userData.direct_resource_offset = header.direct.data();
    header.userData.direct_resource_count = static_cast<std::uint16_t>(header.direct.size());
    header.semantics[0].semantic = 0;
    header.semantics[0].hardware_mapping = 10;
    header.semantics[0].size_in_elements = 4;
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(InputHeader);
    header.shader.shader_size = static_cast<std::uint32_t>(sizeof(InputVertexCode));
    header.shader.code = InputVertexCode.data();
    header.shader.type = 2;
    header.shader.user_data = &header.userData;
    header.shader.input_semantics = header.semantics.data();
    header.shader.num_input_semantics = static_cast<std::uint32_t>(header.semantics.size());
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

std::uint32_t* GuestBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    Require(block != nullptr, "shape user words: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> Setup(std::uintptr_t vertex = reinterpret_cast<std::uintptr_t>(TriangleVertexCode.data()), const std::vector<std::uint32_t>& vertexWords = {}) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
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
    std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, static_cast<std::uint32_t>(vertexWords.size()) << 1u},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 3u << 1u},
    };
    if (vertexWords.empty()) shader.emplace_back(0x8c, 0);
    for (std::uint32_t word = 0; word < vertexWords.size(); ++word) shader.emplace_back(0x8cu + word, vertexWords[word]);
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    return commands;
}

std::uint32_t Unread = 0x1000;

DrawEntryCounters Draw(const std::uint32_t* table, std::uintptr_t vertex = reinterpret_cast<std::uintptr_t>(TriangleVertexCode.data()), const std::vector<std::uint32_t>& vertexWords = {}) {
    auto commands = Setup(vertex, vertexWords);
    const auto address = reinterpret_cast<std::uintptr_t>(table);
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(address)});
    Append(commands, 0x76, {0x00d, static_cast<std::uint32_t>(address >> 32u)});
    Append(commands, 0x76, {0x00e, ++Unread});
    Append(commands, 0x2d, {3, 2});
    ColorMemory.fill(std::byte{0});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return Driver::Get().DrawCacheCounters();
}

unsigned Brightest() {
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(ColorMemory.data()), ColorMemory.size());
    unsigned brightest = 0;
    for (const auto value : ColorMemory) brightest = std::max(brightest, static_cast<unsigned>(std::to_integer<std::uint8_t>(value)));
    return brightest;
}

void Describe(std::uint32_t* table, std::uint32_t* data, float value) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    data[0] = std::bit_cast<std::uint32_t>(value);
    table[0] = static_cast<std::uint32_t>(address);
    table[1] = static_cast<std::uint32_t>(address >> 32u) & 0xffffu;
    table[2] = 16;
    table[3] = 0x3004bfacu;
}

std::string Counts(const DrawEntryCounters& counters) {
    const auto list = [](const auto& values) {
        std::string text;
        for (const auto value : values) text += (text.empty() ? "" : "/") + std::to_string(value);
        return text;
    };
    return std::to_string(counters.lookups) + " lookups, " + std::to_string(counters.hits) + " hits, " + std::to_string(counters.dataHits) + " data-only hits, stages " + std::to_string(counters.dataStagesReused) + " reused / " + std::to_string(counters.dataStagesCompiled) + " compiled / " + std::to_string(counters.dataVerified) + " verified, " + std::to_string(counters.inserts) + " inserts, " + std::to_string(counters.shapeUpdates) + " shape updates, reuse by rank " + list(counters.shapeHitsByRank) + ", by kind " + list(counters.shapeReuses) + ", captures by reason " + list(counters.shapeMisses) + ", " + std::to_string(counters.shapeKeysDropped) + " dropped, vertex-input stages " + std::to_string(counters.shapeVertexReused) + " reused (" + std::to_string(counters.shapeVertexRebased) + " rebased) / " + std::to_string(counters.shapeVertexCaptured) + " captured, refusing words " + list(counters.shapeVertexWords) + ", verified " + std::to_string(counters.shapeVertexVerified) + " (" + std::to_string(counters.shapeVertexDiffered) + " differed)";
}

struct Step {
    std::uint64_t reused, compiled, verified, updates, dropped;
};

Step Between(const DrawEntryCounters& before, const DrawEntryCounters& after) {
    return {after.dataStagesReused - before.dataStagesReused, after.dataStagesCompiled - before.dataStagesCompiled, after.dataVerified - before.dataVerified, after.shapeUpdates - before.shapeUpdates, after.shapeKeysDropped - before.shapeKeysDropped};
}

bool CloseTo(unsigned value, unsigned expected) {
    return value + 2 >= expected && value <= expected + 2;
}

std::size_t Kind(DrawShapeReuse kind) {
    return static_cast<std::size_t>(kind);
}

std::size_t Reason(DrawShapeMiss reason) {
    return static_cast<std::size_t>(reason);
}

void RememberedDrawsAlternate(bool enabled, bool verify) {
    Registered(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    const auto limit = enabled ? std::size_t{4} : std::size_t{1};
    constexpr std::size_t Sets = 6;
    constexpr std::array<float, Sets> values{1.0f, 0.75f, 0.5f, 0.25f, 0.875f, 0.625f};
    constexpr std::array<unsigned, Sets> colors{255, 191, 128, 64, 223, 159};
    auto* block = GuestBlock();
    std::array<std::uint32_t*, Sets> tables{};
    for (std::size_t set = 0; set < Sets; ++set) {
        tables[set] = block + set * 64;
        Describe(tables[set], tables[set] + DataWord, values[set]);
    }
    const auto draw = [&](std::size_t set, const std::uint32_t* table, const char* what) {
        const auto counters = Draw(table);
        const auto color = Brightest();
        Require(CloseTo(color, colors[set]), std::string(what) + ": a draw of buffer " + std::to_string(set) + " wrote " + std::to_string(color) + ", expected " + std::to_string(colors[set]) + " (" + Counts(counters) + ")");
        return counters;
    };

    auto counters = draw(0, tables[0], "the first draw of the shape");
    Require(counters.lookups == 1 && counters.dataHits == 0 && counters.inserts == 0, "the first draw of the shape: " + Counts(counters));
    counters = draw(0, tables[0], "the second draw of the shape");
    Require(counters.inserts == 1 && counters.dataHits == 0, "the shape's second draw was not cached: " + Counts(counters));

    for (std::size_t set = 1; set < 4; ++set) {
        for (std::size_t sighting = 0; sighting < 2; ++sighting) {
            const auto before = counters;
            counters = draw(set, tables[set], "teaching");
            const auto step = Between(before, counters);
            Require(counters.dataHits == before.dataHits + 1 && counters.inserts == before.inserts, "a draw of another buffer through the same shape was not a data-only hit: " + Counts(counters));
            Require(step.compiled == (verify ? 2u : 1u), "sighting " + std::to_string(sighting + 1) + " of buffer " + std::to_string(set) + ", which no remembered draw of the shape binds, built " + std::to_string(step.compiled) + " stages: " + Counts(counters));
            Require(step.updates == (enabled && sighting == 1 ? 1u : 0u), "sighting " + std::to_string(sighting + 1) + " of buffer " + std::to_string(set) + " added " + std::to_string(step.updates) + " remembered draws to the shape: " + Counts(counters));
            if (enabled && !verify) Require(counters.shapeMisses[Reason(DrawShapeMiss::Memory)] == before.shapeMisses[Reason(DrawShapeMiss::Memory)] + 1, "a pixel stage whose table names another buffer was not captured because its memory differed: " + Counts(counters));
        }
    }

    const auto taught = counters;
    for (std::size_t round = 0; round < 3; ++round) {
        for (std::size_t set = 0; set < 4; ++set) {
            const auto before = counters;
            counters = draw(set, tables[set], "alternating");
            const auto step = Between(before, counters);
            Require(counters.dataHits == before.dataHits + 1 && counters.hits == before.hits, "an alternating draw with a new unread user word was not a data-only hit: " + Counts(counters));
            if (verify) {
                Require(step.verified == (enabled || set == 0 ? 2u : 1u), "verify mode compared " + std::to_string(step.verified) + " reused stages of buffer " + std::to_string(set) + " with their full build: " + Counts(counters));
                continue;
            }
            const bool remembered = set < limit;
            Require(step.reused == (remembered ? 2u : 1u) && step.compiled == (remembered ? 0u : 1u), "round " + std::to_string(round) + ": a draw of buffer " + std::to_string(set) + " reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ", expected " + (remembered ? "both reused" : "its pixel stage built, as only one draw of the shape is remembered") + ": " + Counts(counters));
            Require(step.updates == 0 && step.dropped == 0, "an alternating draw changed the shape's remembered draws: " + Counts(counters));
        }
    }
    const auto beyondFirst = std::accumulate(counters.shapeHitsByRank.begin() + 1, counters.shapeHitsByRank.end(), std::uint64_t{0}) - std::accumulate(taught.shapeHitsByRank.begin() + 1, taught.shapeHitsByRank.end(), std::uint64_t{0});
    Require(verify || (enabled ? beyondFirst >= 9 : beyondFirst == 0), std::to_string(beyondFirst) + " stages were reused from a remembered draw other than the most recent one: " + Counts(counters));
    Require(counters.shapeReuses[Kind(DrawShapeReuse::Relocated)] == 0 && counters.shapeReuses[Kind(DrawShapeReuse::Forwarded)] == 0, "draws at unmoved pointers were counted as relocated or forwarded: " + Counts(counters));

    const std::array<std::uint32_t*, 2> moved{block + 2048, block + 2112};
    for (auto* table : moved) std::memcpy(table, tables[0], 4 * sizeof(std::uint32_t));
    for (const std::uint32_t* table : {static_cast<const std::uint32_t*>(moved[0]), static_cast<const std::uint32_t*>(moved[1]), static_cast<const std::uint32_t*>(moved[0])}) {
        const auto before = counters;
        counters = draw(0, table, "relocated");
        const auto step = Between(before, counters);
        const auto relocated = counters.shapeReuses[Kind(DrawShapeReuse::Relocated)] - before.shapeReuses[Kind(DrawShapeReuse::Relocated)];
        if (verify) Require(step.verified == (enabled ? 2u : 1u), "verify mode compared " + std::to_string(step.verified) + " stages of a draw whose descriptor table moved with their full build: " + Counts(counters));
        else if (enabled) Require(step.reused == 2 && step.compiled == 0 && relocated == 1, "a draw whose descriptor table moved to another address with the same words reused " + std::to_string(step.reused) + " stages (" + std::to_string(relocated) + " relocated) and built " + std::to_string(step.compiled) + ": " + Counts(counters));
        else Require(step.reused == 1 && step.compiled == 1 && relocated == 0, "with the shape user words off, a draw whose descriptor table moved reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ": " + Counts(counters));
    }
    if (verify) return;

    const auto changed = counters;
    Describe(moved[0], tables[4] + DataWord, values[4]);
    counters = draw(4, moved[0], "a moved table that names another buffer");
    auto step = Between(changed, counters);
    Require(step.compiled == 1 && step.reused == 1, "a draw whose moved table names another buffer reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ", expected its pixel stage built: " + Counts(counters));
    if (enabled) Require(counters.shapeMisses[Reason(DrawShapeMiss::Memory)] == changed.shapeMisses[Reason(DrawShapeMiss::Memory)] + 1, "the pixel stage of a moved table that names another buffer was not captured because its memory differed: " + Counts(counters));
    std::memcpy(moved[0], tables[0], 4 * sizeof(std::uint32_t));

    alignas(16) static std::array<std::uint32_t, 4> distant{};
    std::memcpy(distant.data(), tables[0], 4 * sizeof(std::uint32_t));
    if ((reinterpret_cast<std::uintptr_t>(distant.data()) >> 32u) != (reinterpret_cast<std::uintptr_t>(block) >> 32u)) {
        const auto before = counters;
        counters = draw(0, distant.data(), "a table whose pointer's high word moved");
        step = Between(before, counters);
        Require(step.compiled == 1 && step.reused == 1, "a draw whose table pointer changed its high word, which the pixel stage reads as a value, reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ": " + Counts(counters));
        if (enabled) Require(counters.shapeMisses[Reason(DrawShapeMiss::UserWord)] == before.shapeMisses[Reason(DrawShapeMiss::UserWord)] + 1, "the pixel stage of a table pointer whose high word moved was not captured for a user word it depends on: " + Counts(counters));
    }

    if (!enabled) {
        Require(counters.shapeUpdates == 0 && counters.shapeKeysDropped == 0 && counters.shapeDraws == 0, "with the shape user words off, the shape's remembered draw changed: " + Counts(counters));
        return;
    }

    for (std::size_t set = 1; set < 4; ++set) counters = draw(set, tables[set], "ordering");
    for (std::size_t sighting = 0; sighting < 2; ++sighting) {
        const auto before = counters;
        counters = draw(5, tables[5], "a fifth buffer");
        step = Between(before, counters);
        Require(step.compiled == 1 && step.updates == sighting && step.dropped == sighting, "sighting " + std::to_string(sighting + 1) + " of a fifth buffer built " + std::to_string(step.compiled) + " stages, added " + std::to_string(step.updates) + " remembered draws and dropped " + std::to_string(step.dropped) + ": " + Counts(counters));
    }
    auto before = counters;
    counters = draw(0, tables[0], "the dropped draw");
    step = Between(before, counters);
    Require(step.compiled == 1 && step.reused == 1, "a draw of the buffer whose remembered draw was dropped beyond k reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ", expected its pixel stage built: " + Counts(counters));
    for (const std::size_t set : {std::size_t{5}, std::size_t{3}, std::size_t{2}, std::size_t{1}}) {
        before = counters;
        counters = draw(set, tables[set], "the kept draws");
        step = Between(before, counters);
        Require(step.compiled == 0 && step.reused == 2, "a draw of buffer " + std::to_string(set) + ", whose remembered draw is within k, reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ": " + Counts(counters));
    }
}

void VertexInputsFollowMovedTables(bool enabled, bool vertexInputs, bool verify) {
    RegisterInputVertex();
    auto* block = GuestBlock();
    auto* pixelTable = block;
    Describe(pixelTable, pixelTable + DataWord, 1.0f);
    using Positions = std::array<std::array<float, 4>, 3>;
    constexpr Positions Covering{{{-1.0f, -1.0f, 0.0f, 1.0f}, {3.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 3.0f, 0.0f, 1.0f}}};
    constexpr Positions Outside{{{2.0f, 2.0f, 0.0f, 1.0f}, {3.0f, 2.0f, 0.0f, 1.0f}, {2.0f, 3.0f, 0.0f, 1.0f}}};
    const auto write = [&](std::size_t word, std::uint32_t stride, const Positions& positions) {
        for (std::size_t vertex = 0; vertex < positions.size(); ++vertex) std::memcpy(block + word + vertex * (stride / sizeof(std::uint32_t)), positions[vertex].data(), sizeof(positions[vertex]));
        return static_cast<const std::uint32_t*>(block + word);
    };
    const auto* covering = write(4096, 16, Covering);
    const auto* outside = write(4160, 16, Outside);
    const auto* wide = write(4224, 32, Covering);
    constexpr std::uint32_t Float4 = 0x0104dfacu;
    constexpr std::uint32_t Float3One = (Float4 & ~(7u << 9u)) | (1u << 9u);
    const auto sharp = [](const std::uint32_t* buffer, std::uint32_t stride, std::uint32_t format) {
        const auto address = reinterpret_cast<std::uintptr_t>(buffer);
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(address), (static_cast<std::uint32_t>(address >> 32u) & 0xffffu) | (stride << 16u), 3u, format};
    };
    std::array<std::uint32_t*, 5> tables{};
    for (std::size_t n = 0; n < tables.size(); ++n) tables[n] = block + 8192 + n * 64;
    const auto put = [](std::uint32_t* table, const std::array<std::uint32_t, 4>& descriptor) { std::memcpy(table, descriptor.data(), sizeof(descriptor)); };
    put(tables[0], sharp(covering, 16, Float4));
    put(tables[1], sharp(covering, 16, Float4));
    put(tables[2], sharp(outside, 16, Float4));
    put(tables[3], sharp(wide, 32, Float4));
    put(tables[4], sharp(covering, 16, Float3One));
    const std::array<std::uint32_t*, 2> attributes{block + 12288, block + 12352};
    for (auto* attribute : attributes) attribute[0] = 0;

    const auto draw = [&](const std::uint32_t* table, const std::uint32_t* attribute, bool lit, const std::string& what) {
        const auto buffers = reinterpret_cast<std::uintptr_t>(table);
        const auto fetches = reinterpret_cast<std::uintptr_t>(attribute);
        const auto counters = Draw(pixelTable, reinterpret_cast<std::uintptr_t>(InputVertexCode.data()), {static_cast<std::uint32_t>(buffers), static_cast<std::uint32_t>(buffers >> 32u), static_cast<std::uint32_t>(fetches), static_cast<std::uint32_t>(fetches >> 32u)});
        const auto color = Brightest();
        Require(lit ? CloseTo(color, 255) : color == 0, what + ": the draw wrote " + std::to_string(color) + ", expected " + (lit ? "255" : "nothing") + " (" + Counts(counters) + ")");
        return counters;
    };
    const auto kind = [](const DrawEntryCounters& before, const DrawEntryCounters& after, DrawShapeReuse reuse) { return after.shapeReuses[Kind(reuse)] - before.shapeReuses[Kind(reuse)]; };
    const auto reason = [](const DrawEntryCounters& before, const DrawEntryCounters& after, DrawShapeMiss miss) { return after.shapeMisses[Reason(miss)] - before.shapeMisses[Reason(miss)]; };

    auto before = Driver::Get().DrawCacheCounters();
    auto counters = draw(tables[0], attributes[0], true, "the first draw with vertex inputs");
    Require(counters.inserts == before.inserts, "the first draw with vertex inputs was cached: " + Counts(counters));
    before = counters;
    counters = draw(tables[0], attributes[0], true, "the second draw with vertex inputs");
    Require(counters.inserts == before.inserts + 1, "the second draw with vertex inputs was not cached: " + Counts(counters));
    const auto start = counters;

    struct Case {
        std::uint32_t* table;
        std::uint32_t* attribute;
        bool lit;
        bool sameLayout;
        bool rebased;
        const char* what;
    };
    const std::array<Case, 5> cases{{
        {tables[1], attributes[0], true, true, false, "a vertex buffer table moved to another address with the same V#"},
        {tables[2], attributes[0], false, true, true, "a moved vertex buffer table whose V# names another vertex buffer"},
        {tables[0], attributes[1], true, true, false, "a vertex attribute table moved to another address with the same word"},
        {tables[3], attributes[0], true, false, false, "a moved vertex buffer table whose V# has another stride"},
        {tables[4], attributes[0], true, false, false, "a moved vertex buffer table whose V# has another format word"},
    }};
    for (const auto& tried : cases) {
        before = counters;
        counters = draw(tried.table, tried.attribute, tried.lit, tried.what);
        const auto step = Between(before, counters);
        Require(counters.dataHits == before.dataHits + 1 && counters.inserts == before.inserts, std::string(tried.what) + ": the draw was not a data-only hit of its shape: " + Counts(counters));
        const bool reuses = vertexInputs && tried.sameLayout;
        if (verify) {
            Require(step.verified == (reuses ? 2u : 1u) && counters.shapeVertexVerified - before.shapeVertexVerified == (reuses ? 1u : 0u), std::string(tried.what) + ": verify mode compared " + std::to_string(step.verified) + " reused stages with their full build: " + Counts(counters));
            continue;
        }
        Require(step.reused == (reuses ? 2u : 1u) && step.compiled == (reuses ? 0u : 1u), std::string(tried.what) + ": the draw reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ", expected " + (reuses ? "both reused" : "its vertex stage built") + ": " + Counts(counters));
        if (!enabled) continue;
        Require(kind(before, counters, DrawShapeReuse::VertexTables) == (reuses ? 1u : 0u) && counters.shapeVertexRebased - before.shapeVertexRebased == (reuses && tried.rebased ? 1u : 0u), std::string(tried.what) + ": the vertex stage was not counted as re-decoded" + (tried.rebased ? " with its V# base patched" : "") + ": " + Counts(counters));
        if (reuses) continue;
        const auto expected = vertexInputs ? DrawShapeMiss::VertexLayout : DrawShapeMiss::VertexInputs;
        Require(reason(before, counters, expected) == 1 && counters.shapeVertexCaptured - before.shapeVertexCaptured == 1, std::string(tried.what) + ": the vertex stage was not captured for " + (vertexInputs ? "inputs that decode to another layout" : "a moved word on a stage not tagged for re-decoding") + ": " + Counts(counters));
        if (!vertexInputs) Require(counters.shapeVertexWords[static_cast<std::size_t>(DrawShapeWord::VertexTable)] == before.shapeVertexWords[static_cast<std::size_t>(DrawShapeWord::VertexTable)] + 1, std::string(tried.what) + ": the refused vertex stage did not name a vertex table pointer as the moved word: " + Counts(counters));
    }

    put(tables[0], sharp(outside, 16, Float4));
    before = counters;
    counters = draw(tables[0], attributes[0], false, "a vertex buffer table changed in place to name another vertex buffer");
    auto step = Between(before, counters);
    if (verify) Require(step.verified == 1, "verify mode compared " + std::to_string(step.verified) + " stages of a draw whose vertex table changed in place: " + Counts(counters));
    else Require(step.reused == 1 && step.compiled == 1, "a draw whose vertex buffer table changed in place reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ", expected its vertex stage built: " + Counts(counters));
    if (enabled && !verify) Require(reason(before, counters, DrawShapeMiss::Memory) == 1, "the vertex stage of a table changed in place was not captured because its memory differed: " + Counts(counters));
    put(tables[0], sharp(covering, 16, Float4));
    before = counters;
    counters = draw(tables[0], attributes[0], true, "the restored vertex buffer table");
    step = Between(before, counters);
    if (!verify) Require(step.reused == 2 && step.compiled == 0, "a draw of the restored vertex buffer table reused " + std::to_string(step.reused) + " stages and built " + std::to_string(step.compiled) + ": " + Counts(counters));
    if (verify) Require(counters.shapeVertexDiffered == start.shapeVertexDiffered && (counters.shapeVertexVerified > start.shapeVertexVerified) == vertexInputs, "verify mode found reused stages with vertex inputs that differ from their full build: " + Counts(counters));
}

std::shared_ptr<DispatchVariant> VertexVariant() {
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.fetchEmbedded = true;
    info.fetchBufferReg = 0;
    info.fetchAttribReg = 2;
    info.resourcesNum = 1;
    info.resources[0].fields = {0x5000u, 16u << 16u, 3u, 0x0104dfacu};
    info.resourcesDst[0] = {10, 4, 0, 0};
    auto compiled = std::make_shared<RecompileResult>();
    compiled->shaderDataUserWords = {5u};
    compiled->pushConstants.resize(sizeof(std::uint32_t));
    compiled->userDataRegisterReads = std::uint64_t{1} << 5u;
    compiled->vertexAttributes.push_back({0u, 4u, info.resources[0], 0u, 4u});
    auto variant = std::make_shared<DispatchVariant>();
    variant->compiled = std::move(compiled);
    variant->vertexInfo = std::make_shared<const ShaderRecompiler::ShaderVertexStageInfo>(info);
    variant->userDataMask = 0x3fu;
    variant->vertexTableWords = 0xfu;
    variant->relocatableWords = std::uint64_t{1} << 4u;
    variant->userWordsWalked = std::uint64_t{1} << 4u;
    variant->forwardedWords = std::uint64_t{1} << 5u;
    variant->runs = {{0x1000u, 0x1010u}, {0x6000u, 0x6004u}, {0x7000u, 0x7010u}};
    variant->runBases = {4};
    variant->words = {1u, 2u, 3u, 4u, 0u, 0x5000u, 16u << 16u, 3u, 0x0104dfacu};
    variant->decodeRuns = 2;
    return variant;
}

void VertexTablesAreClassified() {
    const auto variant = VertexVariant();
    const std::vector<std::uint32_t> was{0x7000u, 0u, 0x6000u, 0u, 0x1000u, 0x55u};
    std::vector<UserWordMove> moves;
    std::uint32_t refused = ~0u;
    const auto classify = [&](const DispatchVariant& tried, std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> changes) {
        auto now = was;
        for (const auto& [index, value] : changes) now[index] = value;
        refused = ~0u;
        return Driver::shapeUserWordMoves(tried, was, now, moves, &refused);
    };
    Require(classify(*variant, {{0u, 0x7100u}}) == DrawShapeReuse::VertexTables && moves.size() == 1 && moves[0].index == 0, "a moved vertex buffer table pointer did not classify as a vertex table move");
    Require(classify(*variant, {{3u, 1u}}) == DrawShapeReuse::VertexTables, "a vertex attribute table pointer whose high word moved did not classify as a vertex table move");
    Require(classify(*variant, {{0u, 0x7100u}, {4u, 0x1100u}, {5u, 0x56u}}) == DrawShapeReuse::VertexTables && moves.size() == 3, "a moved table with a relocated pointer and a forwarded word did not classify as a vertex table move");
    Require(classify(*variant, {{4u, 0x1100u}}) == DrawShapeReuse::Relocated, "a relocated pointer on a stage whose vertex tables stayed did not classify as relocated");
    auto untagged = VertexVariant();
    untagged->decodeRuns = 0;
    Require(!classify(*untagged, {{0u, 0x7100u}}) && refused == 0, "a moved table pointer of a stage not tagged for re-decoding was not refused");
    auto walked = VertexVariant();
    walked->userWordsWalked |= 1u;
    Require(!classify(*walked, {{0u, 0x7100u}}) && refused == 0, "a table pointer the walk reads but cannot relocate moved without a refusal");
    walked->relocatableWords |= 1u;
    Require(classify(*walked, {{0u, 0x7100u}}) == DrawShapeReuse::VertexTables, "a table pointer the walk reads as a relocatable base was refused");
    auto read = VertexVariant();
    auto reads = std::make_shared<RecompileResult>(*read->compiled);
    reads->userDataRegisterReads |= 1u;
    read->compiled = reads;
    Require(!classify(*read, {{0u, 0x7100u}}) && refused == 0, "a table pointer the stage reads at run time without a forwarded slot moved without a refusal");
    auto valued = VertexVariant();
    valued->valueWords = 1u;
    Require(!classify(*valued, {{0u, 0x7100u}}) && refused == 0, "a table pointer word also tagged as a buffer V# word moved without a refusal");
    auto value = VertexVariant();
    value->userWordsAsValue = std::uint64_t{1} << 4u;
    Require(Driver::shapeWordKind(*variant, 1, 0xfu) == DrawShapeWord::VertexTable && Driver::shapeWordKind(*variant, 4, 0xfu) == DrawShapeWord::Pointer && Driver::shapeWordKind(*value, 4, 0xfu) == DrawShapeWord::Value && Driver::shapeWordKind(*variant, 5, 0xfu) == DrawShapeWord::Read, "refusing moved words were not classified as table pointer, pointer, value and other read word");

    const auto& info = *variant->vertexInfo;
    auto other = info;
    Require(Driver::shapeVertexInputsMatch(info, info), "equal decoded vertex inputs did not match");
    other.resources[0].fields[0] = 0x9000u;
    other.resources[0].fields[2] = 5u;
    Require(Driver::shapeVertexInputsMatch(info, other), "decoded vertex inputs that differ only in their V# base and record count did not match");
    other = info;
    other.resources[0].fields[1] = 32u << 16u;
    Require(!Driver::shapeVertexInputsMatch(info, other), "decoded vertex inputs with another stride matched");
    other = info;
    other.resources[0].fields[3] ^= 1u << 9u;
    Require(!Driver::shapeVertexInputsMatch(info, other), "decoded vertex inputs with another format word matched");
    other = info;
    other.resourcesDst[0].registersNum = 3;
    Require(!Driver::shapeVertexInputsMatch(info, other), "decoded vertex inputs with another destination matched");
    other = info;
    other.resourcesNum = 2;
    other.resources[1] = info.resources[0];
    Require(!Driver::shapeVertexInputsMatch(info, other), "decoded vertex inputs with another count matched");
    other = info;
    other.fetchEmbedded = false;
    Require(!Driver::shapeVertexInputsMatch(info, other), "vertex inputs that are not fetched by embedded code matched");

    const std::vector<UserWordMove> moved{{0u, 0x7000u, 0x7100u}, {2u, 0x6000u, 0x6100u}, {4u, 0x1000u, 0x1100u}};
    auto fresh = info;
    fresh.resources[0].fields[0] = 0x9000u;
    std::vector<AgcDriver::Graphics::DecodeRead> decoded(2);
    decoded[0].address = 0x6100u;
    decoded[0].size = 4;
    decoded[1].address = 0x7100u;
    decoded[1].size = 16;
    std::memcpy(decoded[1].data.data(), fresh.resources[0].fields.data(), 16);
    const auto copy = Driver::movedShapeVariant(variant, moved);
    Require(Driver::rebaseVertexInputs(*copy, fresh, decoded), "a moved stage with vertex inputs was not rebased on its re-decoded tables");
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> runs{{0x1100u, 0x1110u}, {0x6100u, 0x6104u}, {0x7100u, 0x7110u}};
    Require(copy->runs == runs && copy->decodeRuns == 2 && variant->runs[1].first == 0x6000u, "the rebased stage does not validate its shifted walk runs and its fresh decode reads, or the remembered stage changed");
    Require(copy->words == std::vector<std::uint32_t>{1u, 2u, 3u, 4u, 0u, 0x9000u, 16u << 16u, 3u, 0x0104dfacu}, "the rebased stage does not hold the fresh decode reads");
    Require(copy->vertexInfo->resources[0].fields[0] == 0x9000u && variant->vertexInfo->resources[0].fields[0] == 0x5000u && copy->vertexPatches.size() == 1, "the rebased stage does not carry the re-decoded vertex inputs and one V# patch");
    Driver::applyShaderDataPatches(*copy);
    Require(copy->compiled->vertexAttributes[0].resource.fields[0] == 0x9000u && variant->compiled->vertexAttributes[0].resource.fields[0] == 0x5000u && copy->vertexPatches.empty(), "the V# base was not patched into the rebased stage's vertex attributes only");
    const auto same = Driver::movedShapeVariant(variant, moved);
    std::memcpy(decoded[1].data.data(), info.resources[0].fields.data(), 16);
    Require(Driver::rebaseVertexInputs(*same, info, decoded) && same->vertexPatches.empty(), "a moved table holding the same V# patched the stage");
    const auto beyond = Driver::movedShapeVariant(variant, moved);
    auto none = fresh;
    none.resourcesNum = 0;
    Require(!Driver::rebaseVertexInputs(*beyond, none, decoded), "a vertex attribute beyond the re-decoded inputs was rebased");
}

std::shared_ptr<DispatchVariant> Variant() {
    auto compiled = std::make_shared<RecompileResult>();
    ShaderRecompiler::DescriptorBinding binding{ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::GuestBuffers, 0, 0, 1, {0x4000u, 16u << 16u, 3u, 0u}};
    compiled->bindings.push_back(std::move(binding));
    compiled->shaderDataUserWords = {7u, 2u};
    compiled->pushConstants.resize(2 * sizeof(std::uint32_t));
    auto variant = std::make_shared<DispatchVariant>();
    variant->compiled = std::move(compiled);
    variant->userDataMask = 0xffu & ~(std::uint64_t{1} << 5u);
    variant->relocatableWords = 1u << 2u;
    variant->valueWords = (1u << 3u) | (1u << 4u);
    variant->valueSlots = {{3u, 0u, 0u}, {4u, 0u, 2u}};
    variant->forwardedWords = (1u << 6u) | (1u << 7u);
    variant->runs = {{0x1000u, 0x1010u}, {0x2000u, 0x2008u}};
    variant->runBases = {2, -1};
    variant->words = {1u, 2u, 3u, 4u, 5u, 6u};
    variant->shared.push_back({});
    return variant;
}

void MovedWordsAreClassified() {
    const auto variant = Variant();
    const std::vector<std::uint32_t> was{0x10u, 0x20u, 0x1000u, 0x4000u, 3u, 0x50u, 0x60u, 0x70u, 0x80u};
    std::vector<UserWordMove> moves;
    const auto classify = [&](std::size_t index, std::uint32_t value) {
        auto now = was;
        now[index] = value;
        return Driver::shapeUserWordMoves(*variant, was, now, moves);
    };
    Require(Driver::shapeUserWordMoves(*variant, was, was, moves) == DrawShapeReuse::EqualWords && moves.empty(), "equal user words were not classified as equal");
    Require(classify(5, 0x51u) == DrawShapeReuse::EqualWords && moves.empty(), "a word outside the stage's user-data mask was not ignored");
    Require(classify(8, 0x81u) == DrawShapeReuse::EqualWords && moves.empty(), "a word outside the stage's user-data mask was not ignored");
    Require(!classify(0, 0x11u) && !classify(1, 0x21u), "a walked user word that is neither a base nor a tagged value moved without a refusal");
    Require(classify(2, 0x3000u) == DrawShapeReuse::Relocated && moves.size() == 1 && moves[0].index == 2 && moves[0].to == 0x3000u, "a base-only pointer word did not classify as relocated");
    Require(!classify(2, 0x3001u), "a base pointer whose low two bits changed was not refused");
    Require(!classify(2, 0u), "a base pointer that became null was not refused");
    Require(classify(3, 0x8000u) == DrawShapeReuse::Relocated, "a tagged V# base word did not classify as relocated");
    Require(!classify(3, 0x8002u), "a V# base whose alignment bits changed was not refused");
    Require(classify(4, 7u) == DrawShapeReuse::Relocated, "a tagged V# record count with the same size tail did not classify as relocated");
    auto unstrided = Variant();
    auto bytes = std::make_shared<RecompileResult>(*unstrided->compiled);
    bytes->bindings[0].guestDescriptor[1] = 0;
    unstrided->compiled = bytes;
    {
        auto now = was;
        now[4] = 6u;
        Require(!Driver::shapeUserWordMoves(*unstrided, was, now, moves), "a V# size whose low two bits changed was not refused");
        now[4] = 7u;
        Require(Driver::shapeUserWordMoves(*unstrided, was, now, moves) == DrawShapeReuse::Relocated, "a V# size with the same low two bits was refused");
    }
    Require(classify(7, 0x71u) == DrawShapeReuse::Forwarded && moves.size() == 1, "a word the stage only forwards did not classify as forwarded");
    Require(!classify(6, 0x61u), "a forwarded word without a forwarded slot was not refused");
    {
        auto now = was;
        now[2] = 0x3000u;
        now[7] = 0x71u;
        Require(Driver::shapeUserWordMoves(*variant, was, now, moves) == DrawShapeReuse::Relocated && moves.size() == 2, "a relocated pointer with a forwarded word did not classify as relocated");
        const auto moved = Driver::movedShapeVariant(variant, moves);
        Require(moved->runs[0] == std::pair<std::uint64_t, std::uint64_t>{0x3000u, 0x3010u} && moved->runs[1] == variant->runs[1], "the runs read through the moved pointer did not move with it, or another run moved");
        Require(moved->shared.empty(), "a variant whose runs moved kept the shared words of its old addresses");
        Require(moved->shaderDataPatches == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1u, 0x3000u}, {0u, 0x71u}}, "the forwarded copies of the moved words were not patched");
        Driver::applyShaderDataPatches(*moved);
        std::array<std::uint32_t, 2> pushed{};
        std::memcpy(pushed.data(), moved->compiled->pushConstants.data(), sizeof(pushed));
        Require(pushed == std::array<std::uint32_t, 2>{0x71u, 0x3000u} && variant->compiled->pushConstants != moved->compiled->pushConstants, "the moved variant's push data does not hold the moved words, or the remembered variant was patched");
    }
    {
        auto now = was;
        now[3] = 0x8000u;
        now[4] = 7u;
        Require(Driver::shapeUserWordMoves(*variant, was, now, moves) == DrawShapeReuse::Relocated, "a moved V# base and record count were refused");
        const auto moved = Driver::movedShapeVariant(variant, moves);
        Require(moved->runs == variant->runs && moved->shared.size() == 1, "a moved V# base moved a captured run or dropped the shared words");
        Driver::applyShaderDataPatches(*moved);
        Require(moved->compiled->bindings[0].guestDescriptor == std::vector<std::uint32_t>{0x8000u, 16u << 16u, 7u, 0u} && variant->compiled->bindings[0].guestDescriptor[0] == 0x4000u, "the moved V# words were not patched into the moved variant's binding only");
    }
    std::vector<std::uint32_t> wide(70, 1u), wideNow(70, 1u);
    wideNow[65] = 2u;
    Require(!Driver::shapeUserWordMoves(*variant, wide, wideNow, moves), "a moved word beyond the tracked user words was not refused");
}

void UnusedRepeatsBackOff() {
    DrawShapeKeys keys;
    keys.PushFront({1, false, false});
    std::uint64_t key = 10;
    const auto add = [&] {
        while (keys.count >= 4) keys.Erase(keys.slots[keys.count - 1u].key);
        keys.PushFront({key++, true, false});
    };
    for (std::size_t i = 0; i < 3; ++i) add();
    Require(keys.Admits() && keys.wasted == 0, "a shape with room refused a repeated stage");
    add();
    Require(keys.wasted == 0 && !keys.Contains(1), "dropping a draw that was not added for a repeated stage counted as waste");
    for (std::size_t i = 0; i < DrawShapeKeys::WastedLimit; ++i) {
        Require(keys.Admits(), "a shape refused a repeated stage before " + std::to_string(DrawShapeKeys::WastedLimit) + " unused ones were dropped");
        add();
    }
    for (std::size_t i = 0; i + 1 < DrawShapeKeys::Cooldown; ++i) Require(!keys.Admits(), "a shape whose added draws were dropped unused admitted another before its pause ended");
    Require(!keys.Admits() && keys.Admits(), "a shape did not admit a repeated stage again after its pause");
    keys.Use(keys.slots[1].key);
    Require(keys.wasted == 0 && keys.slots[0].used, "reusing a draw added for a repeated stage did not clear the waste count");
}

}

int main() {
    try {
        MovedWordsAreClassified();
        VertexTablesAreClassified();
        UnusedRepeatsBackOff();
        bool libraries = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            libraries = device->GraphicsPipelineLibraries();
        }
        if (!libraries && std::getenv("APS5_NO_GPL") == nullptr) return VulkanTestSkipped;
        const bool enabled = std::getenv("APS5_NO_DRAW_SHAPE_USER_WORDS") == nullptr;
        const bool verify = std::getenv("APS5_VERIFY_DRAW_DATA_HITS") != nullptr;
        const bool vertexInputs = enabled && std::getenv("APS5_NO_SHAPE_VERTEX_INPUT_STAGES") == nullptr;
        RememberedDrawsAlternate(enabled, verify);
        VertexInputsFollowMovedTables(enabled, vertexInputs, verify);
        AgcDriverShutdown_nid_postfix();
        std::cout << "Draws of one shape " << (verify ? "verified" : enabled ? "reused" : "rebuilt, with the shape user words off,") << " the stages of the shape's remembered draws across moved user words; stages with vertex inputs " << (vertexInputs ? "followed moved vertex tables" : "were captured again when their tables moved") << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
