#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
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
constexpr std::size_t Images = 8;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, 0x12345678u, 0xf0200108, 0x00020102, 0xbf810000,
};

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("idle store resume: " + what);
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
        std::vector<std::uint32_t> storage((Images * BlockBytes + 0x10000u) / 4u);
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        auto* first = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(first, Images * BlockBytes, true, true, true);
        }
        const auto submit = [](std::vector<std::uint32_t>& words) {
            Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
            check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
        };
        const auto within = [](const auto& done, std::chrono::milliseconds limit) {
            const auto deadline = std::chrono::steady_clock::now() + limit;
            while (!done() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return done();
        };
        const auto dormant = [] { return AgcDriver::DriverDetail::queue0Dormant.load(std::memory_order_relaxed); };
        const auto pending = [] { return AgcDriver::Graphics::StorageTexture::AnyPending(); };

        std::vector<std::vector<std::uint32_t>> stores;
        for (std::size_t image = 0; image < Images; ++image) stores.push_back(StoreCommands(first + image * (BlockBytes / 4u)));
        for (auto& store : stores) submit(store);
        check(within([&] { return pending() && dormant(); }, std::chrono::seconds(2)), "queue 0 did not wait over the pending storage results");

        auto& gpuMutex = AgcDriver::GuestMemory::GpuMutex();
        bool handedOff = false;
        for (int round = 0; round < 40 && !handedOff; ++round) {
            if (!pending()) {
                for (auto& store : stores) submit(store);
                check(within([&] { return pending() && dormant(); }, std::chrono::seconds(2)), "queue 0 did not wait over storage results submitted again");
            }
            gpuMutex.lock();
            if (!pending() || !within([&] { return gpuMutex.Waiters() == 1; }, std::chrono::milliseconds(300))) {
                gpuMutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            std::atomic<int> first{-1};
            std::atomic<std::uint32_t> waitersAtFirst{0};
            std::atomic<bool> pendingAtFirst{false};
            std::atomic<bool> release{false};
            const auto blocked = [&](int index) {
                std::lock_guard held(gpuMutex);
                int none = -1;
                if (!first.compare_exchange_strong(none, index)) return;
                pendingAtFirst.store(pending(), std::memory_order_relaxed);
                waitersAtFirst.store(gpuMutex.Waiters(), std::memory_order_release);
                while (!release.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::microseconds(100));
            };
            std::thread one(blocked, 0);
            std::thread two(blocked, 1);
            const bool counted = within([&] { return gpuMutex.Waiters() == 3; }, std::chrono::seconds(2));
            gpuMutex.unlock();
            const bool held = within([&] { return waitersAtFirst.load(std::memory_order_acquire) != 0; }, std::chrono::seconds(2));
            bool returned = false;
            const bool yielded = counted && held && waitersAtFirst.load() == 1 && pendingAtFirst.load();
            if (yielded) {
                const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(25);
                while (!returned && std::chrono::steady_clock::now() < until) {
                    returned = gpuMutex.Waiters() > 1;
                    std::this_thread::sleep_for(std::chrono::microseconds(200));
                }
            }
            release.store(true, std::memory_order_release);
            one.join();
            two.join();
            check(counted, "queue 0 and two threads were not all counted as blocked on the GPU mutex");
            check(held, "neither blocked thread took the GPU mutex after it was released");
            check(!returned, "queue 0 came back for the GPU mutex while a thread blocked on it since before the store yielded had not had it");
            handedOff = yielded;
        }
        check(handedOff, "queue 0 never stored a piece ahead of two threads blocked on the GPU mutex");
        check(within([&] { return !pending(); }, std::chrono::seconds(2)), "the idle store did not resume after the blocked threads had the GPU mutex");
        for (auto& store : stores) submit(store);
        check(within([&] { return pending() && dormant(); }, std::chrono::seconds(2)), "queue 0 did not wait over the storage results submitted for the contended store");

        std::atomic<bool> contend{true};
        std::atomic<std::uint64_t> holds{0};
        std::thread contender([&] {
            while (contend.load(std::memory_order_relaxed)) {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                holds.fetch_add(1, std::memory_order_relaxed);
            }
        });
        const bool storedUnderContention = within([&] { return !pending(); }, std::chrono::milliseconds(1000));
        contend.store(false, std::memory_order_relaxed);
        contender.join();
        check(holds.load() != 0, "the contending thread never held the GPU mutex");
        check(within([&] { return !pending(); }, std::chrono::seconds(2)),
            "an idle store that yielded to a thread blocked on the GPU mutex left results pending while queue 0 stayed idle");
        check(storedUnderContention, "the idle store did not finish while another thread kept taking the GPU mutex");

        AgcDriverWaitIdle_nid_postfix();
        AgcDriverShutdown_nid_postfix();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(first);
        }
        std::puts("idle store resume tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
