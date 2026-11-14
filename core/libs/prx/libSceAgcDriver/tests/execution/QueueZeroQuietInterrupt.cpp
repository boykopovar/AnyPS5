#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 0x60000;
constexpr int Attempts = 5;
constexpr int Rounds = 20;
constexpr auto QuietPeriod = std::chrono::milliseconds(50);

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, 0x12345678u, 0xf0200108, 0x00020102, 0xbf810000,
};

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("queue zero quiet interrupt: " + what);
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::vector<std::uint32_t> StoreCommands(const std::uint32_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::vector<std::uint32_t> words;
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(StoreCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    return words;
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
        std::vector<std::uint32_t> storage((BlockBytes + 0x10000u) / 4u);
        std::vector<std::uint32_t> labelStorage(0x20000u / 4u);
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        auto* label = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(labelStorage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(texels, BlockBytes, true, true, true);
            mutation.Add(label, 0x10000u, true, true, false);
        }

        KernelEqueue eq = 0;
        check(sceKernelCreateEqueue(&eq, "AGC quiet interrupt") == 0, "event queue creation failed");
        auto owner = EqueuePin_nid_postfix(eq);
        int tag = 0;
        check(sceAgcDriverAddEqEvent(eq, 0, &tag) == 0, "graphics event registration failed");

        const auto labelAddress = reinterpret_cast<std::uintptr_t>(label);
        auto store = StoreCommands(texels);
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
        const auto pending = [] { return AgcDriver::Graphics::StorageTexture::AnyPending(); };
        auto& pipeline = AgcDriver::DriverDetail::DrawPipeline::Queue0();

        const auto within = [](const auto& done, std::chrono::milliseconds limit) {
            const auto deadline = std::chrono::steady_clock::now() + limit;
            while (!done() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return done();
        };
        bool inside = false;
        int measured = 0;
        auto shortest = std::chrono::steady_clock::duration::max();
        for (int round = 0; round < Rounds && measured < Attempts && !inside; ++round) {
            label[0] = 0u;
            submit(store);
            waitFor([&] { return pending() && dormant(); }, "queue 0 did not wait over the pending storage result");
            bool quiet = false;
            std::chrono::steady_clock::time_point released;
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                submit(release);
                quiet = within([&] { return pipeline.Outstanding() != 0; }, std::chrono::milliseconds(500)) && within(dormant, std::chrono::milliseconds(500)) && pending();
                released = std::chrono::steady_clock::now();
            }
            std::array<KernelEvent, 1> events{};
            int delivered = 0;
            const auto deadline = released + std::chrono::seconds(2);
            while ((delivered = owner->GetTriggeredEvents(events.data(), 1)) == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            const auto took = std::chrono::steady_clock::now() - released;
            check(delivered == 1 && events[0].udata == &tag, "an end-of-pipe interrupt committed while queue 0 waited was not delivered");
            AgcDriverWaitIdle_nid_postfix();
            check(label[0] == 1u, "the end-of-pipe label was not written");
            if (!quiet) continue;
            ++measured;
            shortest = std::min(shortest, took);
            inside = took < QuietPeriod / 2;
        }
        check(measured != 0, "queue 0 never waited in the idle store's quiet period while the draw pipeline held the end-of-pipe label");
        check(inside, "queue 0 waited out the idle store's quiet period before it delivered an end-of-pipe interrupt the draw pipeline committed meanwhile (fastest of " + std::to_string(measured) + ": "
            + std::to_string(std::chrono::duration<double, std::milli>(shortest).count()) + " ms)");

        check(sceAgcDriverDeleteEqEvent(eq, 0) == 0, "graphics event deletion failed");
        owner.reset();
        check(sceKernelDeleteEqueue(eq) == 0, "event queue deletion failed");
        AgcDriverShutdown_nid_postfix();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(texels);
            mutation.Remove(label);
        }
        std::puts("queue zero quiet interrupt tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
