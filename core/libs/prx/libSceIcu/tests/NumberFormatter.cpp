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

namespace UnumfFormatAndResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unumf_formatDecimal_nid_postfix), void (APS5_VABI*)(const UNumberFormatter*, const char*, std::int32_t, UFormattedNumber*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unumf_formatDouble_nid_postfix), void (APS5_VABI*)(const UNumberFormatter*, double, UFormattedNumber*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unumf_openForSkeletonAndLocale_nid_postfix), UNumberFormatter* (APS5_VABI*)(const char16_t*, std::int32_t, const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unumf_resultGetAllFieldPositions_nid_postfix), void (APS5_VABI*)(const UFormattedNumber*, UFieldPositionIterator*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unumf_resultToString_nid_postfix), std::int32_t (APS5_VABI*)(const UFormattedNumber*, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::array<std::int32_t, 7> Failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
constexpr std::array<std::int32_t, 4> Passing{0, -128, -124, -1};

} // namespace

void Run() {
    auto storage = Filler();
    const auto formatter = reinterpret_cast<const UNumberFormatter*>(storage.data());
    const auto result = reinterpret_cast<UFormattedNumber*>(storage.data());
    const auto iterator = reinterpret_cast<UFieldPositionIterator*>(storage.data());
    char16_t buffer[8]{};
    for (const auto incoming : Failing) {
        std::int32_t status = incoming;
        unumf_formatDecimal_nid_postfix(formatter, "1.5", 3, result, &status);
        unumf_formatDecimal_nid_postfix(nullptr, nullptr, 0, nullptr, &status);
        unumf_formatDouble_nid_postfix(formatter, 1.5, result, &status);
        unumf_formatDouble_nid_postfix(nullptr, 1.5, nullptr, &status);
        unumf_resultGetAllFieldPositions_nid_postfix(result, iterator, &status);
        unumf_resultGetAllFieldPositions_nid_postfix(nullptr, nullptr, &status);
        Require(unumf_resultToString_nid_postfix(result, buffer, 8, &status) == 0);
        Require(unumf_resultToString_nid_postfix(nullptr, nullptr, -1, &status) == 0);
        Require(status == incoming);
        RequireThrows([&] { unumf_openForSkeletonAndLocale_nid_postfix(u"percent", 7, "en", &status); });
        Require(status == incoming);
    }
    for (const auto incoming : Passing) {
        std::int32_t status = incoming;
        unumf_formatDecimal_nid_postfix(nullptr, "1.5", 3, result, &status);
        Require(status == IllegalArgument);
        status = incoming;
        unumf_formatDouble_nid_postfix(nullptr, 1.5, result, &status);
        Require(status == IllegalArgument);
        status = incoming;
        unumf_resultGetAllFieldPositions_nid_postfix(nullptr, iterator, &status);
        Require(status == IllegalArgument);
        status = incoming;
        Require(unumf_resultToString_nid_postfix(nullptr, buffer, 8, &status) == 0);
        Require(status == IllegalArgument);
        status = incoming;
        RequireThrows([&] { unumf_formatDecimal_nid_postfix(formatter, "1.5", 3, result, &status); });
        RequireThrows([&] { unumf_formatDecimal_nid_postfix(formatter, "1.5", 3, nullptr, &status); });
        RequireThrows([&] { unumf_formatDouble_nid_postfix(formatter, 1.5, result, &status); });
        RequireThrows([&] { unumf_formatDouble_nid_postfix(formatter, 1.5, nullptr, &status); });
        RequireThrows([&] { unumf_resultGetAllFieldPositions_nid_postfix(result, iterator, &status); });
        RequireThrows([&] { unumf_resultGetAllFieldPositions_nid_postfix(result, nullptr, &status); });
        RequireThrows([&] { unumf_resultToString_nid_postfix(result, buffer, 8, &status); });
        RequireThrows([&] { unumf_resultToString_nid_postfix(result, nullptr, 1, &status); });
        RequireThrows([&] { unumf_openForSkeletonAndLocale_nid_postfix(u"percent", 7, "en", &status); });
        RequireThrows([&] { unumf_openForSkeletonAndLocale_nid_postfix(nullptr, 0, nullptr, &status); });
        Require(status == incoming);
    }
    RequireThrows([&] { unumf_formatDecimal_nid_postfix(formatter, "1.5", 3, result, nullptr); });
    RequireThrows([&] { unumf_formatDouble_nid_postfix(formatter, 1.5, result, nullptr); });
    RequireThrows([&] { unumf_resultGetAllFieldPositions_nid_postfix(result, iterator, nullptr); });
    RequireThrows([&] { unumf_resultToString_nid_postfix(result, buffer, 8, nullptr); });
    RequireThrows([] { unumf_openForSkeletonAndLocale_nid_postfix(u"percent", 7, "en", nullptr); });
    for (const auto unit : buffer) Require(unit == 0);
    Require(Unchanged(storage));
}

} // namespace UnumfFormatAndResult

void CheckIcuNumberFormatter() {
    UnumfClose::Run();
    UnumfCloseResult::Run();
    UnumfFormatAndResult::Run();
    UnumfOpenResult::Run();
}
