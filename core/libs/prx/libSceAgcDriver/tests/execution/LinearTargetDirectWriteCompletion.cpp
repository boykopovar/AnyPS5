#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
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

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::size_t Pitch = std::size_t{Width} * 4u;
constexpr std::size_t TargetBytes = Pitch * Height;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t RegionBytes = 16 * BlockBytes;
constexpr std::size_t LabelOffset = 3 * BlockBytes;
constexpr std::uint32_t Untouched = 0x5a5a5a5au;
constexpr std::uint32_t Filled = 0x9abcdef0u;
constexpr std::uint32_t Drawn = 0xff0000ffu;
constexpr std::uint32_t LabelValue = 0x600dcafeu;
constexpr std::uint32_t CpuStored = 0x0badf00du;
constexpr std::uint32_t Rgba8UnormInfo = 0x28028u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t WaitEqual = 3;
constexpr std::uint32_t WaitAtLeast = 5;
constexpr std::array<std::array<std::uint32_t, 4>, 2> Scissors{{{16, 8, 128, 96}, {128, 32, 240, 120}}};
constexpr std::array<std::uint32_t, 4> BlockZeroScissor{24, 8, 232, 56};
constexpr std::size_t CpuStoreOffset = BlockBytes + 32 * Pitch + 64 * 4;
static_assert(BlockZeroScissor[3] * Pitch <= BlockBytes);
static_assert(CpuStoreOffset >= BlockBytes && CpuStoreOffset < TargetBytes);

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 9> FillCode{
    0xd7460004, 0x04010c08, 0x7e000204, 0x7e020205, 0x7e040206, 0x7e060207, 0xe01c2000, 0x80000004, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 11> PairFillCode{
    0xd7460004, 0x04010c0c, 0x7e000204, 0x7e020205, 0x7e040206, 0x7e060207, 0xe01c2000, 0x80000004, 0xe01c2000, 0x80020004, 0xbf810000,
};

alignas(4) std::atomic<std::uint32_t> Gate = 0;

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

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void AppendDraw(std::vector<std::uint32_t>& commands, std::uint64_t target, const std::array<std::uint32_t, 4>& scissor) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto extent = (Height << 16u) | Width;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
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
        {0x90, 0x80000000u | (scissor[1] << 16u) | scissor[0]}, {0x91, (scissor[3] << 16u) | scissor[2]},
        {0x94, 0x80000000}, {0x95, extent},
        {0x10f, std::bit_cast<std::uint32_t>(2.0f * Width)}, {0x110, std::bit_cast<std::uint32_t>(2.0f * Width)},
        {0x111, std::bit_cast<std::uint32_t>(2.0f * Height)}, {0x112, std::bit_cast<std::uint32_t>(2.0f * Height)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(target >> 8u)}, {0x390, static_cast<std::uint32_t>(target >> 40u)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
}

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::size_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(bytes / 16u), 0x4bfacu};
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

void AppendFill(std::vector<std::uint32_t>& commands, std::uint64_t target, const std::uint32_t* copied, std::size_t bytes) {
    const auto direct = BufferDescriptor(target, bytes);
    const auto other = BufferDescriptor(reinterpret_cast<std::uintptr_t>(copied), bytes);
    std::vector<std::uint32_t> userData(direct.begin(), direct.end());
    userData.insert(userData.end(), {Filled, Filled, Filled, Filled});
    userData.insert(userData.end(), other.begin(), other.end());
    AppendDispatch(commands, PairFillCode.data(), userData, static_cast<std::uint32_t>(bytes / 1024u));
}

void AppendLabel(std::vector<std::uint32_t>& commands, std::uint64_t label) {
    commands.insert(commands.end(), {0xc0064900u, 0x514u, 1u << 29u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), LabelValue, 0, 0});
}

void AppendGate(std::vector<std::uint32_t>& commands, std::uint32_t function, std::uint32_t reference) {
    const auto gate = reinterpret_cast<std::uintptr_t>(&Gate);
    commands.insert(commands.end(), {0xc0053c00u, 0x10u | function, static_cast<std::uint32_t>(gate), static_cast<std::uint32_t>(gate >> 32u), reference, 0xffffffffu, 0x19u});
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
            Require(raw != MAP_FAILED, "linear target direct write completion: cannot map the guest region");
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
        Require(data != nullptr, "linear target direct write completion: cannot allocate the guest region");
        auto* words = reinterpret_cast<std::uint32_t*>(data);
        std::fill(words, words + bytes / 4u, Untouched);
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
    void Put(std::size_t offset, std::uint32_t value) const { std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(data + offset)).store(value, std::memory_order_release); }
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

bool Covered(std::uint32_t x, std::uint32_t y) {
    return std::any_of(Scissors.begin(), Scissors.end(), [&](const std::array<std::uint32_t, 4>& scissor) { return x >= scissor[0] && x < scissor[2] && y >= scissor[1] && y < scissor[3]; });
}

