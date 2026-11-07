#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 0x40000;
constexpr std::uint64_t TableOffset = 0x1000;
constexpr std::uint32_t TableWords = 36;
constexpr std::uint64_t ColorOffset = 0x8000;
constexpr std::size_t ColorBytes = 1024;
constexpr std::uint64_t FillOffset = 0x20000;
constexpr std::uint32_t FillBytes = 0x20000;
constexpr std::uint64_t LabelOffset = 0x3000;

alignas(256) constexpr std::array<std::uint32_t, 16> TriangleVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> ConstantPixelCode{0xf4000080u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};
alignas(256) std::array<float, 4> FirstConstant{0.25f, 0.0f, 0.0f, 0.0f};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

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
        Require(block != nullptr, "dumped constant capture: cannot allocate the guest block");
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

void AppendState(std::vector<std::uint32_t>& commands, std::uint64_t color) {
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
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
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

void AppendConstantDraw(std::vector<std::uint32_t>& commands, std::uint64_t constant) {
    Append(commands, 0x76, {0x00c, static_cast<std::uint32_t>(constant), static_cast<std::uint32_t>(constant >> 32u)});
    Append(commands, 0x2d, {3, 2});
}

void AppendConstantRamDump(std::vector<std::uint32_t>& commands, const std::vector<std::uint32_t>& table, std::uint64_t destination) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(table.size()) << 16u) | (0x81u << 8u));
    commands.push_back(0x100);
    commands.insert(commands.end(), table.begin(), table.end());
    Append(commands, 0x83, {0x100, static_cast<std::uint32_t>(table.size()), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u)});
}

int DumpedConstantIsServedFromTheStore() {
    const GuestBlock block;
    Register(VertexHeader, TriangleVertexCode.data(), sizeof(TriangleVertexCode), 2);
    Register(PixelHeader, ConstantPixelCode.data(), sizeof(ConstantPixelCode), 1);
    std::vector<std::uint32_t> table(TableWords);
    for (std::uint32_t word = 0; word < TableWords; ++word) table[word] = 0x10000u + word;
    table[0] = std::bit_cast<std::uint32_t>(1.0f);
    const auto destination = block.Address() + TableOffset;
    const auto fill = block.Address() + FillOffset;
    std::vector<std::uint32_t> commands;
    AppendState(commands, block.Address() + ColorOffset);
    AppendConstantDraw(commands, reinterpret_cast<std::uintptr_t>(FirstConstant.data()));
    Append(commands, 0x50, {0x40000000u, 0x5a5a5a5au, 0, static_cast<std::uint32_t>(fill), static_cast<std::uint32_t>(fill >> 32u), FillBytes});
    AppendConstantRamDump(commands, table, destination);
    AppendConstantDraw(commands, destination);
    const auto storesBefore = AgcDriver::Graphics::Recorder::StoreCounts().stores;
    const auto knownBefore = AgcDriver::ShaderMemory::KnownValues();
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "dumped constant capture: the submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto stored = AgcDriver::Graphics::Recorder::StoreCounts().stores - storesBefore;
    const auto known = AgcDriver::ShaderMemory::KnownValues();
    AgcDriver::GuestMemory::FlushGpuWrites(destination, TableWords * 4u);
    Require(std::memcmp(block.Data() + TableOffset, table.data(), TableWords * 4u) == 0, "dumped constant capture: the constant RAM dump did not land in guest memory");
    AgcDriver::GuestMemory::FlushGpuWrites(block.Address() + ColorOffset, ColorBytes);
    const auto brightest = *std::max_element(block.Data() + ColorOffset, block.Data() + ColorOffset + ColorBytes);
    Require(brightest == 255, "dumped constant capture: the draw reading the dumped constant 1.0 wrote " + std::to_string(brightest) + ", expected 255");
    if (stored == 0) {
        std::puts("skipped, the constant RAM dump was not recorded on the GPU (the guest block is not host-imported)");
        return VulkanTestSkipped;
    }
    Require(known.served > knownBefore.served, "dumped constant capture: the capture of the draw after a constant RAM dump recorded on the GPU waited for the GPU instead of reading the dumped words");
    if (std::getenv("APS5_VERIFY_KNOWN_VALUES") != nullptr) Require(known.mismatches == knownBefore.mismatches, "dumped constant capture: a word served from the recorded dump differs from what the GPU stored");
    std::cout << "a capture after a constant RAM dump read " << known.served - knownBefore.served << " dumped words without waiting for the GPU\n";
    return 0;
}

int CpuWriteAfterTheDumpIsRead() {
    const GuestBlock block;
    std::vector<std::uint32_t> table(TableWords);
    for (std::uint32_t word = 0; word < TableWords; ++word) table[word] = 0x20000u + word;
    table[0] = std::bit_cast<std::uint32_t>(1.0f);
    const auto destination = block.Address() + TableOffset;
    const auto label = block.Address() + LabelOffset;
    const auto fill = block.Address() + FillOffset;
    const auto storesBefore = AgcDriver::Graphics::Recorder::StoreCounts().stores;
    std::vector<std::uint32_t> dump;
    Append(dump, 0x50, {0x40000000u, 0x5a5a5a5au, 0, static_cast<std::uint32_t>(fill), static_cast<std::uint32_t>(fill >> 32u), FillBytes});
    AppendConstantRamDump(dump, table, destination);
    Append(dump, 0x37, {0x00100200u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), 1});
    Packet dumpPacket{dump.data(), static_cast<std::uint32_t>(dump.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dumpPacket) == 0, "cpu write after the dump: the dump submission failed");
    volatile const std::uint32_t* landed = reinterpret_cast<volatile const std::uint32_t*>(block.Data() + LabelOffset);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (*landed != 1) {
        Require(std::chrono::steady_clock::now() < deadline, "cpu write after the dump: the label after the dump never landed");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    const auto stored = AgcDriver::Graphics::Recorder::StoreCounts().stores - storesBefore;
    const auto rewritten = std::bit_cast<std::uint32_t>(0.5f);
    std::memcpy(block.Data() + TableOffset, &rewritten, sizeof(rewritten));
    std::vector<std::uint32_t> commands;
    AppendState(commands, block.Address() + ColorOffset);
    AppendConstantDraw(commands, destination);
    Packet drawPacket{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&drawPacket) == 0, "cpu write after the dump: the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(block.Address() + ColorOffset, ColorBytes);
    const auto brightest = *std::max_element(block.Data() + ColorOffset, block.Data() + ColorOffset + ColorBytes);
    if (stored == 0) {
        std::puts("skipped, the constant RAM dump was not recorded on the GPU (the guest block is not host-imported)");
        return VulkanTestSkipped;
    }
    Require(brightest == 127 || brightest == 128, "cpu write after the dump: the draw after the guest rewrote a dumped constant to 0.5 once the label behind the dump had landed wrote " + std::to_string(brightest) + ", expected 127 or 128 (255 is the dumped 1.0)");
    std::cout << "a capture after a CPU write over a dumped constant read the CPU's value\n";
    return 0;
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "0");
#else
    setenv("APS5_PIPELINED_DRAWS", "0", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        auto result = DumpedConstantIsServedFromTheStore();
        if (result == 0) result = CpuWriteAfterTheDumpIsRead();
        AgcDriverShutdown_nid_postfix();
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
