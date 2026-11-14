#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uprv_tzset_nid_postfix), void (APS5_VABI*)()>);

namespace {

void SetZone(const char* value) {
    const int result = value == nullptr ? unsetenv_nid_postfix("TZ") : setenv_nid_postfix("TZ", value, 1);
    if (result != 0) std::abort();
}

void Check(const char* zone, int year, int month, int day, int hour) {
    SetZone(zone);
    uprv_tzset_nid_postfix();
    const std::time_t instant = 1704067200;
    const std::tm* local = std::localtime(&instant);
    if (local == nullptr || local->tm_year != year - 1900 || local->tm_mon != month - 1 ||
        local->tm_mday != day || local->tm_hour != hour) {
        std::fprintf(stderr, "timezone refresh failed for %s\n", zone);
        std::abort();
    }
}

}

int main() {
    const char* original = getenv_nid_postfix("TZ");
    const bool hadOriginal = original != nullptr;
    const std::string saved = hadOriginal ? original : "";
    Check("UTC0", 2024, 1, 1, 0);
    Check("EST5", 2023, 12, 31, 19);
    SetZone(hadOriginal ? saved.c_str() : nullptr);
    uprv_tzset_nid_postfix();
    std::puts("ICU timezone refresh tests passed");
}
