#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uidna_close_nid_postfix), void (APS5_VABI*)(UIDNA*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU IDNA: %s\n", message);
    std::abort();
}

void CloseNull() {
    const auto before = NotImplementedCalls_nid_no_patch();
    uidna_close_nid_postfix(nullptr);
    uidna_close_nid_postfix(nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before, "null close does not call NotImplemented");
}

void CloseNonNullThrows() {
    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    const auto expected = storage;
    for (UIDNA* candidate : {
             reinterpret_cast<UIDNA*>(storage.data()),
             reinterpret_cast<UIDNA*>(std::uintptr_t{1}),
         }) {
        const auto before = NotImplementedCalls_nid_no_patch();
        bool threw = false;
        try {
            uidna_close_nid_postfix(candidate);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "non-null close throws NotImplemented");
        Require(NotImplementedCalls_nid_no_patch() == before + 1, "non-null close records NotImplemented");
        Require(storage == expected, "non-null close does not mutate caller memory");
    }
}

}

int main() {
    CloseNull();
    CloseNonNullThrows();
    return 0;
}
