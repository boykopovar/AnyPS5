#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("GPU-written indirect buffer, queued writer: " + what);
}

struct Gate {
    std::atomic<bool> release{false};
    ~Gate() { release.store(true, std::memory_order_release); }
};

alignas(256) std::array<std::uint32_t, 8> CommandMemory{};
alignas(256) std::array<std::uint32_t, 4> MarkerMemory{};

std::uint32_t Low(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }
std::uint32_t High(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); }

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
        constexpr std::uint32_t Marker = 0x5eed0102u;
        constexpr std::uint32_t Nop = 0xc0031000u;
        constexpr std::uint32_t WriteData = 0xc0033700u;
        CommandMemory[0] = Nop;
        CommandMemory[1] = 0x500u;
        CommandMemory[2] = Low(MarkerMemory.data());
        CommandMemory[3] = High(MarkerMemory.data());
        CommandMemory[4] = Marker;
        MarkerMemory.fill(0);
        const auto commands = reinterpret_cast<std::uint64_t>(CommandMemory.data());
        {
            Gate gate;
            DrawPipeline::Queue0().Enqueue([&gate] {
                while (!gate.release.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                CommandMemory[0] = WriteData;
            }, {{commands, commands + 5u * sizeof(std::uint32_t)}});
            const auto before = DrawPipeline::DrainRequestsByOpcode(0x3f);
            std::thread releaser([&] {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                while (DrawPipeline::DrainRequestsByOpcode(0x3f) == before && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                gate.release.store(true, std::memory_order_release);
            });
            std::vector<std::uint32_t> words{0xc0023f00u, Low(CommandMemory.data()), High(CommandMemory.data()) & 0xffffu, 5u};
            Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
            check(sceAgcDriverSubmitDcb(&packet) == 0, "the submission failed");
            releaser.join();
        }
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(MarkerMemory.data()), sizeof(MarkerMemory));
        check(CommandMemory[0] == WriteData, "the queued writer did not store its packet header");
        check(MarkerMemory[0] == Marker, "an INDIRECT_BUFFER under a write still queued in the draw pipeline ran the command buffer as it was at submission: the marker is " + std::to_string(MarkerMemory[0]));
        LibcRunShutdown_nid_postfix();
        std::puts("an INDIRECT_BUFFER under a queued draw pipeline write runs the packet the write stored");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
