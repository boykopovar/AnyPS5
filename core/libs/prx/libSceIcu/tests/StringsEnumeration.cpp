#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU enumeration check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

void Dispose(Icu::UEnumeration* enumeration) {
    std::free(enumeration->baseContext);
    std::free(enumeration);
}

const char* APS5_VABI FailedNext_nid_no_patch(Icu::UEnumeration*, std::int32_t* length, std::int32_t* status) {
    *length = 1;
    *status = 1;
    return "a";
}

const char* APS5_VABI OverflowNext_nid_no_patch(Icu::UEnumeration*, std::int32_t* length, std::int32_t*) {
    *length = std::numeric_limits<std::int32_t>::max();
    return "a";
}

}

int main() {
    std::int32_t status = 0;
    Require(!uenum_openCharStringsEnumeration_nid_postfix(nullptr, -1, &status) && !status);
    Require(!uenum_openUCharStringsEnumeration_nid_postfix(nullptr, 1, &status) && !status);
    auto* empty = uenum_openCharStringsEnumeration_nid_postfix(nullptr, 0, &status);
    std::int32_t length = 99;
    Require(!uenum_unextDefault_nid_postfix(empty, &length, &status) && length == 0 && !status);
    Dispose(empty);
    status = 1;
    Require(!uenum_openUCharStringsEnumeration_nid_postfix(nullptr, 0, &status) && status == 1);
    status = -128;
    const std::string longText(100, 'z');
    const char* strings[] = {"abc", "", "\xff", longText.c_str()};
    auto* chars = uenum_openCharStringsEnumeration_nid_postfix(strings, 4, &status);
    Require(chars && status == -128);
    auto* text = uenum_unextDefault_nid_postfix(chars, &length, &status);
    Require(length == 3 && std::u16string(text) == u"abc");
    auto* firstBuffer = chars->baseContext;
    text = uenum_unextDefault_nid_postfix(chars, nullptr, &status);
    Require(text[0] == 0 && chars->baseContext == firstBuffer);
    text = uenum_unextDefault_nid_postfix(chars, &length, &status);
    Require(length == 1 && text[0] == 0xff && text[1] == 0);
    text = uenum_unextDefault_nid_postfix(chars, &length, &status);
    Require(length == 100 && std::u16string(text) == std::u16string(100, u'z'));
    length = 37;
    Require(!uenum_unextDefault_nid_postfix(chars, &length, &status) && length == 0);
    status = 1;
    uenum_reset_nid_postfix(chars, &status);
    Require(!uenum_unextDefault_nid_postfix(chars, &length, &status));
    status = 0;
    uenum_reset_nid_postfix(chars, &status);
    Require(std::u16string(uenum_unextDefault_nid_postfix(chars, nullptr, &status)) == u"abc");
    Dispose(chars);
    const char16_t* units[] = {u"abc", u"a\u00e9b", u"", u"last"};
    auto* unicode = uenum_openUCharStringsEnumeration_nid_postfix(units, 4, &status);
    const auto* bytes = uenum_nextDefault_nid_postfix(unicode, &length, &status);
    Require(length == 3 && !std::strcmp(bytes, "abc"));
    bytes = uenum_nextDefault_nid_postfix(unicode, &length, &status);
    Require(length == 3 && bytes[0] == 'a' && bytes[1] == 0 && bytes[2] == 'b' && bytes[3] == 0);
    bytes = uenum_nextDefault_nid_postfix(unicode, &length, &status);
    Require(length == 0 && !bytes[0]);
    bytes = uenum_nextDefault_nid_postfix(unicode, &length, &status);
    Require(length == 4 && !std::strcmp(bytes, "last"));
    Require(!uenum_nextDefault_nid_postfix(unicode, &length, &status) && length == 4);
    uenum_reset_nid_postfix(unicode, &status);
    Invalid([&] { uenum_nextDefault_nid_postfix(unicode, nullptr, &status); });
    Dispose(unicode);
    Icu::UEnumeration unsupported{};
    length = 17;
    Require(!uenum_unextDefault_nid_postfix(&unsupported, &length, &status) && length == 0 && status == 16);
    status = 0;
    length = 17;
    Require(!uenum_nextDefault_nid_postfix(&unsupported, &length, &status) && length == 17 && status == 16);
    status = 0;
    uenum_reset_nid_postfix(&unsupported, &status);
    Require(status == 16);
    uenum_reset_nid_postfix(nullptr, nullptr);
    Invalid([&] { uenum_unextDefault_nid_postfix(nullptr, &length, &status); });
    Invalid([] { uenum_openCharStringsEnumeration_nid_postfix(nullptr, 0, nullptr); });
    unsupported.next = FailedNext_nid_no_patch;
    status = 1;
    text = uenum_unextDefault_nid_postfix(&unsupported, &length, &status);
    Require(text && text[0] == u'a' && text[1] == 0 && length == 1 && status == 1);
    unsupported.next = OverflowNext_nid_no_patch;
    Invalid([&] { uenum_unextDefault_nid_postfix(&unsupported, &length, &status); });
    std::free(unsupported.baseContext);
    std::puts("ICU enumeration tests passed");
}
