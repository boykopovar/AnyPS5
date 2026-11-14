#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace UresClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ures_close), void (APS5_VABI*)(UResourceBundle*)>);

namespace {

void RequireNonNullThrows(UResourceBundle* bundle) {
    try {
        ures_close(bundle);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null UResourceBundle handle did not throw");
}

}

void Run() {
    UResourceBundle* bundle = nullptr;
    ures_close(bundle);
    for (int i = 0; i < 10; ++i) {
        ures_close(nullptr);
    }

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0x5a);
    const auto expected = storage;
    RequireNonNullThrows(reinterpret_cast<UResourceBundle*>(storage.data()));
    Require(storage == expected, "non-null UResourceBundle handle was modified before the throw");
    RequireNonNullThrows(reinterpret_cast<UResourceBundle*>(static_cast<std::uintptr_t>(1)));
}

} // namespace UresClose

void CheckIcuResourceBundle() {
    UresClose::Run();
}
