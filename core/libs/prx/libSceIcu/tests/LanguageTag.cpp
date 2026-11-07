#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uloc_forLanguageTag),
                             std::int32_t (APS5_VABI*)(const char*, char*, std::int32_t, std::int32_t*, std::int32_t*)>);

namespace {

constexpr std::int32_t kIllegalArgument = 1;
constexpr std::int32_t kBufferOverflow = 15;
constexpr std::int32_t kNotTerminated = -124;

struct Vector {
    const char* tag;
    const char* locale;
    std::int32_t parsed;
};

constexpr Vector kVectors[] = {
    {"en", "en", 2},
    {"en-us", "en_US", 5},
    {"und-US", "_US", 6},
    {"und-latn", "_Latn", 8},
    {"en-US-posix", "en_US_POSIX", 11},
    {"de-de_euro", "de", 2},
    {"kok-IN", "kok_IN", 6},
    {"123", "", 0},
    {"en_us", "", 0},
    {"en-latn-x", "en_Latn", 7},
    {"art-lojban", "jbo", 10},
    {"zh-hakka", "hak", 8},
    {"zh-cmn-CH", "cmn_CH", 9},
    {"xxx-yy", "xxx_YY", 6},
    {"fr-234", "fr_234", 6},
    {"i-default", "en@x=i-default", 9},
    {"i-test", "", 0},
    {"ja-jp-jp", "ja_JP", 5},
    {"bogus", "bogus", 5},
    {"boguslang", "", 0},
    {"EN-lATN-us", "en_Latn_US", 10},
    {"und-variant-1234", "__1234_VARIANT", 16},
    {"ja-9876-5432", "ja__5432_9876", 12},
    {"en-US-varianta-variantb", "en_US_VARIANTA_VARIANTB", 23},
    {"en-US-variantb-varianta", "en_US_VARIANTA_VARIANTB", 23},
    {"sl-rozaj-1994-biske", "sl__1994_BISKE_ROZAJ", 19},
    {"sl-biske-1994-rozaj", "sl__1994_BISKE_ROZAJ", 19},
    {"sl-1994-rozaj-biske", "sl__1994_BISKE_ROZAJ", 19},
    {"sl-rozaj-biske-1994", "sl__1994_BISKE_ROZAJ", 19},
    {"en-fonipa-scouse", "en__FONIPA_SCOUSE", 16},
    {"en-scouse-fonipa", "en__FONIPA_SCOUSE", 16},
    {"und-varzero-var1-vartwo", "__VARZERO", 11},
    {"und-a-abc-def", "und@a=abc-def", 13},
    {"x-elmer", "@x=elmer", 7},
    {"ar-x-1-2-3", "ar@x=1-2-3", 10},
    {"de-1901-1901", "de__1901", 7},
    {"de-DE-1901-1901", "de_DE_1901", 10},
    {"en-a-bbb-a-ccc", "en@a=bbb", 8},
    {"en-a-bar-u-baz", "en@a=bar;attribute=baz", 14},
    {"en-a-bar-u-baz-x-u-foo", "en@a=bar;attribute=baz;x=u-foo", 22},
    {"en-u-baz", "en@attribute=baz", 8},
    {"hant-cmn-cn", "hant", 4},
    {"zh-cmn-TW", "cmn_TW", 9},
    {"zh-x_t-ab", "zh", 2},
    {"en-gb-oed", "en_GB_OXENDICT", 9},
    {"i-navajo", "nv", 8},
    {"i-navajo-a-foo", "nv@a=foo", 14},
    {"i-navajo-latn-us", "nv_Latn_US", 16},
    {"sgn-br", "bzs", 6},
    {"ja-latn-hepburn-heploc", "ja_Latn__ALALC97", 22},
    {"en-a-bcde-0-fgh", "en@0=fgh;a=bcde", 15},
    {"", "", 0},
    {"e", "", 0},
    {"-", "", 0},
    {"en-", "en", 2},
    {"en--US", "en", 2},
    {"en-x", "en", 2},
    {"en-x-", "en", 2},
    {"en-a", "en", 2},
    {"en-a-b", "en", 2},
    {"en-x-lvariant-foo", "en__FOO", 17},
    {"en-x-foo-lvariant-bar", "en__BAR", 21},
    {"en-US-x-lvariant-foo-bar", "en_US_BAR_FOO", 24},
    {"en-x-lvariantx-foo", "en__FOO", 18},
    {"no-bok", "nb", 6},
    {"no-bok-x-a", "nb@x=a", 10},
    {"NO-BOK", "nb", 6},
    {"zh-min-nan-TW", "nan_TW", 13},
    {"zh-min-nan", "nan", 10},
    {"zh-min", "nan@x=zh-min", 6},
    {"zh-minx", "zh_Minx", 7},
    {"sgn-be-fr-x-a", "sfb@x=a", 13},
    {"SGN-us-latn", "ase_Latn", 11},
    {"zh-cmn-hans", "cmn_Hans", 11},
    {"zh-cmn-hans-cn", "cmn_Hans_CN", 14},
    {"zh-yue-HK", "yue_HK", 9},
    {"abc-def-ghi-jkl-mno", "def", 15},
    {"abc-def-ghi-jkl", "def", 15},
    {"abc-def-ghi", "def", 11},
    {"en-US-x-abc-defghijkl", "en_US@x=abc", 11},
    {"en-US-x-abc-", "en_US@x=abc", 11},
    {"en-x-a-b-c", "en@x=a-b-c", 10},
    {"en-b-ab-a-cd", "en@a=cd;b=ab", 12},
    {"en-a-cd-b-ab", "en@a=cd;b=ab", 12},
    {"en-a-cd-a-ab", "en@a=cd", 7},
    {"en-a-bcde-x-fgh", "en@a=bcde;x=fgh", 15},
    {"en-a-x-b", "en", 2},
    {"en-u-attr2-attr1", "en@attribute=attr1-attr2", 16},
    {"en-u-attr1-attr1", "en@attribute=attr1", 16},
    {"en-u-abcdefg1-abcdefg2-abcdefg3-abcdefg4-abcdefg5-abcdefg6-abcdefg7-abcdefg8-abcdefg9-abcdefga-abcdefgb", "en@attribute=abcdefg1-abcdefg2-abcdefg3-abcdefg4-abcdefg5-abcdefg6-abcdefg7-abcdefg8-abcdefg9-abcdefga-abcdefgb", 103},
    {"en-US-u-attr1-attr2", "en_US@attribute=attr1-attr2", 19},
    {"en-t-ja-latn", "en@t=ja-latn", 12},
    {"en-t-ja-latn-u-attr", "en@attribute=attr;t=ja-latn", 19},
    {"en-t-ja-latn-u-attr-x-priv", "en@attribute=attr;t=ja-latn;x=priv", 26},
    {"und-t-ab", "und@t=ab", 8},
    {"und-x-priv", "@x=priv", 10},
    {"und-Latn-x-priv", "_Latn@x=priv", 15},
    {"en-fonipa-u-attr", "en__FONIPA@attribute=attr", 16},
    {"a", "", 0},
    {"ab", "ab", 2},
    {"abcdefghi", "", 0},
    {"abcdefgh", "abcdefgh", 8},
    {"en-abcdefghi", "en", 2},
    {"en-Latn-Latn", "en_Latn", 7},
    {"en-US-US", "en_US", 5},
    {"en-US-Latn", "en_US", 5},
    {"en-latn-us-posix-posix", "en_Latn_US_POSIX", 16},
    {"en-posix-POSIX", "en__POSIX", 8},
    {"en-12345", "en__12345", 8},
    {"en-1234", "en__1234", 7},
    {"en-123", "en_123", 6},
    {"en-1abc", "en__1ABC", 7},
    {"en-a1bc", "en", 2},
    {"en-abcd1", "en__ABCD1", 8},
    {"en-fonipa-t-ab", "en__FONIPA_POSIX@t=ab", 14},
    {"en-US-posix-a-foo", "en_US_POSIX_POSIX@a=foo", 17},
    {"en-US-posix-x-priv", "en_US_POSIX_POSIX@x=priv", 18},
    {"en-1994-a-foo", "en__1994_POSIX@a=foo", 13},
    {"en-US-a-foo-x-lvariant-bar", "en_US_BAR_POSIX@a=foo", 26},
    {"und-variant-u-attr", "__VARIANT@attribute=attr", 18},
    {"und-1994-u-attr", "__1994@attribute=attr", 15},
    {"und-US-u-attr", "_US@attribute=attr", 13},
};

[[noreturn]] void Fail(const char* tag, const char* what) {
    std::fprintf(stderr, "uloc_forLanguageTag \"%s\": %s\n", tag, what);
    std::abort();
}

void CheckVectors() {
    for (const Vector& vector : kVectors) {
        char buffer[256];
        std::memset(buffer, 'Z', sizeof(buffer));
        std::int32_t parsed = -7;
        std::int32_t status = 0;
        const std::int32_t length = uloc_forLanguageTag(vector.tag, buffer, sizeof(buffer), &parsed, &status);
        if (status != 0) Fail(vector.tag, "unexpected status");
        if (length != static_cast<std::int32_t>(std::strlen(vector.locale))) Fail(vector.tag, "wrong length");
        if (std::strcmp(buffer, vector.locale) != 0) Fail(vector.tag, buffer);
        if (parsed != vector.parsed) Fail(vector.tag, "wrong parsed length");
    }
}

void CheckCapacity() {
    const char* tag = "en-US-varianta-variantb";
    const char expected[] = "en_US_VARIANTA_VARIANTB";
    const std::int32_t full = sizeof(expected) - 1;

    std::int32_t status = 0;
    std::int32_t parsed = -7;
    if (uloc_forLanguageTag(tag, nullptr, 0, &parsed, &status) != full) Fail(tag, "preflight length");
    if (status != kBufferOverflow || parsed != 23) Fail(tag, "preflight status");

    char exact[full];
    std::memset(exact, 'Z', sizeof(exact));
    status = 0;
    if (uloc_forLanguageTag(tag, exact, full, &parsed, &status) != full) Fail(tag, "exact length");
    if (status != kNotTerminated || std::memcmp(exact, expected, full) != 0) Fail(tag, "exact capacity");

    char roomy[full + 1];
    std::memset(roomy, 'Z', sizeof(roomy));
    status = kNotTerminated;
    if (uloc_forLanguageTag(tag, roomy, full + 1, &parsed, &status) != full) Fail(tag, "terminated length");
    if (status != 0 || std::strcmp(roomy, expected) != 0) Fail(tag, "terminated capacity");

    char shortBuffer[full - 1];
    std::memset(shortBuffer, 'Z', sizeof(shortBuffer));
    status = 0;
    if (uloc_forLanguageTag(tag, shortBuffer, full - 1, &parsed, &status) != full) Fail(tag, "short length");
    if (status != kBufferOverflow || std::memcmp(shortBuffer, expected, full - 1) != 0) Fail(tag, "short capacity");

    char negative[4];
    status = 0;
    if (uloc_forLanguageTag("en", negative, -1, &parsed, &status) != 2 || status != kBufferOverflow) Fail("en", "negative capacity");

    status = 0;
    if (uloc_forLanguageTag("", nullptr, 0, &parsed, &status) != 0 || status != kNotTerminated) Fail("", "empty exact");
    status = 0;
    if (uloc_forLanguageTag("en", roomy, full + 1, nullptr, &status) != 2 || status != 0) Fail("en", "null parsed length");
}

void CheckStatus() {
    char buffer[16];
    std::memset(buffer, 'Z', sizeof(buffer));
    std::int32_t parsed = -7;
    std::int32_t status = kIllegalArgument;
    if (uloc_forLanguageTag("en-US", buffer, sizeof(buffer), &parsed, &status) != 0) Fail("en-US", "failure status length");
    if (status != kIllegalArgument || parsed != -7 || buffer[0] != 'Z') Fail("en-US", "failure status touched output");

    std::string attributes = "en-u";
    for (const char* attribute : {"abcdefg1", "abcdefg2", "abcdefg3", "abcdefg4", "abcdefg5", "abcdefg6",
                                  "abcdefg7", "abcdefg8", "abcdefg9", "abcdefga", "abcdefgb", "abcdefgc"})
        attributes += std::string("-") + attribute;
    char large[256];
    std::memset(large, 'Z', sizeof(large));
    status = 0;
    if (uloc_forLanguageTag(attributes.c_str(), large, sizeof(large), &parsed, &status) != 2) Fail("attributes", "overflow length");
    if (status != kIllegalArgument || parsed != 112) Fail("attributes", "overflow status");
    if (large[0] != 'e' || large[1] != 'n' || large[2] != 'Z') Fail("attributes", "overflow output");
}

void CheckThrows(const char* tag) {
    char buffer[64];
    std::int32_t parsed = 0;
    std::int32_t status = 0;
    try {
        uloc_forLanguageTag(tag, buffer, sizeof(buffer), &parsed, &status);
    } catch (const std::exception&) {
        return;
    }
    Fail(tag, "did not throw");
}

void CheckInvalidArguments() {
    char buffer[8];
    std::int32_t parsed = 0;
    std::int32_t status = 0;
    try {
        uloc_forLanguageTag(nullptr, buffer, sizeof(buffer), &parsed, &status);
        Fail("null", "null tag did not throw");
    } catch (const std::invalid_argument&) {
    }
    try {
        uloc_forLanguageTag("en", nullptr, 4, &parsed, &status);
        Fail("en", "null buffer did not throw");
    } catch (const std::invalid_argument&) {
    }
    try {
        uloc_forLanguageTag("en", buffer, sizeof(buffer), &parsed, nullptr);
        Fail("en", "null status did not throw");
    } catch (const std::exception&) {
    }
}

}

int main() {
    CheckVectors();
    CheckCapacity();
    CheckStatus();
    CheckThrows("en-u-ca-gregory");
    CheckThrows("en-u-baz-ca-gregory");
    CheckThrows("und-u-em-emoji");
    CheckThrows("en-US-u-va-posix");
    CheckInvalidArguments();
    std::puts("ICU language tag tests passed");
}
