#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static bool Equal(const GuestTm& left, const GuestTm& right) {
    return left.tm_sec == right.tm_sec && left.tm_min == right.tm_min && left.tm_hour == right.tm_hour
        && left.tm_mday == right.tm_mday && left.tm_mon == right.tm_mon && left.tm_year == right.tm_year
        && left.tm_wday == right.tm_wday && left.tm_yday == right.tm_yday && left.tm_isdst == right.tm_isdst;
}

struct TimegmCase {
    int sec, min, hour, mday, mon, year;
    std::int64_t expected;
    int normalizedMday, normalizedMon, normalizedYear, wday, yday;
};

static void CheckTimegm(const TimegmCase& test) {
    GuestTm time{};
    time.tm_sec = test.sec;
    time.tm_min = test.min;
    time.tm_hour = test.hour;
    time.tm_mday = test.mday;
    time.tm_mon = test.mon;
    time.tm_year = test.year;
    time.tm_wday = 9;
    time.tm_yday = 999;
    time.tm_isdst = 5;
    time.tm_gmtoff = 77;
    if (timegm_nid_postfix(&time) != test.expected) {
        std::fprintf(stderr, "timegm(%d-%d-%d) returned the wrong time\n", test.year, test.mon, test.mday);
        std::abort();
    }
    Require(time.tm_mday == test.normalizedMday && time.tm_mon == test.normalizedMon && time.tm_year == test.normalizedYear
        && time.tm_wday == test.wday && time.tm_yday == test.yday && time.tm_isdst == 0 && time.tm_gmtoff == 0,
        "timegm did not normalize the calendar fields");
}

static void CheckReentrantConversions() {
    constexpr std::int64_t timers[] = {0, -1, 86399, 951782400, 4107542400, 253402300799, -62135596800};
    for (const std::int64_t timer : timers) {
        GuestTm expected{};
        GuestTm actual{};
        Require(gmtime_s_nid_postfix(&timer, &expected) == &expected, "gmtime_s rejected a representable time");
        Require(expected.tm_isdst == -1, "gmtime_s did not leave daylight saving undetermined");
        expected.tm_isdst = 0;
        Require(gmtime_r_nid_postfix(&timer, &actual) == &actual && Equal(actual, expected), "gmtime_r differs from gmtime_s");
        Require(localtime_s_nid_postfix(&timer, &expected) == &expected, "localtime_s rejected a representable time");
        Require(localtime_r_nid_postfix(&timer, &actual) == &actual && Equal(actual, expected) && actual.tm_gmtoff == 7200,
            "localtime_r differs from localtime_s");
    }
    const std::int64_t epoch = 0;
    Require(gmtime_r_nid_postfix(&epoch, nullptr) == nullptr, "gmtime_r accepted a null result");
    Require(localtime_r_nid_postfix(&epoch, nullptr) == nullptr, "localtime_r accepted a null result");
    const std::int64_t underflow = std::numeric_limits<std::int64_t>::min();
    GuestTm scratch{};
    Require(gmtime_r_nid_postfix(&underflow, &scratch) == nullptr, "gmtime_r accepted an unrepresentable time");
}

