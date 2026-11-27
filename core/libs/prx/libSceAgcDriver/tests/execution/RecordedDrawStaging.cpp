#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::size_t Pitch = std::size_t{Width} * 4u;
constexpr std::size_t TargetBytes = Pitch * Height;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t NodeBytes = 4 * BlockBytes;
constexpr std::uint32_t Nodes = NodeBytes / 4u;
constexpr std::size_t LabelOffset = 0;
constexpr std::size_t CounterOffset = 64;
constexpr std::uint32_t GdsCounter = 0x10;
constexpr std::uint32_t Untouched = 0x5a5a5a5au;
constexpr std::uint32_t Drawn = 0xff0000ffu;
constexpr std::uint32_t LabelValue = 0x600dcafeu;
constexpr std::uint32_t Rgba8UnormInfo = 0x28028u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::array<std::uint32_t, 4> Scissor{8, 4, 24, 20};
constexpr std::uint32_t Covered = (Scissor[2] - Scissor[0]) * (Scissor[3] - Scissor[1]);
static_assert(Covered < Nodes);

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 24> PixelCode{
    0x7e060f00u, 0x7e080f01u, 0x34080890u, 0x38080903u,
    0xbefc03ffu, 0x0000ffffu, 0xd8fa0010u, 0x02000000u, 0xd7650005u, 0x0001007eu, 0xbf8cc07fu, 0x4a040b02u,
    0xe0702000u, 0x80000402u, 0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u,
    0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 24> StorePixelCode{
    0x7e060f00u, 0x7e080f01u, 0xd76f0002u, 0x040d0d04u, 0x34080890u, 0x38080903u,
    0xe0702000u, 0x80000402u, 0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u,
    0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
    0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 24> MarkedStorePixelCode{
    0x7e060f00u, 0x7e080f01u, 0xd76f0002u, 0x040d0d04u, 0x4a0404ffu, 0x00000800u, 0x34080890u, 0x38080903u, 0x380808ffu, 0x80000000u,
    0xe0702000u, 0x80000402u, 0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u,
    0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
    0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 24> SplitStorePixelCode{
    0x7e060f00u, 0x7e080f01u, 0xd76f0002u, 0x040d0d04u, 0x34080890u, 0x38080903u,
    0xe0702000u, 0x80000402u, 0x4a0404ffu, 0x00000800u, 0xe0702000u, 0x80010402u,
    0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u,
    0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
constexpr std::uint32_t StoreMark = 0x80000000u;
constexpr std::uint32_t MarkedSlice = 0x800u;
static_assert(MarkedSlice >= Width * Height && 2u * MarkedSlice <= Nodes);
constexpr std::uint32_t SplitRecords = 0x1800u;
constexpr std::uint32_t SplitSecond = 0x1000u;
constexpr std::uint32_t SplitShift = 0x800u;
static_assert(Width * Height <= SplitShift && SplitShift <= SplitSecond && SplitSecond < SplitRecords && SplitSecond + SplitShift >= SplitRecords && SplitSecond + SplitRecords <= Nodes);
alignas(256) constexpr std::array<std::uint32_t, 9> FillCode{
    0xd7460004, 0x04010c08, 0x7e000204, 0x7e020205, 0x7e040206, 0x7e060207, 0xe01c2000, 0x80000004, 0xbf810000,
};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};
alignas(8) Header StorePixelHeader{};
alignas(8) Header MarkedStorePixelHeader{};
alignas(8) Header SplitStorePixelHeader{};

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

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::size_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(bytes / 16u), 0x4bfacu};
}

std::array<std::uint32_t, 4> NodeDescriptor(std::uint64_t address, std::uint32_t records = Nodes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), records, 0x01016facu};
}

