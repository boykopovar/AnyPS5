#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t FetchBufferTableOffset = 0x000;
constexpr std::size_t FetchAttribTableOffset = 0x100;
constexpr std::size_t SlotsOffset = 0x400;
constexpr std::uint32_t SlotRecords = 32;
constexpr std::uint32_t OutputSlots = 32;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;
constexpr std::uint32_t FetchIndexOffset = 5;
constexpr std::uint32_t FetchVertexOffset = 3;
constexpr std::uint32_t FetchVertices = 3;
constexpr std::size_t ColorBytes = 65536;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;

alignas(256) constexpr std::array<std::uint32_t, 30> FetchVertexCode{
    0x7e020305u, 0x7e040308u,
    0xd70f6a05u, 0x00020a10u,
    0xd70f6a08u, 0x00021011u,
    0xf4080506u, 0xfa000000u,
    0xf4080606u, 0xfa000010u,
    0x02060b08u,
    0xbf8cc07fu,
    0xe0002000u, 0x80050a03u,
    0xe0002000u, 0x80060b03u,
    0xbf8c3f70u,
    0x4a08170au, 0x34080884u,
    0x7e180301u, 0x7e1a0302u, 0x7e1c0305u, 0x7e1e0308u,
    0xe0781000u, 0x80020c04u,
    0x7e200280u, 0x7e2202f2u,
    0xf80008cfu, 0x11101010u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelRead{0x7e1a0280u, 0xdc308000u, 0x0c02000du, 0xbf8c3f70u, 0x7e10030cu, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};

struct FetchHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    std::array<ShaderSemantic, 2> semantics{};
};

alignas(8) FetchHeader FetchVertexHeader{};

void RegisterFetchVertex() {
    auto& header = FetchVertexHeader;
    header.direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 4;
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 6;
    header.userData.direct_resource_offset = header.direct.data();
    header.userData.direct_resource_count = static_cast<std::uint16_t>(header.direct.size());
    header.semantics[0].semantic = 0;
    header.semantics[0].hardware_mapping = 10;
    header.semantics[0].size_in_elements = 1;
    header.semantics[1].semantic = 1;
    header.semantics[1].hardware_mapping = 11;
    header.semantics[1].size_in_elements = 1;
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(FetchHeader);
    header.shader.shader_size = static_cast<std::uint32_t>(sizeof(FetchVertexCode));
    header.shader.code = FetchVertexCode.data();
    header.shader.type = 2;
    header.shader.user_data = &header.userData;
    header.shader.input_semantics = header.semantics.data();
    header.shader.num_input_semantics = static_cast<std::uint32_t>(header.semantics.size());
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

struct PixelHeader {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) PixelHeader PixelShaderHeader{};

void RegisterPixel() {
    auto& header = PixelShaderHeader;
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(PixelHeader);
    header.shader.shader_size = static_cast<std::uint32_t>(sizeof(PixelRead));
    header.shader.code = PixelRead.data();
    header.shader.type = 1;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "fetch relocation: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> SlotDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), SlotRecords, 0x01014facu};
}

std::array<std::uint32_t, 4> OutputDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), OutputSlots * 16u, 0x31016facu};
}

void WriteSlots(const GuestBlock& block, std::size_t offset, std::uint32_t shift) {
    for (std::uint32_t record = 0; record < SlotRecords; ++record) {
        const std::uint32_t slot = record + shift;
        std::memcpy(block.Bytes(offset + record * sizeof(slot)), &slot, sizeof(slot));
    }
}

void WriteFetchTables(const GuestBlock& tables, const GuestBlock& slots) {
    const std::array<std::uint32_t, 2> attributes{0u, 1u | (1u << 26u)};
    std::memcpy(tables.Bytes(FetchAttribTableOffset), attributes.data(), sizeof(attributes));
    const auto descriptor = SlotDescriptor(slots.At(SlotsOffset));
    std::memcpy(tables.Bytes(FetchBufferTableOffset), descriptor.data(), sizeof(descriptor));
    const auto instance = SlotDescriptor(slots.At(SlotsOffset));
    std::memcpy(tables.Bytes(FetchBufferTableOffset + 16u), instance.data(), sizeof(instance));
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

AgcDriver::DriverDetail::DrawEntryCounters SubmitFetchDraw(const GuestBlock& tables, const GuestBlock& output, const GuestBlock& data, const GuestBlock& target) {
    const auto color = target.At(0);
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelRead.data());
    const auto pointer = data.At(0);
    const float scale = 32.0f;
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
        {0x3b8, 0x9000000u | (RenderTargetTileMode << 14u)}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(scale)}, {0x110, std::bit_cast<std::uint32_t>(scale)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const auto stored = OutputDescriptor(output.At(0));
    const auto buffers = tables.At(FetchBufferTableOffset);
    const auto attributes = tables.At(FetchAttribTableOffset);
    const std::array<std::uint32_t, 10> user{stored[0], stored[1], stored[2], stored[3], static_cast<std::uint32_t>(buffers), static_cast<std::uint32_t>(buffers >> 32u), static_cast<std::uint32_t>(attributes), static_cast<std::uint32_t>(attributes >> 32u), FetchVertexOffset, 0u};
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, FetchIndexOffset});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    const auto vertex = reinterpret_cast<std::uintptr_t>(FetchVertexCode.data());
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x8b, static_cast<std::uint32_t>(user.size()) << 1u});
    for (std::uint32_t word = 0; word < user.size(); ++word) Append(commands, 0x76, {0x8cu + word, user[word]});
    Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
    Append(commands, 0x76, {0x00e, static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u)});
    Append(commands, 0x2d, {3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "fetch relocation: the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    return AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
}

using FetchOutput = std::array<std::uint32_t, OutputSlots * 4u>;

void ClearFetchOutput(const GuestBlock& output) {
    FetchOutput words;
    words.fill(Unwritten);
    std::memcpy(output.Bytes(0), words.data(), sizeof(words));
}

FetchOutput ReadFetchOutput(const GuestBlock& output) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(output.At(0)), sizeof(FetchOutput));
    FetchOutput words;
    std::memcpy(words.data(), output.Bytes(0), sizeof(words));
    return words;
}

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("fetch relocation: " + what);
}

