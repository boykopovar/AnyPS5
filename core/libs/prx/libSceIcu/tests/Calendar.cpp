#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UcalClone {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ucal_clone_nid_postfix), UCalendar* (APS5_VABI*)(const UCalendar*, std::int32_t*)>);

namespace {

void CheckFailureStatus() {
    for (std::int32_t error : {1, 2, 7, 10, 15, 16, 65536, INT32_MAX}) {
        std::int32_t status = error;
        Require(ucal_clone_nid_postfix(nullptr, &status) == nullptr);
        Require(status == error);

        status = error;
        const auto opaqueCalendar = reinterpret_cast<const UCalendar*>(0x12345678);
        Require(ucal_clone_nid_postfix(opaqueCalendar, &status) == nullptr);
        Require(status == error);
    }
}

}

void Run() {
    CheckFailureStatus();
}

} // namespace UcalClone

namespace UcalClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ucal_close_nid_postfix), void (APS5_VABI*)(UCalendar*)>);

namespace {

void CheckNonNullThrows(UCalendar* cal) {
    try {
        ucal_close_nid_postfix(cal);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null calendar handle did not throw");
}

}

void Run() {
    ucal_close_nid_postfix(nullptr);
    ucal_close_nid_postfix(nullptr);

    std::array<std::uint8_t, 64> storage{};
    storage.fill(0xa5);
    const auto expected = storage;
    CheckNonNullThrows(reinterpret_cast<UCalendar*>(storage.data()));
    Require(storage == expected, "non-null calendar handle modified storage before throwing");
    CheckNonNullThrows(reinterpret_cast<UCalendar*>(std::uintptr_t{1}));
}

} // namespace UcalClose

namespace UcalGet {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ucal_get_nid_postfix), std::int32_t (APS5_VABI*)(const UCalendar*, UCalendarDateFields, std::int32_t*)>);

void Run() {
    constexpr std::array<std::int32_t, 5> failingStatuses{1, 2, 10, 15, 16};
    constexpr std::array<UCalendarDateFields, 6> sampleFields{0, 1, 2, 5, 10, 23};

    for (const auto failingStatus : failingStatuses) {
        for (const auto field : sampleFields) {
            std::int32_t status = failingStatus;
            const auto result = ucal_get_nid_postfix(nullptr, field, &status);
            Require(result == -1);
            Require(status == failingStatus);

            const auto* dummyCalendar = reinterpret_cast<const UCalendar*>(0x12345678);
            std::int32_t statusWithDummy = failingStatus;
            const auto resultWithDummy = ucal_get_nid_postfix(dummyCalendar, field, &statusWithDummy);
            Require(resultWithDummy == -1);
            Require(statusWithDummy == failingStatus);
        }
    }
}

} // namespace UcalGet

namespace UcalGetCanonicalTimeZoneID {

using IcuRecoveredTest::Require;

namespace {

void NullStatus() {
    const char16_t id[] = u"UTC";
    char16_t result[16] = {u'x', u'y', u'z', 0};
    std::int8_t isSystemID = 42;
    Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 3, result, 16, &isSystemID, nullptr) == 0);
    Require(isSystemID == 42);
    Require(result[0] == u'x' && result[1] == u'y' && result[2] == u'z' && result[3] == 0);

    Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 0, nullptr, 0, &isSystemID, nullptr) == 0);
    Require(isSystemID == 42);

    Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 3, result, 16, nullptr, nullptr) == 0);
    Require(result[0] == u'x');
}

void PreexistingFailure() {
    const char16_t id[] = u"America/New_York";
    char16_t result[32] = {u'k', u'e', u'e', u'p', 0};
    for (std::int32_t incoming : {1, 7, 10, 15, 16, 27, 0x7fffffff}) {
        std::int32_t status = incoming;
        std::int8_t isSystemID = 99;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 16, result, 32, &isSystemID, &status) == 0);
        Require(status == incoming);
        Require(isSystemID == 99);
        Require(result[0] == u'k' && result[1] == u'e');

        isSystemID = 99;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 0, nullptr, 0, &isSystemID, &status) == 0);
        Require(status == incoming);
        Require(isSystemID == 99);
    }
}

