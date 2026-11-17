#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucpmap.h"
#include "Testing.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

void ExpectName(const char* actual, const char* expected, const char* what) {
    if (expected ? actual && std::strcmp(actual, expected) == 0 : !actual) return;
    std::fprintf(stderr, "%s returned %s, expected %s\n", what, actual ? actual : "null", expected ? expected : "null");
    std::abort();
}

struct Range {
    std::int32_t start;
    std::int32_t limit;
    std::int32_t type;
};

struct Ranges {
    std::vector<Range> seen;
    std::size_t stopAfter;
};

std::int8_t APS5_VABI Collect(const void* context, std::int32_t start, std::int32_t limit, std::int32_t type) {
    auto& ranges = *static_cast<Ranges*>(const_cast<void*>(context));
    ranges.seen.push_back({start, limit, type});
    return ranges.seen.size() < ranges.stopAfter;
}

void TestCategories() {
    ExpectInt(u_charType_nid_postfix(0x41), 1, "u_charType(A)");
    ExpectInt(u_charType_nid_postfix(0x61), 2, "u_charType(a)");
    ExpectInt(u_charType_nid_postfix(0x1c5), 3, "u_charType(U+01C5)");
    ExpectInt(u_charType_nid_postfix(0x4e00), 5, "u_charType(U+4E00)");
    ExpectInt(u_charType_nid_postfix(0x301), 6, "u_charType(U+0301)");
    ExpectInt(u_charType_nid_postfix(0x30), 9, "u_charType(0)");
    ExpectInt(u_charType_nid_postfix(0x20), 12, "u_charType(space)");
    ExpectInt(u_charType_nid_postfix(0xe000), 17, "u_charType(U+E000)");
    ExpectInt(u_charType_nid_postfix(0xd800), 18, "u_charType(U+D800)");
    ExpectInt(u_charType_nid_postfix(0x1f600), 27, "u_charType(U+1F600)");
    ExpectInt(u_charType_nid_postfix(0x20c1), 25, "u_charType(U+20C1)");
    ExpectInt(u_charType_nid_postfix(0x378), 0, "u_charType(U+0378)");
    ExpectInt(u_charType_nid_postfix(0x110000), 0, "u_charType(U+110000)");
    ExpectInt(u_charType_nid_postfix(-1), 0, "u_charType(-1)");

    Ranges all{{}, static_cast<std::size_t>(-1)};
    u_enumCharTypes_nid_postfix(Collect, &all);
    Require(!all.seen.empty(), "u_enumCharTypes reports ranges");
    Require(all.seen.front().start == 0 && all.seen.front().limit == 0x20 && all.seen.front().type == 15, "first range is Cc");
    Require(all.seen.back().limit == 0x110000, "last range ends at U+110000");
    for (std::size_t index = 1; index < all.seen.size(); ++index)
        Require(all.seen[index].start == all.seen[index - 1].limit, "ranges are contiguous");
    Ranges two{{}, 2};
    u_enumCharTypes_nid_postfix(Collect, &two);
    ExpectInt(static_cast<std::int64_t>(two.seen.size()), 2, "u_enumCharTypes stops when the callback returns false");
    u_enumCharTypes_nid_postfix(nullptr, nullptr);
}

