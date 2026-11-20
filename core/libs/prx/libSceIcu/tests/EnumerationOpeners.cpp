#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu enumeration: " + message);
}

constexpr std::int32_t ZeroError = 0;

}

namespace {

const char* APS5_VABI NarrowNext(UEnumeration*, std::int32_t* resultLength, std::int32_t*) {
    if (resultLength != nullptr) *resultLength = 1;
    return "x";
}

const char16_t* APS5_VABI WideNext(UEnumeration*, std::int32_t* resultLength, std::int32_t*) {
    if (resultLength != nullptr) *resultLength = 2;
    return u"ab";
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    const char* strings[] = {"alpha", "beta"};
    std::int32_t status = ZeroError;
    UEnumeration* enumeration = uenum_openCharStringsEnumeration_nid_postfix(strings, 2, &status);
    Require(status == ZeroError && enumeration != nullptr, "the char string enumeration did not open");
    Require(uenum_count_nid_postfix(enumeration, &status) == 2, "the enumeration has the wrong count");

    std::int32_t length = 0;
    const char* first = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(first != nullptr && std::strcmp(first, "alpha") == 0 && length == 5, "the first string is wrong");
    const char* second = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(second != nullptr && std::strcmp(second, "beta") == 0 && length == 4, "the second string is wrong");
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr, "the enumeration did not end");

    status = ZeroError;
    uenum_reset_nid_postfix(enumeration, &status);
    Require(status == ZeroError, "uenum_reset failed on a host enumeration");
    const char* again = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(again != nullptr && std::strcmp(again, "alpha") == 0, "the enumeration did not restart");
    uenum_close_nid_postfix(enumeration);

    const char16_t* wide[] = {u"one", u"two", u"three"};
    status = ZeroError;
    UEnumeration* wideEnumeration = uenum_openUCharStringsEnumeration_nid_postfix(wide, 3, &status);
    Require(status == ZeroError && wideEnumeration != nullptr, "the UChar enumeration did not open");
    Require(uenum_count_nid_postfix(wideEnumeration, &status) == 3, "the UChar enumeration has the wrong count");
    const char16_t* wideFirst = uenum_unext_nid_postfix(wideEnumeration, &length, &status);
    Require(wideFirst != nullptr && length == 3, "the first UChar string is wrong");
    uenum_close_nid_postfix(wideEnumeration);

    status = ZeroError;
    UEnumeration* emptyEnumeration = uenum_openCharStringsEnumeration_nid_postfix(nullptr, 0, &status);
    if (emptyEnumeration != nullptr) {
        Require(uenum_count_nid_postfix(emptyEnumeration, &status) == 0, "an empty string array is not empty");
        uenum_close_nid_postfix(emptyEnumeration);
    }

    UEnumeration narrowOnly = {};
    narrowOnly.next = NarrowNext;
    std::int32_t convertedLength = 0;
    status = ZeroError;
    const char16_t* converted = uenum_unextDefault_nid_postfix(&narrowOnly, &convertedLength, &status);
    Require(status == ZeroError && converted != nullptr && convertedLength == 1 && converted[0] == u'x',
        "uenum_unextDefault did not convert the string");
    Require(converted == narrowOnly.baseContext || converted != nullptr, "uenum_unextDefault returned no buffer");

    UEnumeration wideOnly = {};
    wideOnly.uNext = WideNext;
    status = ZeroError;
    const char* narrowed = uenum_nextDefault_nid_postfix(&wideOnly, &convertedLength, &status);
    Require(status == ZeroError && narrowed != nullptr && convertedLength == 2 &&
        narrowed[0] == 'a' && narrowed[1] == 'b', "uenum_nextDefault did not convert the string");

    status = ZeroError;
    Require(uenum_nextDefault_nid_postfix(&narrowOnly, &convertedLength, &status) == nullptr,
        "uenum_nextDefault accepted an enumeration without a wide next");
    status = ZeroError;
    Require(uenum_unextDefault_nid_postfix(&wideOnly, &convertedLength, &status) == nullptr,
        "uenum_unextDefault accepted an enumeration without a narrow next");

    status = ZeroError;
    uenum_reset_nid_postfix(nullptr, &status);
    Require(status == ZeroError, "uenum_reset rejected a null enumeration by changing the status");
    return 0;
}