void PreexistingWarning() {
    constexpr std::int32_t IllegalArgument = 1;
    char16_t result[32] = {u'a', u'b', 0};
    for (std::int32_t warning : {-124, -5, -1}) {
        std::int32_t status = warning;
        std::int8_t isSystemID = 77;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 13, result, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'a');
    }
}

void ArgumentValidation() {
    constexpr std::int32_t IllegalArgument = 1;
    const char16_t id[] = u"Asia/Tokyo";
    char16_t result[32] = {u'i', u'n', u'i', u't', 0};

    {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 10, result, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'i');
    }

    {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 0, result, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'i');
    }

    {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 10, nullptr, 32, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
    }

    for (std::int32_t capacity : {0, -1, -100}) {
        std::int32_t status = 0;
        std::int8_t isSystemID = 1;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(id, 10, result, capacity, &isSystemID, &status) == 0);
        Require(status == IllegalArgument);
        Require(isSystemID == 0);
        Require(result[0] == u'i');
    }

    {
        std::int32_t status = 0;
        Require(ucal_getCanonicalTimeZoneID_nid_postfix(nullptr, 0, nullptr, 0, nullptr, &status) == 0);
        Require(status == IllegalArgument);
    }
}

}

void Run() {
    NullStatus();
    PreexistingFailure();
    PreexistingWarning();
    ArgumentValidation();
}

} // namespace UcalGetCanonicalTimeZoneID

namespace UcalGetDefaultTimeZone {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ucal_getDefaultTimeZone_nid_postfix), std::int32_t (APS5_VABI*)(char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void TestNullStatus() {
    Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 0, nullptr) == 0);
    Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 16, nullptr) == 0);
    Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, -1, nullptr) == 0);

    char16_t buffer[8] = {u'a', u'b', u'c', u'd', u'e', u'f', u'g', u'\0'};
    Require(ucal_getDefaultTimeZone_nid_postfix(buffer, 8, nullptr) == 0);
    Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');
}

void TestFailingStatus() {
    constexpr std::array<std::int32_t, 4> failureCodes{1, 7, 15, 65535};
    for (std::int32_t code : failureCodes) {
        std::int32_t status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 0, &status) == 0);
        Require(status == code);

        status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 32, &status) == 0);
        Require(status == code);

        status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, -1, &status) == 0);
        Require(status == code);

        char16_t buffer[8] = {u'x', u'y', u'z', u'w', u'1', u'2', u'3', u'\0'};
        status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(buffer, 8, &status) == 0);
        Require(status == code);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == u'w');
    }
}

}

void Run() {
    TestNullStatus();
    TestFailingStatus();
}

} // namespace UcalGetDefaultTimeZone

namespace UcalGetKeywordValuesForLocale {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ucal_getKeywordValuesForLocale_nid_postfix),
                             UEnumeration* (APS5_VABI*)(const char*, const char*, std::int8_t, std::int32_t*)>);

void Run() {
    const auto invalidKey = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto invalidLocale = reinterpret_cast<const char*>(std::uintptr_t{2});
    for (std::int32_t incoming : {1, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int8_t commonlyUsed : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}, std::numeric_limits<std::int8_t>::min(),
                                         std::numeric_limits<std::int8_t>::max()}) {
            std::int32_t status = incoming;
            Require(ucal_getKeywordValuesForLocale_nid_postfix("calendar", "en_US", commonlyUsed, &status) == nullptr);
            Require(status == incoming);
            Require(ucal_getKeywordValuesForLocale_nid_postfix(nullptr, nullptr, commonlyUsed, &status) == nullptr);
            Require(status == incoming);
            Require(ucal_getKeywordValuesForLocale_nid_postfix(invalidKey, invalidLocale, commonlyUsed, &status) == nullptr);
            Require(status == incoming);
        }
    }

    RequireThrows([&] { ucal_getKeywordValuesForLocale_nid_postfix("calendar", "en_US", 0, nullptr); });
    RequireThrows([&] { ucal_getKeywordValuesForLocale_nid_postfix(nullptr, nullptr, 1, nullptr); });
}

} // namespace UcalGetKeywordValuesForLocale