void TestClassifiers() {
    Require(u_islower_nid_postfix(0x61) && !u_islower_nid_postfix(0x41), "u_islower");
    Require(u_isupper_nid_postfix(0x41) && !u_isupper_nid_postfix(0x61), "u_isupper");
    Require(u_istitle_nid_postfix(0x1c5) && !u_istitle_nid_postfix(0x41), "u_istitle");
    Require(u_isdigit_nid_postfix(0x660) && !u_isdigit_nid_postfix(0x41), "u_isdigit");
    Require(u_isxdigit_nid_postfix(0xff21) && !u_isxdigit_nid_postfix(0x67), "u_isxdigit");
    Require(u_isalpha_nid_postfix(0x4e00) && !u_isalpha_nid_postfix(0x345), "u_isalpha");
    Require(u_isalnum_nid_postfix(0x35) && !u_isalnum_nid_postfix(0x21), "u_isalnum");
    Require(u_isdefined_nid_postfix(0x41) && !u_isdefined_nid_postfix(0x378), "u_isdefined");
    Require(u_isbase_nid_postfix(0x20dd) && !u_isbase_nid_postfix(0x301), "u_isbase");
    Require(u_iscntrl_nid_postfix(0x7) && u_iscntrl_nid_postfix(0x2028) && !u_iscntrl_nid_postfix(0x41), "u_iscntrl");
    Require(u_ispunct_nid_postfix(0x21) && !u_ispunct_nid_postfix(0x24), "u_ispunct");
    Require(u_isgraph_nid_postfix(0x41) && !u_isgraph_nid_postfix(0x20), "u_isgraph");
    Require(u_isprint_nid_postfix(0x20) && !u_isprint_nid_postfix(0x7), "u_isprint");
    Require(u_isIDStart_nid_postfix(0x61) && !u_isIDStart_nid_postfix(0x31), "u_isIDStart");
    Require(u_isIDPart_nid_postfix(0x31) && !u_isIDPart_nid_postfix(0x2d), "u_isIDPart");
    Require(u_isIDIgnorable_nid_postfix(0x0) && u_isIDIgnorable_nid_postfix(0x200b) && !u_isIDIgnorable_nid_postfix(0x9), "u_isIDIgnorable");
    Require(u_isJavaIDStart_nid_postfix(0x24) && !u_isJavaIDStart_nid_postfix(0x31), "u_isJavaIDStart");
    Require(u_isJavaIDPart_nid_postfix(0x31) && !u_isJavaIDPart_nid_postfix(0x2d), "u_isJavaIDPart");
    Require(u_isUAlphabetic_nid_postfix(0x345) && !u_isUAlphabetic_nid_postfix(0x31), "u_isUAlphabetic");
    Require(u_isULowercase_nid_postfix(0x61) && !u_isULowercase_nid_postfix(0x1c5), "u_isULowercase");
    Require(u_isUUppercase_nid_postfix(0x41) && !u_isUUppercase_nid_postfix(0x1c5), "u_isUUppercase");
}

void TestNumbers() {
    ExpectInt(u_charDigitValue_nid_postfix(0x663), 3, "u_charDigitValue(U+0663)");
    ExpectInt(u_charDigitValue_nid_postfix(0x41), -1, "u_charDigitValue(A)");
    ExpectInt(u_digit_nid_postfix(0x7a, 36), 35, "u_digit(z, 36)");
    ExpectInt(u_digit_nid_postfix(0xff21, 16), 10, "u_digit(U+FF21, 16)");
    ExpectInt(u_digit_nid_postfix(0x38, 8), -1, "u_digit(8, 8)");
    ExpectInt(u_digit_nid_postfix(0x30, 1), -1, "u_digit(0, 1)");
    Require(u_getNumericValue_nid_postfix(0xbd) == 0.5, "u_getNumericValue(U+00BD)");
    Require(u_getNumericValue_nid_postfix(0x216b) == 12.0, "u_getNumericValue(U+216B)");
    Require(u_getNumericValue_nid_postfix(0x41) == -123456789.0, "u_getNumericValue(A)");
    std::uint8_t version[4] = {9, 9, 9, 9};
    u_charAge_nid_postfix(0x41, version);
    Require(version[0] == 1 && version[1] == 1 && version[2] == 0 && version[3] == 0, "u_charAge(A)");
    u_charAge_nid_postfix(0x20ac, version);
    Require(version[0] == 2 && version[1] == 1, "u_charAge(U+20AC)");
    u_charAge_nid_postfix(0x20c1, version);
    Require(version[0] == 17 && version[1] == 0, "u_charAge(U+20C1)");
    u_charAge_nid_postfix(0x41, nullptr);
    ExpectInt(u_getCombiningClass_nid_postfix(0x301), 230, "u_getCombiningClass(U+0301)");
    ExpectInt(u_getCombiningClass_nid_postfix(0x316), 220, "u_getCombiningClass(U+0316)");
    ExpectInt(u_getCombiningClass_nid_postfix(0x41), 0, "u_getCombiningClass(A)");
}