void AppendDraw(std::vector<std::uint32_t>& commands, std::uint64_t target, std::uint64_t nodes, const std::uint32_t* pixelCode = PixelCode.data(), std::vector<std::array<std::uint32_t, 4>> descriptors = {}) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
    const auto extent = (Height << 16u) | Width;
    const auto inputs = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, inputs}, {0x1b4, inputs}, {0x1b6, 0x8000}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, Rgba8UnormInfo},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((Width - 1u) << 14u) | (Height - 1u)},
        {0x3b8, LinearAttrib3}, {0x1e0, 0},
        {0xc, 0}, {0xd, extent},
        {0x81, 0x80000000}, {0x82, extent},
        {0x90, 0x80000000u | (Scissor[1] << 16u) | Scissor[0]}, {0x91, (Scissor[3] << 16u) | Scissor[2]},
        {0x94, 0x80000000}, {0x95, extent},
        {0x10f, std::bit_cast<std::uint32_t>(2.0f * Width)}, {0x110, std::bit_cast<std::uint32_t>(2.0f * Width)},
        {0x111, std::bit_cast<std::uint32_t>(2.0f * Height)}, {0x112, std::bit_cast<std::uint32_t>(2.0f * Height)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(target >> 8u)}, {0x390, static_cast<std::uint32_t>(target >> 40u)},
    };
    if (descriptors.empty()) descriptors.push_back(NodeDescriptor(nodes));
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, static_cast<std::uint32_t>(4u * descriptors.size()) << 1u},
    };
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    commands.push_back(0xc0007600u | (static_cast<std::uint32_t>(4u * descriptors.size()) << 16u));
    commands.push_back(0x00c);
    for (const auto& descriptor : descriptors) commands.insert(commands.end(), descriptor.begin(), descriptor.end());
    Append(commands, 0x2d, {3, 2});
}

void AppendDispatch(std::vector<std::uint32_t>& commands, const std::uint32_t* program, const std::vector<std::uint32_t>& userData, std::uint32_t groups) {
    const auto code = reinterpret_cast<std::uintptr_t>(program);
    Append(commands, 0x76, {0x207, 64, 1, 1});
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)});
    Append(commands, 0x76, {0x213, (static_cast<std::uint32_t>(userData.size()) << 1u) | 0x80u});
    commands.push_back(0xc0007600u | (static_cast<std::uint32_t>(userData.size()) << 16u));
    commands.push_back(0x240u);
    commands.insert(commands.end(), userData.begin(), userData.end());
    commands.insert(commands.end(), {0xc0031500u, groups, 1, 1, 0x41u});
}

void AppendLabel(std::vector<std::uint32_t>& commands, std::uint64_t label) {
    commands.insert(commands.end(), {0xc0064900u, 0x514u, 1u << 29u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), LabelValue, 0, 0});
}

void Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

class Region {
public:
    explicit Region(std::size_t bytes) : bytes(bytes), arena(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (arena) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (arena) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "recorded draw staging: cannot map the guest region");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "recorded draw staging: cannot allocate the guest region");
        Fill(Untouched);
        GuestAllocations::Mutation().Add(data, bytes, true, true, true);
    }
    ~Region() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        if (arena) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (arena) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::uint32_t Word(std::size_t offset) const { return std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(data + offset)).load(std::memory_order_acquire); }
    void Fill(std::uint32_t value) const {
        auto* words = reinterpret_cast<std::uint32_t*>(data);
        std::fill(words, words + bytes / 4u, value);
    }
    std::size_t bytes;

private:
    bool arena;
    std::uint8_t* data = nullptr;
};

void Import(const Region& region) {
    const auto buffer = BufferDescriptor(region.Address(), region.bytes);
    std::vector<std::uint32_t> userData(buffer.begin(), buffer.end());
    userData.insert(userData.end(), {Untouched, Untouched, Untouched, Untouched});
    std::vector<std::uint32_t> commands;
    AppendDispatch(commands, FillCode.data(), userData, static_cast<std::uint32_t>(region.bytes / 1024u));
    Submit(commands);
    AgcDriverWaitIdle_nid_postfix();
}

