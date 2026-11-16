#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstdint>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&uprv_ebcdictolower_nid_postfix), char (APS5_VABI*)(char)>);

std::uint8_t Lower(int value) {
    return static_cast<std::uint8_t>(uprv_ebcdictolower_nid_postfix(static_cast<char>(value)));
}

bool Letter(int value) {
    return (0xc1 <= value && value <= 0xc9) || (0xd1 <= value && value <= 0xd9) || (0xe2 <= value && value <= 0xe9);
}

}

int main() {
    Require(Lower(0xc1) == 0x81 && Lower(0xc9) == 0x89);
    Require(Lower(0xd1) == 0x91 && Lower(0xd9) == 0x99);
    Require(Lower(0xe2) == 0xa2 && Lower(0xe9) == 0xa9);
    Require(Lower(0xc0) == 0xc0 && Lower(0xca) == 0xca && Lower(0xd0) == 0xd0 && Lower(0xda) == 0xda);
    Require(Lower(0xe1) == 0xe1 && Lower(0xea) == 0xea && Lower(0x81) == 0x81 && Lower(0xf0) == 0xf0);
    int changed = 0;
    for (int value = 0; value < 256; ++value) {
        Require(Lower(value) == (Letter(value) ? value - 0x40 : value));
        if (Lower(value) != value) ++changed;
    }
    Require(changed == 26);
    return 0;
}
