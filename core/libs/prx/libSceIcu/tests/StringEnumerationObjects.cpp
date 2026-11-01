// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <cstring>

namespace {
void Check(bool value) { if (!value) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    const char* strings[]{"first", "", "third"};
    std::int32_t status = 0;
    auto* adopted = uenum_openCharStringsEnumeration_nid_postfix(strings, 3, &status);
    auto* object = _ZN6icu_6818UStringEnumeration16fromUEnumerationEP12UEnumerationR10UErrorCode_nid_postfix(adopted, &status);
    Check(object && status == 0);
    Check(_ZNK6icu_6818UStringEnumeration5countER10UErrorCode_nid_postfix(object, &status) == 3);
    auto* text = _ZN6icu_6818UStringEnumeration5snextER10UErrorCode_nid_postfix(object, &status);
    Check(text && _ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(text) == 5 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(text, 0) == u'f');
    std::int32_t length = -1;
    auto* narrow = _ZN6icu_6817StringEnumeration4nextEPiR10UErrorCode_nid_postfix(&object->base, &length, &status);
    Check(narrow && length == 0 && narrow[0] == 0);
    auto* wide = _ZN6icu_6817StringEnumeration5unextEPiR10UErrorCode_nid_postfix(&object->base, &length, &status);
    Check(wide && length == 5 && wide[4] == u'd' && wide[5] == 0);
    Check(_ZN6icu_6818UStringEnumeration5snextER10UErrorCode_nid_postfix(object, &status) == nullptr);
    _ZN6icu_6818UStringEnumeration5resetER10UErrorCode_nid_postfix(object, &status);
    Check(_ZNK6icu_6817StringEnumerationeqERKS0__nid_postfix(&object->base, &object->base) == 1);
    auto* wrapper = uenum_openFromStringEnumeration_nid_postfix(&object->base, &status);
    Check(wrapper && wrapper->count(wrapper, &status) == 3);
    Check(std::strcmp(wrapper->next(wrapper, &length, &status), "first") == 0 && length == 5);
    wrapper->close(wrapper);
    StringEnumeration base{};
    _ZN6icu_6817StringEnumerationC2Ev_nid_postfix(&base);
    Check(base.vtable[0] != nullptr && base.vtable[1] != nullptr);
    status = 3;
    _ZN6icu_6817StringEnumeration19ensureCharsCapacityEiR10UErrorCode_nid_postfix(&base, 100, &status);
    Check(base.chars == base.charsBuffer && base.charsCapacity == 32);
    status = 0;
    _ZN6icu_6817StringEnumeration19ensureCharsCapacityEiR10UErrorCode_nid_postfix(&base, 33, &status);
    Check(base.chars != base.charsBuffer && base.charsCapacity == 48);
    Check(_ZN6icu_6817StringEnumeration8setCharsEPKciR10UErrorCode_nid_postfix(&base, "a\0b", 3, &status) == &base.unistr);
    Check(_ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(&base.unistr) == 3 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&base.unistr, 1) == 0 && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&base.unistr, 2) == u'b');
    _ZN6icu_6817StringEnumerationD1Ev_nid_postfix(&base);
    return 0;
}
