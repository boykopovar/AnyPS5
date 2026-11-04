#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <future>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DrawPipeline;

constexpr std::byte Kept{0x40};
constexpr std::uint32_t BaseVertexLocation = 0x8c;
constexpr std::uint32_t Clipped = 0x40000000u;

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> DirectMemory{};
alignas(256) std::array<std::byte, 1024> FirstMemory{};
alignas(256) std::array<std::byte, 1024> SecondMemory{};
alignas(256) std::array<std::byte, 1024> WrittenMemory{};
alignas(256) std::array<std::uint32_t, 4> Records{3, 1, 0, 0};
alignas(256) std::array<std::uint32_t, 4> LateRecords{0, 1, 0, 0};

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

void AppendState(std::vector<std::uint32_t>& commands) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
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
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
}

void AppendTarget(std::vector<std::uint32_t>& commands, std::array<std::byte, 1024>& memory, std::uint32_t userWord) {
    const auto color = reinterpret_cast<std::uintptr_t>(memory.data());
    Append(commands, 0x69, {0x318, static_cast<std::uint32_t>(color >> 8u)});
    Append(commands, 0x69, {0x390, static_cast<std::uint32_t>(color >> 40u)});
    Append(commands, 0x76, {BaseVertexLocation, userWord});
}

void AppendRecords(std::vector<std::uint32_t>& commands, const void* records) {
    const auto address = reinterpret_cast<std::uintptr_t>(records);
    Append(commands, 0x11, {1, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)});
}

void AppendIndirect(std::vector<std::uint32_t>& commands) {
    Append(commands, 0x24, {0, BaseVertexLocation, 0, 2});
}

void AppendLabel(std::vector<std::uint32_t>& commands, std::uint32_t* target, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    commands.insert(commands.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, 0, 0});
}

std::size_t Covered(const std::array<std::byte, 1024>& memory) {
    return static_cast<std::size_t>(std::count_if(memory.begin(), memory.end(), [](std::byte value) { return value != Kept; }));
}

void SameAsDirect(const std::array<std::byte, 1024>& memory, const std::string& what) {
    for (std::size_t index = 0; index < DirectMemory.size(); ++index) {
        Require(memory[index] == DirectMemory[index], what + ": byte " + std::to_string(index) + " is " + std::to_string(static_cast<unsigned>(memory[index])) + ", the direct draw wrote " + std::to_string(static_cast<unsigned>(DirectMemory[index])));
    }
}

void Flush(const std::array<std::byte, 1024>& memory) {
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(memory.data()), memory.size());
}

struct Blocker {
    struct State {
        std::atomic<bool> release{false};
        std::atomic<bool> done{false};
    };
    std::shared_ptr<State> state = std::make_shared<State>();
    void Release() { state->release = true; }
    void Enqueue() {
        DrawPipeline::Queue0().Enqueue([state = state] {
            while (!state->release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            { std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex()); }
            state->done = true;
        }, {});
    }
    void Join() const {
        const auto start = std::chrono::steady_clock::now();
        while (!state->done.load()) {
            Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(20), "the blocker never ran");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

std::size_t SubmitBehind(Blocker& blocker, std::vector<std::uint32_t>& commands, std::size_t queuedBefore) {
    std::unique_lock gpuLock(AgcDriver::GuestMemory::GpuMutex());
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    const auto start = std::chrono::steady_clock::now();
    while (DrawPipeline::Queue0().Outstanding() < queuedBefore && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto queued = DrawPipeline::Queue0().Outstanding();
    blocker.Enqueue();
    return queued;
}

void Finish() {
    auto idle = std::async(std::launch::async, [] { AgcDriverWaitIdle_nid_postfix(); });
    if (idle.wait_for(std::chrono::seconds(20)) != std::future_status::ready) {
        std::fputs("pipelined indirect draw: the queue worker and the draw pipeline deadlocked\n", stderr);
        std::fflush(stderr);
        std::_Exit(1);
    }
    idle.get();
}

void DrawDirectReference() {
    DirectMemory.fill(Kept);
    std::vector<std::uint32_t> commands;
    AppendState(commands);
    AppendTarget(commands, DirectMemory, 0);
    Append(commands, 0x2d, {3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the direct draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Flush(DirectMemory);
    const auto direct = Covered(DirectMemory);
    Require(direct != 0 && direct != DirectMemory.size(), "the direct draw covered " + std::to_string(direct) + " of " + std::to_string(DirectMemory.size()) + " bytes, expected an edge through the target");
}

void IndirectDrawsQueueBehindTheCommitter() {
    FirstMemory.fill(Kept);
    SecondMemory.fill(Kept);
    std::vector<std::uint32_t> commands;
    AppendState(commands);
    AppendRecords(commands, Records.data());
    AppendTarget(commands, FirstMemory, Clipped);
    AppendIndirect(commands);
    AppendTarget(commands, SecondMemory, Clipped);
    AppendIndirect(commands);
    Blocker blocker;
    const auto queued = SubmitBehind(blocker, commands, 3);
    if (queued < 3) {
        blocker.Release();
        Finish();
        Require(false, "the indirect draws drained the pipeline instead of queuing behind the committer (" + std::to_string(queued) + " items outstanding, expected two draws and the submission's completion)");
    }
    blocker.Release();
    Finish();
    blocker.Join();
    Flush(FirstMemory);
    Flush(SecondMemory);
    SameAsDirect(FirstMemory, "the first queued indirect draw");
    SameAsDirect(SecondMemory, "the second queued indirect draw");
}

void IndirectDrawWaitsForItsQueuedRecordWrite() {
    WrittenMemory.fill(Kept);
    LateRecords = {0, 1, 0, 0};
    std::vector<std::uint32_t> commands;
    AppendState(commands);
    AppendLabel(commands, &LateRecords[0], 3);
    AppendRecords(commands, LateRecords.data());
    AppendTarget(commands, WrittenMemory, Clipped);
    AppendIndirect(commands);
    Blocker blocker;
    const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(0x100);
    SubmitBehind(blocker, commands, 1);
    const auto start = std::chrono::steady_clock::now();
    while (DrawPipeline::DrainRequestsByOpcode(0x100) == drainsBefore && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    blocker.Release();
    Finish();
    blocker.Join();
    Flush(WrittenMemory);
    Require(LateRecords[0] == 3, "the queued label did not store the record's vertex count");
    SameAsDirect(WrittenMemory, "the indirect draw whose record a queued label wrote");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        DrawDirectReference();
        IndirectDrawsQueueBehindTheCommitter();
        IndirectDrawWaitsForItsQueuedRecordWrite();
        AgcDriverShutdown_nid_postfix();
        std::puts("pipelined indirect draw tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
