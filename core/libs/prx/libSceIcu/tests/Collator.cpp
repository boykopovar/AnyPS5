#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <type_traits>

namespace UcolClose {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ucol_close_nid_postfix), void (APS5_VABI*)(UCollator*)>);

void Run() {
    RequireNoThrow([] { ucol_close_nid_postfix(nullptr); });
    RequireNoThrow([] { ucol_close_nid_postfix(nullptr); });

    const auto storage = Filler();
    RequireThrows([&] { ucol_close_nid_postfix(reinterpret_cast<UCollator*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireThrows([&] { ucol_close_nid_postfix(reinterpret_cast<UCollator*>(std::uintptr_t{1})); });
}

} // namespace UcolClose

namespace UcolGetAttribute {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

void Run() {
    for (std::int32_t attr : {0, 1, 2, 3, 4, 5, 6, 7, 8, 100, -1}) {
        std::int32_t status = 0;
        const auto result = ucol_getAttribute_nid_postfix(nullptr, attr, &status);
        Require(result == -1);
        Require(status == 0);
    }
    for (std::int32_t warning : {-1, -124, -128}) {
        std::int32_t status = warning;
        const auto result = ucol_getAttribute_nid_postfix(nullptr, 0, &status);
        Require(result == -1);
        Require(status == warning);
    }
    for (std::int32_t error : {1, 2, 10, 15, 16, 65535, 0x7fffffff}) {
        std::int32_t status = error;
        const auto result = ucol_getAttribute_nid_postfix(nullptr, 0, &status);
        Require(result == -1);
        Require(status == error);
    }
    const auto nonNull = reinterpret_cast<const UCollator*>(0x1000);
    for (std::int32_t error : {1, 15}) {
        std::int32_t status = error;
        const auto result = ucol_getAttribute_nid_postfix(nonNull, 0, &status);
        Require(result == -1);
        Require(status == error);
    }
    RequireThrows([] {
        ucol_getAttribute_nid_postfix(nullptr, 0, nullptr);
    });
    RequireThrows([&] {
        ucol_getAttribute_nid_postfix(nonNull, 0, nullptr);
    });
    RequireThrows([&] {
        std::int32_t status = 0;
        ucol_getAttribute_nid_postfix(nonNull, 0, &status);
    });
    RequireThrows([&] {
        std::int32_t status = -124;
        ucol_getAttribute_nid_postfix(nonNull, 0, &status);
    });
}

} // namespace UcolGetAttribute

namespace UcolGetReorderCodes {

using IcuRecoveredTest::Require;

namespace {

void TestFailureStatus() {
    for (std::int32_t failureCode : {1, 2, 7, 10, 15, 16, 27}) {
        std::int32_t status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(nullptr, nullptr, 0, &status) == 0);
        Require(status == failureCode);

        std::array<std::int32_t, 4> buffer = {0x1234, 0x5678, 0x9abc, 0xdef0};
        status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(nullptr, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status) == 0);
        Require(status == failureCode);
        Require(buffer[0] == 0x1234);
        Require(buffer[1] == 0x5678);
        Require(buffer[2] == 0x9abc);
        Require(buffer[3] == 0xdef0);

        status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(nullptr, buffer.data(), -1, &status) == 0);
        Require(status == failureCode);
        Require(buffer[0] == 0x1234);

        const auto dummy = reinterpret_cast<const UCollator*>(&buffer);
        status = failureCode;
        Require(ucol_getReorderCodes_nid_postfix(dummy, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status) == 0);
        Require(status == failureCode);
        Require(buffer[0] == 0x1234);
    }
}

}

void Run() {
    TestFailureStatus();
}

} // namespace UcolGetReorderCodes

namespace UcolGetTailoredSet {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ucol_getTailoredSet_nid_postfix), USet* (APS5_VABI*)(const UCollator*, std::int32_t*)>);

