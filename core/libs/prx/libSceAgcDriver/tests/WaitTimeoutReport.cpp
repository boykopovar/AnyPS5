#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t Writes = 40;

alignas(64) volatile std::uint64_t label = 0;
alignas(64) volatile std::uint32_t done = 0;
alignas(64) volatile std::uint32_t stuckLabel = 0;
alignas(64) volatile std::uint32_t stuckDone = 0;
alignas(64) volatile std::uint32_t busyWork = 0;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "wait timeout report: cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }

    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

std::array<std::uint32_t, 8> ReleaseMem64(volatile std::uint64_t* destination, std::uint64_t value) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(destination));
    return {0xc0064900, 0x00000514, 0x40000000, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u), 0};
}

std::array<std::uint32_t, 5> WriteData(volatile void* destination, std::uint32_t value) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(destination));
    return {0xc0033700, 0x00100200, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value};
}

std::size_t Count(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) ++count;
    return count;
}

void CheckGroupedWriters() {
    std::vector<std::uint32_t> words;
    for (std::uint32_t write = 0; write < Writes; ++write) {
        const auto packet = WriteData(&label, write + 1);
        words.insert(words.end(), packet.begin(), packet.end());
    }
    const auto release = ReleaseMem64(&label, 1);
    words.insert(words.end(), release.begin(), release.end());
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&label));
    const std::array<std::uint32_t, 7> wait{0xc0053c00, 0x13, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 2, 0xffffffffu, 0x19};
    words.insert(words.end(), wait.begin(), wait.end());
    const auto finished = WriteData(&done, 1);
    words.insert(words.end(), finished.begin(), finished.end());

    StderrCapture capture;
    std::string text;
    try {
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "wait timeout report: submission failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        Require(done == 0, "wait timeout report: the wait for an unwritten value completed");
        label = 2;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (done != 1) {
            Require(std::chrono::steady_clock::now() < deadline, "wait timeout report: the wait did not complete after the label was written");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    text = capture.Finish();
    char target[32];
    std::snprintf(target, sizeof(target), "0x%llx", static_cast<unsigned long long>(address));
    Require(Count(text, std::string("WAIT_REG_MEM at ") + target + " timed out after") == 1, "wait timeout report: the wait was not reported once");
    Require(Count(text, "(function 3 ref 0x2 mask") == 1, "wait timeout report: the first wait's reference was not reported");
    const std::string writer = std::string("earlier write by queue 0x0 opcode 0x37 at ") + target + "+0x4";
    Require(Count(text, writer) == 1, "wait timeout report: the label's writer was not listed once");
    char lastWrite[32];
    std::snprintf(lastWrite, sizeof(lastWrite), "0x%x", Writes);
    Require(Count(text, writer + " (" + std::to_string(Writes) + " in the write history, the last of value " + lastWrite + ")") == 1, "wait timeout report: the writer line does not count the label's " + std::to_string(Writes) + " writes and name the last value written");
    Require(Count(text, std::string("earlier write by queue 0x0 opcode 0x49 at ") + target + "+0x8 (1 in the write history, the last of value 0x1)") == 1, "wait timeout report: the 64-bit RELEASE_MEM was not listed with its 8 bytes and its value");
    char finishedTarget[32];
    std::snprintf(finishedTarget, sizeof(finishedTarget), "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&done)));
    Require(Count(text, std::string("later in this submission: WRITE_DATA at ") + finishedTarget + "+0x4 value 0x1") >= 1, "wait timeout report: the write after the wait was not listed");
    Require(Count(text, "draw pipeline: ") >= 1 && Count(text, "queued label none 0x0") >= 1, "wait timeout report: the draw pipeline line does not say that no queued label holds the awaited value");
}

}

void CheckLaterReference() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&label));
    const std::array<std::uint32_t, 7> wait{0xc0053c00, 0x13, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 3, 0xffffffffu, 0x19};
    std::vector<std::uint32_t> words(wait.begin(), wait.end());
    const auto finished = WriteData(&done, 2);
    words.insert(words.end(), finished.begin(), finished.end());
    StderrCapture capture;
    try {
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "wait timeout report: the second submission failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        Require(done == 1, "wait timeout report: the wait for a later reference completed");
        label = 3;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (done != 2) {
            Require(std::chrono::steady_clock::now() < deadline, "wait timeout report: the second wait did not complete after the label was written");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    const auto text = capture.Finish();
    char target[32];
    std::snprintf(target, sizeof(target), "0x%llx", static_cast<unsigned long long>(address));
    Require(Count(text, std::string("WAIT_REG_MEM at ") + target + " timed out after") == 1 && Count(text, "(function 3 ref 0x3 mask") == 1, "wait timeout report: a later wait on the same label with a new reference was not reported");
}

void CheckStuckWhileOthersProgress() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&stuckLabel));
    const std::array<std::uint32_t, 7> wait{0xc0053c00, 0x13, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 1, 0xffffffffu, 0x19};
    std::vector<std::uint32_t> words(wait.begin(), wait.end());
    const auto finished = WriteData(&stuckDone, 1);
    words.insert(words.end(), finished.begin(), finished.end());
    StderrCapture capture;
    try {
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "wait timeout report: the stuck submission failed");
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
        for (std::uint32_t round = 1; std::chrono::steady_clock::now() < until; ++round) {
            auto busy = WriteData(&busyWork, round);
            Packet compute{busy.data(), static_cast<std::uint32_t>(busy.size()), 0, {}};
            Require(sceAgcDriverSubmitAcb(0x20, &compute) == 0, "wait timeout report: a compute submission failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Require(stuckDone == 0, "wait timeout report: the stuck wait completed before its label was written");
        stuckLabel = 1;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (stuckDone != 1) {
            Require(std::chrono::steady_clock::now() < deadline, "wait timeout report: the stuck wait did not complete after its label was written");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    const auto text = capture.Finish();
    char target[32];
    std::snprintf(target, sizeof(target), "0x%llx", static_cast<unsigned long long>(address));
    char finishedTarget[32];
    std::snprintf(finishedTarget, sizeof(finishedTarget), "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&stuckDone)));
    const std::string stuck = std::string("WAIT_REG_MEM at ") + target + " still waiting after 200ms (function 3 ref 0x1 value 0x0)";
    const auto at = text.find(stuck);
    Require(at != std::string::npos && text.find(stuck, at + stuck.size()) == std::string::npos, "wait timeout report: a wait blocked while another queue progressed was not reported once as still waiting");
    Require(text.find(std::string("later in this submission: WRITE_DATA at ") + finishedTarget + "+0x4 value 0x1", at) != std::string::npos, "wait timeout report: the still-waiting report did not list the write after the wait");
}

int main() {
    try {
        CheckGroupedWriters();
        CheckLaterReference();
        CheckStuckWhileOthersProgress();
        LibcRunShutdown_nid_postfix();
        std::puts("a timed-out wait lists each earlier writer of its label once, with a count");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try {
            LibcRunShutdown_nid_postfix();
        } catch (const std::exception& shutdown) {
            std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
        }
        return 1;
    }
}
