#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

using Wide = __int128;

constexpr Wide Billion = 1000000000;

struct Row {
    const char* label;
    std::int64_t offset;
    std::int64_t nsec;
    std::int64_t normalizedOffset;
    std::int64_t normalizedNsec;
};

const Row FutureRows[] = {
    {"whole seconds", 100, 0, 100, 0},
    {"one nanosecond", 100, 1, 100, 1},
    {"minus one nanosecond", 100, -1, 99, 999999999},
    {"minus one second", 100, -1000000000, 99, 0},
    {"just below minus one second", 100, -1000000001, 98, 999999999},
    {"minus one and a half seconds", 100, -1500000000, 98, 500000000},
    {"minus five seconds", 100, -5000000000LL, 95, 0},
    {"one second", 100, 1000000000, 101, 0},
    {"just below two seconds", 100, 1999999999, 101, 999999999},
    {"two seconds", 100, 2000000000, 102, 0},
    {"two and a half seconds", 100, 2500000000LL, 102, 500000000},
    {"seven seconds and one nanosecond", 100, 7000000001LL, 107, 1},
    {"a few seconds ahead", 5, 999999999, 5, 999999999},
};

const Row PastRows[] = {
    {"one second ago", -1, 0, 0, 0},
    {"normalized into the past", -100, 2500000000LL, 0, 0},
    {"minus one second now", 0, -1000000000, 0, 0},
    {"minus three seconds next second", 1, -3000000000LL, 0, 0},
};

unsigned failures = 0;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: sce v2 timespec64: %s\n", what);
    ++failures;
}

Wide Nanos(const Xtime& time) {
    return static_cast<Wide>(time.sec) * Billion + time.nsec;
}

bool Normalized(const Xtime& time) {
    return time.nsec >= 0 && time.nsec < Billion;
}

void CheckGetSys() {
    Xtime first {};
    Xtime second {};
    _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(&first);
    const auto ticks = _Xtime_get_ticks_nid_postfix();
    _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(&second);
    Require(Normalized(first) && Normalized(second), "get_sys nanoseconds in range");
    Require(first.nsec % 1000 == 0 && second.nsec % 1000 == 0, "get_sys has microsecond resolution");
    Require(Nanos(first) <= Nanos(second), "get_sys does not go back");
    Require(Nanos(first) <= static_cast<Wide>(ticks) * 1000 && static_cast<Wide>(ticks) * 1000 <= Nanos(second), "get_sys reads the realtime ticks");
    bool thrown = false;
    try {
        _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown, "get_sys null output throws");
}

void CheckRemaining(const char* label, const Xtime& target, Wide normalized) {
    Xtime before {};
    Xtime after {};
    _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(&before);
    const Xtime copy = target;
    const Xtime left = _ZNSt8__sce_v222_Timespec64_diff_to_tsEPK8timespec_nid_postfix(&copy);
    _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(&after);
    const bool inside = normalized - Nanos(after) <= Nanos(left) && Nanos(left) <= normalized - Nanos(before);
    const bool unchanged = copy.sec == target.sec && copy.nsec == target.nsec;
    if (inside && Normalized(left) && unchanged) return;
    std::fprintf(stderr, "FAIL: sce v2 timespec64: %s: %lld.%09lld\n", label, static_cast<long long>(left.sec), static_cast<long long>(left.nsec));
    ++failures;
}

void CheckDiffToTs() {
    Xtime now {};
    _ZNSt8__sce_v219_Timespec64_get_sysEP8timespec_nid_postfix(&now);
    for (const auto& row : FutureRows) {
        const Xtime target {now.sec + row.offset, row.nsec};
        CheckRemaining(row.label, target, static_cast<Wide>(now.sec + row.normalizedOffset) * Billion + row.normalizedNsec);
    }
    for (const auto& row : PastRows) {
        const Xtime target {now.sec + row.offset, row.nsec};
        const Xtime left = _ZNSt8__sce_v222_Timespec64_diff_to_tsEPK8timespec_nid_postfix(&target);
        Require(left.sec == 0 && left.nsec == 0, row.label);
    }
    const Xtime lowestNanos {0, INT64_MIN};
    const Xtime zero {0, 0};
    for (const Xtime* target : {&lowestNanos, &zero}) {
        const Xtime left = _ZNSt8__sce_v222_Timespec64_diff_to_tsEPK8timespec_nid_postfix(target);
        Require(left.sec == 0 && left.nsec == 0, "an absolute past target gives zero");
    }
    CheckRemaining("lowest seconds wrap around", Xtime {INT64_MIN, 0}, (static_cast<Wide>(1) << 63) * Billion);
    bool thrown = false;
    try {
        _ZNSt8__sce_v222_Timespec64_diff_to_tsEPK8timespec_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown, "diff_to_ts null target throws");
}

}

int main() {
    CheckGetSys();
    CheckDiffToTs();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