namespace {

void TestFailingStatus() {
    for (std::int32_t errorCode : {1, 2, 7, 15, 16, 100}) {
        std::int32_t status = errorCode;
        USet* result = ucol_getTailoredSet_nid_postfix(nullptr, &status);
        Require(result == nullptr);
        Require(status == errorCode);
    }

    const UCollator* dummyCollator = reinterpret_cast<const UCollator*>(0x12340);
    for (std::int32_t errorCode : {1, 2, 7, 15, 16, 100}) {
        std::int32_t status = errorCode;
        USet* result = ucol_getTailoredSet_nid_postfix(dummyCollator, &status);
        Require(result == nullptr);
        Require(status == errorCode);
    }
}

}

void Run() {
    TestFailingStatus();
}

} // namespace UcolGetTailoredSet

namespace UcolOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ucol_open_nid_postfix), UCollator* (APS5_VABI*)(const char*, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ucol_open_nid_postfix("en", &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ucol_open_nid_postfix(nullptr, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ucol_open_nid_postfix("en", &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ucol_open_nid_postfix("und", &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { ucol_open_nid_postfix("en", nullptr); });
}

} // namespace UcolOpen

namespace UcolSetAttribute {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ucol_setAttribute_nid_postfix), void (APS5_VABI*)(UCollator*, UColAttribute, UColAttributeValue, std::int32_t*)>);

namespace {

void NullCollatorSuccess() {
    std::int32_t status = 0;
    ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_PRIMARY, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_FRENCH_COLLATION, UCOL_ON, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_CASE_LEVEL, UCOL_OFF, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_DECOMPOSITION_MODE, UCOL_OFF, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_HIRAGANA_QUATERNARY_MODE, UCOL_DEFAULT, &status);
    Require(status == 0);
    ucol_setAttribute_nid_postfix(nullptr, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    Require(status == 0);
}

void NullCollatorWarning() {
    for (std::int32_t warning : {-128, -124, -1}) {
        std::int32_t status = warning;
        ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_SECONDARY, &status);
        Require(status == warning);
    }
}

void FailureStatusShortCircuit() {
    std::uint8_t dummy = 0;
    auto* dummyColl = reinterpret_cast<UCollator*>(&dummy);
    for (std::int32_t failure : {1, 2, 7, 10, 15, 16, 27}) {
        std::int32_t status = failure;
        ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_TERTIARY, &status);
        Require(status == failure);
        status = failure;
        ucol_setAttribute_nid_postfix(dummyColl, UCOL_STRENGTH, UCOL_TERTIARY, &status);
        Require(status == failure);
    }
}

void AttributeValues() {
    for (auto attr : {UCOL_FRENCH_COLLATION, UCOL_ALTERNATE_HANDLING, UCOL_CASE_FIRST, UCOL_CASE_LEVEL,
                      UCOL_NORMALIZATION_MODE, UCOL_DECOMPOSITION_MODE, UCOL_STRENGTH,
                      UCOL_HIRAGANA_QUATERNARY_MODE, UCOL_NUMERIC_COLLATION}) {
        for (auto val : {UCOL_DEFAULT, UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_QUATERNARY,
                         UCOL_IDENTICAL, UCOL_OFF, UCOL_ON, UCOL_SHIFTED, UCOL_NON_IGNORABLE,
                         UCOL_LOWER_FIRST, UCOL_UPPER_FIRST}) {
            std::int32_t status = 0;
            ucol_setAttribute_nid_postfix(nullptr, attr, val, &status);
            Require(status == 0);
        }
    }
}

void NullStatusThrows() {
    RequireThrows([] {
        ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_PRIMARY, nullptr);
    });
}

}

void Run() {
    NullCollatorSuccess();
    NullCollatorWarning();
    FailureStatusShortCircuit();
    AttributeValues();
    NullStatusThrows();
}

} // namespace UcolSetAttribute

void CheckIcuCollator() {
    UcolClose::Run();
    UcolGetAttribute::Run();
    UcolGetReorderCodes::Run();
    UcolGetTailoredSet::Run();
    UcolOpen::Run();
    UcolSetAttribute::Run();
}