bool InScissor(std::uint32_t x, std::uint32_t y) {
    return x >= Scissor[0] && x < Scissor[2] && y >= Scissor[1] && y < Scissor[3];
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string Mismatches(const Region& control, const Region& target, const Region& nodes, std::uint32_t draws = 1) {
    std::vector<std::string> parts;
    const auto records = draws * Covered;
    if (const auto counter = control.Word(CounterOffset); counter != records) parts.push_back("the GDS counter is " + std::to_string(counter) + ", not the " + std::to_string(records) + " covered fragments");
    std::vector<std::uint32_t> hits(Width * Height, 0);
    std::size_t unwritten = 0, foreign = 0, repeated = 0;
    std::string firstForeign;
    for (std::uint32_t index = 0; index < records; ++index) {
        const auto record = nodes.Word(std::size_t{index} * 4u);
        if (record == Untouched) {
            ++unwritten;
            continue;
        }
        const auto x = record & 0xffffu;
        const auto y = record >> 16u;
        if (x >= Width || y >= Height || !InScissor(x, y)) {
            if (foreign++ == 0) firstForeign = " (record " + std::to_string(index) + " is " + Hex(record) + ")";
            continue;
        }
        if (hits[y * Width + x]++ >= draws) ++repeated;
    }
    std::size_t missing = 0;
    for (std::uint32_t y = Scissor[1]; y < Scissor[3]; ++y) {
        for (std::uint32_t x = Scissor[0]; x < Scissor[2]; ++x) missing += hits[y * Width + x] < draws ? 1u : 0u;
    }
    if (unwritten != 0) parts.push_back(std::to_string(unwritten) + " of the first " + std::to_string(records) + " node records were never written");
    if (foreign != 0) parts.push_back(std::to_string(foreign) + " node records name a fragment the draw did not cover" + firstForeign);
    if (repeated != 0) parts.push_back(std::to_string(repeated) + " node records name a fragment more often than it was drawn");
    if (missing != 0) parts.push_back(std::to_string(missing) + " covered fragments have fewer node records than draws");
    std::size_t disturbed = 0;
    for (std::uint32_t index = records; index < Nodes; ++index) disturbed += nodes.Word(std::size_t{index} * 4u) == Untouched ? 0u : 1u;
    if (disturbed != 0) parts.push_back(std::to_string(disturbed) + " of the " + std::to_string(Nodes - records) + " node records past the covered ones changed");
    std::size_t wrongColor = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) wrongColor += target.Word(y * Pitch + x * 4u) == (InScissor(x, y) ? Drawn : Untouched) ? 0u : 1u;
    }
    if (wrongColor != 0) parts.push_back(std::to_string(wrongColor) + " of the " + std::to_string(Width * Height) + " target pixels do not hold the draw's color or the untouched pattern");
    std::string text;
    for (const auto& part : parts) text += (text.empty() ? "" : "; ") + part;
    return text;
}

