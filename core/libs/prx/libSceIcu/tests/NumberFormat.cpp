#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&unum_close_nid_postfix), void (APS5_VABI*)(UNumberFormat*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU unum_close: %s\n", message);
    std::abort();
}

template<typename TAction>
bool ThrowsRuntimeError(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    unum_close_nid_postfix(nullptr);
    UNumberFormat* nullFormat = nullptr;
    unum_close_nid_postfix(nullFormat);

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0x5a);
    auto* nonNullFormat = reinterpret_cast<UNumberFormat*>(storage.data());
    Require(ThrowsRuntimeError([&] { unum_close_nid_postfix(nonNullFormat); }), "non-null UNumberFormat must throw");
    for (const std::uint8_t byte : storage) {
        Require(byte == 0x5a, "non-null UNumberFormat must not be modified before throwing");
    }

    auto* unmappedFormat = reinterpret_cast<UNumberFormat*>(std::uintptr_t{1});
    Require(ThrowsRuntimeError([&] { unum_close_nid_postfix(unmappedFormat); }), "unmapped non-null UNumberFormat must throw without dereference");

    std::puts("ICU unum_close tests passed");
    return 0;
}
