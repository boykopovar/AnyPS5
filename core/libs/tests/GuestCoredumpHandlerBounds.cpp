#include "prx/libkernel/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

alignas(16) std::array<std::byte, 0x40> process{};
constexpr int CoredumpInvalidArgument = static_cast<int>(0x81180000u);
constexpr int CoredumpNotRegistered = static_cast<int>(0x81180001u);
constexpr int CoredumpAlreadyRegistered = static_cast<int>(0x81180002u);

int APS5_VABI DummyHandler(void*) {
    return 0;
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    const std::uint64_t processSize = process.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x06500000;
    std::memcpy(process.data(), &processSize, sizeof(processSize));
    std::memcpy(process.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()));

    const std::int32_t testTypes[] = {
        0, 1, 2, 3, 10, 100,
        -1, -2, -100,
        INT32_MIN, INT32_MIN + 1,
        INT32_MAX, INT32_MAX - 1
    };
    for (const std::int32_t type : testTypes) {
        Require(sceCoredumpSetUserDataType(type) == 0);
    }

    Require(sceCoredumpUnregisterCoredumpHandler() == CoredumpNotRegistered);

    Require(sceCoredumpRegisterCoredumpHandler(0, 0x4000, 0) == CoredumpInvalidArgument);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0x8000, 0x1234) == CoredumpInvalidArgument);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0, 0) == CoredumpInvalidArgument);

    const std::size_t invalidLowStackSizes[] = {
        0, 1, 2, 0xff, 0x1000, 0x2000, 0x3000, 0x3ffe, 0x3fff
    };
    for (const std::size_t size : invalidLowStackSizes) {
        Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<std::uint64_t>(&DummyHandler), size, 0) == CoredumpInvalidArgument);
    }

    const std::size_t invalidHighStackSizes[] = {
        0x20000000,
        0x20000001,
        0x30000000,
        0x40000000,
        0x7fffffff,
        0x80000000,
        0xffffffff,
        SIZE_MAX - 1,
        SIZE_MAX
    };
    for (const std::size_t size : invalidHighStackSizes) {
        Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<std::uint64_t>(&DummyHandler), size, 0) == CoredumpInvalidArgument);
    }

    const std::size_t validStackSizes[] = {
        0x4000,
        0x4001,
        0x8000,
        0x10000,
        0x20000,
        0x40000,
        0x100000,
        0x1000000,
        0x10000000,
        0x1fffffff
    };
    for (const std::size_t size : validStackSizes) {
        uint64_t contextValue = static_cast<uint64_t>(size);
        Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<std::uint64_t>(&DummyHandler), size, contextValue) == 0);
        Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<std::uint64_t>(&DummyHandler), size, contextValue) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0x1000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpRegisterCoredumpHandler(0, 0x40000000, 0) == CoredumpAlreadyRegistered);
        Require(sceCoredumpSetUserDataType(static_cast<std::int32_t>(size & 0x7fffffff)) == 0);
        Require(sceCoredumpUnregisterCoredumpHandler() == 0);
        Require(sceCoredumpUnregisterCoredumpHandler() == CoredumpNotRegistered);
    }

    {
        std::atomic<int> successCount{0};
        std::atomic<int> alreadyCount{0};
        std::atomic<int> invalidCount{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> workers;
        workers.reserve(8);
        for (int i = 0; i < 8; ++i) {
            workers.emplace_back([&, i] {
                while (!start.load(std::memory_order_acquire)) {}
                const int result = sceCoredumpRegisterCoredumpHandler(
                    reinterpret_cast<std::uint64_t>(&DummyHandler),
                    0x8000,
                    static_cast<uint64_t>(i));
                if (result == 0) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                } else if (result == CoredumpAlreadyRegistered) {
                    alreadyCount.fetch_add(1, std::memory_order_relaxed);
                } else {
                    invalidCount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& w : workers) {
            w.join();
        }
        Require(successCount.load() == 1);
        Require(alreadyCount.load() == 7);
        Require(invalidCount.load() == 0);
        Require(sceCoredumpUnregisterCoredumpHandler() == 0);
        Require(sceCoredumpUnregisterCoredumpHandler() == CoredumpNotRegistered);
    }

    {
        std::atomic<bool> startSet{false};
        std::atomic<int> errorCount{0};
        std::vector<std::thread> setWorkers;
        setWorkers.reserve(8);
        for (int i = 0; i < 8; ++i) {
            setWorkers.emplace_back([&, i] {
                while (!startSet.load(std::memory_order_acquire)) {}
                for (int iter = 0; iter < 50; ++iter) {
                    if (sceCoredumpSetUserDataType(static_cast<std::int32_t>(i * 1000 + iter)) != 0) {
                        errorCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        startSet.store(true, std::memory_order_release);
        for (auto& w : setWorkers) {
            w.join();
        }
        Require(errorCount.load() == 0);
    }

    return 0;
}
