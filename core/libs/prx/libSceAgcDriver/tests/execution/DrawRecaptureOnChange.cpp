#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TableOffset = 0x300;
constexpr std::size_t MovedOffset = 0x800;
constexpr std::size_t ShiftOffset = 0x900;
constexpr std::size_t StillOffset = 0xa00;
constexpr std::size_t ColorBytes = 65536;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;

alignas(256) constexpr std::array<std::uint32_t, 40> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u,
    0xf4080104u, 0xfa000000u, 0xf4080204u, 0xfa000010u, 0xf4080304u, 0xfa000020u,
    0xbf8cc07fu,
    0xe00c2000u, 0x80010a05u, 0xe00c2000u, 0x80020e05u, 0xe00c2000u, 0x80031205u,
    0xbf8c3f70u,
    0x0602030au, 0x0604050bu, 0x0606070cu, 0x0608090du,
    0x0602030eu, 0x0604050fu, 0x06060710u, 0x06080911u,
    0x06020312u, 0x06040513u, 0x06060714u, 0x06080915u,
    0xf80008cfu, 0x04030201u,
    0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelRead{0x7e1a0280u, 0xdc308000u, 0x0c02000du, 0xbf8c3f70u, 0x7e10030cu, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};

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
        Require(block != nullptr, "draw recapture: cannot allocate a guest block");
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

std::array<std::uint32_t, 4> VertexDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 3u, 0x0104dfacu};
}

void WriteVertices(const GuestBlock& block, std::size_t offset, float x) {
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        const std::array<float, 4> value{x, 0.0f, 0.0f, 0.0f};
        std::memcpy(block.Bytes(offset + vertex * 16u), value.data(), sizeof(value));
    }
}

void WriteDescriptor(const GuestBlock& block, std::size_t slot, std::array<std::uint32_t, 4> descriptor) {
    std::memcpy(block.Bytes(TableOffset + slot * 16u), descriptor.data(), sizeof(descriptor));
}

const GuestBlock* TableBlock = nullptr;
std::atomic<bool> TableChangeArmed{false};
std::atomic<int> TableChanges{0};

void ChangeTableBeforeCheck() {
    if (!TableChangeArmed.exchange(false)) return;
    WriteDescriptor(*TableBlock, 0, VertexDescriptor(TableBlock->At(MovedOffset)));
    TableChanges.fetch_add(1);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void SubmitDraw(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target) {
    const auto color = target.At(0);
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelRead.data());
    const auto table = vertices.At(TableOffset);
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
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : std::initializer_list<std::pair<std::uint32_t, std::uint32_t>>{{0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 2u << 1u}, {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)}, {0x00b, 8u << 1u}, {0x00c, 0}, {0x00d, 0}}) Append(commands, 0x76, {offset, value});
    Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
    Append(commands, 0x76, {0x00e, static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u)});
    Append(commands, 0x2d, {3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "draw recapture: the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

unsigned Brightest(const GuestBlock& target) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(target.At(0)), ColorBytes);
    unsigned brightest = 0;
    for (std::size_t i = 0; i < ColorBytes; ++i) brightest = std::max(brightest, static_cast<unsigned>(target.Bytes(0)[i]));
    return brightest;
}

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("draw recapture: " + what);
}

void ChangedTableIsBound(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target) {
    std::memset(target.Bytes(0), 0, ColorBytes);
    WriteDescriptor(vertices, 0, {});
    const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    TableChangeArmed.store(true);
    SubmitDraw(vertices, data, target);
    const auto brightest = Brightest(target);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    std::printf("a draw whose descriptor table changed from a zero V# to a valid one after its capture: the table changed %d time(s), brightest color byte %u (%s), %llu packet(s) prepared again\n", TableChanges.load(), brightest, brightest == 255u ? "the valid V# was bound" : "the zero V# captured before the change was bound", static_cast<unsigned long long>(redone));
    Expect(TableChanges.load() == 1, "the check seam did not run before the draw was recorded");
    Expect(brightest == 255u, "the draw bound the zero V# captured before the table changed, not the valid V# in memory when it was recorded");
    Expect(redone == 1, "the draw was prepared again " + std::to_string(redone) + " times, not once");
}

void UnchangedTableIsNotRedone(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target) {
    std::memset(target.Bytes(0), 0, ColorBytes);
    const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    SubmitDraw(vertices, data, target);
    const auto brightest = Brightest(target);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    std::printf("the same draw with its table unchanged: brightest color byte %u, %llu packet(s) prepared again\n", brightest, static_cast<unsigned long long>(redone));
    Expect(brightest == 255u, "the draw with an unchanged valid table did not draw");
    Expect(redone == 0, "a draw whose captured memory did not change was prepared again");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelHeader, PixelRead.data(), sizeof(PixelRead), 1);
        GuestBlock vertices;
        GuestBlock data;
        GuestBlock target;
        WriteVertices(vertices, MovedOffset, -8.0f);
        WriteVertices(vertices, ShiftOffset, 8.0f);
        WriteVertices(vertices, StillOffset, 0.0f);
        WriteDescriptor(vertices, 1, VertexDescriptor(vertices.At(ShiftOffset)));
        WriteDescriptor(vertices, 2, VertexDescriptor(vertices.At(StillOffset)));
        const float one = 1.0f;
        std::memcpy(data.Bytes(0), &one, sizeof(one));
        TableBlock = &vertices;
        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(&ChangeTableBeforeCheck);
        ChangedTableIsBound(vertices, data, target);
        UnchangedTableIsNotRedone(vertices, data, target);
        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(nullptr);
        AgcDriverShutdown_nid_postfix();
        std::puts("draw recapture: a draw whose captured descriptor table changed before it was recorded binds the table in memory");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
