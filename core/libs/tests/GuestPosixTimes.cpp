#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libScePosixForWebKit/GuestTimes.hpp"
#include "prx/libc/include/ChildProcesses.hpp"
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static void Require(bool condition) { if (!condition) std::abort(); }

static std::int64_t CpuMicros() {
#ifdef _WIN32
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return -1;
    const auto units = [](const FILETIME& value) { return (static_cast<std::int64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime; };
    return (units(user) + units(kernel)) / 10;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1;
    return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000 + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
#endif
}

static PosixForWebKitTms ChildTicks() {
#ifdef _WIN32
    const auto reaped = GetReapedChildProcessTimes_nid_no_patch();
    const auto ticks = [](std::uint64_t units) {
        const auto micros = static_cast<std::int64_t>(units / 10);
        return PosixForWebKitClockTicks(micros / 1000000, micros % 1000000);
    };
    return {0, 0, ticks(reaped.user), ticks(reaped.kernel)};
#else
    rusage usage{};
    Require(getrusage(RUSAGE_CHILDREN, &usage) == 0);
    return {0, 0, PosixForWebKitClockTicks(usage.ru_utime.tv_sec, usage.ru_utime.tv_usec), PosixForWebKitClockTicks(usage.ru_stime.tv_sec, usage.ru_stime.tv_usec)};
#endif
}

static int Burn() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    volatile std::uint64_t sink = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        for (std::uint64_t index = 0; index < 100000; ++index) sink = sink + index;
        const auto micros = CpuMicros();
        if (micros < 0) return 4;
        if (micros >= 100000) return 0;
    }
    return 3;
}

static void ReapBurningChild() {
#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    Require(length != 0 && length < executable.size());
    executable.resize(length);
    std::wstring command = L"\"" + executable + L"\" burn";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    Require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &child) != FALSE);
    Require(CloseHandle(child.hThread) != FALSE);
    const auto processId = static_cast<std::int32_t>(child.dwProcessId);
    RegisterChildProcess_nid_no_patch(processId, reinterpret_cast<std::int64_t>(child.hProcess));
    int status = -1;
    Require(WaitChildProcess_nid_no_patch(processId, 0, &status) == processId);
    Require(status == 0);
#else
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) ::_exit(Burn());
    int status = -1;
    Require(::waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "burn") == 0) return Burn();

    static_assert(PosixForWebKitClockTicks(0, 0) == 0);
    static_assert(PosixForWebKitClockTicks(0, 7811) == 0);
    static_assert(PosixForWebKitClockTicks(0, 7812) == 1);
    static_assert(PosixForWebKitClockTicks(0, 999999) == 128);
    static_assert(PosixForWebKitClockTicks(1, 0) == 128);
    static_assert(PosixForWebKitClockTicks(2, 500000) == 320);
    static_assert(PosixForWebKitClockTicks(16777215, 992124) == std::numeric_limits<std::int32_t>::max());
    static_assert(PosixForWebKitClockTicks(16777215, 999999) == std::numeric_limits<std::int32_t>::min());

    errno = E2BIG;
    PosixForWebKitTms first{};
    const auto before = times_nid_postfix(&first);
    Require(first.tms_utime >= 0 && first.tms_stime >= 0);
    Require(first.tms_cutime == 0 && first.tms_cstime == 0);
    Require(errno == E2BIG);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    errno = E2BIG;
    PosixForWebKitTms second{};
    const auto after = times_nid_postfix(&second);
    Require(after > before);
    Require(second.tms_utime >= first.tms_utime && second.tms_stime >= first.tms_stime);
    Require(second.tms_cutime == 0 && second.tms_cstime == 0 && errno == E2BIG);

    ReapBurningChild();
    errno = E2BIG;
    PosixForWebKitTms reaped{};
    Require(times_nid_postfix(&reaped) >= after);
    Require(reaped.tms_cutime >= 0 && reaped.tms_cstime >= 0);
    Require(reaped.tms_cutime + reaped.tms_cstime >= 11);
    const auto reapedExpected = ChildTicks();
    Require(reaped.tms_cutime == reapedExpected.tms_cutime && reaped.tms_cstime == reapedExpected.tms_cstime);
    Require(reaped.tms_utime >= second.tms_utime && reaped.tms_stime >= second.tms_stime);
    Require(errno == E2BIG);

    ReapBurningChild();
    PosixForWebKitTms twice{};
    times_nid_postfix(&twice);
    Require(twice.tms_cutime + twice.tms_cstime >= reaped.tms_cutime + reaped.tms_cstime + 11);
    const auto twiceExpected = ChildTicks();
    Require(twice.tms_cutime == twiceExpected.tms_cutime && twice.tms_cstime == twiceExpected.tms_cstime);

    try {
        times_nid_postfix(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) { Require(errno == E2BIG); }

    try {
        times_nid_postfix(reinterpret_cast<PosixForWebKitTms*>(0x1000));
        std::abort();
    } catch (const std::invalid_argument&) { Require(errno == E2BIG); }
}