void MovedFetchTablesRelocate() {
    GuestBlock evenTables, oddTables, slots, output, data, target;
    WriteSlots(slots, SlotsOffset, 0);
    WriteFetchTables(evenTables, slots);
    WriteFetchTables(oddTables, slots);
    const auto draw = [&](const GuestBlock& tables) {
        ClearFetchOutput(output);
        return SubmitFetchDraw(tables, output, data, target);
    };
    const auto start = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    AgcDriver::DriverDetail::DrawEntryCounters counters{};
    for (std::size_t i = 0; i < 4; ++i) counters = draw(i % 2 == 0 ? evenTables : oddTables);
    Expect(counters.inserts == start.inserts + 2, "the teaching draws made " + std::to_string(counters.inserts - start.inserts) + " inserts, expected 2");
    const auto reference = ReadFetchOutput(output);
    Expect(std::any_of(reference.begin(), reference.end(), [](std::uint32_t word) { return word != Unwritten; }), "the fresh draw wrote no fetched output at all");
    const auto beforeEven = counters;
    counters = draw(evenTables);
    Expect(counters.hits == beforeEven.hits + 1 && counters.inserts == beforeEven.inserts, "a draw whose fetch tables moved to the even copy with identical contents was not a relocated hit (" + std::to_string(counters.hits - beforeEven.hits) + " hits, " + std::to_string(counters.inserts - beforeEven.inserts) + " inserts)");
    const auto relocated = ReadFetchOutput(output);
    Expect(relocated == reference, "the relocated hit's fetched output differs from the freshly recorded draw's");
    const auto beforeOdd = counters;
    counters = draw(oddTables);
    Expect(counters.hits == beforeOdd.hits + 1 && counters.inserts == beforeOdd.inserts, "a draw whose fetch tables moved to the odd copy with identical contents was not a relocated hit (" + std::to_string(counters.hits - beforeOdd.hits) + " hits, " + std::to_string(counters.inserts - beforeOdd.inserts) + " inserts)");
    Expect(ReadFetchOutput(output) == reference, "the second relocated hit's fetched output differs from the freshly recorded draw's");
    std::printf("moved fetch tables: %llu hits in %llu lookups, fetched output identical to the fresh recordings\n", static_cast<unsigned long long>(counters.hits), static_cast<unsigned long long>(counters.lookups));
}

void ChangedFetchTablesDoNotRelocate() {
    GuestBlock evenTables, oddTables, slots, otherSlots, output, data, target;
    WriteSlots(slots, SlotsOffset, 0);
    WriteSlots(otherSlots, SlotsOffset, 1);
    WriteFetchTables(evenTables, slots);
    WriteFetchTables(oddTables, slots);
    const auto draw = [&](const GuestBlock& tables) {
        ClearFetchOutput(output);
        return SubmitFetchDraw(tables, output, data, target);
    };
    const auto start = AgcDriver::DriverDetail::Driver::Get().DrawCacheCounters();
    AgcDriver::DriverDetail::DrawEntryCounters counters{};
    for (std::size_t i = 0; i < 4; ++i) counters = draw(i % 2 == 0 ? evenTables : oddTables);
    counters = draw(evenTables);
    Expect(counters.hits > start.hits, "the teaching did not relocate a moved identical table (" + std::to_string(counters.hits - start.hits) + " hits)");
    const auto reference = ReadFetchOutput(output);
    const auto descriptor = SlotDescriptor(otherSlots.At(SlotsOffset));
    std::memcpy(oddTables.Bytes(FetchBufferTableOffset), descriptor.data(), sizeof(descriptor));
    const auto before = counters;
    counters = draw(oddTables);
    Expect(counters.inserts >= before.inserts + 1 && counters.hits == before.hits, "a draw whose odd table contents changed reused the relocated entry (" + std::to_string(counters.hits - before.hits) + " hits, " + std::to_string(counters.inserts - before.inserts) + " inserts)");
    const auto output64 = ReadFetchOutput(output);
    Expect(std::any_of(output64.begin(), output64.end(), [](std::uint32_t word) { return word != Unwritten; }), "the draw through the changed table wrote no fetched output");
    Expect(output64 != reference, "the draw through the changed table produced the unchanged table's output");
    std::printf("changed fetch table contents: no reuse, %llu inserts, fetched through the changed table\n", static_cast<unsigned long long>(counters.inserts));
}

}

int main() {
    RegisterFetchVertex();
    RegisterPixel();
    MovedFetchTablesRelocate();
    ChangedFetchTablesDoNotRelocate();
    std::printf("draw fetch relocation tests passed\n");
    return 0;
}
