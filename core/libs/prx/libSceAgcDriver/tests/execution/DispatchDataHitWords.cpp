#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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

std::uint32_t* Constants = nullptr;
std::uint32_t* Input = nullptr;
std::uint32_t* Output = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 13> AddressCode{0xf4000200u, 0xfa000000u, 0x34020082u, 0xdc308000u, 0x03020001u, 0xbf8cc07fu, 0x7e040208u, 0x4a040481u, 0xbf8c3f70u, 0x4a040503u, 0xe0701000u, 0x80010201u, 0xbf810000u};

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("dispatch data hit words: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("dispatch data hit words: " + what);
}

template <typename Ready>
void WaitUntil(Ready ready, const std::string& what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("dispatch data hit words: timed out waiting for " + what);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::vector<std::uint32_t> Dispatch() {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(AddressCode.data());
    const auto constants = reinterpret_cast<std::uintptr_t>(Constants);
    const auto input = reinterpret_cast<std::uintptr_t>(Input);
    const auto output = BufferDescriptor(Output, BufferBytes);
    return {
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(constants), static_cast<std::uint32_t>(constants >> 32u), static_cast<std::uint32_t>(input), static_cast<std::uint32_t>(input >> 32u), output[0], output[1], output[2], output[3],
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
}

void SetConstant(std::uint32_t value) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Constants[lane] = value;
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Constants), BufferBytes);
}

void CheckOutput(std::uint32_t expected, const std::string& what) {
    WaitUntil([] {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            if (std::atomic_ref<std::uint32_t>(Output[lane]).load(std::memory_order_acquire) == Unwritten) return false;
        }
        return true;
    }, what + " to store");
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) check(Output[lane] == expected, what + ": lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + ", expected " + std::to_string(expected));
}

void Round(std::uint32_t value, const std::string& what) {
    SetConstant(value);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Output[lane] = Unwritten;
    auto words = Dispatch();
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
    CheckOutput(value + 1u, what);
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
        shader.shader_size = sizeof(AddressCode);
        shader.code = AddressCode.data();
        AgcDriverRegisterShader_nid_postfix(&shader);

        Constants = NewBlock();
        Input = NewBlock();
        Output = NewBlock();

        Round(0x11112222u, "the first dispatch");
        const std::array<std::uint32_t, 4> values{0x33334444u, 0x55556666u, 0x7a7b7c7du, 0x0f1e2d3cu};
        for (std::size_t round = 0; round < values.size(); ++round) {
            const auto mismatches = AgcDriver::Graphics::SnapshotMismatches();
            const auto what = "data hit " + std::to_string(round + 1) + ", resolved ahead on the compute queue";
            Round(values[round], what);
            const auto added = AgcDriver::Graphics::SnapshotMismatches() - mismatches;
            check(added == 0, what + ": its snapshots differ from the registered memory serving them (" + std::to_string(added) + " snapshot mismatches)");
        }
        std::printf("dispatch data hit words: %zu data hits resolved ahead read their own live words\n", values.size());

        LibcRunShutdown_nid_postfix();
        std::puts("dispatch data hit words tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
