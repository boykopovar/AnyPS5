#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
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
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
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
constexpr std::size_t LabelOffset = 3 * BlockBytes;
constexpr std::uint32_t Untouched = 0x5a5a5a5au;
constexpr std::uint32_t Drawn = 0xff0000ffu;
constexpr std::uint32_t LabelValue = 0x600dcafeu;
constexpr std::uint32_t OutputHandle = 9;
constexpr std::uint32_t Rgba8UnormInfo = 0x28028u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::array<std::uint32_t, 4> Scissor{32, 16, 160, 80};

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 9> FillCode{
    0xd7460004, 0x04010c08, 0x7e000204, 0x7e020205, 0x7e040206, 0x7e060207, 0xe01c2000, 0x80000004, 0xbf810000,
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

void AppendDraw(std::vector<std::uint32_t>& commands, std::uint64_t target) {
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
        {0x90, 0x80000000u | (Scissor[1] << 16u) | Scissor[0]}, {0x91, (Scissor[3] << 16u) | Scissor[2]},
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

void AppendGate(std::vector<std::uint32_t>& commands) {
    const auto gate = reinterpret_cast<std::uintptr_t>(&Gate);
    commands.insert(commands.end(), {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(gate), static_cast<std::uint32_t>(gate >> 32u), 1u, 0xffffffffu, 0x19u});
}

void Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

class Region {
public:
    Region(std::size_t bytes, bool watched) : bytes(bytes), arena(AgcDriver::GuestMemory::WriteWatched()) {
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
            Require(raw != MAP_FAILED, "linear target completion points: cannot map the guest region");
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
        Require(data != nullptr, "linear target completion points: cannot allocate the guest region");
        auto* words = reinterpret_cast<std::uint32_t*>(data);
        std::fill(words, words + bytes / 4u, Untouched);
        if (arena && !watched) {
            AgcDriver::GuestMemory::Unwatch(Address(), bytes);
            Require(!AgcDriver::GuestMemory::Watched(Address(), bytes), "linear target completion points: the region is still write-watched");
        }
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
    std::size_t bytes;

private:
    bool arena;
    std::uint8_t* data = nullptr;
};

void Import(const Region& region) {
    const auto address = region.Address();
    const std::vector<std::uint32_t> userData{
        static_cast<std::uint32_t>(address),
        static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u),
        static_cast<std::uint32_t>(region.bytes / 16u),
        0x4bfacu,
        Untouched,
        Untouched,
        Untouched,
        Untouched,
    };
    std::vector<std::uint32_t> commands;
    const auto code = reinterpret_cast<std::uintptr_t>(FillCode.data());
    Append(commands, 0x76, {0x207, 64, 1, 1});
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)});
    Append(commands, 0x76, {0x213, (static_cast<std::uint32_t>(userData.size()) << 1u) | 0x80u});
    commands.push_back(0xc0007600u | (static_cast<std::uint32_t>(userData.size()) << 16u));
    commands.push_back(0x240u);
    commands.insert(commands.end(), userData.begin(), userData.end());
    commands.insert(commands.end(), {0xc0031500u, static_cast<std::uint32_t>(region.bytes / 1024u), 1, 1, 0x41u});
    Submit(commands);
    AgcDriverWaitIdle_nid_postfix();
}

bool InsideScissor(std::uint32_t x, std::uint32_t y) {
    return x >= Scissor[0] && x < Scissor[2] && y >= Scissor[1] && y < Scissor[3];
}

std::string Mismatches(const Region& region) {
    std::size_t drawn = 0, outside = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto value = region.Word(y * Pitch + x * 4u);
            if (InsideScissor(x, y)) drawn += value == Drawn ? 0u : 1u;
            else outside += value == Untouched ? 0u : 1u;
        }
    }
    if (drawn == 0 && outside == 0) return {};
    return std::to_string(drawn) + " of the " + std::to_string((Scissor[2] - Scissor[0]) * (Scissor[3] - Scissor[1])) + " pixels inside the scissor do not hold the draw's color and " + std::to_string(outside) + " pixels outside it changed";
}

bool Await(const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

class Request final : public AgcDriver::IFlipRequest {
public:
    explicit Request(std::atomic<int>& ready) : ready(ready) {}
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override { ++ready; }
    void Fail(std::exception_ptr) noexcept override {}
private:
    std::atomic<int>& ready;
};

class Output final : public AgcDriver::IVideoOutput {
public:
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return std::make_shared<Request>(ready); }
    void Fail(std::exception_ptr) noexcept override {}
    std::atomic<int> ready = 0;
};

enum class Point { Flip, Label, Interrupt };

struct Events {
    KernelEqueue queue = 0;
    KernelEqueueRef owner;
    int tag = 0;
};

