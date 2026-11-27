#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

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
        for (const char* locale : {"en", "und", "", static_cast<const char*>(nullptr)}) {
            std::int32_t status = incoming;
            UCollator* collator = ucol_open_nid_postfix(locale, &status);
            Require(collator != nullptr);
            Require(status <= 0);
            ucol_close_nid_postfix(collator);
        }
    }

    std::int32_t status = 0;
    auto* closed = ucol_open_nid_postfix("en", &status);
    Require(closed != nullptr && status <= 0);
    ucol_close_nid_postfix(closed);
    status = 0;
    auto* live = ucol_open_nid_postfix("en", &status);
    Require(live != nullptr && live != closed && status <= 0);
    RequireThrows([&] { ucol_close_nid_postfix(closed); });
    Require(ucol_strcoll_nid_postfix(live, u"a", -1, u"b", -1) == UCOL_LESS);
    ucol_close_nid_postfix(live);

    const std::string previous = uloc_getDefault();
    status = 0;
    uloc_setDefault_nid_postfix("de", &status);
    Require(status <= 0);
    auto* selected = ucol_open_nid_postfix(nullptr, &status);
    Require(selected != nullptr && status <= 0);
    Require(ucol_strcoll_nid_postfix(selected, u"z", -1, u"ä", -1) == UCOL_GREATER);
    ucol_close_nid_postfix(selected);
    status = 0;
    uloc_setDefault_nid_postfix("sv", &status);
    Require(status <= 0);
    selected = ucol_open_nid_postfix(nullptr, &status);
    Require(selected != nullptr && status <= 0);
    Require(ucol_strcoll_nid_postfix(selected, u"z", -1, u"ä", -1) == UCOL_LESS);
    ucol_close_nid_postfix(selected);
    status = 0;
    uloc_setDefault_nid_postfix(previous.c_str(), &status);
    Require(status <= 0);

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

namespace UcolCollation {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ucol_strcoll_nid_postfix),
                             UCollationResult (APS5_VABI*)(const UCollator*, const char16_t*, std::int32_t, const char16_t*, std::int32_t)>);
static_assert(std::is_same_v<decltype(&ucol_strcollUTF8_nid_postfix),
                             UCollationResult (APS5_VABI*)(const UCollator*, const char*, std::int32_t, const char*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ucol_getStrength_nid_postfix), UCollationStrength (APS5_VABI*)(const UCollator*)>);
static_assert(std::is_same_v<decltype(&ucol_setStrength_nid_postfix), void (APS5_VABI*)(UCollator*, UCollationStrength)>);

