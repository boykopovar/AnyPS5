#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ufieldpositer_close_nid_postfix), void (APS5_VABI*)(UFieldPositionIterator*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU ufieldpositer_close: %s\n", message);
    std::abort();
}

void RequireNonNullThrows(UFieldPositionIterator* iterator) {
    try {
        ufieldpositer_close_nid_postfix(iterator);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null UFieldPositionIterator handle did not throw");
}

}

int main() {
    ufieldpositer_close_nid_postfix(nullptr);
    UFieldPositionIterator* nullIterator = nullptr;
    ufieldpositer_close_nid_postfix(nullIterator);

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0x5a);
    const auto expected = storage;
    RequireNonNullThrows(reinterpret_cast<UFieldPositionIterator*>(storage.data()));
    Require(storage == expected, "non-null UFieldPositionIterator handle was modified before the throw");
    RequireNonNullThrows(reinterpret_cast<UFieldPositionIterator*>(static_cast<std::uintptr_t>(1)));

    std::puts("ufieldpositer_close null disposal passed");
    return 0;
}
