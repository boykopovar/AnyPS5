#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "SceShaders.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;
using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 0x40000;
constexpr std::uint64_t TableOffset = 0x1000;
constexpr std::uint32_t TableWords = 36;
constexpr std::array<std::uint64_t, 3> ColorOffsets{0x8000, 0x9000, 0xa000};
constexpr std::size_t ColorBytes = 1024;
constexpr std::uint32_t DumpOpcode = 0x83;

alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> ConstantPixelCode{0xf4000080u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

alignas(256) std::array<std::uint32_t, 16> TableVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) std::array<float, 4> OneConstant{1.0f, 0.0f, 0.0f, 0.0f};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

struct VertexTableHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    ShaderSemantic semantic{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) VertexTableHeader TableHeader{};

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

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "queued constant RAM dump: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void AppendState(std::vector<std::uint32_t>& commands) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(TriangleVertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(ConstantPixelCode.data());
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
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 2u << 1u},
    };
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
}

void AppendConstantDraw(std::vector<std::uint32_t>& commands, std::uint64_t color, std::uint64_t constant) {
    Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
    Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(constant), static_cast<std::uint32_t>(constant >> 32u)});
    Append(commands, 0x2d, {3, 2});
}

std::vector<std::uint32_t> Table(float constant) {
    std::vector<std::uint32_t> table(TableWords);
    for (std::uint32_t word = 0; word < TableWords; ++word) table[word] = 0x10000u + word;
    table[0] = std::bit_cast<std::uint32_t>(constant);
    return table;
}

void AppendConstantRamDump(std::vector<std::uint32_t>& commands, const std::vector<std::uint32_t>& table, std::uint64_t destination) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(table.size()) << 16u) | (0x81u << 8u));
    commands.push_back(0x100);
    commands.insert(commands.end(), table.begin(), table.end());
    Append(commands, DumpOpcode, {0x100, static_cast<std::uint32_t>(table.size()), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u)});
}

std::uint64_t DrainRequests() {
    std::uint64_t total = 0;
    for (std::size_t reason = 0; reason < static_cast<std::size_t>(DrawPipeline::DrainReason::Count); ++reason) total += DrawPipeline::DrainRequestsByReason(static_cast<DrawPipeline::DrainReason>(reason));
    return total;
}

