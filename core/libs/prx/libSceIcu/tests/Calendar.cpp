#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_close_nid_postfix), void (APS5_VABI*)(UCalendar*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar: %s\n", message);
    std::abort();
}

void CheckNonNullThrows(UCalendar* cal) {
    try {
        ucal_close_nid_postfix(cal);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null calendar handle did not throw");
}

}

int main() {
    ucal_close_nid_postfix(nullptr);
    ucal_close_nid_postfix(nullptr);

    std::array<std::uint8_t, 64> storage{};
    storage.fill(0xa5);
    const auto expected = storage;
    CheckNonNullThrows(reinterpret_cast<UCalendar*>(storage.data()));
    Require(storage == expected, "non-null calendar handle modified storage before throwing");
    CheckNonNullThrows(reinterpret_cast<UCalendar*>(std::uintptr_t{1}));

    std::puts("ICU calendar close tests passed");
}
