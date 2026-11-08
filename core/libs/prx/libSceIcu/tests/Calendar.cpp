#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_setMillis_nid_postfix), void (APS5_VABI*)(UCalendar*, UDate, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (...) {
        return;
    }
    Require(false);
}

void TestPreexistingFailure() {
    for (std::int32_t failureCode : {1, 2, 7, 10, 15, 16, 1000}) {
        std::int32_t status = failureCode;
        ucal_setMillis_nid_postfix(nullptr, 0.0, &status);
        Require(status == failureCode);

        status = failureCode;
        ucal_setMillis_nid_postfix(nullptr, 1700000000000.0, &status);
        Require(status == failureCode);

        status = failureCode;
        ucal_setMillis_nid_postfix(nullptr, -1000.0, &status);
        Require(status == failureCode);

        alignas(8) char dummy[64]{};
        auto* dummyCal = reinterpret_cast<UCalendar*>(dummy);
        status = failureCode;
        ucal_setMillis_nid_postfix(dummyCal, 12345.678, &status);
        Require(status == failureCode);
    }
}

void TestNullStatus() {
    RequireThrows([] {
        ucal_setMillis_nid_postfix(nullptr, 0.0, nullptr);
    });
}

} // namespace

int main() {
    TestPreexistingFailure();
    TestNullStatus();
}