namespace UcalGetTimeZoneDisplayName {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ucal_getTimeZoneDisplayName_nid_postfix),
                             std::int32_t (APS5_VABI*)(const void*, std::int32_t, const char*, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void FailureShortCircuit() {
    int opaqueCalendar = 0;
    for (std::int32_t incoming : {1, 2, 7, 10, 15, 16, 27, std::numeric_limits<std::int32_t>::max()}) {
        std::array<char16_t, 4> buffer{u'A', u'B', u'C', u'D'};
        std::int32_t status = incoming;
        Require(ucal_getTimeZoneDisplayName_nid_postfix(nullptr, 0, nullptr, buffer.data(), 4, &status) == -1);
        Require(status == incoming);
        Require(buffer == std::array<char16_t, 4>{u'A', u'B', u'C', u'D'});

        status = incoming;
        Require(ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, 1, "en_US", buffer.data(), 0, &status) == -1);
        Require(status == incoming);
        Require(buffer == std::array<char16_t, 4>{u'A', u'B', u'C', u'D'});

        status = incoming;
        Require(ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, -99, "fr_FR", nullptr, -5, &status) == -1);
        Require(status == incoming);
    }
}

void NonFailurePathsThrow() {
    int opaqueCalendar = 0;
    std::array<char16_t, 4> buffer{u'X', u'Y', u'Z', u'W'};
    RequireThrows([&] {
        ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, 0, "en_US", buffer.data(), 4, nullptr);
    });
    Require(buffer == std::array<char16_t, 4>{u'X', u'Y', u'Z', u'W'});

    for (std::int32_t incoming : {0, -128, -124, -1, std::numeric_limits<std::int32_t>::min()}) {
        std::int32_t status = incoming;
        RequireThrows([&] {
            ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, 0, "en_US", buffer.data(), 4, &status);
        });
        Require(status == incoming);
        Require(buffer == std::array<char16_t, 4>{u'X', u'Y', u'Z', u'W'});

        status = incoming;
        RequireThrows([&] {
            ucal_getTimeZoneDisplayName_nid_postfix(nullptr, 2, nullptr, nullptr, 0, &status);
        });
        Require(status == incoming);
    }
}

} // namespace

void Run() {
    FailureShortCircuit();
    NonFailurePathsThrow();
}

} // namespace UcalGetTimeZoneDisplayName

namespace UcalOpen {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ucal_open_nid_postfix),
                             UCalendar* (APS5_VABI*)(const char16_t*, std::int32_t, const char*, std::int32_t, std::int32_t*)>);