namespace {

UCollator* Open(const char* locale) {
    std::int32_t status = 0;
    UCollator* collator = ucol_open_nid_postfix(locale, &status);
    Require(collator != nullptr);
    Require(status <= 0);
    return collator;
}

UCollationResult Compare(const UCollator* collator, const char16_t* source, const char16_t* target) {
    return ucol_strcoll_nid_postfix(collator, source, -1, target, -1);
}

void Lengths(const UCollator* root) {
    Require(Compare(root, u"a", u"b") == UCOL_LESS);
    Require(Compare(root, u"b", u"a") == UCOL_GREATER);
    Require(Compare(root, u"a", u"a") == UCOL_EQUAL);
    Require(ucol_strcoll_nid_postfix(root, u"ab", 1, u"a", -1) == UCOL_EQUAL);
    Require(ucol_strcoll_nid_postfix(root, u"a", -1, u"ab", 2) == UCOL_LESS);
    Require(ucol_strcoll_nid_postfix(root, nullptr, 0, u"", -1) == UCOL_EQUAL);
    Require(ucol_strcoll_nid_postfix(root, nullptr, 0, u"a", -1) == UCOL_LESS);
    Require(ucol_strcoll_nid_postfix(root, nullptr, 1, u"a", -1) == UCOL_EQUAL);
    Require(ucol_strcoll_nid_postfix(root, u"a", -1, nullptr, -1) == UCOL_EQUAL);
}

void PageCrossing(const UCollator* root) {
    std::vector<char16_t> left(5000, u'a');
    left.back() = 0;
    std::vector<char16_t> right(left);
    Require(Compare(root, left.data(), right.data()) == UCOL_EQUAL);
    right[4998] = u'b';
    Require(Compare(root, left.data(), right.data()) == UCOL_LESS);
    Require(ucol_strcoll_nid_postfix(root, left.data(), 4998, right.data(), 4998) == UCOL_EQUAL);
}

void Strength(UCollator* root) {
    Require(ucol_getStrength_nid_postfix(root) == UCOL_TERTIARY);
    Require(Compare(root, u"a", u"A") == UCOL_LESS);
    ucol_setStrength_nid_postfix(root, UCOL_PRIMARY);
    Require(ucol_getStrength_nid_postfix(root) == UCOL_PRIMARY);
    Require(Compare(root, u"a", u"A") == UCOL_EQUAL);
    Require(Compare(root, u"a", u"á") == UCOL_EQUAL);
    ucol_setStrength_nid_postfix(root, UCOL_SECONDARY);
    Require(Compare(root, u"a", u"á") == UCOL_LESS);
    Require(Compare(root, u"a", u"A") == UCOL_EQUAL);
    ucol_setStrength_nid_postfix(root, UCOL_DEFAULT);
    Require(ucol_getStrength_nid_postfix(root) == UCOL_TERTIARY);
    Require(ucol_getStrength_nid_postfix(nullptr) == UCOL_DEFAULT);
    ucol_setStrength_nid_postfix(nullptr, UCOL_PRIMARY);
}

void Attributes(UCollator* root) {
    std::int32_t status = 0;
    Require(ucol_getAttribute_nid_postfix(root, UCOL_NUMERIC_COLLATION, &status) == UCOL_OFF);
    Require(status == 0);
    Require(Compare(root, u"9", u"10") == UCOL_GREATER);
    ucol_setAttribute_nid_postfix(root, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    Require(status == 0);
    Require(ucol_getAttribute_nid_postfix(root, UCOL_NUMERIC_COLLATION, &status) == UCOL_ON);
    Require(Compare(root, u"9", u"10") == UCOL_LESS);
    ucol_setAttribute_nid_postfix(root, UCOL_NUMERIC_COLLATION, UCOL_OFF, &status);
    Require(status == 0);

    for (std::int32_t incoming : {0, -128}) {
        status = incoming;
        Require(ucol_getAttribute_nid_postfix(root, 100, &status) == UCOL_DEFAULT);
        Require(status == 1);
        status = incoming;
        Require(ucol_getAttribute_nid_postfix(root, -1, &status) == UCOL_DEFAULT);
        Require(status == 1);
        status = incoming;
        ucol_setAttribute_nid_postfix(root, UCOL_STRENGTH, static_cast<UColAttributeValue>(100), &status);
        Require(status == 1);
        status = incoming;
        ucol_setAttribute_nid_postfix(root, static_cast<UColAttribute>(100), UCOL_ON, &status);
        Require(status == 1);
        status = incoming;
        ucol_setAttribute_nid_postfix(root, UCOL_FRENCH_COLLATION, UCOL_PRIMARY, &status);
        Require(status == 1);
    }
    status = 0;
    Require(ucol_getAttribute_nid_postfix(root, UCOL_STRENGTH, &status) == UCOL_TERTIARY);
    Require(status == 0);
}

void Utf8(const UCollator* root) {
    std::int32_t status = 0;
    Require(ucol_strcollUTF8_nid_postfix(root, "a", -1, "b", -1, &status) == UCOL_LESS);
    Require(ucol_strcollUTF8_nid_postfix(root, "ab", 1, "a", 1, &status) == UCOL_EQUAL);
    Require(status == 0);
    Require(ucol_strcollUTF8_nid_postfix(root, nullptr, 1, "a", -1, &status) == UCOL_EQUAL);
    Require(status == 1);
    for (std::int32_t failure : {1, 15}) {
        status = failure;
        Require(ucol_strcollUTF8_nid_postfix(nullptr, "a", -1, "b", -1, &status) == UCOL_EQUAL);
        Require(status == failure);
    }
    status = 0;
    RequireThrows([&] { ucol_strcollUTF8_nid_postfix(nullptr, "a", -1, "b", -1, &status); });
    RequireThrows([&] { ucol_strcollUTF8_nid_postfix(root, "a", -1, "b", -1, nullptr); });
}

void Locales() {
    UCollator* german = Open("de");
    UCollator* swedish = Open("sv");
    Require(Compare(german, u"ä", u"z") == UCOL_LESS);
    Require(Compare(swedish, u"ä", u"z") == UCOL_GREATER);
    std::int32_t status = 0;
    Require(ucol_strcollUTF8_nid_postfix(german, "\xc3\xa4", -1, "z", -1, &status) == UCOL_LESS);
    Require(ucol_strcollUTF8_nid_postfix(swedish, "\xc3\xa4", -1, "z", -1, &status) == UCOL_GREATER);
    Require(status == 0);
    ucol_close_nid_postfix(german);
    ucol_close_nid_postfix(swedish);
}

void GuestDefault() {
    const std::string saved = uloc_getDefault();
    std::int32_t status = 0;
    uloc_setDefault_nid_postfix("sv", &status);
    Require(status <= 0);
    for (const char* locale : {static_cast<const char*>(nullptr), "sv"}) {
        UCollator* collator = Open(locale);
        Require(Compare(collator, u"ä", u"z") == UCOL_GREATER);
        ucol_close_nid_postfix(collator);
    }
    UCollator* root = Open("");
    Require(Compare(root, u"ä", u"z") == UCOL_LESS);
    ucol_close_nid_postfix(root);
    status = 0;
    uloc_setDefault_nid_postfix("de", &status);
    Require(status <= 0);
    UCollator* german = Open(nullptr);
    Require(Compare(german, u"ä", u"z") == UCOL_LESS);
    ucol_close_nid_postfix(german);
    status = 0;
    uloc_setDefault_nid_postfix(saved.c_str(), &status);
    Require(status <= 0);
}

void Handles() {
    UCollator* collator = Open("und");
    ucol_close_nid_postfix(collator);
    RequireThrows([&] { ucol_close_nid_postfix(collator); });
    RequireThrows([&] { Compare(collator, u"a", u"b"); });
    RequireThrows([&] { ucol_getStrength_nid_postfix(collator); });
    RequireThrows([&] { ucol_setStrength_nid_postfix(collator, UCOL_PRIMARY); });
    RequireThrows([] { ucol_strcoll_nid_postfix(nullptr, u"a", -1, u"b", -1); });
}

}

void Run() {
    UCollator* root = Open("und");
    Lengths(root);
    PageCrossing(root);
    Strength(root);
    Attributes(root);
    Utf8(root);
    ucol_close_nid_postfix(root);
    Locales();
    GuestDefault();
    Handles();
}

} // namespace UcolCollation

