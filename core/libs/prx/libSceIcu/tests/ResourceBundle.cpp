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
    constexpr std::array<std::int32_t, 8> sizes{
        std::numeric_limits<std::int32_t>::min(), -1, 0, 1, 2, 7, 64, 4096};
    constexpr std::array<std::int32_t, 10> indices{
        std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 6, 63, 4094, 4095,
        std::numeric_limits<std::int32_t>::max()};
    for (const auto size : sizes) {
        bundle.fSize = size;
        for (const auto index : indices) {
            bundle.fIndex = index;
            const auto expected = static_cast<std::int64_t>(index) < static_cast<std::int64_t>(size) - 1;
            Require(ures_hasNext_nid_postfix(&bundle) == expected,
                    "resource iteration differs from ICU");
        }
    }

    std::array<std::int32_t, 2> iterator{3, 5};
    const auto iteratorAddress = reinterpret_cast<std::uintptr_t>(iterator.data());
    Require(iteratorAddress >= offsetof(UResourceBundle, fIndex), "iterator address cannot form a resource handle");
    const auto* partial = reinterpret_cast<const UResourceBundle*>(iteratorAddress - offsetof(UResourceBundle, fIndex));
    Require(ures_hasNext_nid_postfix(partial), "a readable iterator does not require the rest of the resource handle");

    RequireUnreadableThrows(reinterpret_cast<const UResourceBundle*>(std::uintptr_t{1}));
    RequireUnreadableThrows(reinterpret_cast<const UResourceBundle*>(
        std::numeric_limits<std::uintptr_t>::max() - offsetof(UResourceBundle, fIndex) + 1));
}

} // namespace UresHasNext

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using LocaleGetter = const char* (APS5_VABI*)(const UResourceBundle*, std::int32_t*);

static_assert(std::is_same_v<decltype(&ures_getLocale_nid_postfix), LocaleGetter>);
static_assert(std::is_same_v<decltype(&ures_getLocaleInternal_nid_postfix), LocaleGetter>);

void CheckLocaleGetters() {
    const char name[] = "fr_FR";
    const char* entry = name;
    struct BundlePrefix {
        const char* key;
        const char* const* data;
    };
    static_assert(offsetof(BundlePrefix, data) == 8);
    const BundlePrefix prefix{"resource-key", &entry};
    const auto* bundle = reinterpret_cast<const UResourceBundle*>(&prefix);
    const auto* invalid = reinterpret_cast<const UResourceBundle*>(UINTPTR_MAX);

    for (const LocaleGetter get : {ures_getLocale_nid_postfix, ures_getLocaleInternal_nid_postfix}) {
        for (std::int32_t status : {0, -128}) {
            const auto incoming = status;
            Require(get(bundle, &status) == name && status == incoming);
        }
        std::int32_t status = 15;
        Require(get(invalid, &status) == nullptr && status == 15);
        Require(get(invalid, nullptr) == nullptr);
        status = -128;
        Require(get(nullptr, &status) == nullptr && status == 1);
        status = 0;
        RequireThrows([&] { get(invalid, &status); });
        const BundlePrefix empty{};
        RequireThrows([&] { get(reinterpret_cast<const UResourceBundle*>(&empty), &status); });
    }
}

}

void CheckIcuResourceBundle() {
    UresClose::Run();
    UresHasNext::Run();
    CheckLocaleGetters();
}
