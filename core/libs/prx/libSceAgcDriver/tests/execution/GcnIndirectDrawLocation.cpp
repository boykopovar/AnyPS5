#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
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
#include <exception>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using Words = std::vector<std::pair<std::size_t, std::size_t>>;

constexpr std::uint32_t Width = 16;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Extent = (Height << 16u) | Width;
constexpr std::size_t TargetBytes = std::size_t{Width} * Height * 4u;
constexpr std::size_t TargetsBytes = 3u * TargetBytes;
constexpr std::size_t PositionsOffset = 0x1000;
constexpr std::size_t BlockBytes = 0x10000;
constexpr std::uint8_t Kept = 0x40;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t Thin1D = 13u;
constexpr std::uint32_t DepthSgpr = 4;
constexpr std::uint32_t DepthLocation = 0x4cu + DepthSgpr;
constexpr std::uint32_t Clipped = 0x40000000u;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xe0382000u, 0x80000000u,
    0xbf8c0f70u,
    0x7e040200u | DepthSgpr,
    0xf80008cfu, 0x03020100u,
    0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 5> PixelCode{0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};
alignas(256) constexpr std::array<std::array<float, 4>, 3> Positions{{{-1.0f, -1.0f, 0.5f, 1.0f}, {0.9f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.7f, 0.5f, 1.0f}}};
alignas(256) constexpr std::array<std::uint32_t, 8> Records{3, 1, 0, 0, 3, 1, Clipped, 0};

class LowBlock {
public:
    LowBlock() {
#if defined(_WIN32) || defined(__APPLE__)
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
#ifdef _WIN32
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uint32_t ReadWriteProtection = 0x04;
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, ReadWriteProtection, BlockBytes);
#endif
#else
#ifdef ANYPS5_ENABLE_ASAN
        constexpr std::uintptr_t FirstHint = 0x60000000;
#else
        constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
        for (std::uintptr_t hint = FirstHint; data == nullptr && hint < GcnAddressLimit; hint += std::uintptr_t{1} << 32u) {
            void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped != MAP_FAILED) data = static_cast<std::uint8_t*>(mapped);
        }
        if (data != nullptr && AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        if (data == nullptr) return;
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~LowBlock() {
        if (data == nullptr) return;
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(data);
#if defined(_WIN32) || defined(__APPLE__)
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    bool Usable() const { return data != nullptr && Address() + BlockBytes <= GcnAddressLimit; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }
    std::uint64_t At(std::size_t offset) const { return Address() + offset; }

private:
    std::uint8_t* data = nullptr;
};

std::array<std::uint32_t, 4> PositionBuffer(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(sizeof(Positions)), 0xfacu | (7u << 12u) | (14u << 15u)};
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> DrawThree(const LowBlock& block) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto records = reinterpret_cast<std::uintptr_t>(Records.data());
    const auto buffer = PositionBuffer(block.At(PositionsOffset));
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0}, {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4},
        {0x319, Width / 8u - 1u}, {0x31a, Width * Height / 64u - 1u}, {0x31b, 0}, {0x31c, (Format8888 << 2u) | BlendClamp}, {0x31d, Thin1D},
        {0x1e0, 0},
        {0xc, 0}, {0xd, Extent},
        {0x81, 0x80000000}, {0x82, Extent},
        {0x90, 0x80000000}, {0x91, Extent},
        {0x94, 0x80000000}, {0x95, Extent},
        {0x10f, std::bit_cast<std::uint32_t>(8.0f)}, {0x110, std::bit_cast<std::uint32_t>(8.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-8.0f)}, {0x112, std::bit_cast<std::uint32_t>(8.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0x48, static_cast<std::uint32_t>(vertex >> 8u)}, {0x49, static_cast<std::uint32_t>(vertex >> 40u)}, {0x4a, 0}, {0x4b, 5u << 1u},
        {0x4c, buffer[0]}, {0x4d, buffer[1]}, {0x4e, buffer[2]}, {0x4f, buffer[3]},
        {0x08, static_cast<std::uint32_t>(pixel >> 8u)}, {0x09, static_cast<std::uint32_t>(pixel >> 40u)}, {0x0a, 0}, {0x0b, 0},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x11, {1, static_cast<std::uint32_t>(records), static_cast<std::uint32_t>(records >> 32u)});
    const auto target = [&](std::size_t index, std::uint32_t depth) {
        Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(block.At(index * TargetBytes) >> 8u)});
        Append(commands, 0x76, {DepthLocation, depth});
    };
    target(0, 0);
    Append(commands, 0x2d, {3, 2});
    target(1, Clipped);
    Append(commands, 0x24, {0, DepthLocation, 0, 2});
    target(2, 0);
    Append(commands, 0x24, {16, DepthLocation, 0, 2});
    return commands;
}