void TestBidiAndCase() {
    ExpectInt(u_charDirection_nid_postfix(0x41), 0, "u_charDirection(A)");
    ExpectInt(u_charDirection_nid_postfix(0x5d0), 1, "u_charDirection(U+05D0)");
    ExpectInt(u_charDirection_nid_postfix(0x31), 2, "u_charDirection(1)");
    ExpectInt(u_charDirection_nid_postfix(0x627), 13, "u_charDirection(U+0627)");
    Require(u_isMirrored_nid_postfix(0x28) && !u_isMirrored_nid_postfix(0x41), "u_isMirrored");
    ExpectInt(u_charMirror_nid_postfix(0x28), 0x29, "u_charMirror(()");
    ExpectInt(u_charMirror_nid_postfix(0x226e), 0x226f, "u_charMirror(U+226E)");
    ExpectInt(u_charMirror_nid_postfix(0x41), 0x41, "u_charMirror(A)");
    ExpectInt(u_getBidiPairedBracket_nid_postfix(0x5b), 0x5d, "u_getBidiPairedBracket([)");
    ExpectInt(u_getBidiPairedBracket_nid_postfix(0x226e), 0x226e, "u_getBidiPairedBracket(U+226E)");
    ExpectInt(u_foldCase_nid_postfix(0x41, 0), 0x61, "u_foldCase(A)");
    ExpectInt(u_foldCase_nid_postfix(0x49, 1), 0x131, "u_foldCase(I, exclude special I)");
    ExpectInt(u_foldCase_nid_postfix(0x130, 0), 0x130, "u_foldCase(U+0130)");
}

void TestPropertyNames() {
    ExpectInt(u_getPropertyEnum_nid_postfix("Alphabetic"), 0, "u_getPropertyEnum(Alphabetic)");
    ExpectInt(u_getPropertyEnum_nid_postfix("gc"), 0x1005, "u_getPropertyEnum(gc)");
    ExpectInt(u_getPropertyEnum_nid_postfix("general category"), 0x1005, "u_getPropertyEnum(general category)");
    ExpectInt(u_getPropertyEnum_nid_postfix("nonsense"), -1, "u_getPropertyEnum(nonsense)");
    ExpectInt(u_getPropertyEnum_nid_postfix(nullptr), -1, "u_getPropertyEnum(null)");
    ExpectName(u_getPropertyName_nid_postfix(0x1005, 0), "gc", "u_getPropertyName(gc, short)");
    ExpectName(u_getPropertyName_nid_postfix(0x1005, 1), "General_Category", "u_getPropertyName(gc, long)");
    ExpectName(u_getPropertyName_nid_postfix(0x1005, 9), nullptr, "u_getPropertyName(gc, 9)");
    ExpectName(u_getPropertyName_nid_postfix(0x6000, 0), nullptr, "u_getPropertyName(0x6000)");
    ExpectInt(u_getPropertyValueEnum_nid_postfix(0x1005, "Lu"), 1, "u_getPropertyValueEnum(gc, Lu)");
    ExpectInt(u_getPropertyValueEnum_nid_postfix(0x100a, "Latn"), 25, "u_getPropertyValueEnum(sc, Latn)");
    ExpectInt(u_getPropertyValueEnum_nid_postfix(0x1005, nullptr), -1, "u_getPropertyValueEnum(gc, null)");
    ExpectInt(u_getPropertyValueEnum_nid_postfix(0x6000, "Lu"), -1, "u_getPropertyValueEnum(0x6000, Lu)");
    ExpectName(u_getPropertyValueName_nid_postfix(0x1005, 1, 1), "Uppercase_Letter", "u_getPropertyValueName(gc, Lu, long)");
    ExpectName(u_getPropertyValueName_nid_postfix(0x100a, 25, 0), "Latn", "u_getPropertyValueName(sc, Latin, short)");
    ExpectName(u_getPropertyValueName_nid_postfix(0x1005, 99, 0), nullptr, "u_getPropertyValueName(gc, 99)");
}