namespace {

void FailingStatusShortCircuits() {
    const char16_t zoneId[] = u"UTC";
    const auto* unmappedZone = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* unmappedLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    for (const std::int32_t incoming : {1, 2, 7, 15, 16, 0x10000, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = incoming;
        Require(ucal_open_nid_postfix(nullptr, -1, nullptr, 0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ucal_open_nid_postfix(zoneId, 3, "en_US", 1, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ucal_open_nid_postfix(unmappedZone, -99, unmappedLocale, -1, &status) == nullptr);
        Require(status == incoming);
    }
}

void UnsupportedPathsThrow() {
    const char16_t zoneId[] = u"UTC";
    RequireThrows([&] { ucal_open_nid_postfix(nullptr, -1, nullptr, 0, nullptr); });
    RequireThrows([&] { ucal_open_nid_postfix(zoneId, 3, "en_US", 1, nullptr); });

    for (const std::int32_t incoming : {0, -1, -124, -128, std::numeric_limits<std::int32_t>::min()}) {
        std::int32_t status = incoming;
        RequireThrows([&] { ucal_open_nid_postfix(nullptr, -1, nullptr, 0, &status); });
        Require(status == incoming);

        status = incoming;
        auto* handle = ucal_open_nid_postfix(zoneId, 3, "en_US", 1, &status);
        Require(handle != nullptr && status <= 0);
        ucal_close_nid_postfix(handle);
    }
}

} // namespace

void Run() {
    FailingStatusShortCircuits();
    UnsupportedPathsThrow();
}

} // namespace UcalOpen

namespace UcalSetGregorianChange {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ucal_setGregorianChange_nid_postfix), void (APS5_VABI*)(UCalendar*, UDate, std::int32_t*)>);

namespace {

void NullCalendar() {
    for (double date : {0.0, -1000.0, 14400000.0, 1.0e12, -1.0e12}) {
        std::int32_t status = 0;
        ucal_setGregorianChange_nid_postfix(nullptr, date, &status);
        Require(status == Icu::IllegalArgument);
    }
}

void WarningStatus() {
    for (std::int32_t warning : {-1, -124, -128}) {
        std::int32_t status = warning;
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, &status);
        Require(status == Icu::IllegalArgument);
    }
}

void FailureShortCircuit() {
    int dummy = 0;
    auto* cal = reinterpret_cast<UCalendar*>(&dummy);
    for (std::int32_t failure : {1, 2, 7, 10, 15, 16}) {
        std::int32_t status = failure;
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, &status);
        Require(status == failure);
        status = failure;
        ucal_setGregorianChange_nid_postfix(cal, 0.0, &status);
        Require(status == failure);
    }
}

void NullStatus() {
    RequireThrows([] {
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, nullptr);
    });
}

}

void Run() {
    NullCalendar();
    WarningStatus();
    FailureShortCircuit();
    NullStatus();
}

} // namespace UcalSetGregorianChange

namespace UcalSetMillis {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ucal_setMillis_nid_postfix), void (APS5_VABI*)(UCalendar*, UDate, std::int32_t*)>);

namespace {

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (...) {
        return;
    }
    Require(false);
}

void TestPreexistingFailure() {
    for (std::int32_t failureCode : {1, 2, 7, 10, 15, 16, 1000}) {
        std::int32_t status = failureCode;
        ucal_setMillis_nid_postfix(nullptr, 0.0, &status);
        Require(status == failureCode);

        status = failureCode;
        ucal_setMillis_nid_postfix(nullptr, 1700000000000.0, &status);
        Require(status == failureCode);

        status = failureCode;
        ucal_setMillis_nid_postfix(nullptr, -1000.0, &status);
        Require(status == failureCode);

        alignas(8) char dummy[64]{};
        auto* dummyCal = reinterpret_cast<UCalendar*>(dummy);
        status = failureCode;
        ucal_setMillis_nid_postfix(dummyCal, 12345.678, &status);
        Require(status == failureCode);
    }
}

void TestNullStatus() {
    RequireThrows([] {
        ucal_setMillis_nid_postfix(nullptr, 0.0, nullptr);
    });
}

} // namespace

void Run() {
    TestPreexistingFailure();
    TestNullStatus();
}

} // namespace UcalSetMillis

void CheckIcuCalendar() {
    UcalClone::Run();
    UcalClose::Run();
    UcalGet::Run();
    UcalGetCanonicalTimeZoneID::Run();
    UcalGetDefaultTimeZone::Run();
    UcalGetKeywordValuesForLocale::Run();
    UcalGetTimeZoneDisplayName::Run();
    UcalOpen::Run();
    UcalSetGregorianChange::Run();
    UcalSetMillis::Run();
}
