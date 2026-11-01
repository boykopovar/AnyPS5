#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
alignas(256) std::array<std::uint32_t, 15> Code{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};
std::uint32_t Low(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t High(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p) >> 32u); }
void Set(std::vector<std::uint32_t>& words, std::uint32_t reg, std::uint32_t value) { words.insert(words.end(), {0xc0017600u, reg, value}); }
void Dispatch(std::vector<std::uint32_t>& words, const void* descriptor, std::uint32_t iterations = 32) {
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    Set(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    Set(words, 0x20d, static_cast<std::uint32_t>(address >> 40u));
    for (auto reg : {0x207u, 0x208u, 0x209u}) Set(words, reg, 1);
    Set(words, 0x212, 0); Set(words, 0x213, 6); Set(words, 0x240, Low(descriptor)); Set(words, 0x241, High(descriptor)); Set(words, 0x242, iterations);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}
void Release(std::vector<std::uint32_t>& words, const void* label, std::uint32_t value) {
    const std::uint32_t interrupt = value % 2 == 0 ? 2 : 0;
    words.insert(words.end(), {0xc0064900u, 0x514u, (2u << 29u) | (interrupt << 24u), Low(label), High(label), value, 0u, 0u});
}
void Await(const std::uint32_t* label, std::uint32_t value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (*static_cast<const volatile std::uint32_t*>(label) != value) {
        Require(std::chrono::steady_clock::now() < deadline, "submission lost its real GPU completion store");
        std::this_thread::yield();
    }
    AgcDriverWaitIdle_nid_postfix();
}
}

int main() {
    void* block = nullptr;
    try {
        auto device = OpenVulkanTestDevice(); if (!device) return VulkanTestSkipped; device.reset();
#ifdef _WIN32
        block = VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        block = std::aligned_alloc(65536, 65536);
#endif
        Require(block != nullptr, "cannot allocate submit-progress guest memory");
        std::memset(block, 0, 65536);
        GuestAllocations::Mutation().Add(block, 65536, true, true, true);
        auto* output = static_cast<std::uint32_t*>(block);
        auto* descriptor = output + 64; auto* label = output + 128; auto* neighbor = output + 192;
        descriptor[0] = Low(output); descriptor[1] = High(output) & 0xffffu; descriptor[2] = 16; descriptor[3] = 0x31016facu;
        *neighbor = 0xabcdef12u;
        const auto submit = [&](std::uint32_t value) {
            *label = 0; label[1] = 0xfeedbeefu;
            AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(label), 8);
            std::vector<std::uint32_t> words; Dispatch(words, descriptor);
            if (value % 2 != 0) {
                auto* selfGate = output + 384; selfGate[0] = 0; selfGate[1] = 0;
                AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(selfGate), 8);
                Release(words, selfGate, 1);
                if (value % 3 == 0) words.insert(words.end(), {0xc0079300u, 0x13u, Low(selfGate), High(selfGate), 1, 0, 0xffffffffu, 0xffffffffu, 4});
                else words.insert(words.end(), {0xc0053c00u, 0x13u, Low(selfGate), High(selfGate), 1, 0xffffffffu, 4});
            }
            Release(words, label, value);
            Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
            AgcDriver::Submit(&packet, 0);
        };
        submit(1); Await(label, 1);
        std::uint32_t ready = 0;
        for (std::uint32_t value = 2; value < 66; ++value) {
            submit(value);
            ready += *static_cast<volatile std::uint32_t*>(label) == value;
            Await(label, value);
            Require(*label == value, "submission lost its real GPU completion store");
            Require(label[1] == 0, "64-bit GPU marker did not replace its genuine CPU-written high word");
            Require(*neighbor == 0xabcdef12u, "progress wait damaged neighboring guest bytes");
        }
        std::cout << ready << "/64 small dependent submissions completed at return\n";
        std::uint32_t prefixesReady = 0;
        for (std::uint32_t index = 0; index < 16; ++index) {
            auto* continued = output + 320;
            *continued = 0; *label = 0;
            AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(label), 8);
            AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(continued), 8);
            std::vector<std::uint32_t> prefix; Dispatch(prefix, descriptor); Release(prefix, label, 100 + index);
            prefix.insert(prefix.end(), {0xc0005900u, 0u});
            const auto control = prefix.size() - 1;
            Release(prefix, continued, 200 + index);
            Packet prefixPacket{prefix.data(), static_cast<std::uint32_t>(prefix.size()), 0, {}};
            AgcDriver::Submit(&prefixPacket, 0);
            prefixesReady += *static_cast<volatile std::uint32_t*>(label) == 100 + index;
            Require(*continued == 0, "an unreleased REWIND continuation executed early");
            std::atomic<bool> logicalDone{false}, waiterFailed{false};
            std::thread waiter;
            bool earlyLogicalCompletion = false;
            if (index == 0) {
                waiter = std::thread([&] {
                    try { AgcDriverWaitIdle_nid_postfix(); logicalDone.store(true, std::memory_order_release); }
                    catch (...) { waiterFailed.store(true, std::memory_order_release); }
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                earlyLogicalCompletion = logicalDone.load(std::memory_order_acquire);
            }
            std::atomic_ref<std::uint32_t>(prefix[control]).store(0x80000000u, std::memory_order_release);
            AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(&prefix[control]), 4);
            Await(label, 100 + index); Await(continued, 200 + index);
            if (waiter.joinable()) waiter.join();
            Require(!waiterFailed.load() && !earlyLogicalCompletion, "prefix acknowledgement completed the unreleased logical submission");
        }
        std::cout << prefixesReady << "/16 finite REWIND prefixes completed at return\n";
        auto* gate = output + 256; *gate = 0; *label = 0;
        AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(gate), 4);
        AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(label), 4);
        std::vector<std::uint32_t> waiting{0xc0053c00u, 0x13u, Low(gate), High(gate), 1, 0xffffffffu, 0x19u};
        Release(waiting, label, 77);
        Packet waitPacket{waiting.data(), static_cast<std::uint32_t>(waiting.size()), 0, {}};
        AgcDriver::Submit(&waitPacket, 0);
        Require(*label == 0, "dependent wait fabricated its completion");
        *gate = 1;
        AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(gate), 4);
        Await(label, 77);
        *label = 0;
        AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(label), 4);
        std::vector<std::uint32_t> slow; Dispatch(slow, descriptor, 1u << 22u); Release(slow, label, 78);
        Packet slowPacket{slow.data(), static_cast<std::uint32_t>(slow.size()), 0, {}};
        const auto began = std::chrono::steady_clock::now();
        AgcDriver::Submit(&slowPacket, 0);
        Require(std::chrono::steady_clock::now() - began < std::chrono::milliseconds(100), "progress budget became an unbounded GPU drain");
        Await(label, 78);
        Require(*neighbor == 0xabcdef12u, "pending progress work damaged neighboring guest bytes");
        AgcDriverShutdown_nid_postfix();
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
