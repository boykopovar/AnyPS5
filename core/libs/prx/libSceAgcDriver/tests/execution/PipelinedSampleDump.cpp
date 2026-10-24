#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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

constexpr std::size_t BlockBytes = 131072;
constexpr std::size_t SlotsOffset = 65536;
constexpr std::size_t SlotCount = 64;
constexpr std::uint64_t SampleReady = 1ull << 63u;
constexpr std::uint32_t FirstFill = 0x5eed1234u;
constexpr std::uint32_t LaterFill = 0x0badf00du;

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(256) volatile std::uint32_t Done = 0;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "pipelined sample dump: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Target() const { return block; }
    std::uint64_t* Slots() const { return reinterpret_cast<std::uint64_t*>(block + SlotsOffset); }

private:
    std::uint8_t* block = nullptr;
};

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

void Draw(std::vector<std::uint32_t>& commands, const void* target) {
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
        {0x3b8, 0x9000000u | (0x1bu << 14u)}, {0x1e0, 0},
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
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    const auto color = reinterpret_cast<std::uintptr_t>(target);
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
    Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
    Append(commands, 0x2d, {3, 2});
}

void Fill(std::vector<std::uint32_t>& commands, std::uint32_t data, const void* destination, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    Append(commands, 0x50, {2u << 29u, data, 0u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), bytes});
}

void Dump(std::vector<std::uint32_t>& commands, const void* destination) {
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    Append(commands, 0x46, {0x139u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)});
}

void Submit(std::vector<std::uint32_t>& commands) {
    const auto done = reinterpret_cast<std::uintptr_t>(&Done);
    Done = 0;
    Append(commands, 0x37, {0x00100200u, static_cast<std::uint32_t>(done), static_cast<std::uint32_t>(done >> 32u), 1u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(Done == 1, "the submission did not finish");
}

std::uint64_t Count(const std::uint64_t* slots, std::size_t slot) {
    Require((slots[slot] & SampleReady) != 0, "slot " + std::to_string(slot) + " has no ready bit: its dump did not land after the fill before it");
    return slots[slot] & ~SampleReady;
}

void DumpsCountTheDrawsBeforeThem(const GuestBlock& guest) {
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
    std::vector<std::uint32_t> warmUp;
    Draw(warmUp, guest.Target());
    Submit(warmUp);
    const bool queued = std::getenv("APS5_PIPELINE_DRAIN_SAMPLE_DUMPS") == nullptr;
    const auto queuedBefore = AgcDriver::DriverDetail::sampleDumpsQueued.load();
    auto* slots = guest.Slots();
    std::vector<std::uint32_t> commands;
    Fill(commands, FirstFill, slots, SlotCount * sizeof(std::uint64_t));
    Dump(commands, &slots[0]);
    Draw(commands, guest.Target());
    Dump(commands, reinterpret_cast<const std::byte*>(slots) + 8);
    Dump(commands, &slots[32]);
    Dump(commands, reinterpret_cast<const std::byte*>(&slots[32]) + 8);
    Fill(commands, LaterFill, &slots[63], 8);
    Submit(commands);
    const auto begin = Count(slots, 0);
    const auto end = Count(slots, 1);
    Require(end > begin, "the end dump counted " + std::to_string(end - begin) + " samples for the draw before it");
    Require(Count(slots, 32) == end && Count(slots, 33) == end, "the dumps after the end dump counted " + std::to_string(Count(slots, 32)) + " and " + std::to_string(Count(slots, 33)) + " samples, expected " + std::to_string(end));
    for (std::size_t slot = 2; slot < SlotCount - 1; ++slot) {
        if (slot == 32 || slot == 33) continue;
        Require(slots[slot] == SampleReady, "slot " + std::to_string(slot) + " is not an empty ready block");
    }
    Require(slots[SlotCount - 1] == ((static_cast<std::uint64_t>(LaterFill) << 32u) | LaterFill), "the fill after the last dump landed before it");
    const auto dumps = AgcDriver::DriverDetail::sampleDumpsQueued.load() - queuedBefore;
    Require(dumps == (queued ? 4u : 0u), std::to_string(dumps) + " of 4 dumps were queued behind the draws");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
    _putenv_s("APS5_PIPELINE_DMA", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
    setenv("APS5_PIPELINE_DMA", "1", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        {
            const GuestBlock guest;
            DumpsCountTheDrawsBeforeThem(guest);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("pipelined sample dump tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
