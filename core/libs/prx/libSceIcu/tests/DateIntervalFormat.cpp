#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <type_traits>

namespace UdtitvfmtClose {

static_assert(std::is_same_v<decltype(&udtitvfmt_close_nid_postfix), void (APS5_VABI*)(UDateIntervalFormat*)>);

namespace {

void CheckNonNullThrows() {
    alignas(8) unsigned char dummy[8]{};
    try {
        udtitvfmt_close_nid_postfix(reinterpret_cast<UDateIntervalFormat*>(dummy));
    } catch (const std::exception&) {
        for (const auto byte : dummy) {
            if (byte != 0) std::abort();
        }
        return;
    }
    std::fputs("non-null UDateIntervalFormat did not throw\n", stderr);
    std::abort();
}

} // namespace

void Run() {
    udtitvfmt_close_nid_postfix(nullptr);
    udtitvfmt_close_nid_postfix(nullptr);
    CheckNonNullThrows();
}

} // namespace UdtitvfmtClose

namespace UdtitvfmtCloseResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&udtitvfmt_closeResult_nid_postfix), void (APS5_VABI*)(UFormattedDateInterval*)>);

void Run() {
    RequireNoThrow([] { udtitvfmt_closeResult_nid_postfix(nullptr); });
    RequireNoThrow([] { udtitvfmt_closeResult_nid_postfix(nullptr); });

    const auto storage = Filler();
    RequireThrows([&] { udtitvfmt_closeResult_nid_postfix(reinterpret_cast<UFormattedDateInterval*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireThrows([&] { udtitvfmt_closeResult_nid_postfix(reinterpret_cast<UFormattedDateInterval*>(std::uintptr_t{1})); });
}

} // namespace UdtitvfmtCloseResult

namespace UdtitvfmtFormatCalendarToResult {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udtitvfmt_formatCalendarToResult_nid_postfix),
                             void (APS5_VABI*)(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

} // namespace

void Run() {
    std::int32_t formatterStorage = 0x11111111;
    std::int32_t fromStorage = 0x22222222;
    std::int32_t toStorage = 0x33333333;
    std::int32_t resultStorage = 0x46444956;
    const auto* formatter = reinterpret_cast<const UDateIntervalFormat*>(&formatterStorage);
    auto* fromCalendar = reinterpret_cast<UCalendar*>(&fromStorage);
    auto* toCalendar = reinterpret_cast<UCalendar*>(&toStorage);
    auto* result = reinterpret_cast<UFormattedDateInterval*>(&resultStorage);

    for (const std::int32_t incoming : {1, 2, 3, 7, 15, 16, 27, 0x7fffffff}) {
        std::int32_t status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status);
        Require(status == incoming);

        status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(formatter, fromCalendar, toCalendar, result, &status);
        Require(status == incoming);
        Require(formatterStorage == 0x11111111 && fromStorage == 0x22222222 && toStorage == 0x33333333 && resultStorage == 0x46444956);
    }

    for (const std::int32_t incoming : {0, -128, -124, -1}) {
        std::int32_t status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status);
        Require(status == IllegalArgument);

        status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(formatter, fromCalendar, toCalendar, nullptr, &status);
        Require(status == IllegalArgument);
        Require(formatterStorage == 0x11111111 && fromStorage == 0x22222222 && toStorage == 0x33333333);
    }

    RequireThrows([&] { udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { udtitvfmt_formatCalendarToResult_nid_postfix(formatter, fromCalendar, toCalendar, result, nullptr); });
}

} // namespace UdtitvfmtFormatCalendarToResult

namespace UdtitvfmtFormatToResult {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udtitvfmt_formatToResult_nid_postfix),
                             void (APS5_VABI*)(const UDateIntervalFormat*, double, double, UFormattedDateInterval*, std::int32_t*)>);

namespace {

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t InvalidFormatError = 3;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t BufferOverflowError = 15;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::int32_t WarningStart = -128;
constexpr std::int32_t NotTerminatedWarning = -124;

void PreexistingFailureShortCircuits() {
    const auto* unalignedFormatter = reinterpret_cast<const UDateIntervalFormat*>(static_cast<std::uintptr_t>(1));
    auto* unalignedResult = reinterpret_cast<UFormattedDateInterval*>(static_cast<std::uintptr_t>(1));
    for (std::int32_t incoming : {IllegalArgumentError, InvalidFormatError, MemoryAllocationError, BufferOverflowError, UnsupportedError, 27, 65536}) {
        std::int32_t status = incoming;
        udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 0.0, nullptr, &status);
        Require(status == incoming);

        status = incoming;
        udtitvfmt_formatToResult_nid_postfix(unalignedFormatter, -1000.5, 2000.25, unalignedResult, &status);
        Require(status == incoming);
    }
}

void NullResultValidation() {
    const auto* unalignedFormatter = reinterpret_cast<const UDateIntervalFormat*>(static_cast<std::uintptr_t>(1));
    for (std::int32_t incoming : {ZeroError, WarningStart, NotTerminatedWarning, -1}) {
        std::int32_t status = incoming;
        udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 0.0, nullptr, &status);
        Require(status == IllegalArgumentError);

        status = incoming;
        udtitvfmt_formatToResult_nid_postfix(unalignedFormatter, std::numeric_limits<double>::quiet_NaN(),
                                             std::numeric_limits<double>::infinity(), nullptr, &status);
        Require(status == IllegalArgumentError);
    }
}