bool Await(const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

bool StagingExpected() {
    return std::getenv("APS5_NO_DRAW_STAGING") == nullptr && std::getenv("APS5_SYNC_DRAWS") == nullptr;
}

void Run(const char* name, bool deviceFull, const Region& control, const Region& target, const Region& nodes, std::vector<std::string>& failures) {
    control.Fill(Untouched);
    target.Fill(Untouched);
    nodes.Fill(Untouched);
    const auto refusedBefore = AgcDriver::Graphics::StagingShadowsRefused();
    std::vector<std::uint32_t> commands;
    Append(commands, 0x50, {(2u << 29u) | (1u << 20u), 0, 0, GdsCounter, 0, 4});
    AppendDraw(commands, target.Address(), nodes.Address());
    AppendLabel(commands, control.Address() + LabelOffset);
    AgcDriver::Graphics::SetDeviceLocalFullForTests(deviceFull);
    Submit(commands);
    std::string failure;
    if (!Await([&] { return control.Word(LabelOffset) == LabelValue; })) failure = "the label after the draw never landed";
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
    std::vector<std::uint32_t> readback;
    const auto counter = control.Address() + CounterOffset;
    Append(readback, 0x50, {1u << 29u, GdsCounter, 0, static_cast<std::uint32_t>(counter), static_cast<std::uint32_t>(counter >> 32u), 4});
    Submit(readback);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::Graphics::StorageTexture::FlushPending(target.Address(), target.bytes, nullptr, "test release");
    AgcDriver::GuestMemory::FlushGpuWrites(control.Address(), control.bytes);
    AgcDriver::GuestMemory::FlushGpuWrites(nodes.Address(), nodes.bytes);
    AgcDriver::GuestMemory::FlushGpuWrites(target.Address(), TargetBytes);
    if (failure.empty()) failure = Mismatches(control, target, nodes);
    const auto refused = AgcDriver::Graphics::StagingShadowsRefused() - refusedBefore;
    if (failure.empty() && deviceFull && StagingExpected() && refused == 0) failure = "a device-local heap reported full refused no shadow, so the draw did not try to stage its node buffer";
    std::printf("recorded draw staging, %s (shadows refused: %llu): %s\n", name, static_cast<unsigned long long>(refused), failure.empty() ? "the node records of every covered fragment and the GDS counter" : failure.c_str());
    if (!failure.empty()) failures.push_back(std::string(name) + ": " + failure);
}

std::string StoreMismatches(const Region& target, const Region& nodes, bool plain, bool marked) {
    std::vector<std::string> parts;
    std::size_t wrong = 0, disturbed = 0;
    std::string firstWrong;
    for (std::uint32_t index = 0; index < Nodes; ++index) {
        const bool inMarked = index >= MarkedSlice && index < 2u * MarkedSlice;
        const auto local = inMarked ? index - MarkedSlice : index;
        const auto x = local % Width;
        const auto y = local / Width;
        const bool written = (inMarked ? marked : plain && index < MarkedSlice) && y < Height && InScissor(x, y);
        const auto record = nodes.Word(std::size_t{index} * 4u);
        if (written) {
            const auto expected = ((y << 16u) | x) | (inMarked ? StoreMark : 0u);
            if (record != expected && wrong++ == 0) firstWrong = " (record " + std::to_string(index) + " is " + Hex(record) + ", not " + Hex(expected) + ")";
        } else if (record != Untouched) {
            ++disturbed;
        }
    }
    if (wrong != 0) parts.push_back(std::to_string(wrong) + " covered fragments do not hold their draws' record" + firstWrong);
    if (disturbed != 0) parts.push_back(std::to_string(disturbed) + " records outside the covered fragments changed");
    std::size_t wrongColor = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) wrongColor += target.Word(y * Pitch + x * 4u) == (InScissor(x, y) ? Drawn : Untouched) ? 0u : 1u;
    }
    if (wrongColor != 0) parts.push_back(std::to_string(wrongColor) + " target pixels have the wrong color");
    std::string text;
    for (const auto& part : parts) text += (text.empty() ? "" : "; ") + part;
    return text;
}

void RunPass(const char* name, bool passStaging, bool deferrable, std::initializer_list<const std::uint32_t*> pixels, const Region& control, const Region& target, const Region& nodes, std::vector<std::string>& failures) {
    using AgcDriver::Graphics::GuestBufferMemory;
    using AgcDriver::Graphics::RuntimeSwitch;
    namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;
    control.Fill(Untouched);
    target.Fill(Untouched);
    nodes.Fill(Untouched);
    const bool configured = RuntimeSwitches::Set(RuntimeSwitch::PassStaging);
    struct Restore {
        bool configured;
        ~Restore() { AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::PassStaging, configured); }
    } restore{configured};
    RuntimeSwitches::Assign(RuntimeSwitch::PassStaging, passStaging);
    const auto draws = static_cast<std::uint32_t>(pixels.size());
    const bool plain = std::ranges::find(pixels, StorePixelCode.data()) != pixels.end();
    const bool marked = std::ranges::find(pixels, MarkedStorePixelCode.data()) != pixels.end();
    const auto passesBefore = AgcDriver::Graphics::DrawPassTotals()[0];
    const auto stagingBefore = GuestBufferMemory::PassStagingTotals();
    std::vector<std::uint32_t> commands;
    for (const auto* pixel : pixels) AppendDraw(commands, target.Address(), nodes.Address(), pixel);
    AppendLabel(commands, control.Address() + LabelOffset);
    Submit(commands);
    std::string failure;
    if (!Await([&] { return control.Word(LabelOffset) == LabelValue; })) failure = "the label after the draws never landed";
    AgcDriverWaitIdle_nid_postfix();
    const auto passesAfter = AgcDriver::Graphics::DrawPassTotals()[0];
    const auto stagingAfter = GuestBufferMemory::PassStagingTotals();
    AgcDriver::Graphics::StorageTexture::FlushPending(target.Address(), target.bytes, nullptr, "test release");
    AgcDriver::GuestMemory::FlushGpuWrites(nodes.Address(), nodes.bytes);
    AgcDriver::GuestMemory::FlushGpuWrites(target.Address(), TargetBytes);
    if (failure.empty()) failure = StoreMismatches(target, nodes, plain, marked);
    const auto begun = passesAfter.passesBegun - passesBefore.passesBegun;
    const auto continued = passesAfter.passesContinued - passesBefore.passesContinued;
    const auto joined = stagingAfter.joined - stagingBefore.joined;
    const auto deferred = stagingAfter.deferred - stagingBefore.deferred;
    const auto landed = stagingAfter.landed - stagingBefore.landed;
    const auto counts = std::to_string(begun) + " passes begun, " + std::to_string(continued) + " draws continued one, " + std::to_string(joined) + " uses joined a shadow, " + std::to_string(deferred) + " armed one, " + std::to_string(landed) + " copy-backs landed at a pass end";
    if (failure.empty() && StagingExpected()) {
        if (!passStaging && (joined != 0 || deferred != 0 || landed != 0)) failure = "a draw joined or deferred a pass shadow without APS5_PASS_STAGING: " + counts;
        if (!passStaging && begun != draws) failure = "the draws storing to one staged buffer did not each end the pass without APS5_PASS_STAGING: " + counts;
        if (passStaging && deferrable && (begun != 1 || continued != draws - 1 || joined != draws - 1 || deferred != 1 || landed != 1)) failure = "the draws storing to one staged buffer did not share one render pass and one copy-back: " + counts;
    }
    std::printf("recorded draw staging, %s (%s): %s\n", name, counts.c_str(), failure.empty() ? "every covered fragment holds its draws' record" : failure.c_str());
    if (!failure.empty()) failures.push_back(std::string(name) + ": " + failure);
}