template <typename Ready>
void WaitUntil(Ready ready, const std::string& what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!ready()) {
        Require(std::chrono::steady_clock::now() < deadline, "queued constant RAM dump: timed out waiting for " + what);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::uint8_t Brightest(const GuestBlock& block, std::uint64_t offset) {
    AgcDriver::GuestMemory::FlushGpuWrites(block.Address() + offset, ColorBytes);
    return *std::max_element(block.Data() + offset, block.Data() + offset + ColorBytes);
}

int DrawsReadQueuedDumps() {
    const GuestBlock block;
    Register(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Register(PixelHeader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
    const auto destination = block.Address() + TableOffset;
    const auto before = Table(0.25f);
    std::memcpy(block.Data() + TableOffset, before.data(), TableWords * 4u);
    const auto first = Table(1.0f);
    const auto second = Table(0.2f);
    std::vector<std::uint32_t> commands;
    AppendState(commands);
    AppendConstantDraw(commands, block.Address() + ColorOffsets[0], destination);
    AppendConstantRamDump(commands, first, destination);
    AppendConstantDraw(commands, block.Address() + ColorOffsets[1], destination);
    AppendConstantRamDump(commands, second, destination);
    AppendConstantDraw(commands, block.Address() + ColorOffsets[2], destination);

    WaitUntil([] { return DrawPipeline::QueuedItems() == 0; }, "the pipeline to empty");
    const auto drainsBefore = DrainRequests();
    const auto dumpDrainsBefore = DrawPipeline::DrainRequestsByOpcode(DumpOpcode);
    const auto captureDrainsBefore = DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture);
    const auto knownBefore = AgcDriver::ShaderMemory::KnownValues();
    std::atomic<bool> released{false};
    struct Release {
        std::atomic<bool>& flag;
        ~Release() { flag.store(true, std::memory_order_release); }
    } release{released};
    DrawPipeline::Queue0().Enqueue([&released] {
        while (!released.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "queued constant RAM dump: the submission failed");
    WaitUntil([&] { return DrawPipeline::QueuedItems() >= 6 || DrainRequests() != drainsBefore; }, "three draws and two dumps queued behind the held committer, or a drain");
    const auto dumpDrains = DrawPipeline::DrainRequestsByOpcode(DumpOpcode) - dumpDrainsBefore;
    const auto captureDrains = DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture) - captureDrainsBefore;
    const auto drains = DrainRequests() - drainsBefore;
    released.store(true, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    const auto known = AgcDriver::ShaderMemory::KnownValues();

    AgcDriver::GuestMemory::FlushGpuWrites(destination, TableWords * 4u);
    Require(std::memcmp(block.Data() + TableOffset, second.data(), TableWords * 4u) == 0, "queued constant RAM dump: the table in guest memory is not the second dump's");
    const std::array<std::uint8_t, 3> expected{64, 255, 51};
    for (std::size_t draw = 0; draw < expected.size(); ++draw) {
        const auto brightest = Brightest(block, ColorOffsets[draw]);
        Require(brightest == expected[draw], "queued constant RAM dump: draw " + std::to_string(draw) + " wrote " + std::to_string(brightest) + ", expected " + std::to_string(expected[draw]) + " (draw 0 reads the table before both dumps, draw 1 after the first, draw 2 after the second)");
    }
    Require(dumpDrains == 0, "queued constant RAM dump: a DUMP_CONST_RAM drained the draw pipeline " + std::to_string(dumpDrains) + " times");
    Require(known.served > knownBefore.served, "queued constant RAM dump: no capture word was served from a queued dump");
    if (std::getenv("APS5_VERIFY_KNOWN_VALUES") != nullptr) {
        Require(known.mismatches == knownBefore.mismatches, "queued constant RAM dump: a word served from a queued dump differs from what the dump stored");
    } else {
        Require(captureDrains == 0, "queued constant RAM dump: a capture after a queued dump drained the draw pipeline " + std::to_string(captureDrains) + " times instead of reading the queued bytes");
        Require(drains == 0, "queued constant RAM dump: the submission drained the draw pipeline " + std::to_string(drains) + " times");
    }
    std::cout << "three draws around two queued constant RAM dumps read " << known.served - knownBefore.served << " dumped words without draining the draw pipeline\n";
    return 0;
}

void RegisterTableVertex() {
    TableHeader.direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    TableHeader.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 4;
    TableHeader.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 6;
    TableHeader.userData.direct_resource_offset = TableHeader.direct.data();
    TableHeader.userData.direct_resource_count = ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT;
    TableHeader.semantic.size_in_elements = 1;
    TableHeader.shader.file_header = 0x34333231u;
    TableHeader.shader.version = 0x18u;
    TableHeader.shader.header_size = sizeof(VertexTableHeader);
    TableHeader.shader.shader_size = static_cast<std::uint32_t>(sizeof(TableVertexCode));
    TableHeader.shader.code = TableVertexCode.data();
    TableHeader.shader.type = 2;
    TableHeader.shader.user_data = &TableHeader.userData;
    TableHeader.shader.input_semantics = &TableHeader.semantic;
    TableHeader.shader.num_input_semantics = 1;
    AgcDriverRegisterShader_nid_postfix(&TableHeader.shader);
}

int VertexTableDecodeWaitsForQueuedDump() {
    constexpr std::uint64_t BufferTableOffset = 0x2000;
    constexpr std::uint64_t AttributeTableOffset = 0x2100;
    constexpr std::uint64_t VertexDataOffset = 0x3000;
    const GuestBlock block;
    RegisterTableVertex();
    Register(PixelHeader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
    const auto bufferTable = block.Address() + BufferTableOffset;
    const auto attributeTable = block.Address() + AttributeTableOffset;
    const auto vertexData = block.Address() + VertexDataOffset;
    const std::array<std::uint32_t, 8> sharps{static_cast<std::uint32_t>(vertexData), static_cast<std::uint32_t>(vertexData >> 32u) | (16u << 16u), 3u, 0u, static_cast<std::uint32_t>(vertexData + 0x100), static_cast<std::uint32_t>((vertexData + 0x100) >> 32u) | (16u << 16u), 3u, 0u};
    std::memcpy(block.Data() + BufferTableOffset, sharps.data(), sizeof(sharps));
    const std::vector<std::uint32_t> dumped{1u};
    std::vector<std::uint32_t> commands;
    AppendState(commands);
    const auto vertex = reinterpret_cast<std::uintptr_t>(TableVertexCode.data());
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u), static_cast<std::uint32_t>(vertex >> 40u)});
    Append(commands, 0x76, {0x8b, 8u << 1u});
    Append(commands, 0x76, {0x8c, 0, 0, 0, 0, static_cast<std::uint32_t>(bufferTable), static_cast<std::uint32_t>(bufferTable >> 32u), static_cast<std::uint32_t>(attributeTable), static_cast<std::uint32_t>(attributeTable >> 32u)});
    AppendConstantRamDump(commands, dumped, attributeTable);
    AppendConstantDraw(commands, block.Address() + ColorOffsets[0], reinterpret_cast<std::uintptr_t>(OneConstant.data()));

    WaitUntil([] { return DrawPipeline::QueuedItems() == 0; }, "the pipeline to empty");
    const auto drainsBefore = DrainRequests();
    const auto dumpDrainsBefore = DrawPipeline::DrainRequestsByOpcode(DumpOpcode);
    const auto captureDrainsBefore = DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture);
    std::atomic<bool> released{false};
    struct Release {
        std::atomic<bool>& flag;
        ~Release() { flag.store(true, std::memory_order_release); }
    } release{released};
    DrawPipeline::Queue0().Enqueue([&released] {
        while (!released.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "queued constant RAM dump: the vertex table submission failed");
    WaitUntil([&] { return DrawPipeline::QueuedItems() >= 3 || DrainRequests() != drainsBefore; }, "the dump and the draw queued behind the held committer, or a drain");
    const auto dumpDrains = DrawPipeline::DrainRequestsByOpcode(DumpOpcode) - dumpDrainsBefore;
    released.store(true, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    const auto captureDrains = DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture) - captureDrainsBefore;

    AgcDriver::GuestMemory::FlushGpuWrites(attributeTable, sizeof(std::uint32_t));
    Require(std::memcmp(block.Data() + AttributeTableOffset, dumped.data(), sizeof(std::uint32_t)) == 0, "queued constant RAM dump: the dumped vertex attribute table did not land in guest memory");
    const auto brightest = Brightest(block, ColorOffsets[0]);
    Require(brightest == 255, "queued constant RAM dump: the draw with vertex tables wrote " + std::to_string(brightest) + ", expected 255");
    Require(dumpDrains == 0, "queued constant RAM dump: the DUMP_CONST_RAM of a vertex attribute table drained the draw pipeline " + std::to_string(dumpDrains) + " times");
    Require(captureDrains >= 1, "queued constant RAM dump: the draw decoded its vertex attribute table while a queued dump still wrote it, without waiting for the dump");
    std::cout << "the vertex table decode of a draw waited for the queued dump of its attribute table\n";
    return 0;
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "8");
#else
    setenv("APS5_PIPELINED_DRAWS", "8", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        auto result = DrawsReadQueuedDumps();
        if (result == 0) result = VertexTableDecodeWaitsForQueuedDump();
        AgcDriverShutdown_nid_postfix();
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