std::string Mismatches(const Region& region, bool drawsLast) {
    std::size_t covered = 0, wrongCovered = 0, filledCovered = 0, wrongElsewhere = 0, drawnElsewhere = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto value = region.Word(y * Pitch + x * 4u);
            if (drawsLast && Covered(x, y)) {
                ++covered;
                wrongCovered += value == Drawn ? 0u : 1u;
                filledCovered += value == Filled ? 1u : 0u;
            } else {
                wrongElsewhere += value == Filled ? 0u : 1u;
                drawnElsewhere += value == Drawn ? 1u : 0u;
            }
        }
    }
    if (wrongCovered == 0 && wrongElsewhere == 0) return {};
    const auto elsewhere = std::to_string(std::size_t{Width} * Height - covered);
    if (!drawsLast) return std::to_string(wrongElsewhere) + " of the " + elsewhere + " pixels do not hold the fill's pattern (" + std::to_string(drawnElsewhere) + " hold the draws' color)";
    return std::to_string(wrongCovered) + " of the " + std::to_string(covered) + " pixels the draws covered do not hold their color (" + std::to_string(filledCovered) + " hold the fill's pattern) and " + std::to_string(wrongElsewhere) + " of the " + elsewhere + " pixels outside them do not hold the fill's pattern";
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string CpuStoreMismatches(const Region& region) {
    std::size_t covered = 0, wrongCovered = 0, filledCovered = 0, others = 0, wrongOthers = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = y * Pitch + x * 4u;
            if (offset == CpuStoreOffset) continue;
            const auto value = region.Word(offset);
            if (x >= BlockZeroScissor[0] && x < BlockZeroScissor[2] && y >= BlockZeroScissor[1] && y < BlockZeroScissor[3]) {
                ++covered;
                wrongCovered += value == Drawn ? 0u : 1u;
                filledCovered += value == Filled ? 1u : 0u;
            } else {
                ++others;
                wrongOthers += value == (offset < BlockBytes ? Filled : Untouched) ? 0u : 1u;
            }
        }
    }
    std::vector<std::string> parts;
    if (wrongCovered != 0) parts.push_back(std::to_string(wrongCovered) + " of the " + std::to_string(covered) + " pixels the draw covered in block 0 do not hold its color (" + std::to_string(filledCovered) + " hold the fill's pattern)");
    if (const auto stored = region.Word(CpuStoreOffset); stored != CpuStored) parts.push_back("the CPU store in block 1 holds " + Hex(stored) + ", not " + Hex(CpuStored));
    if (wrongOthers != 0) parts.push_back(std::to_string(wrongOthers) + " of the " + std::to_string(others) + " other pixels do not hold the fill's pattern in block 0 or the untouched pattern in block 1");
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

enum class Order { FillFirst, DrawsFirst };

bool Run(Order order, const Region& region, std::vector<std::string>& failures) {
    const bool fillFirst = order == Order::FillFirst;
    const char* name = fillFirst ? "fill, then draws" : "draws, then fill";
    const auto target = region.Address();
    Import(region);
    if (!AgcDriver::GuestMemory::Watched(target, TargetBytes)) return false;
    std::vector<std::uint32_t> copied(TargetBytes / 4u + 4u, Untouched);
    GuestAllocations::Mutation().Add(copied.data(), TargetBytes + 16u, true, true, true);
    struct Unregister {
        void* data;
        ~Unregister() { GuestAllocations::Mutation().Remove(data); }
    } unregister{copied.data()};
    Gate.store(0, std::memory_order_release);
    std::vector<std::uint32_t> commands;
    if (fillFirst) AppendFill(commands, target, copied.data(), TargetBytes);
    for (const auto& scissor : Scissors) AppendDraw(commands, target, scissor);
    if (!fillFirst) AppendFill(commands, target, copied.data(), TargetBytes);
    AppendLabel(commands, target + LabelOffset);
    if (fillFirst) AppendGate(commands, WaitEqual, 1);
    Submit(commands);
    std::string failure;
    if (fillFirst) {
        if (!Await([&] { return region.Word(LabelOffset) == LabelValue; })) failure = "the label after the draws never landed";
        else if (const auto mismatches = Mismatches(region, true); !mismatches.empty()) failure = "when the label after the draws landed: " + mismatches;
        Gate.store(1, std::memory_order_release);
        AgcDriverWaitIdle_nid_postfix();
    } else {
        AgcDriverWaitIdle_nid_postfix();
        if (region.Word(LabelOffset) != LabelValue) failure = "the label after the fill had not landed by the device wait";
        else if (const auto mismatches = Mismatches(region, false); !mismatches.empty()) failure = "after the label and the device wait: " + mismatches;
    }
    AgcDriver::Graphics::StorageTexture::FlushPending(target, region.bytes, nullptr, "test release");
    std::printf("linear target direct write completion, %s: %s\n", name, failure.empty() ? "the later writer's values" : failure.c_str());
    if (!failure.empty()) failures.push_back(std::string(name) + ": " + failure);
    return true;
}

