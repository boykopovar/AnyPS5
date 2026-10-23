#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Marker = 0x5a17c0deu;
constexpr std::size_t AllocationBytes = 65536u;
alignas(256) std::array<std::uint32_t, 96> Output{};
alignas(256) std::array<std::uint32_t, 16> Table{};
alignas(256) std::array<std::byte, 1024> Pixels{};
alignas(256) std::array<std::uint32_t, 8> RecordDescriptor{};
alignas(256) constexpr std::array<std::uint16_t, 3> Indices{0, 1, 2};
std::uint32_t* Records = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 36> VertexCode{
    0x4a0814f9u, 0x84060608u,
    0xf4100204u, 0xfa000000u, 0xbf8cc07fu,
    0x7e000304u, 0x7e020305u, 0x7e04020au, 0x7e0602ffu, 0x12345678u,
    0x360c0087u, 0x4a0e0d06u, 0x4a0c0d07u, 0x4a0c0b06u, 0x340e0c84u,
    0xe0781000u, 0x80030007u,
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u,
    0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u,
    0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u,
    0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{
    0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 10> WriterCode{
    0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020203u, 0x7e040204u, 0x7e060205u,
    0xe0780000u, 0x80020000u, 0xbf810000u,
};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};
alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

std::uint32_t Low(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }
std::uint32_t High(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); }

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
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
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1u) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void State(std::vector<std::uint32_t>& commands, std::uint32_t wave, std::uint32_t base) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto color = reinterpret_cast<std::uintptr_t>(Pixels.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000u | (wave == 32u ? 0x400000u : 0u)},
        {0x1b6, wave == 32u ? 0x8000u : 0u}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040}, {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040}, {0x94, 0x80000000}, {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 4u << 1u},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
        {0x8c, Low(Table.data())}, {0x8d, High(Table.data())}, {0x8e, base}, {0x8f, 0u},
    };
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
}

void WriteRecord(std::vector<std::uint32_t>& commands, std::uint32_t base, std::uint32_t instances, std::uint32_t record = 0u) {
    const auto code = reinterpret_cast<std::uintptr_t>(WriterCode.data());
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(code >> 8u)});
    Append(commands, 0x76, {0x20d, static_cast<std::uint32_t>(code >> 40u)});
    for (const auto offset : {0x207u, 0x208u, 0x209u}) Append(commands, 0x76, {offset, 1u});
    Append(commands, 0x76, {0x212, 0});
    Append(commands, 0x76, {0x213, 6u << 1u});
    const auto* descriptor = RecordDescriptor.data() + record * 4u;
    const std::array<std::uint32_t, 6> words{Low(descriptor), High(descriptor), 3u, instances, 0u, base};
    for (std::uint32_t i = 0; i < words.size(); ++i) Append(commands, 0x76, {0x240u + i, words[i]});
    Append(commands, 0x15, {1, 1, 1, 0x8041u});
}

void Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the instance-input submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void Check(std::uint32_t base, std::uint32_t instances, std::uint32_t secondBase = 0u, std::uint32_t secondInstances = 0u) {
    std::array<std::uint32_t, Output.size()> expected{};
    expected.fill(Sentinel);
    for (const auto& [start, count] : {std::pair{base, instances}, std::pair{secondBase, secondInstances}}) {
        for (std::uint32_t instance = 0; instance < count; ++instance) {
            const auto index = instance + (start & 0xffffu);
            for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) {
                const auto at = ((index & 7u) * 3u + vertex) * 4u;
                expected[at] = index;
                expected[at + 1u] = vertex;
                expected[at + 2u] = Marker;
                expected[at + 3u] = 0x12345678u;
            }
        }
    }
    for (std::size_t i = 0; i < Output.size(); ++i) Require(Output[i] == expected[i],
        "base " + std::to_string(base) + " word " + std::to_string(i) + " is " + std::to_string(Output[i]) + ", expected " + std::to_string(expected[i]));
}

using Counts = decltype(AgcDriver::Graphics::IndirectDrawCounts());

void CheckPath(const Counts& before, AgcDriver::Graphics::IndirectDrawPath expected) {
    const auto after = AgcDriver::Graphics::IndirectDrawCounts();
    for (std::size_t path = 0; path < after.size(); ++path) Require(after[path] == before[path] + (path == static_cast<std::size_t>(expected) ? 1u : 0u),
        "unexpected indirect draw count for " + std::string(AgcDriver::Graphics::IndirectDrawPathName(static_cast<AgcDriver::Graphics::IndirectDrawPath>(path))) + ": " + std::to_string(after[path] - before[path]));
}

