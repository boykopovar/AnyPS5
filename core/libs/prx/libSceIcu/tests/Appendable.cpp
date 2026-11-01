#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

int main() {
    const auto check = [](bool value) { if (!value) std::abort(); };
    std::array<char16_t, 8> scratch{};
    for (auto minimum : {std::numeric_limits<std::int32_t>::min(), -1, 0, 1, 4, 9}) {
        for (auto capacity : {-1, 0, 1, 4, 8}) {
            std::int32_t result = -99;
            const bool fits = minimum >= 1 && capacity >= minimum;
            const auto buffer = _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(nullptr, minimum, -1, scratch.data(), capacity, &result);
            check(buffer == (fits ? scratch.data() : nullptr) && result == (fits ? capacity : 0));
            check(_ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix(nullptr, minimum) == 1);
        }
    }
    std::int32_t result = 0;
    check(_ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(nullptr, 1, 0, nullptr, 8, &result) == nullptr && result == 8);
    bool threw = false;
    try { _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(nullptr, 0, 0, nullptr, 0, nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw);
    std::puts("ICU Appendable buffer tests passed");
}
