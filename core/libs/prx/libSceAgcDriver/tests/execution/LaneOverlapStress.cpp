#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t HalfBytes = BlockBytes / 2;
constexpr std::uint32_t SharedBlocks = 6;
constexpr std::uint32_t OwnBlocks = 4;
constexpr std::uint32_t Programs = 8;
constexpr std::uint32_t ProgramDwords = 64;
constexpr std::uint32_t OutputBase = 0x20;
constexpr std::uint32_t OutputStride = 0x400;
constexpr std::uint32_t CodeDwords = 13;
constexpr std::uint32_t MainQueue = 0x20;
constexpr std::uint32_t SideQueue = 0x38;
constexpr std::uint32_t AsyncQueue = 0x51;
constexpr std::uint32_t CopyQueue = 0x30;

alignas(256) constexpr std::array<std::uint32_t, 8> IncrementCode{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040481u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, Programs * ProgramDwords> AddressCode{};

std::array<std::uint32_t*, SharedBlocks> Shared{};
std::array<std::uint32_t*, OwnBlocks> Own{};
std::array<std::uint32_t*, 2> Address{};
std::uint32_t* Labels = nullptr;
std::uint32_t* Copied = nullptr;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("lane overlap stress: " + what);
}

std::uint32_t* NewBlock(bool halves) {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    check(block != nullptr, "cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    if (halves) {
        GuestAllocations::Mutation().Add(block, HalfBytes, true, true, true);
        GuestAllocations::Mutation().Add(reinterpret_cast<std::uint8_t*>(block) + HalfBytes, HalfBytes, true, true, true);
    } else {
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }
    return block;
}

void Register(const std::uint32_t* code, std::size_t bytes) {
    Shader shader{};
    shader.file_header = 0x34333231;
    shader.version = 0x18;
    shader.header_size = sizeof(Shader);
    shader.shader_size = static_cast<std::uint32_t>(bytes);
    shader.code = code;
    AgcDriverRegisterShader_nid_postfix(&shader);
}

void MakeAddressPrograms() {
    for (std::uint32_t program = 0; program < Programs; ++program) {
        const std::array<std::uint32_t, CodeDwords> code{
            0x3404008au, 0x34060082u, 0x4a0606ffu, OutputBase + program * OutputStride,
            0xdc308010u, 0x04000002u, 0xdc308014u, 0x05000002u,
            0xbf8c3f70u, 0x4a080b04u, 0xe0701000u, 0x80010403u, 0xbf810000u};
        std::copy(code.begin(), code.end(), AddressCode.begin() + program * ProgramDwords);
        Register(AddressCode.data() + program * ProgramDwords, CodeDwords * 4u);
    }
}

void Increments(std::vector<std::uint32_t>& words, std::span<std::uint32_t* const> blocks) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(IncrementCode.data());
    words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0037600u, 0x207u, Lanes, 1u, 1u, 0xc0017600u, 0x213u, 4u << 1u});
    for (auto* block : blocks) {
        const auto data = reinterpret_cast<std::uintptr_t>(block);
        words.insert(words.end(), {0xc0047600u, 0x240u, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), BufferBytes, 0x31016facu, 0xc0031500u, 1u, 1u, 1u, 0x8041u});
    }
}

void AddressDispatches(std::vector<std::uint32_t>& words, std::uint32_t* block) {
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    words.insert(words.end(), {
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(base >> 32u), 0u, 0u,
        static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 0xffffu), static_cast<std::uint32_t>(HalfBytes), 0x31016facu,
    });
    for (std::uint32_t program = 0; program < Programs; ++program) {
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(AddressCode.data() + program * ProgramDwords);
        words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
    }
}

void WaitLabel(std::vector<std::uint32_t>& words, const std::uint32_t* label, std::uint32_t atLeast) {
    const auto address = reinterpret_cast<std::uintptr_t>(label);
    words.insert(words.end(), {0xc0053c00u, 0x15u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), atLeast, 0xffffffffu, 0x19u});
}

void CopyData(std::vector<std::uint32_t>& words, const std::uint32_t* source, std::uint32_t* destination, std::uint32_t bytes) {
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    words.insert(words.end(), {0xc0055000u, 0u, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(from >> 32u), static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(to >> 32u), bytes});
}

void ReleaseLabel(std::vector<std::uint32_t>& words, std::uint32_t* label, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(label);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, 0u, 0u});
}

std::uint32_t AddressInput(std::uint32_t byte) {
    return 0x1000u + byte / 4u * 3u;
}

void FillAddressBlock(std::uint32_t* block) {
    for (std::uint32_t dword = 0; dword < BlockBytes / 4u; ++dword) block[dword] = AddressInput(dword * 4u);
    for (std::uint32_t program = 0; program < Programs; ++program) {
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) block[(OutputBase + program * OutputStride) / 4u + lane] = 0;
    }
}

void CheckValues(std::uint32_t round, std::uint32_t sharedExpected, std::uint32_t ownExpected) {
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Copied), BufferBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(Copied[lane]).load(std::memory_order_acquire);
        check(value == sharedExpected, "after round " + std::to_string(round) + " the copy of shared block 0 lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(sharedExpected));
    }
    for (std::uint32_t index = 0; index < SharedBlocks; ++index) {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Shared[index]), BufferBytes);
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto value = std::atomic_ref<std::uint32_t>(Shared[index][lane]).load(std::memory_order_acquire);
            check(value == sharedExpected, "after round " + std::to_string(round) + " shared block " + std::to_string(index) + " lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(sharedExpected));
        }
    }
    for (std::uint32_t index = 0; index < OwnBlocks; ++index) {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Own[index]), BufferBytes);
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto value = std::atomic_ref<std::uint32_t>(Own[index][lane]).load(std::memory_order_acquire);
            check(value == ownExpected, "after round " + std::to_string(round) + " side-ring block " + std::to_string(index) + " lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(ownExpected));
        }
    }
    for (std::uint32_t which = 0; which < Address.size(); ++which) {
        for (std::uint32_t program = 0; program < Programs; ++program) {
            const auto out = (OutputBase + program * OutputStride) / 4u;
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Address[which] + out), Lanes * 4u);
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                const auto read = lane * 1024u;
                const auto expected = AddressInput(read + 0x10u) + AddressInput(read + 0x14u);
                const auto value = std::atomic_ref<std::uint32_t>(Address[which][out + lane]).load(std::memory_order_acquire);
                check(value == expected, "after round " + std::to_string(round) + " address block " + std::to_string(which) + " program " + std::to_string(program) + " lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
            }
        }
    }
}

