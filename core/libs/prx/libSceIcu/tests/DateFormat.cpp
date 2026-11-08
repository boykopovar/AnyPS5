#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udat_close_nid_postfix), void (APS5_VABI*)(UDateFormat*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU udat_close: %s\n", message);
    std::abort();
}

void RequireNonNullThrows(UDateFormat* format) {
    try {
        udat_close_nid_postfix(format);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null UDateFormat handle did not throw");
}

}

int main() {
    udat_close_nid_postfix(nullptr);
    UDateFormat* nullFormat = nullptr;
    udat_close_nid_postfix(nullFormat);

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0x5a);
    const auto expected = storage;
    RequireNonNullThrows(reinterpret_cast<UDateFormat*>(storage.data()));
    Require(storage == expected, "non-null UDateFormat handle was modified before the throw");
    RequireNonNullThrows(reinterpret_cast<UDateFormat*>(static_cast<std::uintptr_t>(1)));

    std::puts("ICU udat_close tests passed");
    return 0;
}
