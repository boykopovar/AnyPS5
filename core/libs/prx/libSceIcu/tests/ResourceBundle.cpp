#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "uresimp.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

std::int8_t NativeHasNext(const UResourceBundle* bundle) {
    return ures_hasNext(bundle);
}

void NativeInit(UResourceBundle* bundle) {
    ures_initStackObject(bundle);
}

}

#undef ures_close

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

namespace UresHasNext {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ures_hasNext_nid_postfix),
                             std::int8_t (APS5_VABI*)(const UResourceBundle*)>);

namespace {

void RequireUnreadableThrows(const UResourceBundle* bundle) {
    try {
        static_cast<void>(ures_hasNext_nid_postfix(bundle));
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "unreadable UResourceBundle handle did not throw");
}

}

void Run() {
    Require(ures_hasNext_nid_postfix(nullptr) == NativeHasNext(nullptr), "null resource iteration differs from ICU");

    UResourceBundle bundle{};
    NativeInit(&bundle);
    constexpr std::array<std::int32_t, 6> sizes{0, 1, 2, 7, 64, 4096};
    constexpr std::array<std::int32_t, 8> indices{-2, -1, 0, 1, 6, 63, 4094, 4095};
    for (const auto size : sizes) {
        bundle.fSize = size;
        for (const auto index : indices) {
            bundle.fIndex = index;
            Require(ures_hasNext_nid_postfix(&bundle) == NativeHasNext(&bundle),
                    "resource iteration differs from ICU");
        }
    }

    RequireUnreadableThrows(reinterpret_cast<const UResourceBundle*>(std::uintptr_t{1}));
    RequireUnreadableThrows(reinterpret_cast<const UResourceBundle*>(
        std::numeric_limits<std::uintptr_t>::max() - offsetof(UResourceBundle, fIndex) + 1));
}

} // namespace UresHasNext

void CheckIcuResourceBundle() {
    UresClose::Run();
    UresHasNext::Run();
}
