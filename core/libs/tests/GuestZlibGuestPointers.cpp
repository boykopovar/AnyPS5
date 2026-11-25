#include "prx/libSceZlib/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <semaphore>
#include <thread>

namespace {

void Require(bool valid, int line) {
    if (valid) return;
    std::fprintf(stderr, "Zlib guest pointer check failed at line %d\n", line);
    std::abort();
}

#define Check(value) Require((value), __LINE__)

struct RequestWriteInterrupted {};

std::uintptr_t requestAddress = 0;
int requestWrites = 0;

void InterruptRequestWrite(std::uintptr_t address, std::size_t) {
    if (address == requestAddress && ++requestWrites == 2) throw RequestWriteInterrupted{};
}

struct WaitHandoff {
    std::array<std::uint32_t, 2> timeouts{10000000, 10000000};
    std::array<std::uint64_t, 2> completed{};
    std::counting_semaphore<2> waiting{0};
    std::counting_semaphore<2> finished{0};
    bool interrupted = false;
    int interruptedErrno = 0;
};

WaitHandoff* activeHandoff = nullptr;

void RecordWaitTimeoutRead(std::uintptr_t address, std::size_t) {
    auto& handoff = *activeHandoff;
    for (const auto& timeout : handoff.timeouts) {
        if (address == reinterpret_cast<std::uintptr_t>(&timeout)) handoff.waiting.release();
    }
}

void InterruptFirstWaitWrite(std::uintptr_t address, std::size_t) {
    auto& handoff = *activeHandoff;
    if (handoff.interrupted) return;
    for (const auto& result : handoff.completed) {
        if (address != reinterpret_cast<std::uintptr_t>(&result)) continue;
        handoff.interrupted = true;
        errno = ERANGE;
        throw RequestWriteInterrupted{};
    }
}

void CheckWaitHandoff(const void* input, std::uint32_t length, void* output, std::uint32_t capacity) {
    using namespace std::chrono_literals;
    WaitHandoff handoff;
    activeHandoff = &handoff;
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(RecordWaitTimeoutRead);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(InterruptFirstWaitWrite);
    auto wait = [&](std::size_t index) {
        errno = EDOM;
        try {
            Check(sceZlibWaitForDone(&handoff.completed[index], &handoff.timeouts[index]) == 0);
        } catch (const RequestWriteInterrupted&) {
            handoff.interruptedErrno = errno;
        }
        handoff.finished.release();
    };
    std::jthread first(wait, 0), second(wait, 1);
    Check(handoff.waiting.try_acquire_for(3s) && handoff.waiting.try_acquire_for(3s));
    std::uint64_t submitted = 0;
    Check(sceZlibInflate(input, length, output, capacity, &submitted) == 0 && submitted != 0);
    Check(handoff.finished.try_acquire_for(3s));
    Check(handoff.finished.try_acquire_for(3s));
    first.join();
    second.join();
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    activeHandoff = nullptr;
    Check(handoff.interrupted && handoff.interruptedErrno == EDOM);
    Check(handoff.completed[0] == submitted || handoff.completed[1] == submitted);
    Check(handoff.completed[0] == 0 || handoff.completed[1] == 0);
    std::uint64_t remaining = 0;
    std::uint32_t poll = 0;
    Check(sceZlibWaitForDone(&remaining, &poll) == static_cast<std::int32_t>(0x81120027));
}

}

int main() {
    constexpr std::array<std::uint8_t, 77> compressed{0x78, 0x9c, 0xed, 0xca, 0xb1, 0xd, 0x80, 0x20, 0x10, 0x40, 0xd1, 0x9e, 0x29, 0x6e, 0x2, 0x3a, 0x6, 0x60, 0x3, 0x12, 0x26, 0x10, 0x30, 0x91, 0x88, 0x47, 0x71, 0xa1, 0xd0, 0xe9, 0x1d, 0x82, 0xf6, 0xbf, 0xfa, 0x45, 0x7d, 0x53, 0xe, 0xf2, 0x8d, 0x5e, 0xa4, 0x5e, 0x67, 0xbd, 0x6d, 0x3d, 0x72, 0x68, 0x93, 0x32, 0x97, 0x36, 0xf3, 0x2e, 0x12, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0xec, 0x85, 0x1f, 0xf5, 0xf3, 0xbd, 0x4c};
    const auto invalidSource = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    const auto invalidDestination = reinterpret_cast<void*>(std::uintptr_t{0x1000});
    const auto invalidId = reinterpret_cast<std::uint64_t*>(std::uintptr_t{0x10});
    constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x81120016);
    constexpr std::int32_t timedOut = static_cast<std::int32_t>(0x81120027);
    alignas(2048) std::array<std::uint8_t, 8192> output{};
    std::uint64_t requestId = 0x1122334455667788;

    Check(sceZlibInitialize(nullptr, 0) == 0);
    errno = EDOM;
    Check(sceZlibInflate(invalidSource, 1, output.data(), output.size(), &requestId) == invalidArgument);
    Check(requestId == 0x1122334455667788 && errno == EDOM);
    Check(sceZlibInflate(compressed.data(), compressed.size(), invalidDestination, output.size(), &requestId) == invalidArgument);
    Check(requestId == 0x1122334455667788 && errno == EDOM);
    output.fill(0x5a);
    const auto originalOutput = output;
    Check(sceZlibInflate(compressed.data(), compressed.size(), output.data(), output.size(), invalidId) == invalidArgument);
    Check(output == originalOutput && errno == EDOM);
    std::uint64_t completed = 0;
    std::uint32_t timeout = 0;
    Check(sceZlibWaitForDone(&completed, &timeout) == timedOut);

    requestAddress = reinterpret_cast<std::uintptr_t>(&requestId);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(InterruptRequestWrite);
    bool interrupted = false;
    try {
        sceZlibInflate(compressed.data(), compressed.size(), output.data(), output.size(), &requestId);
    } catch (const RequestWriteInterrupted&) {
        interrupted = true;
    }
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    Check(interrupted && requestWrites == 2);
    Check(requestId == 0x1122334455667788);
    Check(sceZlibWaitForDone(&completed, &timeout) == timedOut && completed == 0);

    constexpr std::array<std::uint8_t, 8> empty{0x78, 0x9c, 0x03, 0, 0, 0, 0, 1};
    Check(sceZlibInflate(empty.data(), empty.size(), output.data(), output.size(), &requestId) == 0);
    Check(requestId != 0x1122334455667788);
    Check(sceZlibWaitForDone(&completed, &timeout) == 0 && completed == requestId);
    Check(sceZlibWaitForDone(&completed, &timeout) == timedOut);
    std::uint32_t produced = 1;
    std::int32_t status = -1;
    Check(sceZlibGetResult(requestId, &produced, &status) == 0 && produced == 0 && status == 0);
    CheckWaitHandoff(empty.data(), empty.size(), output.data(), output.size());
    Check(sceZlibFinalize() == 0);
}
