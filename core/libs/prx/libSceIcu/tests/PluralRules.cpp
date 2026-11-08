#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uplrules_close_nid_postfix), void (APS5_VABI*)(UPluralRules*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU plural rules: %s\n", message);
    std::abort();
}

void CheckNonNullThrows(UPluralRules* handle) {
    try {
        uplrules_close_nid_postfix(handle);
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()) == "uplrules_close_nid_postfix not implemented", "unexpected runtime_error message");
        return;
    }
    Require(false, "non-null handle did not throw");
}

}

int main() {
    uplrules_close_nid_postfix(nullptr);
    uplrules_close_nid_postfix(nullptr);

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    CheckNonNullThrows(reinterpret_cast<UPluralRules*>(storage.data()));
    for (const std::uint8_t byte : storage) {
        Require(byte == 0xa5, "non-null close modified caller storage");
    }
    CheckNonNullThrows(reinterpret_cast<UPluralRules*>(static_cast<std::uintptr_t>(1)));
    std::puts("ICU plural rules tests passed");
}
