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
    RequireNoThrow([&] { unumf_closeResult_nid_postfix(reinterpret_cast<UFormattedNumber*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireNoThrow([&] { unumf_closeResult_nid_postfix(reinterpret_cast<UFormattedNumber*>(std::uintptr_t{1})); });

    std::int32_t status = 0;
    auto* result = unumf_openResult_nid_postfix(&status);
    Require(result != nullptr && status == 0);
    unumf_closeResult_nid_postfix(result);
    RequireNoThrow([&] { unumf_closeResult_nid_postfix(result); });

    auto* foreign = udtitvfmt_openResult_nid_postfix(&status);
    Require(foreign != nullptr && status == 0);
    RequireNoThrow([&] { unumf_closeResult_nid_postfix(reinterpret_cast<UFormattedNumber*>(foreign)); });
    udtitvfmt_closeResult_nid_postfix(foreign);
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
        auto* result = unumf_openResult_nid_postfix(&status);
        Require(result != nullptr);
        Require(status == incoming);
        unumf_closeResult_nid_postfix(result);
    }

    std::int32_t status = 0;
    auto* first = unumf_openResult_nid_postfix(&status);
    auto* second = unumf_openResult_nid_postfix(&status);
    Require(status == 0 && first != nullptr && second != nullptr && first != second);
    unumf_closeResult_nid_postfix(first);
    unumf_closeResult_nid_postfix(second);

    RequireThrows([&] { unumf_openResult_nid_postfix(nullptr); });
}

} // namespace UnumfOpenResult

void CheckIcuNumberFormatter() {
    UnumfClose::Run();
    UnumfCloseResult::Run();
    UnumfOpenResult::Run();
}