class Watchdog {
public:
    explicit Watchdog(int seconds) : seconds(seconds), thread([this] { run(); }) {}
    ~Watchdog() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        thread.join();
    }
    void Arm(std::string what) {
        std::lock_guard lock(mutex);
        armed = std::move(what);
        since = std::chrono::steady_clock::now();
        changed.notify_all();
    }
    void Disarm() {
        std::lock_guard lock(mutex);
        armed.clear();
    }

private:
    void run() {
        std::unique_lock lock(mutex);
        while (!stopping) {
            changed.wait_for(lock, std::chrono::milliseconds(250));
            if (!armed.empty() && std::chrono::steady_clock::now() - since > std::chrono::seconds(seconds)) {
                std::fprintf(stderr, "lane overlap stress: GPU-side stall: %s did not finish within %d s\n", armed.c_str(), seconds);
                std::fflush(stderr);
                std::_Exit(3);
            }
        }
    }
    int seconds;
    std::mutex mutex;
    std::condition_variable changed;
    std::string armed;
    std::chrono::steady_clock::time_point since{};
    bool stopping = false;
    std::thread thread;
};

void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    if (std::getenv(name) == nullptr) _putenv_s(name, value);
#else
    setenv(name, value, 0);
#endif
}

}

int main(int argc, char** argv) {
    SetEnv("APS5_ASYNC_LANE", "51");
    SetEnv("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
    SetEnv("APS5_BDA_STAGING_MIN_THREADS", "0");
    SetEnv("APS5_STAGING_DIFF", "1");
    try {
        const std::uint32_t rounds = argc > 1 ? static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 400u;
        const std::uint32_t checkEvery = argc > 2 ? static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 16u;
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }
        Register(IncrementCode.data(), sizeof(IncrementCode));
        MakeAddressPrograms();
        for (auto& block : Shared) block = NewBlock(false);
        for (auto& block : Own) block = NewBlock(false);
        for (auto& block : Address) {
            block = NewBlock(true);
            FillAddressBlock(block);
        }
        Labels = NewBlock(false);
        Copied = NewBlock(false);
        auto* mainLabel = Labels;
        auto* asyncLabel = Labels + 64;
        auto* copyLabel = Labels + 128;
        Watchdog watchdog(30);
        std::deque<std::vector<std::uint32_t>> kept;
        const auto submit = [&](std::uint32_t queue, std::vector<std::uint32_t> words) {
            kept.push_back(std::move(words));
            Packet packet{kept.back().data(), static_cast<std::uint32_t>(kept.back().size()), 0, {}};
            check(sceAgcDriverSubmitAcb(queue, &packet) == 0, "submission failed");
        };
        const auto started = std::chrono::steady_clock::now();
        for (std::uint32_t round = 0; round < rounds; ++round) {
            std::vector<std::uint32_t> main;
            WaitLabel(main, copyLabel, round);
            Increments(main, Shared);
            AddressDispatches(main, Address[0]);
            ReleaseLabel(main, mainLabel, round + 1);
            submit(MainQueue, std::move(main));
            std::vector<std::uint32_t> side;
            Increments(side, Own);
            AddressDispatches(side, Address[1]);
            submit(SideQueue, std::move(side));
            std::vector<std::uint32_t> async;
            WaitLabel(async, mainLabel, round + 1);
            Increments(async, Shared);
            AddressDispatches(async, Address[0]);
            ReleaseLabel(async, asyncLabel, round + 1);
            submit(AsyncQueue, std::move(async));
            std::vector<std::uint32_t> copy;
            WaitLabel(copy, asyncLabel, round + 1);
            CopyData(copy, Shared[0], Copied, BufferBytes);
            ReleaseLabel(copy, copyLabel, round + 1);
            submit(CopyQueue, std::move(copy));
            if ((round + 1) % checkEvery == 0 || round + 1 == rounds) {
                watchdog.Arm("round " + std::to_string(round + 1));
                AgcDriverWaitIdle_nid_postfix();
                CheckValues(round + 1, 2u * (round + 1), round + 1);
                watchdog.Disarm();
                kept.clear();
                if (round + 1 == checkEvery) {
                    std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
                    auto* recorder = AgcDriver::Graphics::Recorder::Active();
                    if (recorder == nullptr || !recorder->AsyncLaneActive()) {
                        std::puts("skipped: the device has no eligible compute-only queue for the async lane");
                        lock.unlock();
                        LibcRunShutdown_nid_postfix();
                        return VulkanTestSkipped;
                    }
                }
            }
        }
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::printf("lane overlap stress: %u rounds of lane-0 staged and address-based dispatch runs on 0x%x and 0x%x with 0x%x on the async lane, label-ordered, in %.1f s\n", rounds, MainQueue, SideQueue, AsyncQueue, seconds);
        LibcRunShutdown_nid_postfix();
        std::puts("lane overlap stress tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