void TestPropertyValues() {
    ExpectInt(u_getIntPropertyMinValue_nid_postfix(0x1005), 0, "u_getIntPropertyMinValue(gc)");
    ExpectInt(u_getIntPropertyMinValue_nid_postfix(-5), 0, "u_getIntPropertyMinValue(-5)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0), 1, "u_getIntPropertyMaxValue(Alphabetic)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(57), 1, "u_getIntPropertyMaxValue(Emoji)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0x1005), 29, "u_getIntPropertyMaxValue(gc)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0x100f), 2, "u_getIntPropertyMaxValue(NFKC_QC)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0x6000), -1, "u_getIntPropertyMaxValue(0x6000)");

    Require(u_hasBinaryProperty_nid_postfix(0x41, 0) && !u_hasBinaryProperty_nid_postfix(0x31, 0), "u_hasBinaryProperty(Alphabetic)");
    Require(u_hasBinaryProperty_nid_postfix(0x2ffe, 72), "u_hasBinaryProperty(IDS_Unary_Operator)");
    Require(u_hasBinaryProperty_nid_postfix(0xc5, 9) == 0 && u_hasBinaryProperty_nid_postfix(0x340, 9), "u_hasBinaryProperty(Full_Composition_Exclusion)");
    Require(u_hasBinaryProperty_nid_postfix(0x41, 54) && !u_hasBinaryProperty_nid_postfix(0x61, 54), "u_hasBinaryProperty(Changes_When_Casefolded)");
    Require(!u_hasBinaryProperty_nid_postfix(0x41, 0x100f) && !u_hasBinaryProperty_nid_postfix(0x41, -1), "u_hasBinaryProperty outside binary properties");

    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0x1005), 1, "u_getIntPropertyValue(A, gc)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0x100a), 25, "u_getIntPropertyValue(A, sc)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0x2000), 2, "u_getIntPropertyValue(A, gcm)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x301, 0x1002), 230, "u_getIntPropertyValue(U+0301, ccc)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x301, 0x100e), 2, "u_getIntPropertyValue(U+0301, NFC_QC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0), 1, "u_getIntPropertyValue(A, Alphabetic)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0x6000), 0, "u_getIntPropertyValue(A, 0x6000)");
}

void TestIntPropertyMap() {
    std::int32_t status = 0;
    const UCPMap* map = u_getIntPropertyMap_nid_postfix(0x1005, &status);
    Require(map != nullptr, "u_getIntPropertyMap(gc) returns a map");
    ExpectInt(status, 0, "u_getIntPropertyMap(gc) preserves successful status");
    ExpectInt(ucpmap_get(map, 0x41), 1, "u_getIntPropertyMap(gc, A)");
    ExpectInt(ucpmap_get(map, 0x301), 6, "u_getIntPropertyMap(gc, U+0301)");

    status = 0;
    Require(u_getIntPropertyMap_nid_postfix(-1, &status) == nullptr, "u_getIntPropertyMap rejects an invalid property");
    ExpectInt(status, 1, "u_getIntPropertyMap reports illegal argument");

    status = 1;
    Require(u_getIntPropertyMap_nid_postfix(0x1005, &status) == nullptr, "u_getIntPropertyMap skips failed status");
    ExpectInt(status, 1, "u_getIntPropertyMap preserves failed status");
}