bool RunCpuStore(bool watched, const Region& region, std::vector<std::string>& failures) {
    const std::string name = std::string("fill, draw in block 0, CPU store in block 1, ") + (watched ? "write-watched" : "unwatched") + " memory";
    const auto target = region.Address();
    Import(region);
    if (watched && !AgcDriver::GuestMemory::Watched(target, TargetBytes)) return false;
    if (!watched) {
        AgcDriver::GuestMemory::Unwatch(target, region.bytes);
        Require(!AgcDriver::GuestMemory::Watched(target, region.bytes), "linear target direct write completion: the region is still write-watched");
    }
    std::vector<std::uint32_t> copied(TargetBytes / 4u + 4u, Untouched);
    GuestAllocations::Mutation().Add(copied.data(), TargetBytes + 16u, true, true, true);
    struct Unregister {
        void* data;
        ~Unregister() { GuestAllocations::Mutation().Remove(data); }
    } unregister{copied.data()};
    Gate.store(0, std::memory_order_release);
    std::vector<std::uint32_t> commands;
    AppendFill(commands, target, copied.data(), BlockBytes);
    AppendDraw(commands, target, BlockZeroScissor);
    AppendGate(commands, WaitAtLeast, 1);
    AppendLabel(commands, target + LabelOffset);
    AppendGate(commands, WaitAtLeast, 2);
    Submit(commands);
    const auto completed = [&] { return std::atomic_ref<std::uint32_t>(copied[0]).load(std::memory_order_acquire) == Filled; };
    bool pendingAtStore = false;
    const bool recorded = Await([&] {
        std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex());
        if (AgcDriver::Graphics::StorageTexture::FindLive(target, TargetBytes) == nullptr) return false;
        pendingAtStore = !completed();
        region.Put(CpuStoreOffset, CpuStored);
        return true;
    });
    Gate.store(1, std::memory_order_release);
    std::string failure;
    bool completedAtLabel = false;
    if (!recorded) failure = "the draw into block 0 never made the target's image";
    else if (!Await([&] { return region.Word(LabelOffset) == LabelValue; })) failure = "the label after the draw never landed";
    else {
        completedAtLabel = completed();
        failure = CpuStoreMismatches(region);
        if (!failure.empty()) failure = "when the label after the draw landed: " + failure;
        if (watched && (!pendingAtStore || !completedAtLabel)) failure += std::string(failure.empty() ? "" : "; ") + (pendingAtStore ? "the fill's completion had not run when the label landed" : "the fill's completion had run before the CPU store") + ", so the ordering under test did not occur";
    }
    const bool watchedAtLabel = AgcDriver::GuestMemory::Watched(target, TargetBytes);
    Gate.store(2, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::Graphics::StorageTexture::FlushPending(target, region.bytes, nullptr, "test release");
    std::printf("linear target direct write completion, %s (watched at the label: %d, completion pending at the CPU store: %d, run by the label: %d): %s\n", name.c_str(), watchedAtLabel ? 1 : 0, pendingAtStore ? 1 : 0, completedAtLabel ? 1 : 0, failure.empty() ? "the draw's color and the CPU store" : failure.c_str());
    if (!failure.empty()) failures.push_back(name + ": " + failure);
    return true;
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).hostImportAlignment == 0) {
                std::puts("linear target direct write completion tests skipped, host imports are unavailable on this device, so the target is never resident");
                return VulkanTestSkipped;
            }
        }
        std::vector<std::unique_ptr<Region>> regions;
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        const auto unwatchedImports = [] {
            std::puts("linear target direct write completion tests skipped, the device does not keep host imports write-watched, so no completion stamp over the target is tracked");
            return VulkanTestSkipped;
        };
        std::vector<std::string> failures;
        for (const auto order : {Order::FillFirst, Order::DrawsFirst}) {
            regions.push_back(std::make_unique<Region>(RegionBytes));
            if (!Run(order, *regions.back(), failures)) return unwatchedImports();
        }
        for (const bool watched : {true, false}) {
            regions.push_back(std::make_unique<Region>(RegionBytes));
            if (!RunCpuStore(watched, *regions.back(), failures)) return unwatchedImports();
        }
        if (!failures.empty()) {
            std::string message = "linear target direct write completion: at a completion point, the memory of a resident linear target did not hold the later of a direct buffer write with completion work and draws into it, or lost a CPU store made outside the draws:";
            for (const auto& failure : failures) message += "\n  " + failure;
            Require(false, message);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("linear target direct write completion tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
