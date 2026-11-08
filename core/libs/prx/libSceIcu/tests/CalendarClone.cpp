#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_clone_nid_postfix), UCalendar* (APS5_VABI*)(const UCalendar*, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar clone check failed at line %u\n", location.line());
    std::abort();
}

void CheckFailureStatus() {
    for (std::int32_t error : {1, 2, 7, 10, 15, 16, 65536, INT32_MAX}) {
        std::int32_t status = error;
        Require(ucal_clone_nid_postfix(nullptr, &status) == nullptr);
        Require(status == error);

        status = error;
        const auto opaqueCalendar = reinterpret_cast<const UCalendar*>(0x12345678);
        Require(ucal_clone_nid_postfix(opaqueCalendar, &status) == nullptr);
        Require(status == error);
    }
}

}

int main() {
    CheckFailureStatus();
}