void TestDataFileProperties() {
    Require(u_hasBinaryProperty_nid_postfix(0x1f600, 57) && !u_hasBinaryProperty_nid_postfix(0x41, 57), "u_hasBinaryProperty(Emoji)");
    Require(u_hasBinaryProperty_nid_postfix(0x1f600, 58) && !u_hasBinaryProperty_nid_postfix(0x263a, 58), "u_hasBinaryProperty(Emoji_Presentation)");
    Require(u_hasBinaryProperty_nid_postfix(0x1f3fb, 59) && !u_hasBinaryProperty_nid_postfix(0x1f600, 59), "u_hasBinaryProperty(Emoji_Modifier)");
    Require(u_hasBinaryProperty_nid_postfix(0x1f44d, 60) && !u_hasBinaryProperty_nid_postfix(0x1f600, 60), "u_hasBinaryProperty(Emoji_Modifier_Base)");
    Require(u_hasBinaryProperty_nid_postfix(0x23, 61) && !u_hasBinaryProperty_nid_postfix(0x41, 61), "u_hasBinaryProperty(Emoji_Component)");
    Require(u_hasBinaryProperty_nid_postfix(0xa9, 64) && !u_hasBinaryProperty_nid_postfix(0x41, 64), "u_hasBinaryProperty(Extended_Pictographic)");
    Require(u_hasBinaryProperty_nid_postfix(0x1f600, 65) && !u_hasBinaryProperty_nid_postfix(0x41, 65), "u_hasBinaryProperty(Basic_Emoji)");
    Require(u_hasBinaryProperty_nid_postfix(0x1f600, 71) && !u_hasBinaryProperty_nid_postfix(0x1f1e6, 68), "u_hasBinaryProperty(RGI_Emoji)");
    Require(u_hasBinaryProperty_nid_postfix(0x41, 56) && !u_hasBinaryProperty_nid_postfix(0x61, 56), "u_hasBinaryProperty(Changes_When_NFKC_Casefolded)");

    ExpectInt(u_getIntPropertyValue_nid_postfix(0x1f600, 57), 1, "u_getIntPropertyValue(U+1F600, Emoji)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0xfb01, 0x100f), 0, "u_getIntPropertyValue(U+FB01, NFKC_QC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0x100f), 1, "u_getIntPropertyValue(A, NFKC_QC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x93f, 0x1016), 4, "u_getIntPropertyValue(U+093F, InPC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x915, 0x1016), 0, "u_getIntPropertyValue(U+0915, InPC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x915, 0x1017), 5, "u_getIntPropertyValue(U+0915, InSC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x93f, 0x1017), 34, "u_getIntPropertyValue(U+093F, InSC)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x4e00, 0x1018), 3, "u_getIntPropertyValue(U+4E00, vo)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x3001, 0x1018), 2, "u_getIntPropertyValue(U+3001, vo)");
    ExpectInt(u_getIntPropertyValue_nid_postfix(0x41, 0x1018), 0, "u_getIntPropertyValue(A, vo)");

    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0x1016), 15, "u_getIntPropertyMaxValue(InPC)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0x1017), 36, "u_getIntPropertyMaxValue(InSC)");
    ExpectInt(u_getIntPropertyMaxValue_nid_postfix(0x1018), 3, "u_getIntPropertyMaxValue(vo)");

    std::int32_t status = 0;
    const UCPMap* insc = u_getIntPropertyMap_nid_postfix(0x1017, &status);
    Require(insc != nullptr && status == 0, "u_getIntPropertyMap(InSC) returns a map");
    ExpectInt(ucpmap_get(insc, 0x915), 5, "u_getIntPropertyMap(InSC, U+0915)");
    ExpectInt(ucpmap_get(insc, 0x41), 0, "u_getIntPropertyMap(InSC, A)");
    const UCPMap* vo = u_getIntPropertyMap_nid_postfix(0x1018, &status);
    Require(vo != nullptr && status == 0, "u_getIntPropertyMap(vo) returns a map");
    ExpectInt(ucpmap_get(vo, 0x4e00), 3, "u_getIntPropertyMap(vo, U+4E00)");
    const UCPMap* nfkc = u_getIntPropertyMap_nid_postfix(0x100f, &status);
    Require(nfkc != nullptr && status == 0, "u_getIntPropertyMap(NFKC_QC) returns a map");
    ExpectInt(ucpmap_get(nfkc, 0xfb01), 0, "u_getIntPropertyMap(NFKC_QC, U+FB01)");
}

}

int main() {
    TestCategories();
    TestClassifiers();
    TestNumbers();
    TestBidiAndCase();
    TestPropertyNames();
    TestPropertyValues();
    TestIntPropertyMap();
    TestDataFileProperties();
    std::puts("ICU property tests passed");
}