namespace CollatorGuestPointers {

void Run() {
    auto rejectsRuntime = [](auto&& call) {
        errno = 71;
        bool threw = false;
        try {
            call();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        IcuRecoveredTest::Require(threw && errno == 71);
    };

    auto* badStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    rejectsRuntime([&] { ucol_getAttribute_nid_postfix(nullptr, 0, badStatus); });
    rejectsRuntime([&] { ucol_getReorderCodes_nid_postfix(nullptr, nullptr, 0, badStatus); });
    rejectsRuntime([&] { ucol_getTailoredSet_nid_postfix(nullptr, badStatus); });
    rejectsRuntime([&] { ucol_open_nid_postfix("en", badStatus); });
    rejectsRuntime([&] { ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_PRIMARY, badStatus); });

    std::int32_t status = 0;
    UCollator* collator = ucol_open_nid_postfix("und", &status);
    IcuRecoveredTest::Require(collator != nullptr);
    auto* badText = reinterpret_cast<const char16_t*>(std::uintptr_t{0x10});
    auto* badBytes = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    rejectsRuntime([&] { ucol_open_nid_postfix(badBytes, &status); });
    rejectsRuntime([&] { ucol_strcoll_nid_postfix(collator, badText, -1, u"a", -1); });
    rejectsRuntime([&] { ucol_strcoll_nid_postfix(collator, u"a", -1, badText, 4); });
    rejectsRuntime([&] { ucol_strcoll_nid_postfix(collator, badText, std::numeric_limits<std::int32_t>::max(), u"a", -1); });
    rejectsRuntime([&] { ucol_strcollUTF8_nid_postfix(collator, badBytes, -1, "a", -1, &status); });
    rejectsRuntime([&] { ucol_strcollUTF8_nid_postfix(collator, "a", -1, "b", -1, badStatus); });
    rejectsRuntime([&] { ucol_getAttribute_nid_postfix(collator, UCOL_STRENGTH, badStatus); });
    ucol_close_nid_postfix(collator);
}

}

namespace UcolQueries {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ucol_getContractionsAndExpansions_nid_postfix), void (APS5_VABI*)(const UCollator*, USet*, USet*, std::int8_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ucol_getKeywordValues_nid_postfix), UEnumeration* (APS5_VABI*)(const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ucol_getKeywordValuesForLocale_nid_postfix), UEnumeration* (APS5_VABI*)(const char*, const char*, std::int8_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ucol_getRules_nid_postfix), const char16_t* (APS5_VABI*)(const UCollator*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ucol_strcollIter_nid_postfix), std::int32_t (APS5_VABI*)(const UCollator*, UCharIterator*, UCharIterator*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::array<std::int32_t, 7> Failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
constexpr std::array<std::int32_t, 4> Passing{0, -128, -124, -1};

} // namespace

