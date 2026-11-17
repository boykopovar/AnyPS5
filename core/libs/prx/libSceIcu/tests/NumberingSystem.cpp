#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UnumsysClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&unumsys_close_nid_postfix), void (APS5_VABI*)(UNumberingSystem*)>);

namespace {

void CheckNonNullThrows(UNumberingSystem* unumsys) {
    try {
        unumsys_close_nid_postfix(unumsys);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "a non-null numbering system did not throw");
}

} // namespace

void Run() {
    unumsys_close_nid_postfix(nullptr);
    void (APS5_VABI* closeFn)(UNumberingSystem*) = &unumsys_close_nid_postfix;
    closeFn(nullptr);

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0xa5);
    const auto expected = storage;
    CheckNonNullThrows(reinterpret_cast<UNumberingSystem*>(storage.data()));
    Require(storage == expected, "closing an unsupported handle must not modify caller memory");
    CheckNonNullThrows(reinterpret_cast<UNumberingSystem*>(std::uintptr_t{1}));
}

} // namespace UnumsysClose

namespace UnumsysOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unumsys_open_nid_postfix), UNumberingSystem* (APS5_VABI*)(const char*, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(unumsys_open_nid_postfix("en", &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(unumsys_open_nid_postfix(nullptr, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { unumsys_open_nid_postfix("en", &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { unumsys_open_nid_postfix("en@numbers=hanidec", &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { unumsys_open_nid_postfix("en", nullptr); });
}

} // namespace UnumsysOpen

namespace UnumsysOpenByName {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unumsys_openByName_nid_postfix), UNumberingSystem* (APS5_VABI*)(const char*, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(unumsys_openByName_nid_postfix("latn", &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(unumsys_openByName_nid_postfix(nullptr, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { unumsys_openByName_nid_postfix("latn", &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { unumsys_openByName_nid_postfix("", &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { unumsys_openByName_nid_postfix("latn", nullptr); });
}

} // namespace UnumsysOpenByName

void CheckIcuNumberingSystem() {
    UnumsysClose::Run();
    UnumsysOpen::Run();
    UnumsysOpenByName::Run();
}
