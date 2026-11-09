#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct Case {
    const char* label;
    std::int64_t target[2];
    std::int64_t now[2];
    std::int64_t millis;
};

const Case Cases[] = {
    {"one second ahead", {101LL, 0LL}, {100LL, 0LL}, 1000LL},
    {"half a millisecond rounds up", {100LL, 500000LL}, {100LL, 0LL}, 1LL},
    {"one nanosecond rounds up", {100LL, 1LL}, {100LL, 0LL}, 1LL},
    {"exact milliseconds", {100LL, 250000000LL}, {100LL, 0LL}, 250LL},
    {"borrow from seconds", {101LL, 100LL}, {100LL, 999999999LL}, 1LL},
    {"equal times", {100LL, 5LL}, {100LL, 5LL}, 0LL},
    {"target in the past", {99LL, 0LL}, {100LL, 0LL}, 0LL},
    {"nanoseconds behind in the same second", {100LL, 4LL}, {100LL, 5LL}, 0LL},
    {"negative nanoseconds", {100LL, -1LL}, {98LL, 0LL}, 2000LL},
    {"nanoseconds of minus one second", {100LL, -1000000000LL}, {98LL, 0LL}, 1000LL},
    {"nanoseconds below minus one second", {100LL, -1000000001LL}, {97LL, 0LL}, 2000LL},
    {"nanoseconds of minus one and a half seconds", {100LL, -1500000000LL}, {97LL, 0LL}, 1500LL},
    {"large negative nanoseconds", {727420490LL, -508875901615542LL}, {-925054LL, 893190025LL}, 727836667206LL},
    {"borrow with seconds ahead", {105LL, 100LL}, {100LL, 200LL}, 5000LL},
    {"now ahead by seconds", {100LL, 900000000LL}, {103LL, 0LL}, 0LL},
    {"lowest nanoseconds", {0LL, INT64_MIN}, {-9300000000LL, 0LL}, 76627963146LL},
    {"nanoseconds of one second", {100LL, 1000000000LL}, {100LL, 0LL}, 1000LL},
    {"nanoseconds of two seconds less one", {100LL, 1999999999LL}, {100LL, 0LL}, 2000LL},
    {"nanoseconds of two seconds", {100LL, 2000000000LL}, {100LL, 0LL}, 2000LL},
    {"largest nanoseconds", {0LL, INT64_MAX}, {0LL, 0LL}, 9223372036855LL},
    {"unnormalized now nanoseconds", {100LL, 0LL}, {99LL, 1500000000LL}, 0LL},
    {"negative now nanoseconds", {100LL, 0LL}, {100LL, -300000000LL}, 300LL},
    {"seconds wrap", {INT64_MAX, 0LL}, {-1LL, 0LL}, 0LL},
    {"millisecond product wraps", {9232604641496272LL, 0LL}, {0LL, 0LL}, -9214139432213279616LL},
    {"now just before zero", {0LL, INT64_MAX}, {-1LL, -1LL}, 9223372037855LL},
    {"lowest seconds now", {0LL, 0LL}, {INT64_MIN, 0LL}, 0LL},
};

}

int main() {
    for (const auto& entry : Cases) {
        const Xtime target {entry.target[0], entry.target[1]};
        const Xtime now {entry.now[0], entry.now[1]};
        const std::int64_t millis = _ZNSt8__sce_v222_Xtime_diff_to_millis2EPK8timespecS2__nid_postfix(&target, &now);
        if (millis == entry.millis && target.sec == entry.target[0] && target.nsec == entry.target[1] && now.sec == entry.now[0] && now.nsec == entry.now[1]) continue;
        std::fprintf(stderr, "sce v2 xtime diff to millis: %s: %lld\n", entry.label, static_cast<long long>(millis));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
