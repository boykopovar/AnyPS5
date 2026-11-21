#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
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
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::uint32_t ComputeQueue = 0x20;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Unwritten = 0xcafebabeu;
constexpr std::uint32_t Constant = 0x00010000u;

constexpr std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return (opcode << 25u) | (destination << 17u) | (source1 << 9u) | source0;
}

constexpr std::uint32_t BufferOperands = (0x80u << 24u) | (1u << 16u) | (2u << 8u) | 1u;

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    Vop2(0x1au, 1u, 0x82u, 0u),
    0xf4080100u, 0xfa000000u,
    0xbf8cc07fu,
    0xe0301000u, BufferOperands,
    0xbf8c3f70u,
    0xf4080100u, 0xfa000010u,
    0xbf8cc07fu,
    Vop2(0x25u, 2u, 0xffu, 2u), Constant,
    0xe0701000u, BufferOperands,
    0xbf810000u,
};

std::uint32_t* Tables = nullptr;
std::uint32_t* Data = nullptr;

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("dispatch relocation: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("dispatch relocation: " + what);
}

std::uint32_t* InputA() { return Data; }
std::uint32_t* InputB() { return Data + 0x400; }
std::uint32_t* Output() { return Data + 0x800; }

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t* WriteTable(std::size_t slot, const std::uint32_t* input, std::uint32_t inputBytes) {
    auto* table = Tables + slot * 64u;
    const auto in = BufferDescriptor(input, inputBytes);
    const auto out = BufferDescriptor(Output(), BufferBytes);
    std::memcpy(table, in.data(), sizeof(in));
    std::memcpy(table + 4, out.data(), sizeof(out));
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(table), 64u);
    return table;
}

std::vector<std::uint32_t> Dispatch(const std::uint32_t* table) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(Code.data());
    const auto address = reinterpret_cast<std::uintptr_t>(table);
    return {
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 2u << 1u,
        0xc0027600u, 0x240u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u),
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
}

void Round(const std::uint32_t* table, const std::uint32_t* input, const std::string& what) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Output()[lane] = Unwritten;
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output()), BufferBytes);
    auto words = Dispatch(table);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, what + ": submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output()), BufferBytes);
        bool stored = true;
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) stored = stored && std::atomic_ref<std::uint32_t>(Output()[lane]).load(std::memory_order_acquire) != Unwritten;
        if (stored) break;
        check(std::chrono::steady_clock::now() < deadline, what + ": timed out waiting for the output");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto expected = input[lane] + Constant;
        check(Output()[lane] == expected, what + ": lane " + std::to_string(lane) + " is " + std::to_string(Output()[lane]) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }

        Shader shader{};
        shader.file_header = 0x34333231;
        shader.version = 0x18;
        shader.header_size = sizeof(Shader);
        shader.shader_size = sizeof(Code);
        shader.code = Code.data();
        AgcDriverRegisterShader_nid_postfix(&shader);

        Tables = NewBlock();
        Data = NewBlock();
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            InputA()[lane] = 0xa000u + lane;
            InputB()[lane] = 0xb000u + lane;
        }
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Data), BlockBytes);

        Round(WriteTable(0, InputA(), BufferBytes), InputA(), "the first dispatch");
        const auto first = AgcDriver::DriverDetail::DispatchRelocationsMade();

        Round(WriteTable(1, InputA(), BufferBytes), InputA(), "the table moved with the same contents");
        const auto moved = AgcDriver::DriverDetail::DispatchRelocationsMade();
        check(moved == first + 1, "the moved table was not served by a relocated entry");

        Round(WriteTable(2, InputB(), BufferBytes), InputB(), "the table moved with its input buffer moved");
        const auto rebound = AgcDriver::DriverDetail::DispatchRelocationsMade();
        check(rebound == moved + 1, "the moved input buffer was not served by a relocated entry");

        Round(WriteTable(3, InputA(), BufferBytes * 2u), InputA(), "the table moved with its input buffer resized");
        check(AgcDriver::DriverDetail::DispatchRelocationsMade() == rebound, "a resized input buffer was served by a relocated entry");

        std::printf("dispatch relocation: %llu dispatches served by relocated entries\n", static_cast<unsigned long long>(AgcDriver::DriverDetail::DispatchRelocationsMade() - first));
        LibcRunShutdown_nid_postfix();
        std::puts("dispatch relocation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
