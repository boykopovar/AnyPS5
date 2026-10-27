#include "prx/libSceZlib/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <cstring>
#include <stdexcept>
#include <thread>

static void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Zlib assertion failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
static void RequireThrows(TAction action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

int main() {
    constexpr std::array<std::uint8_t, 77> compressed{0x78, 0x9c, 0xed, 0xca, 0xb1, 0xd, 0x80, 0x20, 0x10, 0x40, 0xd1, 0x9e, 0x29, 0x6e, 0x2, 0x3a, 0x6, 0x60, 0x3, 0x12, 0x26, 0x10, 0x30, 0x91, 0x88, 0x47, 0x71, 0xa1, 0xd0, 0xe9, 0x1d, 0x82, 0xf6, 0xbf, 0xfa, 0x45, 0x7d, 0x53, 0xe, 0xf2, 0x8d, 0x5e, 0xa4, 0x5e, 0x67, 0xbd, 0x6d, 0x3d, 0x72, 0x68, 0x93, 0x32, 0x97, 0x36, 0xf3, 0x2e, 0x12, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0xec, 0x85, 0x1f, 0xf5, 0xf3, 0xbd, 0x4c};
    constexpr char text[] = "AnyPS5 zlib checksum and bounds.\n";
    constexpr std::uint32_t expectedLength = (sizeof(text) - 1) * 128;
    alignas(2048) std::array<std::uint8_t, 2048 + 65536 + 2048> storage{};
    auto* output = storage.data() + 2048;
    std::uint64_t id = 0;
    std::uint32_t produced = 0;
    std::int32_t status = 0;
    Require(sceZlibFinalize() == static_cast<std::int32_t>(0x81120032));
    Require(sceZlibInflate(nullptr, 0, nullptr, 0, nullptr) == static_cast<std::int32_t>(0x81120032));
    Require(sceZlibInitialize(nullptr, 1) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibInitialize(output, 0) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibInitialize(nullptr, 0) == 0);
    Require(sceZlibInitialize(nullptr, 0) == static_cast<std::int32_t>(0x81120033));
    RequireThrows([&] { sceZlibInflate(compressed.data(), compressed.size(), output + 1, 8192, &id); });
    for (const auto capacity : {0u, 1u, 2047u, 2049u, 65535u, 65537u}) {
        Require(sceZlibInflate(compressed.data(), compressed.size(), output, capacity, &id) == static_cast<std::int32_t>(0x81120016));
        Require(id == 0);
    }
    Require(sceZlibInflate(nullptr, compressed.size(), output, 8192, &id) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibWaitForDone(nullptr, nullptr) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibGetResult(0, nullptr, &status) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibGetResult(0, &produced, nullptr) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibGetResult(0, &produced, &status) == static_cast<std::int32_t>(0x81120002));
    std::uint64_t previous = 0;
    auto decode = [&](const void* input, std::uint32_t inputLength, std::uint32_t capacity, std::int32_t expectedStatus) {
        storage.fill(0xA5);
        Require(sceZlibInflate(input, inputLength, output, capacity, &id) == 0);
        Require(id > previous);
        previous = id;
        std::uint64_t completed;
        Require(sceZlibWaitForDone(&completed, nullptr) == 0);
        Require(completed == id);
        std::uint32_t timeout = 0;
        Require(sceZlibWaitForDone(&completed, &timeout) == static_cast<std::int32_t>(0x81120027));
        Require(sceZlibGetResult(id + 1, &produced, &status) == static_cast<std::int32_t>(0x81120002));
        Require(sceZlibGetResult(id, nullptr, &status) == static_cast<std::int32_t>(0x81120016));
        Require(sceZlibGetResult(id, &produced, &status) == 0);
        if (status != expectedStatus) std::fprintf(stderr, "input=%u capacity=%u status=%x expected=%x produced=%u\n", inputLength, capacity, static_cast<unsigned>(status), static_cast<unsigned>(expectedStatus), produced);
        Require(status == expectedStatus && produced <= capacity);
        std::uint32_t producedAgain = 0;
        std::int32_t statusAgain = 0;
        Require(sceZlibGetResult(id, &producedAgain, &statusAgain) == 0);
        Require(producedAgain == produced && statusAgain == status);
        Require(std::all_of(storage.begin(), storage.begin() + 2048, [](auto byte) { return byte == 0xA5; }));
        Require(std::all_of(storage.begin() + 2048 + capacity, storage.end(), [](auto byte) { return byte == 0xA5; }));
    };
    decode(compressed.data(), compressed.size(), 8192, 0);
    Require(produced == expectedLength);
    for (std::uint32_t offset = 0; offset < produced; offset += sizeof(text) - 1)
        Require(std::memcmp(output + offset, text, sizeof(text) - 1) == 0);
    decode(compressed.data(), compressed.size(), 2048, static_cast<std::int32_t>(0x8112001C));
    auto corrupt = compressed;
    corrupt.back() ^= 1;
    decode(corrupt.data(), corrupt.size(), 8192, static_cast<std::int32_t>(0x811200FF));
    decode(compressed.data(), compressed.size() - 1, 65536, static_cast<std::int32_t>(0x811200FF));
    decode(compressed.data(), compressed.size() - 4, 8192, static_cast<std::int32_t>(0x811200FF));
    decode(compressed.data(), compressed.size() - 1, 8192, static_cast<std::int32_t>(0x811200FF));
    alignas(2048) std::array<std::uint8_t, 3 * 8192> batch{};
    std::array<std::uint64_t, 3> ids{};
    for (std::size_t index = 0; index < ids.size(); ++index)
        Require(sceZlibInflate(compressed.data(), compressed.size(), batch.data() + index * 8192, 8192, &ids[index]) == 0);
    Require(ids[0] < ids[1] && ids[1] < ids[2]);
    for (std::uint64_t expected : ids) {
        std::uint64_t completed = 0;
        Require(sceZlibWaitForDone(&completed, nullptr) == 0);
        Require(completed == expected);
    }
    for (std::size_t index = 0; index < ids.size(); ++index) {
        Require(sceZlibGetResult(ids[index], &produced, &status) == 0);
        Require(status == 0 && produced == expectedLength);
        Require(std::memcmp(batch.data() + index * 8192, text, sizeof(text) - 1) == 0);
    }
    std::uint64_t completed = 0;
    std::uint32_t timeout = 0;
    Require(sceZlibInflate(compressed.data(), compressed.size(), output, 8192, &id) == 0);
    Require(sceZlibWaitForDone(&completed, &timeout) == 0);
    Require(completed == id);
    timeout = 20000;
    const auto start = std::chrono::steady_clock::now();
    Require(sceZlibWaitForDone(&completed, &timeout) == static_cast<std::int32_t>(0x81120027));
    const auto waited = std::chrono::steady_clock::now() - start;
    Require(waited >= std::chrono::microseconds(20000) && waited < std::chrono::seconds(1));
    timeout = 1000;
    const auto shortStart = std::chrono::steady_clock::now();
    for (int round = 0; round < 20; ++round) Require(sceZlibWaitForDone(&completed, &timeout) == static_cast<std::int32_t>(0x81120027));
    const auto shortWaits = std::chrono::steady_clock::now() - shortStart;
    Require(shortWaits >= std::chrono::milliseconds(20) && shortWaits < std::chrono::milliseconds(200));
    std::thread waiter([&] { Require(sceZlibWaitForDone(&completed, nullptr) == 0); });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(sceZlibInflate(compressed.data(), compressed.size(), output, 8192, &id) == 0);
    waiter.join();
    Require(completed == id);
    Require(sceZlibInflate(compressed.data(), compressed.size(), output, 8192, &id) == 0);
    Require(sceZlibFinalize() == 0);
    Require(sceZlibInitialize(nullptr, 0) == 0);
    Require(sceZlibGetResult(id, &produced, &status) == static_cast<std::int32_t>(0x81120002));
    bool interrupted = false;
    std::thread interruptedWaiter([&] {
        std::uint32_t longTimeout = 5000000;
        const auto waitStart = std::chrono::steady_clock::now();
        try {
            interrupted = sceZlibWaitForDone(&completed, &longTimeout) == static_cast<std::int32_t>(0x81120032);
        } catch (const std::runtime_error&) {
            interrupted = true;
        }
        Require(std::chrono::steady_clock::now() - waitStart < std::chrono::seconds(1));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(sceZlibFinalize() == 0);
    interruptedWaiter.join();
    Require(interrupted);
    Require(sceZlibGetResult(id, &produced, &status) == static_cast<std::int32_t>(0x81120032));
    std::array<std::uint8_t, 65536> scratch{};
    RequireThrows([&] { sceZlibInitialize(scratch.data(), scratch.size()); });
    Require(sceZlibInitialize(nullptr, 0) == 0);
    Require(sceZlibInflate(compressed.data(), compressed.size(), output, 65536, &id) == 0);
    Require(sceZlibFinalize() == 0);
    Require(sceZlibInitialize(nullptr, 0) == 0);
    Require(sceZlibGetResult(id, &produced, &status) == static_cast<std::int32_t>(0x81120002));
    Require(sceZlibFinalize() == 0);
}
