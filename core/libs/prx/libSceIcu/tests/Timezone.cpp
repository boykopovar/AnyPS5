#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include "prx/libc/include/Api.hpp"
#endif

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu timezone: " + message);
}

#if defined(_WIN32)
void SetGuestZone(const char* value) {
    Require(setenv_nid_postfix("TZ", value, 1) == 0, "the guest TZ could not be set");
}

void RestoreGuestZone(const char* value, bool had) {
    if (had) {
        setenv_nid_postfix("TZ", value, 1);
    } else {
        unsetenv_nid_postfix("TZ");
    }
    uprv_tzset_nid_postfix();
}
#endif

}

int main() {
    uprv_tzset_nid_postfix();

    const char* gmt = uprv_tzname_nid_postfix(1);
    Require(gmt != nullptr, "the GMT-style timezone name is null");

    const std::int32_t offset = uprv_timezone_nid_postfix();
    Require(offset >= -14 * 3600 && offset <= 14 * 3600,
        "the timezone offset is outside the range of any real timezone");

    const char* daylight = uprv_tzname_nid_postfix(0);
    Require(daylight != nullptr, "the daylight timezone name is null");

    uprv_tzset_nid_postfix();
    Require(uprv_timezone_nid_postfix() >= -14 * 3600, "a second tzset produced an impossible offset");

#if defined(_WIN32)
    const char* original = getenv_nid_postfix("TZ");
    const bool hadOriginal = original != nullptr;
    const std::string saved = hadOriginal ? original : "";

    SetGuestZone("EST5");
    uprv_tzset_nid_postfix();
    Require(uprv_timezone_nid_postfix() == 18000, "a guest TZ of EST5 did not produce 18000 seconds west");
    const char* western = uprv_tzname_nid_postfix(1);
    Require(western != nullptr && std::strcmp(western, "GMT-05:00") == 0,
        "a guest TZ of EST5 did not name GMT-05:00");

    SetGuestZone("UTC0");
    uprv_tzset_nid_postfix();
    Require(uprv_timezone_nid_postfix() == 0, "a guest TZ of UTC0 did not produce a zero offset");
    const char* universal = uprv_tzname_nid_postfix(1);
    Require(universal != nullptr && std::strcmp(universal, "GMT+00:00") == 0,
        "a guest TZ of UTC0 did not name GMT+00:00");

    RestoreGuestZone(saved.c_str(), hadOriginal);
#endif
    return 0;
}
