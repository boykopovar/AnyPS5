#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <thread>

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;

        std::array<std::uint32_t, 2> nop{0xc0001000u, 0u};
        Packet packet{nop.data(), static_cast<std::uint32_t>(nop.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);

        const auto dormant = [] { return AgcDriver::DriverDetail::queue0Dormant.load(std::memory_order_relaxed); };
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!dormant() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        AgcDriver::Graphics::Require(!AgcDriver::Graphics::StorageTexture::AnyPending(), "a no-op submission left storage results pending");
        AgcDriver::Graphics::Require(dormant(), "queue 0 kept polling with every queue idle and no storage results to store instead of waiting");

        AgcDriverShutdown_nid_postfix();
        std::puts("queue zero idle wait tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
