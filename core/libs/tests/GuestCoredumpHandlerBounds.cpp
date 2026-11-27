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
        std::atomic<bool> startSet{false};
        std::atomic<int> errorCount{0};
        std::vector<std::thread> setWorkers;
        setWorkers.reserve(8);
        for (int i = 0; i < 8; ++i) {
            setWorkers.emplace_back([&, i] {
                while (!startSet.load(std::memory_order_acquire)) { std::this_thread::yield(); }
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

    for (int round = 0; round < 128; ++round) {
        std::atomic<int> readyReg{0};
        std::atomic<bool> startReg{false};
        std::atomic<int> regOkCount{0};
        std::atomic<int> regAlreadyCount{0};
        std::atomic<int> regUnexpectedCount{0};
        std::vector<std::thread> regWorkers;
        regWorkers.reserve(8);
        for (int i = 0; i < 8; ++i) {
            regWorkers.emplace_back([&, i] {
                readyReg.fetch_add(1, std::memory_order_release);
                while (!startReg.load(std::memory_order_acquire)) { std::this_thread::yield(); }
                const int rc = sceCoredumpRegisterCoredumpHandler(
                    reinterpret_cast<std::uint64_t>(&DummyHandler),
                    0x4000,
                    static_cast<std::uint64_t>(i));
                if (rc == 0) {
                    regOkCount.fetch_add(1, std::memory_order_relaxed);
                } else if (rc == CoredumpAlreadyRegistered) {
                    regAlreadyCount.fetch_add(1, std::memory_order_relaxed);
                } else {
                    regUnexpectedCount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        while (readyReg.load(std::memory_order_acquire) < 8) { std::this_thread::yield(); }
        startReg.store(true, std::memory_order_release);
        for (auto& w : regWorkers) {
            w.join();
        }
        Require(regOkCount.load() == 1);
        Require(regAlreadyCount.load() == 7);
        Require(regUnexpectedCount.load() == 0);

        std::atomic<int> readyUnreg{0};
        std::atomic<bool> startUnreg{false};
        std::atomic<int> unregOkCount{0};
        std::atomic<int> unregNotRegCount{0};
        std::atomic<int> unregUnexpectedCount{0};
        std::vector<std::thread> unregWorkers;
        unregWorkers.reserve(8);
        for (int i = 0; i < 8; ++i) {
            unregWorkers.emplace_back([&] {
                readyUnreg.fetch_add(1, std::memory_order_release);
                while (!startUnreg.load(std::memory_order_acquire)) { std::this_thread::yield(); }
                const int rc = sceCoredumpUnregisterCoredumpHandler();
                if (rc == 0) {
                    unregOkCount.fetch_add(1, std::memory_order_relaxed);
                } else if (rc == CoredumpNotRegistered) {
                    unregNotRegCount.fetch_add(1, std::memory_order_relaxed);
                } else {
                    unregUnexpectedCount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        while (readyUnreg.load(std::memory_order_acquire) < 8) { std::this_thread::yield(); }
        startUnreg.store(true, std::memory_order_release);
        for (auto& w : unregWorkers) {
            w.join();
        }
        Require(unregOkCount.load() == 1);
        Require(unregNotRegCount.load() == 7);
        Require(unregUnexpectedCount.load() == 0);
    }

    return 0;
}
