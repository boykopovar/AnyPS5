#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}

static constexpr auto ObservationTime = std::chrono::milliseconds(1500);
static std::atomic<bool> started{false};
static std::atomic<bool> returned{false};
static std::int64_t requestedSeconds = 0;
static bool posix = true;

static void* APS5_VABI Sleeper(void*) {
    const KernelTimespec request{requestedSeconds, 0};
    started.store(true);
    if (posix) nanosleep_nid_postfix(&request, nullptr);
    else sceKernelNanosleep(&request, nullptr);
    returned.store(true);
    return nullptr;
}

static void SleepsPastTheObservation(std::int64_t seconds, bool usePosix) {
    started.store(false);
    returned.store(false);
    requestedSeconds = seconds;
    posix = usePosix;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Sleeper, nullptr, nullptr) == 0);
    while (!started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(ObservationTime);
    Require(!returned.load());
}

int main() {
    for (const bool usePosix : {true, false}) {
        SleepsPastTheObservation(INT64_MAX, usePosix);
        SleepsPastTheObservation(18446744074LL, usePosix);
        SleepsPastTheObservation(std::int64_t{1} << 40, usePosix);
    }
    std::fflush(nullptr);
    std::_Exit(0);
}
