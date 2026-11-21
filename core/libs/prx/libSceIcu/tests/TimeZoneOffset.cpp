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

}

int main() {
    const char* original = getenv_nid_postfix("TZ");
    const bool hadOriginal = original != nullptr;
    const std::string saved = hadOriginal ? original : "";
    Check("UTC0", 0);
    Check("EST5", 18000);
    Check("JST-9", -32400);
    if (hadOriginal ? setenv_nid_postfix("TZ", saved.c_str(), 1) != 0 : unsetenv_nid_postfix("TZ") != 0)
        std::abort();
    std::puts("ICU timezone offset tests passed");
}
