#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udatpg_close_nid_postfix), void (APS5_VABI*)(UDateTimePatternGenerator*)>);

namespace {

void CheckNonNullThrows() {
    alignas(8) std::array<std::uint8_t, 16> storage{};
    storage.fill(0xa5);
    bool threw = false;
    try {
        udatpg_close_nid_postfix(reinterpret_cast<UDateTimePatternGenerator*>(storage.data()));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) {
        std::fputs("non-null UDateTimePatternGenerator close did not throw\n", stderr);
        std::abort();
    }
    for (const auto byte : storage) {
        if (byte != 0xa5) {
            std::fputs("non-null UDateTimePatternGenerator close modified storage\n", stderr);
            std::abort();
        }
    }
}

} // namespace

int main() {
    udatpg_close_nid_postfix(nullptr);
    udatpg_close_nid_postfix(nullptr);
    CheckNonNullThrows();
    std::puts("ICU date time pattern generator tests passed");
}