static void CheckCtimeR() {
    struct CtimeCase {
        std::int64_t timer;
        const char* expected;
    };
    constexpr CtimeCase cases[] = {
        {0, "Thu Jan  1 02:00:00 1970\n"},
        {-7201, "Wed Dec 31 23:59:59 1969\n"},
        {951782400, "Tue Feb 29 02:00:00 2000\n"},
        {1792379647, "Mon Oct 19 05:14:07 2026\n"},
        {253402293599, "Fri Dec 31 23:59:59 9999\n"},
        {-62135596800, "Mon Jan  1 02:00:00 0001\n"},
        {-30641767200, "Tue Jan  1 00:00:00 0999\n"},
        {253402293600, "Sat Jan  1 00:00:00 0000\n"},
    };
    for (const CtimeCase& test : cases) {
        char buffer[32];
        std::memset(buffer, 'x', sizeof(buffer));
        Require(ctime_r_nid_postfix(&test.timer, buffer) == buffer, "ctime_r rejected a representable time");
        if (std::strcmp(buffer, test.expected) != 0) {
            std::fprintf(stderr, "ctime_r(%lld) returned '%s'\n", static_cast<long long>(test.timer), buffer);
            std::abort();
        }
        Require(std::strlen(buffer) == 25 && buffer[26] == 'x', "ctime_r wrote past 26 bytes");
        GuestTm local{};
        Require(localtime_r_nid_postfix(&test.timer, &local) != nullptr, "localtime_r rejected a representable time");
        Require(std::strcmp(buffer, asctime_nid_postfix(&local)) == 0, "ctime_r differs from asctime of localtime_r");
    }
    char buffer[26] = "unchanged";
    Require(ctime_r_nid_postfix(nullptr, buffer) == nullptr && std::strcmp(buffer, "unchanged") == 0, "ctime_r accepted a null time");
    const std::int64_t underflow = std::numeric_limits<std::int64_t>::min();
    Require(ctime_r_nid_postfix(&underflow, buffer) == nullptr && std::strcmp(buffer, "unchanged") == 0, "ctime_r accepted an unrepresentable time");
    const std::int64_t epoch = 0;
    bool threw = false;
    try {
        ctime_r_nid_postfix(&epoch, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "ctime_r accepted a null buffer");
}

static void CheckTimegmCases() {
    constexpr TimegmCase cases[] = {
        {0, 0, 0, 1, 0, 70, 0, 1, 0, 70, 4, 0},
        {59, 59, 23, 31, 11, 69, -1, 31, 11, 69, 3, 364},
        {7, 14, 3, 19, 9, 126, 1792379647, 19, 9, 126, 1, 291},
        {0, 0, 0, 0, 0, 70, -86400, 31, 11, 69, 3, 364},
        {0, 0, 0, 1, 12, 69, 0, 1, 0, 70, 4, 0},
        {0, 0, 0, 1, -1, 71, 28857600, 1, 11, 70, 2, 334},
        {0, 0, 0, 1, -13, 71, -2678400, 1, 11, 69, 1, 334},
        {0, 0, 0, 32, 1, 100, 952041600, 3, 2, 100, 5, 62},
        {31536000, 0, 0, 1, 0, 70, 31536000, 1, 0, 71, 5, 0},
        {0, 0, 0, 1, 2, 200, 4107542400, 1, 2, 200, 1, 59},
        {59, 59, 23, 31, 11, 8099, 253402300799, 31, 11, 8099, 5, 364},
        {0, 0, 0, 1, 0, -1899, -62135596800, 1, 0, -1899, 1, 0},
    };
    for (const auto& test : cases) CheckTimegm(test);

    GuestTm beyond{};
    beyond.tm_year = std::numeric_limits<int>::max();
    beyond.tm_mon = 12;
    beyond.tm_mday = 1;
    const GuestTm before = beyond;
    Require(timegm_nid_postfix(&beyond) == -1 && Equal(beyond, before), "timegm accepted a year past the int range");

    std::mt19937_64 random(7);
    for (int iteration = 0; iteration < 100000; ++iteration) {
        const std::int64_t timer = static_cast<std::int64_t>(random() % 600000000000ull) - 300000000000;
        GuestTm fields{};
        Require(gmtime_r_nid_postfix(&timer, &fields) == &fields, "gmtime_r rejected a representable time");
        GuestTm copy = fields;
        Require(timegm_nid_postfix(&copy) == timer && Equal(copy, fields), "timegm did not invert gmtime_r");
    }
}

static void CheckXtime() {
    Xtime time{-5, -5};
    Require(xtime_get_nid_postfix(&time, 0) == 0 && time.sec == -5 && time.nsec == -5, "xtime_get accepted base 0");
    Require(xtime_get_nid_postfix(&time, 2) == 0 && time.sec == -5 && time.nsec == -5, "xtime_get accepted base 2");
    Require(xtime_get_nid_postfix(nullptr, 1) == 0, "xtime_get accepted a null xtime");
    errno = 71;
    Require(xtime_get_nid_postfix(reinterpret_cast<Xtime*>(0x10), 1) == 0 && errno == 71, "xtime_get accepted an unwritable xtime or changed errno");
    const std::int64_t before = static_cast<std::int64_t>(std::time(nullptr));
    Require(xtime_get_nid_postfix(&time, 1) == 1, "xtime_get rejected TIME_UTC");
    Require(time.sec >= before - 1 && time.sec <= before + 2 && time.nsec >= 0 && time.nsec < 1000000000 && time.nsec % 1000 == 0,
        "xtime_get did not return the wall clock in microseconds");

    const Xtime unnormalized{7, -3};
    const Xtime copied = _Xtime_to_ts_nid_postfix(&unnormalized);
    Require(copied.sec == 7 && copied.nsec == -3, "_Xtime_to_ts changed the time");
    for (const Xtime* unreadable : {static_cast<const Xtime*>(nullptr), reinterpret_cast<const Xtime*>(0x10)}) {
        bool rejected = false;
        try {
            _Xtime_to_ts_nid_postfix(unreadable);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "_Xtime_to_ts read an unreadable time");
    }

    Xtime now{};
    Require(xtime_get_nid_postfix(&now, 1) == 1, "xtime_get rejected TIME_UTC");
    const Xtime past{now.sec - 1, now.nsec};
    const Xtime remaining = _Xtime_diff_to_ts_nid_postfix(&past);
    Require(remaining.sec == 0 && remaining.nsec == 0, "_Xtime_diff_to_ts did not clamp a past time to zero");
    const Xtime epoch{0, 0};
    const Xtime none = _Xtime_diff_to_ts_nid_postfix(&epoch);
    Require(none.sec == 0 && none.nsec == 0, "_Xtime_diff_to_ts did not clamp the epoch to zero");
    const Xtime later{now.sec + 100, now.nsec};
    const Xtime wait = _Xtime_diff_to_ts_nid_postfix(&later);
    Require(wait.sec >= 98 && wait.sec <= 100 && wait.nsec >= 0 && wait.nsec < 1000000000, "_Xtime_diff_to_ts returned the wrong wait");
    const Xtime carried{now.sec, now.nsec + 5000000000};
    const Xtime carriedWait = _Xtime_diff_to_ts_nid_postfix(&carried);
    Require(carriedWait.sec >= 3 && carriedWait.sec <= 5 && carriedWait.nsec >= 0 && carriedWait.nsec < 1000000000,
        "_Xtime_diff_to_ts did not carry whole seconds out of nsec");
    const Xtime borrowed{now.sec + 10, -1};
    const Xtime borrowedWait = _Xtime_diff_to_ts_nid_postfix(&borrowed);
    Require(borrowedWait.sec >= 8 && borrowedWait.sec <= 9 && borrowedWait.nsec >= 0 && borrowedWait.nsec < 1000000000,
        "_Xtime_diff_to_ts did not borrow a second for a negative nsec");
}

static void CheckGuardedGuestPointers() {
    struct GuardedPages {
        unsigned char* base = nullptr;
        std::size_t pageSize = 4096;

        GuardedPages() {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : 4096;
            base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(base != nullptr, "allocate guarded pages");
#else
            const long size = sysconf(_SC_PAGESIZE);
            pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096;
            void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED, "allocate guarded pages");
            base = static_cast<unsigned char*>(mapped);
#endif
        }

        void Protect() {
#ifdef _WIN32
            DWORD oldProtect = 0;
            Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect read-only page");
            Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
#else
            Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0, "protect read-only page");
            Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0, "protect guard page");
#endif
        }

        ~GuardedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, pageSize * 3);
