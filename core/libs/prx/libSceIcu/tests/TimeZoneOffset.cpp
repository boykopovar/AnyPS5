#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uprv_timezone_nid_postfix), std::int32_t (APS5_VABI*)()>);

namespace {

void Check(const char* zone, std::int32_t expected) {
    if (setenv_nid_postfix("TZ", zone, 1) != 0) std::abort();
    const auto actual = uprv_timezone_nid_postfix();
    if (actual != expected) {
        std::fprintf(stderr, "%s: expected %d, got %d\n", zone, expected, actual);
        std::abort();
    }
}

void CheckAt(const char* zone, std::int64_t time, std::int32_t expected, bool daylight) {
    if (setenv_nid_postfix("TZ", zone, 1) != 0) std::abort();
    GuestTm local{};
    if (!localtime_r_nid_postfix(&time, &local) || (local.tm_isdst != 0) != daylight) std::abort();
    const auto actual = uprv_timezone_at_time_nid_no_patch(time);
    if (actual != expected) {
        std::fprintf(stderr, "%s at %lld: expected %d, got %d\n", zone, static_cast<long long>(time), expected, actual);
        std::abort();
    }
}

}

int main() {
    const char* original = getenv_nid_postfix("TZ");
    const bool hadOriginal = original != nullptr;
    const std::string saved = hadOriginal ? original : "";
    Check("UTC0", 0);
    Check("EST5", 18000);
    Check("JST-9", -32400);
    CheckAt("EST5EDT,M3.2.0/2,M11.1.0/2", 1705320000, 18000, false);
    CheckAt("EST5EDT,M3.2.0/2,M11.1.0/2", 1721044800, 18000, true);
    if (hadOriginal ? setenv_nid_postfix("TZ", saved.c_str(), 1) != 0 : unsetenv_nid_postfix("TZ") != 0)
        std::abort();
    std::puts("ICU timezone offset tests passed");
}
