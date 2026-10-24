#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
extern "C" int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);

namespace {

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 8> Code{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("queue zero late interrupt: " + what);
}

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    check(block != nullptr, "cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, false);
    return block;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_NO_OPPORTUNISTIC_REAP", "1");
#else
    setenv("APS5_NO_OPPORTUNISTIC_REAP", "1", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;

        Shader shader{};
        shader.file_header = 0x34333231;
        shader.version = 0x18;
        shader.header_size = sizeof(Shader);
        shader.shader_size = sizeof(Code);
        shader.code = Code.data();
        AgcDriverRegisterShader_nid_postfix(&shader);

        auto* input = NewBlock();
        auto* output = NewBlock();
        auto* label = NewBlock();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) input[lane] = 0x5b000000u | lane;

        KernelEqueue eq = 0;
        check(sceKernelCreateEqueue(&eq, "AGC late interrupt") == 0, "event queue creation failed");
        auto owner = EqueuePin_nid_postfix(eq);
        int tag = 0;
        check(sceAgcDriverAddEqEvent(eq, 0, &tag) == 0, "graphics event registration failed");

        const auto codeAddress = reinterpret_cast<std::uintptr_t>(Code.data());
        const auto labelAddress = reinterpret_cast<std::uintptr_t>(label);
        const auto in = BufferDescriptor(input, BufferBytes);
        const auto out = BufferDescriptor(output, BufferBytes);
        std::vector<std::uint32_t> dispatch{
            0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
            0xc0037600u, 0x207u, Lanes, 1u, 1u,
            0xc0017600u, 0x213u, 8u << 1u,
            0xc0087600u, 0x240u, in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3],
            0xc0031500u, 1u, 1u, 1u, 0x8041u,
        };
        std::vector<std::uint32_t> release{0xc0064900u, 0x06000528u, (2u << 29u) | (2u << 24u) | (1u << 16u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(labelAddress >> 32u), 1u, 0u, 0u};
        const auto submit = [](std::vector<std::uint32_t>& words) {
            Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
            check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
        };
        const auto waitFor = [](const auto& done, const char* what) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!done() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            check(done(), what);
        };
        const auto dormant = [] { return AgcDriver::DriverDetail::queue0Dormant.load(std::memory_order_relaxed); };
        auto& pipeline = AgcDriver::DriverDetail::DrawPipeline::Queue0();

        submit(dispatch);
        waitFor([&] { return dormant() && pipeline.Outstanding() == 0; }, "queue 0 did not wait for work after the dispatch");
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            submit(release);
            waitFor([&] { return pipeline.Outstanding() != 0; }, "queue 0 did not hand the end-of-pipe label to the draw pipeline");
            waitFor(dormant, "queue 0 did not wait for work while the draw pipeline held the end-of-pipe label");
        }

        std::array<KernelEvent, 1> events{};
        int delivered = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while ((delivered = owner->GetTriggeredEvents(events.data(), 1)) == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(delivered == 1 && events[0].udata == &tag, "an end-of-pipe interrupt the draw pipeline committed after queue 0 went idle was not delivered");

        AgcDriverWaitIdle_nid_postfix();
        check(label[0] == 1u, "the end-of-pipe label was not written");
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) check(output[lane] == input[lane] + 1u, "the dispatch before the interrupt stored a wrong value");

        check(sceAgcDriverDeleteEqEvent(eq, 0) == 0, "graphics event deletion failed");
        owner.reset();
        check(sceKernelDeleteEqueue(eq) == 0, "event queue deletion failed");
        AgcDriverShutdown_nid_postfix();
        std::puts("queue zero late interrupt tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