void RunSplit(const char* name, bool passStaging, bool deferrable, const Region& control, const Region& target, const Region& nodes, std::vector<std::string>& failures) {
    using AgcDriver::Graphics::GuestBufferMemory;
    using AgcDriver::Graphics::RuntimeSwitch;
    namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;
    control.Fill(Untouched);
    target.Fill(Untouched);
    nodes.Fill(Untouched);
    const bool configured = RuntimeSwitches::Set(RuntimeSwitch::PassStaging);
    struct Restore {
        bool configured;
        ~Restore() { AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::PassStaging, configured); }
    } restore{configured};
    RuntimeSwitches::Assign(RuntimeSwitch::PassStaging, passStaging);
    const auto stagingBefore = GuestBufferMemory::PassStagingTotals();
    std::vector<std::uint32_t> commands;
    AppendDraw(commands, target.Address(), nodes.Address(), SplitStorePixelCode.data(), {NodeDescriptor(nodes.Address(), SplitRecords), NodeDescriptor(nodes.Address() + std::uint64_t{SplitSecond} * 4u, SplitRecords)});
    AppendLabel(commands, control.Address() + LabelOffset);
    Submit(commands);
    std::string failure;
    if (!Await([&] { return control.Word(LabelOffset) == LabelValue; })) failure = "the label after the draw never landed";
    AgcDriverWaitIdle_nid_postfix();
    const auto stagingAfter = GuestBufferMemory::PassStagingTotals();
    AgcDriver::Graphics::StorageTexture::FlushPending(target.Address(), target.bytes, nullptr, "test release");
    AgcDriver::GuestMemory::FlushGpuWrites(nodes.Address(), nodes.bytes);
    AgcDriver::GuestMemory::FlushGpuWrites(target.Address(), TargetBytes);
    if (failure.empty()) {
        std::size_t wrong = 0, disturbed = 0;
        std::string firstWrong;
        for (std::uint32_t index = 0; index < Nodes; ++index) {
            const bool second = index >= SplitSecond + SplitShift;
            const auto local = second ? index - SplitSecond - SplitShift : index;
            const auto x = local % Width;
            const auto y = local / Width;
            const bool written = y < Height && InScissor(x, y);
            const auto record = nodes.Word(std::size_t{index} * 4u);
            if (written) {
                const auto expected = (y << 16u) | x;
                if (record != expected && wrong++ == 0) firstWrong = " (record " + std::to_string(index) + " is " + Hex(record) + ", not " + Hex(expected) + ")";
            } else if (record != Untouched) {
                ++disturbed;
            }
        }
        if (wrong != 0) failure = std::to_string(wrong) + " covered fragments of the two written windows do not hold their record" + firstWrong;
        if (disturbed != 0) failure += (failure.empty() ? "" : "; ") + std::to_string(disturbed) + " records outside the two written windows changed";
    }
    const auto joined = stagingAfter.joined - stagingBefore.joined;
    const auto deferred = stagingAfter.deferred - stagingBefore.deferred;
    const auto landed = stagingAfter.landed - stagingBefore.landed;
    const auto counts = std::to_string(joined) + " uses joined a shadow, " + std::to_string(deferred) + " armed one, " + std::to_string(landed) + " copy-backs landed at a pass end";
    if (failure.empty() && StagingExpected()) {
        if (!passStaging && (joined != 0 || deferred != 0 || landed != 0)) failure = "the draw deferred a pass shadow without APS5_PASS_STAGING: " + counts;
        if (passStaging && deferrable && (deferred != 1 || landed != 1)) failure = "the draw storing through two overlapping descriptors of one staged range did not owe both writes to one copy-back at the pass end: " + counts;
    }
    std::printf("recorded draw staging, %s (%s): %s\n", name, counts.c_str(), failure.empty() ? "every covered fragment of both windows holds its record" : failure.c_str());
    if (!failure.empty()) failures.push_back(std::string(name) + ": " + failure);
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
        bool deferrable = false;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            deferrable = context.pushDescriptors || context.bufferDeviceAddress;
            if (AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).hostImportAlignment == 0) {
                std::puts("recorded draw staging tests skipped, host imports are unavailable on this device, so the node buffer is never imported");
                return VulkanTestSkipped;
            }
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        Registered(StorePixelHeader, StorePixelCode.data(), sizeof(StorePixelCode), 1);
        Registered(MarkedStorePixelHeader, MarkedStorePixelCode.data(), sizeof(MarkedStorePixelCode), 1);
        Registered(SplitStorePixelHeader, SplitStorePixelCode.data(), sizeof(SplitStorePixelCode), 1);
        Region control(BlockBytes);
        Region target(BlockBytes);
        Region nodes(NodeBytes);
        Region fullNodes(NodeBytes);
        Region passNodes(NodeBytes);
        Region sharedNodes(NodeBytes);
        Region splitNodes(NodeBytes);
        Import(target);
        Import(nodes);
        Import(fullNodes);
        Import(passNodes);
        Import(sharedNodes);
        Import(splitNodes);
        if (!AgcDriver::GuestMemory::Watched(target.Address(), TargetBytes)) {
            std::puts("recorded draw staging tests skipped, the device does not keep host imports write-watched, so the target is never resident");
            return VulkanTestSkipped;
        }
        std::vector<std::string> failures;
        Run("first draw", false, control, target, nodes, failures);
        Run("same draw again", false, control, target, nodes, failures);
        Run("device memory full", true, control, target, fullNodes, failures);
        RunPass("four storing draws without pass staging", false, deferrable, {StorePixelCode.data(), StorePixelCode.data(), StorePixelCode.data(), StorePixelCode.data()}, control, target, passNodes, failures);
        RunPass("four storing draws of one build with pass staging", true, deferrable, {StorePixelCode.data(), StorePixelCode.data(), StorePixelCode.data(), StorePixelCode.data()}, control, target, passNodes, failures);
        RunPass("four storing draws of two builds with pass staging", true, deferrable, {StorePixelCode.data(), MarkedStorePixelCode.data(), StorePixelCode.data(), MarkedStorePixelCode.data()}, control, target, sharedNodes, failures);
        RunSplit("a draw storing through two overlapping descriptors without pass staging", false, deferrable, control, target, splitNodes, failures);
        RunSplit("a draw storing through two overlapping descriptors with pass staging", true, deferrable, control, target, splitNodes, failures);
        if (!failures.empty()) {
            std::string message = "recorded draw staging: a recorded draw whose pixel shader appends to the GDS and stores a record by the returned index into a large guest buffer did not leave the records of exactly the covered fragments:";
            for (const auto& failure : failures) message += "\n  " + failure;
            Require(false, message);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("recorded draw staging tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