void Run() {
    Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Register(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    Table.fill(0u);
    Table[2] = Marker;
    const std::array<std::uint32_t, 4> descriptor{Low(Output.data()), High(Output.data()) & 0xffffu, sizeof(Output), 0x31016facu};
    std::copy(descriptor.begin(), descriptor.end(), Table.begin() + 4u);
    RecordDescriptor = {Low(Records), High(Records) & 0xffffu, AllocationBytes, 0x31016facu,
        Low(Records + 4u), High(Records + 4u) & 0xffffu, AllocationBytes - 16u, 0x31016facu};
    const auto selected = std::getenv("APS5_NO_GPU_INDIRECT_DRAW") != nullptr ? AgcDriver::Graphics::IndirectDrawPath::Disabled : AgcDriver::Graphics::IndirectDrawPath::Gpu;
    constexpr std::array<std::uint32_t, 7> bases{0u, 65535u, 65536u, 65537u, 917966u, 0x80000000u, 0xffffffffu};
    for (const auto wave : {32u, 64u}) {
        for (const auto base : bases) {
            const auto instances = base == 0xffffffffu ? 1u : 3u;
            for (const bool indirect : {false, true, false, true}) {
                Output.fill(Sentinel);
                std::fill_n(Records, 4u, 0u);
                std::vector<std::uint32_t> commands;
                State(commands, wave, indirect ? 0xbadc0ffeu : base);
                const auto before = AgcDriver::Graphics::IndirectDrawCounts();
                if (indirect) {
                    WriteRecord(commands, base, instances);
                    Append(commands, 0x11, {1, Low(Records), High(Records)});
                    Append(commands, 0x24, {0, 0x280, 0x8e, 2});
                } else {
                    Append(commands, 0x2f, {instances});
                    Append(commands, 0x2d, {3, 2});
                }
                Submit(commands);
                Check(base, instances);
                if (indirect) {
                    Require(Records[0] == 3u && Records[1] == instances && Records[2] == 0u && Records[3] == base, "the compute shader did not write the indirect record");
                    CheckPath(before, selected);
                }
            }
        }
    }
    for (const auto location : {0x8fu, 0x8eu, 0x8cu, 0x8du, 0x8fu}) {
        Output.fill(Sentinel);
        std::fill_n(Records, 4u, 0u);
        const bool pointer = location == 0x8cu || location == 0x8du;
        const auto patched = pointer ? location == 0x8cu ? Low(Table.data()) : High(Table.data()) : 0x80000000u;
        const auto base = location == 0x8eu ? patched : 5u;
        std::vector<std::uint32_t> commands;
        State(commands, 64u, location == 0x8eu ? 0xbadc0ffeu : base);
        WriteRecord(commands, patched, 3u);
        Append(commands, 0x11, {1, Low(Records), High(Records)});
        Append(commands, 0x24, {0, 0x280, location, 2});
        const auto before = AgcDriver::Graphics::IndirectDrawCounts();
        Submit(commands);
        Check(base, 3u);
        CheckPath(before, pointer ? AgcDriver::Graphics::IndirectDrawPath::NotFolded : selected);
    }
    Output.fill(Sentinel);
    std::fill_n(Records, 8u, 0u);
    std::vector<std::uint32_t> commands;
    State(commands, 64u, 0xbadc0ffeu);
    WriteRecord(commands, 65536u, 2u);
    WriteRecord(commands, 917966u, 2u, 1u);
    Append(commands, 0x11, {1, Low(Records), High(Records)});
    Append(commands, 0x2c, {0, 0x280, 0x8e, 0, 2, 0, 0, 16, 2});
    const auto before = AgcDriver::Graphics::IndirectDrawCounts();
    Submit(commands);
    Check(65536u, 2u, 917966u, 2u);
    CheckPath(before, selected);
    Output.fill(Sentinel);
    const std::array<std::uint32_t, 5> indexedRecord{3u, 3u, 0u, 0u, 917966u};
    std::copy(indexedRecord.begin(), indexedRecord.end(), Records);
    commands.clear();
    State(commands, 64u, 0xbadc0ffeu);
    Append(commands, 0x2a, {0});
    Append(commands, 0x26, {Low(Indices.data()), High(Indices.data())});
    Append(commands, 0x13, {3});
    Append(commands, 0x11, {1, Low(Records), High(Records)});
    Append(commands, 0x25, {0, 0x280, 0x8e, 0});
    const auto indexedBefore = AgcDriver::Graphics::IndirectDrawCounts();
    Submit(commands);
    Check(917966u, 3u);
    CheckPath(indexedBefore, selected);
    std::puts("GPU-written single/multi-record and indexed draws retained masked arithmetic across direct/indirect and SGPR-route alternation; resource-pointer patches retained CPU fallback");
}

void Cleanup() {
    AgcDriverShutdown_nid_postfix();
    if (Records != nullptr) {
        {
            GuestAllocations::Mutation mutation;
            mutation.RequireUnpinned(Records, AllocationBytes);
            mutation.Remove(Records);
        }
        ::operator delete(Records, std::align_val_t{AllocationBytes});
        Records = nullptr;
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--late-fallback") {
#ifdef _WIN32
            _putenv_s("APS5_NO_GPU_INDIRECT_DRAW", "1");
#else
            setenv("APS5_NO_GPU_INDIRECT_DRAW", "1", 1);
#endif
        }
        const auto enable = [](const char* name) {
#ifdef _WIN32
            _putenv_s(name, "1");
#else
            setenv(name, "1", 1);
#endif
        };
        enable("APS5_PROFILE_DRAW");
        if (argc == 2 && std::string(argv[1]) == "--verify") {
            enable("APS5_VERIFY_DRAW_ENTRIES");
            enable("APS5_VERIFY_DRAW_DATA_HITS");
            enable("APS5_VERIFY_DRAW_RECIPE");
        }
        if (argc == 2 && std::string(argv[1]) == "--fallback-key") enable("APS5_NO_DRAW_KEY");
        if (argc == 2 && std::string(argv[1]) == "--uncached") enable("APS5_NO_DRAW_SRT_ENTRIES");
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Records = static_cast<std::uint32_t*>(::operator new(AllocationBytes, std::align_val_t{AllocationBytes}));
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(Records, AllocationBytes, true, true, true);
        }
        Run();
        Cleanup();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Cleanup();
        return 1;
    }
}
