#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

constexpr std::uint64_t NoDeadline = 0xFFFFFFFFFFFFFFFFULL;
constexpr std::uint64_t NoTimeout = 0xFFFFFFFFULL;
constexpr std::uint64_t TenDays = 864000000ULL;
constexpr std::uint64_t Slack = 60000ULL;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal atomic wait deadline: %s\n", message);
    std::abort();
}

std::uint64_t Clock() {
#ifdef _WIN32
    return ::GetTickCount64();
#else
    struct timespec now{};
    Require(::clock_gettime(CLOCK_MONOTONIC, &now) == 0, "the test's CLOCK_MONOTONIC sample must succeed");
    return static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + static_cast<std::uint64_t>(now.tv_nsec) / 1000000ULL;
#endif
}

std::uint64_t Deadline(std::uint64_t timeout) {
    return __std_atomic_wait_get_deadline_nid_postfix(timeout);
}

std::uint64_t Remaining(std::uint64_t deadline) {
    return __std_atomic_wait_get_remaining_timeout_nid_postfix(deadline);
}

void RequireRemainingBetween(std::uint64_t deadline, std::uint64_t upper, const char* message) {
    const std::uint64_t left = Remaining(deadline);
    const std::uint64_t after = Clock();
    const std::uint64_t lower = after < deadline ? deadline - after : 0;
    Require(left <= upper && left >= lower, message);
}

void CheckNoDeadline() {
    Require(Deadline(NoDeadline) == NoDeadline, "an endless timeout must give no deadline");
    Require(Remaining(NoDeadline) == NoTimeout, "no deadline must give the no-timeout value");
    Require(Deadline(NoTimeout) != NoDeadline, "the 32-bit no-timeout value is an ordinary timeout here");
}

void CheckDeadlineSamplesTheClock() {
    const std::uint64_t timeouts[] = {0, 1, 25, 5000, NoDeadline - 1};
    for (const std::uint64_t timeout : timeouts) {
        const std::uint64_t before = Clock();
        const std::uint64_t deadline = Deadline(timeout);
        const std::uint64_t after = Clock();
        Require(after >= before, "the monotonic clock must not go backwards");
        Require(deadline - timeout - before <= after - before, "a deadline must be a monotonic millisecond sample plus the timeout, as unsigned arithmetic");
    }
    Require(Remaining(Deadline(NoDeadline - 1)) == 0, "a wrapped deadline has already passed");
}

void CheckRemaining() {
    Require(Remaining(0) == 0, "a deadline at the epoch has passed");
    Require(Remaining(Clock()) == 0, "a deadline at the current monotonic time has passed");
    Require(Remaining(Deadline(0)) == 0, "a zero timeout has no time left");
    RequireRemainingBetween(Clock() + Slack, Slack, "the time left must lie between two monotonic samples");
    Require(Remaining(Deadline(Slack)) > 0, "a future deadline must have time left");
}

void CheckCap() {
    Require(Remaining(Clock() + TenDays + Slack) == TenDays, "the time left is capped at ten days");
    Require(Remaining(NoDeadline - 1) == TenDays, "a far deadline is capped at ten days");
    RequireRemainingBetween(Clock() + TenDays - Slack, TenDays - Slack, "the time left below the cap must be exact");
}

void CheckExpiry() {
    const std::uint64_t deadline = Deadline(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Remaining(deadline) == 0, "an expired deadline has no time left");
}

template <typename TCall>
bool ThrowsRuntimeError(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void CheckPosixSpawnSignalAttributes() {
    std::error_code error;
    const bool createdDirectory = std::filesystem::create_directories("app0", error);
    Require(!error, "app0 directory creation must succeed");
    {
        std::ofstream image("app0/eboot.bin");
        image << "image";
        Require(image.good(), "app0/eboot.bin must be written");
    }

    PosixSpawnAttributes* attributes = nullptr;
    Require(posix_spawnattr_init_nid_postfix(&attributes) == 0, "posix_spawnattr_init must succeed");
    const GuestSignalSet signalDefaults{{(1u << 1) | (1u << 14), 0u, 0u, 0u}};
    Require(posix_spawnattr_setsigdefault_nid_postfix(&attributes, &signalDefaults) == 0, "posix_spawnattr_setsigdefault must succeed");
    const GuestSignalSet emptyMask{};
    Require(posix_spawnattr_setsigmask_nid_postfix(&attributes, &emptyMask) == 0, "posix_spawnattr_setsigmask must succeed");

    char program[] = "/app0/eboot.bin";
    char role[] = "spawn-signal-child";
    char* childArguments[] = {program, role, nullptr};

    for (auto spawn : std::array{&posix_spawn_nid_postfix, &posix_spawnp_nid_postfix}) {
        for (const std::int16_t flags : {std::int16_t{0x10}, std::int16_t{0x20}, std::int16_t{0x30}}) {
            Require(posix_spawnattr_setflags_nid_postfix(&attributes, flags) == 0, "posix_spawnattr_setflags must succeed");
            std::int32_t process = -1;
            Require(spawn(&process, program, nullptr, &attributes, childArguments, nullptr) == 0, "posix_spawn with signal flags must succeed");
            Require(process > 0, "spawned child process id must be positive");
            int status = -1;
            Require(waitpid_nid_postfix(process, &status, 0) == process, "waitpid must reap spawned child");
            Require((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 43, "spawned child must exit with code 43");
        }
    }

#ifdef _WIN32
    Require(posix_spawnattr_setflags_nid_postfix(&attributes, 0x01) == 0, "setflags 0x01 must succeed");
    std::int32_t rejectedProcess = -1;
    Require(ThrowsRuntimeError([&] {
        posix_spawn_nid_postfix(&rejectedProcess, program, nullptr, &attributes, childArguments, nullptr);
    }), "unsupported flag 0x01 must throw on Windows");
    const GuestSignalSet nonEmptyMask{{1u, 0u, 0u, 0u}};
    Require(posix_spawnattr_setsigmask_nid_postfix(&attributes, &nonEmptyMask) == 0, "setsigmask with non-empty mask must succeed");
    Require(posix_spawnattr_setflags_nid_postfix(&attributes, 0x20) == 0, "setflags 0x20 must succeed");
    Require(ThrowsRuntimeError([&] {
        posix_spawn_nid_postfix(&rejectedProcess, program, nullptr, &attributes, childArguments, nullptr);
    }), "non-empty POSIX_SPAWN_SETSIGMASK must throw on Windows");
#else
    Require(posix_spawnattr_setflags_nid_postfix(&attributes, 0x04) == 0, "setflags 0x04 must succeed");
    std::int32_t rejectedProcess = -1;
    Require(ThrowsRuntimeError([&] {
        posix_spawn_nid_postfix(&rejectedProcess, program, nullptr, &attributes, childArguments, nullptr);
    }), "unsupported flag 0x04 must throw on POSIX");
#endif

    Require(posix_spawnattr_destroy_nid_postfix(&attributes) == 0, "posix_spawnattr_destroy must succeed");
    std::filesystem::remove("app0/eboot.bin", error);
    if (createdDirectory) std::filesystem::remove("app0", error);
}

} // namespace

int main(int count, char** arguments) {
#ifdef _WIN32
    if (count >= 3 && std::strcmp(arguments[2], "spawn-signal-child") == 0) return 43;
#else
    if (count >= 2 && std::strcmp(arguments[1], "spawn-signal-child") == 0) return 43;
#endif
    CheckNoDeadline();
    CheckDeadlineSamplesTheClock();
    CheckRemaining();
    CheckCap();
    CheckExpiry();
    CheckPosixSpawnSignalAttributes();
    return 0;
}
