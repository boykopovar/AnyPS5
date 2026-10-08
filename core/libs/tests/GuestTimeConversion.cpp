#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <thread>

extern "C" {
std::tm* APS5_VABI libc_gmtime_nid_postfix(const std::int64_t*);
std::tm* APS5_VABI gmtime_nid_postfix(const std::int64_t*);
std::tm* APS5_VABI libc_localtime_nid_postfix(const std::int64_t*);
std::tm* APS5_VABI localtime_nid_postfix(const std::int64_t*);
std::tm* APS5_VABI gmtime_s_nid_postfix(const std::int64_t*, std::tm*);
std::tm* APS5_VABI localtime_s_nid_postfix(const std::int64_t*, std::tm*);
}

using Converter = std::tm* (APS5_VABI *)(const std::int64_t*);
using BufferedConverter = std::tm* (APS5_VABI *)(const std::int64_t*, std::tm*);

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static bool Equal(const std::tm& left, const std::tm& right) {
    return left.tm_sec == right.tm_sec && left.tm_min == right.tm_min && left.tm_hour == right.tm_hour
        && left.tm_mday == right.tm_mday && left.tm_mon == right.tm_mon && left.tm_year == right.tm_year
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst;
}

static void CheckConcurrent(Converter convert, BufferedConverter buffered, const char* name) {
    constexpr std::array<std::int64_t, 8> timers{0, 86400, 946684800, 1078012800, 1609459200, 1709164800, 1893456000, 2145916799};
    std::array<std::tm, timers.size()> expected{};
    for (std::size_t index = 0; index < timers.size(); ++index)
        Require(buffered(&timers[index], &expected[index]) == &expected[index], "Buffered conversion failed");
    std::barrier start(static_cast<std::ptrdiff_t>(timers.size()));
    std::atomic<bool> matches{true};
    std::array<std::thread, timers.size()> workers;
    for (std::size_t index = 0; index < workers.size(); ++index) {
        workers[index] = std::thread([&, index] {
            start.arrive_and_wait();
            for (int iteration = 0; iteration < 50000; ++iteration) {
                const auto* result = convert(&timers[index]);
                if (result == nullptr || !Equal(*result, expected[index])) {
                    matches = false;
                    break;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(matches, name);
    const std::int64_t invalid = std::numeric_limits<std::int64_t>::max();
    Require(convert(&invalid) == nullptr, "Unrepresentable time was accepted");
}

int main() {
#ifdef _WIN32
    _putenv_s("TZ", "UTC-2");
    _tzset();
#else
    setenv("TZ", "UTC-2", 1);
    tzset();
#endif
    const std::int64_t epoch = 0;
    std::tm utc{}, local{};
    Require(gmtime_s_nid_postfix(&epoch, &utc) == &utc && utc.tm_year == 70 && utc.tm_mon == 0
        && utc.tm_mday == 1 && utc.tm_hour == 0, "UTC epoch conversion failed");
    Require(localtime_s_nid_postfix(&epoch, &local) == &local && local.tm_year == 70 && local.tm_mon == 0
        && local.tm_mday == 1 && local.tm_hour == 2, "Local epoch conversion failed");
    CheckConcurrent(libc_gmtime_nid_postfix, gmtime_s_nid_postfix, "Concurrent libc_gmtime returned another thread's date");
    CheckConcurrent(gmtime_nid_postfix, gmtime_s_nid_postfix, "Concurrent gmtime returned another thread's date");
    CheckConcurrent(libc_localtime_nid_postfix, localtime_s_nid_postfix, "Concurrent libc_localtime returned another thread's date");
    CheckConcurrent(localtime_nid_postfix, localtime_s_nid_postfix, "Concurrent localtime returned another thread's date");
}
