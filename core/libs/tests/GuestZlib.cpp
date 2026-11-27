#include "prx/libSceZlib/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <latch>
#include <semaphore>
#include <source_location>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

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

static std::uint32_t Adler32(const std::uint8_t* data, std::size_t length) {
    std::uint32_t s1 = 1, s2 = 0;
    for (std::size_t i = 0; i < length; ++i) {
        s1 = (s1 + data[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (s2 << 16) | s1;
}

static std::vector<std::uint8_t> MakeStoredZlib(const std::uint8_t* payload, std::size_t payloadLen, std::size_t trailingEmptyBlocks = 0) {
    std::vector<std::uint8_t> out;
    out.push_back(0x78);
    out.push_back(0x01);
    std::size_t offset = 0;
    while (offset < payloadLen || offset == 0) {
        const std::size_t blockLen = std::min<std::size_t>(payloadLen - offset, 65535);
        const bool isLastPayload = (offset + blockLen >= payloadLen);
        const bool isFinal = isLastPayload && (trailingEmptyBlocks == 0);
        out.push_back(isFinal ? 0x01 : 0x00);
        out.push_back(static_cast<std::uint8_t>(blockLen & 0xFF));
        out.push_back(static_cast<std::uint8_t>((blockLen >> 8) & 0xFF));
        const std::uint16_t nlen = static_cast<std::uint16_t>(~static_cast<std::uint16_t>(blockLen));
        out.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
        out.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xFF));
        out.insert(out.end(), payload + offset, payload + offset + blockLen);
        offset += blockLen;
        if (isLastPayload) break;
    }
    for (std::size_t b = 0; b < trailingEmptyBlocks; ++b) {
        const bool isFinal = (b + 1 == trailingEmptyBlocks);
        out.push_back(isFinal ? 0x01 : 0x00);
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0xFF);
        out.push_back(0xFF);
    }
    const std::uint32_t checksum = Adler32(payload, payloadLen);
    out.push_back(static_cast<std::uint8_t>((checksum >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((checksum >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((checksum >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(checksum & 0xFF));
    return out;
}

static void TestConcurrentWaiters(const void* source, std::uint32_t sourceLength) {
    constexpr std::size_t capacity = 8192;
    alignas(2048) std::array<std::uint8_t, 2 * capacity> output{};
    std::array<std::uint64_t, 2> submitted{}, observed{};
    std::array<std::int32_t, 2> statuses{};
    std::latch started{2};
    std::counting_semaphore<2> finished{0};
    auto wait = [&](std::size_t index) {
        std::uint32_t timeout = 10000000;
        started.count_down();
        statuses[index] = sceZlibWaitForDone(&observed[index], &timeout);
        finished.release();
    };
    std::jthread first(wait, 0), second(wait, 1);
    started.wait();
    for (std::size_t index = 0; index < submitted.size(); ++index) {
        Require(sceZlibInflate(source, sourceLength, output.data() + index * capacity, capacity, &submitted[index]) == 0);
        Require(finished.try_acquire_for(std::chrono::seconds(3)));
    }
    first.join();
    second.join();
    Require(statuses[0] == 0 && statuses[1] == 0);
    Require(submitted[0] < submitted[1]);
    std::ranges::sort(observed);
    Require(observed == submitted);
    std::uint64_t remaining = 0;
    std::uint32_t poll = 0;
    Require(sceZlibWaitForDone(&remaining, &poll) == static_cast<std::int32_t>(0x81120027));
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
    Require(sceZlibWaitForDone(nullptr, nullptr) == static_cast<std::int32_t>(0x81120032));
    std::uint64_t earlyId = 0;
    Require(sceZlibWaitForDone(&earlyId, nullptr) == static_cast<std::int32_t>(0x81120032));
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
    Require(sceZlibInflate(compressed.data(), 0, output, 8192, &id) == static_cast<std::int32_t>(0x81120016));
    Require(sceZlibInflate(compressed.data(), compressed.size(), nullptr, 8192, &id) == static_cast<std::int32_t>(0x81120016));
    errno = 71;
    Require(sceZlibInflate(compressed.data(), compressed.size(), output, 8192, reinterpret_cast<std::uint64_t*>(0x10)) == static_cast<std::int32_t>(0x81120016));
    Require(errno == 71);
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
        Require(sceZlibGetResult(id, &produced, nullptr) == static_cast<std::int32_t>(0x81120016));
        produced = 0xFEDCBA98;
        status = static_cast<std::int32_t>(0x12345678);
        Require(sceZlibGetResult(id, reinterpret_cast<std::uint32_t*>(0x10), &status) == static_cast<std::int32_t>(0x81120016));
        Require(produced == 0xFEDCBA98 && status == static_cast<std::int32_t>(0x12345678));
        produced = 0xFEDCBA98;
        status = static_cast<std::int32_t>(0x12345678);
        Require(sceZlibGetResult(id, &produced, reinterpret_cast<std::int32_t*>(0x10)) == static_cast<std::int32_t>(0x81120016));
        Require(produced == 0xFEDCBA98 && status == static_cast<std::int32_t>(0x12345678));
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
    auto badHeader = compressed;
    badHeader.front() = 0;
    decode(badHeader.data(), badHeader.size(), 8192, static_cast<std::int32_t>(0x811200FF));
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
    TestConcurrentWaiters(compressed.data(), compressed.size());
    {
        constexpr auto invalid = static_cast<std::int32_t>(0x81120016);
        Require(sceZlibInflate(compressed.data(), compressed.size(), output, 8192, &id) == 0);
        std::uint32_t poll = 0;
        Require(sceZlibWaitForDone(reinterpret_cast<std::uint64_t*>(0x10), &poll) == invalid);
        std::int32_t retryStatus{};
        std::uint64_t retried{};
        std::thread retry([&] {
            std::uint32_t retryPoll = 1000000;
            retryStatus = sceZlibWaitForDone(&retried, &retryPoll);
        });
        retry.join();
        Require(retryStatus == 0 && retried == id);
        Require(sceZlibWaitForDone(&retried, &poll) == static_cast<std::int32_t>(0x81120027));
    }
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
    Require(sceZlibFinalize() == static_cast<std::int32_t>(0x81120032));
    Require(sceZlibInflate(nullptr, 0, nullptr, 0, nullptr) == static_cast<std::int32_t>(0x81120032));
    Require(sceZlibWaitForDone(nullptr, nullptr) == static_cast<std::int32_t>(0x81120032));
    Require(sceZlibInitialize(nullptr, 0) == 0);
    {
        constexpr std::uint32_t bigPayloadLen = 20480;
        std::vector<std::uint8_t> bigPayload(bigPayloadLen);
        for (std::uint32_t i = 0; i < bigPayloadLen; ++i) bigPayload[i] = static_cast<std::uint8_t>(i & 0xFF);
        const auto bigStream = MakeStoredZlib(bigPayload.data(), bigPayloadLen);
        Require(bigStream.size() > 16384);
        decode(bigStream.data(), static_cast<std::uint32_t>(bigStream.size()), 65536, 0);
        Require(produced == bigPayloadLen);
        Require(std::memcmp(output, bigPayload.data(), bigPayloadLen) == 0);
    }
    {
        constexpr std::uint32_t exactMultiChunkLen = 2048;
        std::vector<std::uint8_t> exactPayload(exactMultiChunkLen);
        for (std::uint32_t i = 0; i < exactMultiChunkLen; ++i) exactPayload[i] = static_cast<std::uint8_t>((i * 7) & 0xFF);
        auto exactMultiChunk = MakeStoredZlib(exactPayload.data(), exactMultiChunkLen, 3000);
        Require(exactMultiChunk.size() > 16384);
        decode(exactMultiChunk.data(), static_cast<std::uint32_t>(exactMultiChunk.size()), 2048, 0);
        Require(produced == exactMultiChunkLen);
        Require(std::memcmp(output, exactPayload.data(), exactMultiChunkLen) == 0);

        exactMultiChunk.back() ^= 1;
        decode(exactMultiChunk.data(), static_cast<std::uint32_t>(exactMultiChunk.size()),
               2048, static_cast<std::int32_t>(0x811200FF));
        Require(produced == exactMultiChunkLen);
    }
    {
        constexpr std::uint32_t oneByteOverflowLen = 4097;
        std::vector<std::uint8_t> overflowPayload(oneByteOverflowLen, 0xAB);
        const auto overflowStream = MakeStoredZlib(overflowPayload.data(), oneByteOverflowLen);
        decode(overflowStream.data(), static_cast<std::uint32_t>(overflowStream.size()),
               4096, static_cast<std::int32_t>(0x8112001C));
        Require(produced == 4096);

        const auto multiChunkOverflow = MakeStoredZlib(overflowPayload.data(), 2049, 3000);
        Require(multiChunkOverflow.size() > 16384);
        decode(multiChunkOverflow.data(), static_cast<std::uint32_t>(multiChunkOverflow.size()),
               2048, static_cast<std::int32_t>(0x8112001C));
        Require(produced == 2048);
    }
    {
        constexpr std::uint32_t truncPayloadLen = 4096;
        std::vector<std::uint8_t> truncPayload(truncPayloadLen, 0xCD);
        auto truncStream = MakeStoredZlib(truncPayload.data(), truncPayloadLen);
        truncStream.resize(truncStream.size() - 4);
        decode(truncStream.data(), static_cast<std::uint32_t>(truncStream.size()),
               4096, static_cast<std::int32_t>(0x811200FF));
        Require(produced == 4096);
    }
    Require(sceZlibFinalize() == 0);
}
