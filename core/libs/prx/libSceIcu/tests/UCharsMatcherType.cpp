#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <type_traits>

using GetMatcherType = std::int32_t (APS5_VABI*)(const void*);
static_assert(std::is_same_v<decltype(&_ZNK6icu_6823UCharsDictionaryMatcher7getTypeEv_nid_postfix), GetMatcherType>);

int main() {
    alignas(16) std::array<std::uint8_t, 64> receiver{};
    for (const auto pattern : {0x00, 0x5a, 0xff}) {
        receiver.fill(static_cast<std::uint8_t>(pattern));
        const auto before = receiver;
        if (_ZNK6icu_6823UCharsDictionaryMatcher7getTypeEv_nid_postfix(receiver.data()) != 1 || receiver != before) {
            std::fprintf(stderr, "UCharsDictionaryMatcher::getType did not return 1 without modifying the receiver\n");
            return 1;
        }
    }
    return 0;
}