void ForeignResultThrowsWithoutTouchingIt() {
    const auto* unalignedFormatter = reinterpret_cast<const UDateIntervalFormat*>(static_cast<std::uintptr_t>(1));
    for (std::int32_t magicLike : {0, 1, -1, 0x55434600, 0x46444956, 0x56494446}) {
        alignas(8) std::array<std::uint8_t, 32> raw{};
        raw.fill(0xa5);
        std::memcpy(raw.data(), &magicLike, sizeof(magicLike));
        const auto snapshot = raw;
        auto* forged = reinterpret_cast<UFormattedDateInterval*>(raw.data());

        for (std::int32_t incoming : {ZeroError, WarningStart, NotTerminatedWarning, -1}) {
            std::int32_t status = incoming;
            RequireThrows([&] { udtitvfmt_formatToResult_nid_postfix(nullptr, 10.0, 20.0, forged, &status); });
            Require(status == incoming);
            Require(raw == snapshot);

            status = incoming;
            RequireThrows([&] {
                udtitvfmt_formatToResult_nid_postfix(unalignedFormatter, -50.0, 50.0, forged, &status);
            });
            Require(status == incoming);
            Require(raw == snapshot);
        }
    }
}

void UnsupportedPathsThrow() {
    RequireThrows([] { udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 0.0, nullptr, nullptr); });

    alignas(8) std::array<std::uint8_t, 32> raw{};
    auto* nonNullResult = reinterpret_cast<UFormattedDateInterval*>(raw.data());
    for (std::int32_t incoming : {ZeroError, NotTerminatedWarning, -1}) {
        std::int32_t status = incoming;
        RequireThrows([&] { udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 1000.0, nonNullResult, &status); });
        Require(status == incoming);
    }
}

}

void Run() {
    PreexistingFailureShortCircuits();
    NullResultValidation();
    ForeignResultThrowsWithoutTouchingIt();
    UnsupportedPathsThrow();
}

} // namespace UdtitvfmtFormatToResult

namespace UdtitvfmtOpen {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udtitvfmt_open_nid_postfix),
                             UDateIntervalFormat* (APS5_VABI*)(const char*, const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void PreexistingFailureShortCircuits() {
    const auto* badLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto* badSkeleton = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* badTz = reinterpret_cast<const char16_t*>(std::uintptr_t{1});

    for (std::int32_t incoming : {1, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        auto status = incoming;
        Require(udtitvfmt_open_nid_postfix("en_US", u"yMMMd", -1, u"UTC", -1, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(udtitvfmt_open_nid_postfix(nullptr, nullptr, 5, nullptr, -2, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(udtitvfmt_open_nid_postfix(badLocale, badSkeleton, -99, badTz, -99, &status) == nullptr);
        Require(status == incoming);
    }
}

void ArgumentValidation() {
    const auto* badLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto* badSkeleton = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* badTz = reinterpret_cast<const char16_t*>(std::uintptr_t{1});

    for (std::int32_t incoming : {0, -128, -124, -1, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int32_t badNullLength : {-1, 1, 4, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            auto status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, nullptr, badNullLength, nullptr, 0, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, u"yMMMd", -1, nullptr, badNullLength, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, nullptr, 0, nullptr, badNullLength, &status) == nullptr);
            Require(status == IllegalArgument);
        }

        for (std::int32_t badNonNullLength : {-2, -100, std::numeric_limits<std::int32_t>::min()}) {
            auto status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, badSkeleton, badNonNullLength, nullptr, 0, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, u"yMMMd", 5, badTz, badNonNullLength, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, badSkeleton, badNonNullLength, badTz, badNonNullLength, &status) == nullptr);
            Require(status == IllegalArgument);
        }
    }
}

void NullStatusThrows() {
    RequireThrows([&] { udtitvfmt_open_nid_postfix("en_US", u"yMMMd", -1, nullptr, 0, nullptr); });
    RequireThrows([&] { udtitvfmt_open_nid_postfix(nullptr, nullptr, 1, nullptr, 0, nullptr); });
}

} // namespace

void Run() {
    PreexistingFailureShortCircuits();
    ArgumentValidation();
    NullStatusThrows();
}

} // namespace UdtitvfmtOpen

namespace UdtitvfmtOpenResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&udtitvfmt_openResult_nid_postfix), UFormattedDateInterval* (APS5_VABI*)(std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(udtitvfmt_openResult_nid_postfix(&status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { udtitvfmt_openResult_nid_postfix(&status); });
        Require(status == incoming);
    }

    RequireThrows([&] { udtitvfmt_openResult_nid_postfix(nullptr); });
}

} // namespace UdtitvfmtOpenResult

void CheckIcuDateIntervalFormat() {
    UdtitvfmtClose::Run();
    UdtitvfmtCloseResult::Run();
    UdtitvfmtFormatCalendarToResult::Run();
    UdtitvfmtFormatToResult::Run();
    UdtitvfmtOpen::Run();
    UdtitvfmtOpenResult::Run();
}
