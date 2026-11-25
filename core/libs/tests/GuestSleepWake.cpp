#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#else
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" {

void KernelWakeSleepers_nid_no_patch();
void KernelNoteGpuSubmit_nid_no_patch();
}

static constexpr KernelUseconds WAKEABLE_MICROS = 1000;
static constexpr KernelUseconds LONG_MICROS = 2000;
static constexpr unsigned EARLY_WAKES_PER_RUN = 8;
static constexpr unsigned TRIES = 20;
static constexpr auto EARLY = std::chrono::microseconds(900);

static void Require(bool value) { if (!value) std::abort(); }

static std::chrono::microseconds Usleep(KernelUseconds microseconds) {
    const auto start = std::chrono::steady_clock::now();
    Require(sceKernelUsleep_nid_postfix(microseconds) == 0);
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
}

static void Idle(std::chrono::microseconds length) {
    const auto until = std::chrono::steady_clock::now() + length;
    while (std::chrono::steady_clock::now() < until) std::this_thread::yield();
}

static void Spin(std::chrono::microseconds length) {
    const auto until = std::chrono::steady_clock::now() + length;
    while (std::chrono::steady_clock::now() < until) {}
}

class Waker {
public:
    Waker() : thread([this] {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif
        while (!stop.load()) {
            KernelWakeSleepers_nid_no_patch();
            started.store(true);
            Spin(std::chrono::microseconds(50));
        }
    }) {
        while (!started.load()) std::this_thread::yield();
    }
    ~Waker() {
        stop.store(true);
        thread.join();
    }

private:
    std::atomic<bool> stop{false};
    std::atomic<bool> started{false};
    std::thread thread;
};

static void DisabledSwitchKeepsEverySleepFull() {
    Waker waker;
    for (unsigned i = 0; i < 12; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
    }
}

#ifdef _WIN32
static void CoarseWaitsKeepEverySleepFull() {
    DisabledSwitchKeepsEverySleepFull();
}

static void VerifyNoSleepWakePresenceSwitch() {
    std::wstring modulePath(260, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        Require(len != 0);
        if (len < modulePath.size()) {
            modulePath.resize(len);
            break;
        }
        modulePath.resize(modulePath.size() * 2);
    }
    const std::wstring cmdLine = L"\"" + modulePath + L"\" --no-sleep-wake";
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessW(
        modulePath.c_str(),
        cmdBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);
    Require(created != FALSE);
    Require(WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0);
    DWORD exitCode = 1;
    Require(GetExitCodeProcess(pi.hProcess, &exitCode) != FALSE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Require(exitCode == 0);
}
#else
static void VerifyNoSleepWakePresenceSwitch() {
    const pid_t pid = ::fork();
    Require(pid >= 0);
    if (pid == 0) {
        Require(::setenv("APS5_NO_SLEEP_WAKE", "1", 1) == 0);
        DisabledSwitchKeepsEverySleepFull();
        ::_exit(0);
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        Require(errno == EINTR);
    }
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif

static void SleepWithoutWakeIsFull() {
    for (unsigned i = 0; i < 4; ++i) Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
}

static void WakeEndsShortSleep() {
    Waker waker;
    bool early = false;
    for (unsigned i = 0; i < TRIES && !early; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        early = Usleep(WAKEABLE_MICROS) < EARLY;
    }
    Require(early);
}

static void WakeLeavesLongSleep() {
    Waker waker;
    for (unsigned i = 0; i < 4; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        Require(Usleep(LONG_MICROS) >= std::chrono::microseconds(LONG_MICROS));
    }
}

static void RunOfSleepsEndsEarlyAtMostEightTimes() {
    Waker waker;
    KernelNoteGpuSubmit_nid_no_patch();
    unsigned early = 0;
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 8; ++i) {
        if (Usleep(WAKEABLE_MICROS) < std::chrono::microseconds(WAKEABLE_MICROS)) ++early;
    }
    Require(early <= EARLY_WAKES_PER_RUN);
    for (unsigned i = 0; i < 4; ++i) Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
}

static void SubmitStartsNewRun() {
    Waker waker;
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 4; ++i) Usleep(WAKEABLE_MICROS);
    bool early = false;
    for (unsigned i = 0; i < TRIES && !early; ++i) {
        KernelNoteGpuSubmit_nid_no_patch();
        early = Usleep(WAKEABLE_MICROS) < EARLY;
    }
    Require(early);
}

static void GapStartsNewRun() {
    Waker waker;
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 4; ++i) Usleep(WAKEABLE_MICROS);
    bool early = false;
    for (unsigned i = 0; i < TRIES && !early; ++i) {
        Idle(std::chrono::microseconds(WAKEABLE_MICROS + 200));
        early = Usleep(WAKEABLE_MICROS) < EARLY;
    }
    Require(early);
}

static void ExhaustRun() {
    KernelNoteGpuSubmit_nid_no_patch();
    for (unsigned i = 0; i < EARLY_WAKES_PER_RUN + 8; ++i) Usleep(WAKEABLE_MICROS);
}

static void LongSleepKeepsRunLimit() {
    Waker waker;
    ExhaustRun();
    for (unsigned i = 0; i < 4; ++i) {
        Require(Usleep(LONG_MICROS) >= std::chrono::microseconds(LONG_MICROS));
        Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
    }
}

static void ZeroSleepCountsAsSleep() {
    Waker waker;
    ExhaustRun();
    for (unsigned i = 0; i < 4; ++i) {
        Idle(std::chrono::microseconds(WAKEABLE_MICROS + 200));
        Require(sceKernelUsleep_nid_postfix(0) == 0);
        Require(Usleep(WAKEABLE_MICROS) >= std::chrono::microseconds(WAKEABLE_MICROS));
    }
}

int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc > 1 && std::strcmp(argv[1], "--no-sleep-wake") == 0) {
        Require(_putenv_s("APS5_NO_SLEEP_WAKE", "1") == 0);
        DisabledSwitchKeepsEverySleepFull();
        return 0;
    }
    if (std::getenv("APS5_COARSE_TIMED_WAITS") != nullptr) {
        CoarseWaitsKeepEverySleepFull();
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif
    if (std::getenv("APS5_NO_SLEEP_WAKE") == nullptr) {
        VerifyNoSleepWakePresenceSwitch();
    }
    SleepWithoutWakeIsFull();
    WakeEndsShortSleep();
    WakeLeavesLongSleep();
    RunOfSleepsEndsEarlyAtMostEightTimes();
    SubmitStartsNewRun();
    GapStartsNewRun();
    LongSleepKeepsRunLimit();
    ZeroSleepCountsAsSleep();
    return 0;
}
