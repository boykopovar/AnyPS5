#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&unumsys_close_nid_postfix), void (APS5_VABI*)(UNumberingSystem*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU numbering system: %s\n", message);
    std::abort();
}

void CheckNonNullThrows(UNumberingSystem* unumsys) {
    try {
        unumsys_close_nid_postfix(unumsys);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "a non-null numbering system did not throw");
}

} // namespace

int main() {
    unumsys_close_nid_postfix(nullptr);
    void (APS5_VABI* closeFn)(UNumberingSystem*) = &unumsys_close_nid_postfix;
    closeFn(nullptr);

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0xa5);
    const auto expected = storage;
    CheckNonNullThrows(reinterpret_cast<UNumberingSystem*>(storage.data()));
    Require(storage == expected, "closing an unsupported handle must not modify caller memory");
    CheckNonNullThrows(reinterpret_cast<UNumberingSystem*>(std::uintptr_t{1}));

    std::puts("ICU numbering system tests passed");
    return 0;
}