void Run() {
    auto storage = Filler();
    const auto coll = reinterpret_cast<const UCollator*>(storage.data());
    const auto set = reinterpret_cast<USet*>(storage.data());
    const auto iterator = reinterpret_cast<UCharIterator*>(storage.data());
    std::int32_t length = 77;
    for (const auto incoming : Failing) {
        std::int32_t status = incoming;
        ucol_getContractionsAndExpansions_nid_postfix(coll, set, set, 1, &status);
        ucol_getContractionsAndExpansions_nid_postfix(nullptr, nullptr, nullptr, 0, &status);
        Require(ucol_getKeywordValues_nid_postfix("collation", &status) == nullptr);
        Require(ucol_getKeywordValues_nid_postfix(nullptr, &status) == nullptr);
        Require(ucol_getKeywordValuesForLocale_nid_postfix("collation", "de", 1, &status) == nullptr);
        Require(ucol_getKeywordValuesForLocale_nid_postfix(nullptr, nullptr, 0, &status) == nullptr);
        Require(ucol_strcollIter_nid_postfix(coll, iterator, iterator, &status) == 0);
        Require(ucol_strcollIter_nid_postfix(nullptr, nullptr, nullptr, &status) == 0);
        Require(status == incoming);
    }
    for (const auto incoming : Passing) {
        std::int32_t status = incoming;
        ucol_getContractionsAndExpansions_nid_postfix(nullptr, set, set, 1, &status);
        Require(status == IllegalArgument);
        for (const char* keyword : {static_cast<const char*>(nullptr), "", "c", "collatio", "collations", "Collation", "calendar"}) {
            status = incoming;
            Require(ucol_getKeywordValues_nid_postfix(keyword, &status) == nullptr);
            Require(status == IllegalArgument);
        }
        const auto illegalIter = [&](const UCollator* target, UCharIterator* source, UCharIterator* other) {
            status = incoming;
            Require(ucol_strcollIter_nid_postfix(target, source, other, &status) == 0);
            Require(status == IllegalArgument);
        };
        illegalIter(nullptr, iterator, iterator);
        illegalIter(coll, nullptr, iterator);
        illegalIter(coll, iterator, nullptr);
        status = incoming;
        RequireThrows([&] { ucol_getContractionsAndExpansions_nid_postfix(coll, set, set, 1, &status); });
        RequireThrows([&] { ucol_getContractionsAndExpansions_nid_postfix(coll, nullptr, nullptr, 0, &status); });
        RequireThrows([&] { ucol_getKeywordValues_nid_postfix("collation", &status); });
        RequireThrows([&] { ucol_getKeywordValuesForLocale_nid_postfix("collation", "de", 1, &status); });
        RequireThrows([&] { ucol_getKeywordValuesForLocale_nid_postfix(nullptr, nullptr, 0, &status); });
        RequireThrows([&] { ucol_strcollIter_nid_postfix(coll, iterator, iterator, &status); });
        Require(status == incoming);
    }
    RequireThrows([&] { ucol_getContractionsAndExpansions_nid_postfix(coll, set, set, 1, nullptr); });
    RequireThrows([] { ucol_getKeywordValues_nid_postfix("collation", nullptr); });
    RequireThrows([] { ucol_getKeywordValuesForLocale_nid_postfix("collation", "de", 1, nullptr); });
    Require(ucol_strcollIter_nid_postfix(coll, iterator, iterator, nullptr) == 0);
    Require(ucol_strcollIter_nid_postfix(nullptr, nullptr, nullptr, nullptr) == 0);
    RequireThrows([&] { ucol_getRules_nid_postfix(coll, &length); });
    RequireThrows([&] { ucol_getRules_nid_postfix(nullptr, &length); });
    RequireThrows([&] { ucol_getRules_nid_postfix(coll, nullptr); });
    Require(length == 77);
    std::int32_t status = 0;
    RequireThrows([&] { ucol_getKeywordValues_nid_postfix(reinterpret_cast<const char*>(0x10), &status); });
    Require(status == 0);
    Require(Unchanged(storage));
}

} // namespace UcolQueries

void CheckIcuCollator() {
    UcolClose::Run();
    UcolGetAttribute::Run();
    UcolGetReorderCodes::Run();
    UcolGetTailoredSet::Run();
    UcolOpen::Run();
    UcolQueries::Run();
    UcolSetAttribute::Run();
    UcolCollation::Run();
    CollatorGuestPointers::Run();
}
