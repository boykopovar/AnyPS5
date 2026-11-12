#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

namespace UnumfClose {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unumf_close_nid_postfix), void (APS5_VABI*)(UNumberFormatter*)>);

void Run() {
    RequireNoThrow([] { unumf_close_nid_postfix(nullptr); });
    RequireNoThrow([] { unumf_close_nid_postfix(nullptr); });

    const auto storage = Filler();
    RequireThrows([&] { unumf_close_nid_postfix(reinterpret_cast<UNumberFormatter*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireThrows([&] { unumf_close_nid_postfix(reinterpret_cast<UNumberFormatter*>(std::uintptr_t{1})); });
}

} // namespace UnumfClose

namespace UnumfCloseResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unumf_closeResult_nid_postfix), void (APS5_VABI*)(UFormattedNumber*)>);

void Run() {
    RequireNoThrow([] { unumf_closeResult_nid_postfix(nullptr); });
    RequireNoThrow([] { unumf_closeResult_nid_postfix(nullptr); });

    const auto storage = Filler();
    RequireThrows([&] { unumf_closeResult_nid_postfix(reinterpret_cast<UFormattedNumber*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireThrows([&] { unumf_closeResult_nid_postfix(reinterpret_cast<UFormattedNumber*>(std::uintptr_t{1})); });
}

} // namespace UnumfCloseResult

namespace UnumfOpenResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unumf_openResult_nid_postfix), UFormattedNumber* (APS5_VABI*)(std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(unumf_openResult_nid_postfix(&status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { unumf_openResult_nid_postfix(&status); });
        Require(status == incoming);
    }

    RequireThrows([&] { unumf_openResult_nid_postfix(nullptr); });
}

} // namespace UnumfOpenResult

void CheckIcuNumberFormatter() {
    UnumfClose::Run();
    UnumfCloseResult::Run();
    UnumfOpenResult::Run();
}