#endif
        }

        unsigned char* ReadOnly() const { return base + pageSize; }
        unsigned char* Guard() const { return base + pageSize * 2; }
    };

    GuardedPages pages;
    auto* roTm = reinterpret_cast<GuestTm*>(pages.ReadOnly());
    *roTm = GuestTm{};
    roTm->tm_mday = 1;
    roTm->tm_year = 70;
    pages.Protect();

    const std::int64_t epoch = 0;
    GuestTm outTm{};
    char outBuf[32]{};

    errno = 0;
    Require(timegm_nid_postfix(nullptr) == -1 && errno == EFAULT, "timegm rejected null with EFAULT");
    errno = 0;
    Require(timegm_nid_postfix(reinterpret_cast<GuestTm*>(pages.Guard())) == -1 && errno == EFAULT, "timegm rejected guard page with EFAULT");
    errno = 0;
    Require(timegm_nid_postfix(roTm) == -1 && errno == EFAULT, "timegm rejected read-only page with EFAULT");

    errno = 0;
    Require(gmtime_r_nid_postfix(reinterpret_cast<const std::int64_t*>(pages.Guard()), &outTm) == nullptr && errno == EFAULT,
        "gmtime_r rejected guard timer with EFAULT");
    errno = 0;
    Require(gmtime_r_nid_postfix(&epoch, reinterpret_cast<GuestTm*>(pages.Guard())) == nullptr && errno == EFAULT,
        "gmtime_r rejected guard result with EFAULT");
    errno = 0;
    Require(gmtime_r_nid_postfix(&epoch, roTm) == nullptr && errno == EFAULT,
        "gmtime_r rejected read-only result with EFAULT");

    errno = 0;
    Require(localtime_r_nid_postfix(reinterpret_cast<const std::int64_t*>(pages.Guard()), &outTm) == nullptr && errno == EFAULT,
        "localtime_r rejected guard timer with EFAULT");
    errno = 0;
    Require(localtime_r_nid_postfix(&epoch, reinterpret_cast<GuestTm*>(pages.Guard())) == nullptr && errno == EFAULT,
        "localtime_r rejected guard result with EFAULT");
    errno = 0;
    Require(localtime_r_nid_postfix(&epoch, roTm) == nullptr && errno == EFAULT,
        "localtime_r rejected read-only result with EFAULT");

    errno = 0;
    Require(ctime_r_nid_postfix(reinterpret_cast<const std::int64_t*>(pages.Guard()), outBuf) == nullptr && errno == EFAULT,
        "ctime_r rejected guard timer with EFAULT");
    errno = 0;
    Require(ctime_r_nid_postfix(&epoch, reinterpret_cast<char*>(pages.Guard())) == nullptr && errno == EFAULT,
        "ctime_r rejected guard buffer with EFAULT");
    errno = 0;
    Require(ctime_r_nid_postfix(&epoch, reinterpret_cast<char*>(pages.ReadOnly())) == nullptr && errno == EFAULT,
        "ctime_r rejected read-only buffer with EFAULT");

    errno = 71;
    Require(xtime_get_nid_postfix(reinterpret_cast<Xtime*>(pages.Guard()), 1) == 0 && errno == 71,
        "xtime_get rejected guard page and preserved errno");
    Require(xtime_get_nid_postfix(reinterpret_cast<Xtime*>(pages.ReadOnly()), 1) == 0 && errno == 71,
        "xtime_get rejected read-only page and preserved errno");

    auto throwsInvalid = [](auto fn) {
        try {
            fn();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    const Xtime validXtime{1, 0};
    errno = 71;
    Require(throwsInvalid([&] { _Xtime_to_ts_nid_postfix(reinterpret_cast<const Xtime*>(pages.Guard())); }) && errno == 71,
        "_Xtime_to_ts rejected guard page and preserved errno");
    Require(throwsInvalid([&] { _Xtime_diff_to_ts_nid_postfix(reinterpret_cast<const Xtime*>(pages.Guard())); }) && errno == 71,
        "_Xtime_diff_to_ts rejected guard page and preserved errno");
    Require(throwsInvalid([&] { _ZNSt8__sce_v222_Xtime_diff_to_millis2EPK8timespecS2__nid_postfix(
        reinterpret_cast<const Xtime*>(pages.Guard()), &validXtime); }) && errno == 71,
        "_Xtime_diff_to_millis2 rejected guard target and preserved errno");
    Require(throwsInvalid([&] { _ZNSt8__sce_v222_Xtime_diff_to_millis2EPK8timespecS2__nid_postfix(
        &validXtime, reinterpret_cast<const Xtime*>(pages.Guard())); }) && errno == 71,
        "_Xtime_diff_to_millis2 rejected guard now and preserved errno");
}

int main() {
    Require(setenv_nid_postfix("TZ", "UTC-2", 1) == 0, "Could not set the time zone");
    tzset_nid_postfix();

    CheckGuardedGuestPointers();
    CheckReentrantConversions();
    CheckTimegmCases();
    CheckCtimeR();
    CheckXtime();
    return 0;
}
