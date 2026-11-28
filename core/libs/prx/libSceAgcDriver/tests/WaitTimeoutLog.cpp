#include "prx/libSceAgcDriver/Submit/include/CommandBuffer.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/LogMacros.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct alignas(64) LabelState {
    volatile std::uint32_t label = 0;
    volatile std::uint32_t entered = 0;
    volatile std::uint32_t after = 0;
};

std::mutex capturedMutex;
std::string captured;

int Capture(std::FILE*, const char* text, std::size_t length) {
    std::lock_guard lock(capturedMutex);
    captured.append(text, length);
    return 0;
}

std::string Captured() {
    std::lock_guard lock(capturedMutex);
    return captured;
}

std::size_t Occurrences(const std::string& text, const std::string& part) {
    std::size_t count = 0;
    for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++count;
    return count;
}

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t Address(volatile std::uint32_t* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::array<std::uint32_t, 5> WriteData(volatile std::uint32_t* destination, std::uint32_t value) {
    const auto address = Address(destination);
    return {0xc0033700, 0x00100200, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value};
}

std::vector<std::uint32_t> WaitingCommands(LabelState& state, std::uint32_t reference) {
    const auto address = Address(&state.label);
    std::vector<std::uint32_t> words;
    const auto entered = WriteData(&state.entered, reference);
    const std::array<std::uint32_t, 7> wait{0xc0053c00, 0x13, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), reference, 0xffffffffu, 0x19};
    const auto after = WriteData(&state.after, reference);
    words.insert(words.end(), entered.begin(), entered.end());
    words.insert(words.end(), wait.begin(), wait.end());
    words.insert(words.end(), after.begin(), after.end());
    return words;
}

void Submit(const std::vector<std::uint32_t>& words) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the waiting submission failed");
}

void AwaitValue(const volatile std::uint32_t& value, std::uint32_t expected, const char* what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (value != expected) {
        Require(std::chrono::steady_clock::now() < deadline, what);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void AwaitCaptured(const std::string& part, const char* what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (Captured().find(part) == std::string::npos) {
        Require(std::chrono::steady_clock::now() < deadline, std::string(what) + "; captured:\n" + Captured());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

void CheckTimeoutReport() {
    static LabelState state;
    const auto label = Hex(Address(&state.label));
    const auto timedOut = "[gpu] queue 0x0 WAIT_REG_MEM at " + label + " timed out after 20ms (function 3 ref 0x1 mask 0xffffffff value 0x0)\n";

    Submit(WaitingCommands(state, 1));
    AwaitValue(state.entered, 1, "the queue did not reach its first wait");
    AwaitCaptured(timedOut, "the timed-out wait was not reported through the log sink");
    AwaitCaptured("[gpu]   preceding packet ", "the packets before the timed-out wait were not reported through the log sink");
    const auto report = Captured();
    Require(report.find("[gpu]   preceding packet ", report.find(timedOut)) != std::string::npos, "the preceding packets were reported before the timeout line");
    state.label = 1;
    AwaitValue(state.after, 1, "the released wait did not complete");

    state.label = 0;
    state.entered = 0;
    state.after = 0;
    Submit(WaitingCommands(state, 1));
    AwaitValue(state.entered, 1, "the queue did not reach its second wait");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto prefix = "[gpu] queue 0x0 WAIT_REG_MEM at " + label + " timed out after";
    Require(Occurrences(Captured(), prefix) == 1, "a second timeout at the same address and reference was reported again");
    state.label = 1;
    AwaitValue(state.after, 1, "the second released wait did not complete");
}

}

int main() {
    LogSink().store(&Capture, std::memory_order_release);
    try {
        CheckTimeoutReport();
        LogSink().store(nullptr, std::memory_order_release);
        LibcRunShutdown_nid_postfix();
        std::puts("a timed-out wait is reported through the log sink, once per address and reference");
        return 0;
    } catch (const std::exception& error) {
        LogSink().store(nullptr, std::memory_order_release);
        std::fprintf(stderr, "%s\n", error.what());
        try {
            LibcRunShutdown_nid_postfix();
        } catch (const std::exception& shutdown) {
            std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
        }
        return 1;
    }
}
