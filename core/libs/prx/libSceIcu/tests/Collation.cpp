#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucol_setAttribute_nid_postfix), void (APS5_VABI*)(UCollator*, UColAttribute, UColAttributeValue, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU collation check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

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

int main() {
    NullCollatorSuccess();
    NullCollatorWarning();
    FailureStatusShortCircuit();
    AttributeValues();
    NullStatusThrows();
    std::puts("ICU collation tests passed");
    return 0;
}