std::string Run(Point point, bool watched, const Region& region, Output& output, Events& events) {
    const char* name = point == Point::Flip ? "flip" : point == Point::Label ? "label" : "end-of-pipe interrupt";
    const auto target = region.Address();
    Import(region);
    Gate.store(0, std::memory_order_release);
    std::vector<std::uint32_t> commands;
    AppendDraw(commands, target);
    if (point == Point::Flip) {
        commands.insert(commands.end(), {AgcDriver::FlipPacketHeader, OutputHandle, 0xfffffffeu, 1, 0, 0});
    } else if (point == Point::Label) {
        const auto label = target + LabelOffset;
        commands.insert(commands.end(), {0xc0064900u, 0x514u, 1u << 29u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), LabelValue, 0, 0});
    } else {
        commands.insert(commands.end(), {0xc0064900u, 0x06000528u, 2u << 24u, 0, 0, 0, 0, 0});
    }
    AppendGate(commands);
    const auto readyBefore = output.ready.load();
    if (point == Point::Interrupt) {
        std::array<KernelEvent, 4> stale{};
        while (events.owner->GetTriggeredEvents(stale.data(), static_cast<int>(stale.size())) > 0) {}
    }
    Submit(commands);
    const bool watchedAtStore = AgcDriver::GuestMemory::Watched(target, TargetBytes);
    std::string failure;
    if (point == Point::Flip) {
        const bool flipped = Await([&] { return output.ready.load() != readyBefore; });
        if (!flipped) failure = "the flip did not reach the GPU";
        else if (!Await([&] { return Mismatches(region).empty(); })) failure = "after the flip, while the queue waited: " + Mismatches(region);
    } else if (point == Point::Label) {
        if (!Await([&] { return region.Word(LabelOffset) == LabelValue; })) failure = "the label after the draw never landed";
        else if (const auto mismatches = Mismatches(region); !mismatches.empty()) failure = "when the label after the draw landed: " + mismatches;
    } else {
        std::array<KernelEvent, 1> delivered{};
        if (!Await([&] { return events.owner->GetTriggeredEvents(delivered.data(), 1) == 1; })) failure = "the end-of-pipe interrupt after the draw was not delivered";
        else if (delivered[0].udata != &events.tag) failure = "an unexpected event was delivered";
        else if (const auto mismatches = Mismatches(region); !mismatches.empty()) failure = "when the end-of-pipe interrupt after the draw was delivered: " + mismatches;
    }
    Gate.store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::Graphics::StorageTexture::FlushPending(target, region.bytes, nullptr, "test release");
    std::printf("linear target completion point: %s, %s memory (watched at the store: %d): %s\n", name, watched ? "write-watched" : "unwatched", watchedAtStore ? 1 : 0, failure.empty() ? "stored" : failure.c_str());
    if (failure.empty()) return {};
    return std::string(name) + (watched ? " (write-watched memory): " : " (unwatched memory): ") + failure;
}

}

int main(int argc, char** argv) {
    try {
        bool flips = argc == 1, labels = argc == 1, interrupts = argc == 1, watchedFlip = argc == 1;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            if (argument == "--flip") flips = true;
            else if (argument == "--label") labels = true;
            else if (argument == "--interrupt") interrupts = true;
            else Require(false, "linear target completion points: unknown argument " + std::string(argument));
        }
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).hostImportAlignment == 0) {
                std::puts("linear target completion point tests skipped: no host import on this device, so the target is never resident");
                return VulkanTestSkipped;
            }
        }
        std::vector<std::unique_ptr<Region>> regions;
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        auto output = std::make_shared<Output>();
        AgcDriverRegisterVideoOutput_nid_postfix(OutputHandle, output);
        Events events;
        if (interrupts) {
            Require(sceKernelCreateEqueue(&events.queue, "AGC linear target") == 0, "linear target completion points: event queue creation failed");
            events.owner = EqueuePin_nid_postfix(events.queue);
            Require(sceAgcDriverAddEqEvent(events.queue, 0, &events.tag) == 0, "linear target completion points: graphics event registration failed");
        }
        std::vector<std::string> failures;
        const auto run = [&](Point point, bool watched) {
            regions.push_back(std::make_unique<Region>(watched ? 16 * BlockBytes : 4 * BlockBytes, watched));
            if (auto failure = Run(point, watched, *regions.back(), *output, events); !failure.empty()) failures.push_back(std::move(failure));
        };
        if (flips) run(Point::Flip, false);
        if (watchedFlip) run(Point::Flip, true);
        if (labels) run(Point::Label, false);
        if (interrupts) run(Point::Interrupt, false);
        if (interrupts) {
            Require(sceAgcDriverDeleteEqEvent(events.queue, 0) == 0, "linear target completion points: graphics event deletion failed");
            events.owner.reset();
            Require(sceKernelDeleteEqueue(events.queue) == 0, "linear target completion points: event queue deletion failed");
        }
        AgcDriverUnregisterVideoOutput_nid_postfix(OutputHandle, output);
        if (!failures.empty()) {
            std::string message = "a CPU read after a point where the guest can observe a draw's completion did not see the drawn linear target:";
            for (const auto& failure : failures) message += "\n  " + failure;
            Require(false, message);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("linear target completion point tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