std::optional<std::size_t> RecordsSetTheDepthWord() {
    const LowBlock block;
    if (!block.Usable()) {
        std::printf("skipped, no guest block below the 40-bit GCN color base range (0x%llx)\n", static_cast<unsigned long long>(block.Address()));
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(TargetsBytes, Kept);
    AgcDriver::GuestMemory::Write(block.Address(), std::as_bytes(std::span(bytes)), 256);
    AgcDriver::GuestMemory::Write(block.At(PositionsOffset), std::as_bytes(std::span(Positions)), 16);
    auto commands = DrawThree(block);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the GCN draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), TargetsBytes, nullptr, "test read-back");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(bytes)), 256);
    const auto covered = [&](std::size_t index) { return static_cast<std::size_t>(std::count_if(bytes.begin() + static_cast<std::ptrdiff_t>(index * TargetBytes), bytes.begin() + static_cast<std::ptrdiff_t>((index + 1u) * TargetBytes), [](std::uint8_t value) { return value != Kept; })); };
    const auto direct = covered(0);
    Require(direct != 0 && direct != TargetBytes, "the direct GCN draw with depth 0 covered " + std::to_string(direct) + " of " + std::to_string(TargetBytes) + " bytes, expected an edge through the target");
    for (std::size_t index = 0; index < TargetBytes; ++index) {
        Require(bytes[TargetBytes + index] == bytes[index], "byte " + std::to_string(index) + " of the GCN indirect draw whose record sets depth 0 in VS user data register 0x50 over a clipped register value is " + std::to_string(bytes[TargetBytes + index]) + ", the direct draw wrote " + std::to_string(bytes[index]));
    }
    const auto clipped = covered(2);
    Require(clipped == 0, "the GCN indirect draw whose record sets a clipped depth in VS user data register 0x50 over a register value of 0 covered " + std::to_string(clipped) + " bytes, expected none");
    return direct;
}

void Bind(AgcDriver::QueueState& queue, const void* code, std::uint32_t program, std::uint32_t resources, std::span<const std::uint32_t> users) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    queue.shader[program] = static_cast<std::uint32_t>(address >> 8u);
    queue.shader[program + 1u] = static_cast<std::uint32_t>(address >> 40u);
    queue.shader[resources] = static_cast<std::uint32_t>(users.size()) << 1u;
    for (std::size_t i = 0; i < users.size(); ++i) queue.shader[resources + 1u + static_cast<std::uint32_t>(i)] = users[i];
}

void LocationsNameTheVertexSideWords() {
    using namespace AgcDriver::DriverDetail;
    constexpr std::array<std::uint32_t, 5> vertexUsers{0x11110001u, 0x11110002u, 0x11110003u, 0x11110004u, 0x11110005u};
    const ShaderRegistry registry;
    AgcDriver::QueueState plain{};
    plain.context[0x31c] = Format8888 << 2u;
    plain.context[0x8e] = 0xfu;
    plain.context[0x8f] = 0xfu;
    Bind(plain, VertexCode.data(), 0x048u, 0x04bu, vertexUsers);
    Bind(plain, PixelCode.data(), 0x008u, 0x00bu, {});
    DrawDecode vertex{};
    vertex.state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt, 1.0f, 64.0f};
    vertex.state.stages.legacyVertex = true;
    vertex.pixel.targetOutputMode[0] = 9;
    vertex.pixel.targetExportMapping.fill(0xe4u);
    DecodeGraphicsPrograms(vertex, plain, registry, false, true);
    Require(vertex.programs.size() == 2u && vertex.programs[0].userDataBase == 0x4cu && vertex.programs[0].firstUserSgpr == 0u && vertex.programs[0].hiddenUserWords == 0u && std::ranges::equal(vertex.programs[0].userData, vertexUsers), "the GCN VS program did not decode with its user data from register 0x4c on");
    Require(UserDataWords(vertex.programs, vertex.roles, 0x50u) == Words{{0u, 4u}}, "GCN VS user data register 0x50 does not name the VS program's user word 4");
    Require(UserDataWords(vertex.programs, vertex.roles, 0x51u).empty(), "a GCN VS user data register past the program's user words names a word");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        AgcDriverWaitForPipelineJournal_nid_postfix();
        ShaderRecompiler::SetGcnGuestShaders(true);
        const auto covered = RecordsSetTheDepthWord();
        LocationsNameTheVertexSideWords();
        AgcDriverShutdown_nid_postfix();
        if (!covered) return VulkanTestSkipped;
        std::printf("GCN indirect draw location tests passed: an indirect draw naming VS user data register 0x50 drew with the depth word its record set (%zu of %zu bytes as the direct draw, none for the clipped record), and VS user data register 0x50 names the VS program's user word 4\n", *covered, TargetBytes);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
