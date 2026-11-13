#include "unicode/udatpg.h"
#include "unicode/uloc.h"
#include "unicode/unorm2.h"
#include "unicode/ures.h"
#include "unicode/utypes.h"
#include "ureslocs.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

using Bundle = std::unique_ptr<UResourceBundle, decltype(&ures_close)>;
using Generator = std::unique_ptr<UDateTimePatternGenerator, decltype(&udatpg_close)>;

Bundle Open(const char* package, const char* locale, bool direct = false) {
    UErrorCode status = U_ZERO_ERROR;
    Bundle result(direct ? ures_openDirect(package, locale, &status) : ures_open(package, locale, &status), ures_close);
    if (!result || U_FAILURE(status) || status == U_USING_DEFAULT_WARNING)
        std::fprintf(stderr, "native resource: package=%s locale=%s status=%s\n", package ? package : "default", locale, u_errorName(status));
    Require(result && U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING, "native data bundle available without default-locale substitution");
    return result;
}

void Key(const UResourceBundle* bundle, const char* name) {
    UErrorCode status = U_ZERO_ERROR;
    Bundle result(ures_getByKey(bundle, name, nullptr, &status), ures_close);
    Require(result && U_SUCCESS(status), name);
}

void Pattern(const char* locale, const char16_t* skeleton, std::u16string_view expected) {
    UErrorCode status = U_ZERO_ERROR;
    Generator generator(udatpg_open(locale, &status), udatpg_close);
    if (!generator || U_FAILURE(status) || status == U_USING_DEFAULT_WARNING)
        std::fprintf(stderr, "native generator: locale=%s status=%s\n", locale, u_errorName(status));
    Require(generator && U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING, "native generator opens from complete locale data");
    std::array<char16_t, 128> output{};
    const auto count = udatpg_getBestPattern(generator.get(), skeleton, -1, output.data(), output.size(), &status);
    Require(U_SUCCESS(status) && count == static_cast<std::int32_t>(expected.size()) &&
        std::u16string_view(output.data(), count) == expected, locale);
}

void Field(const char* locale, std::u16string_view expected) {
    UErrorCode status = U_ZERO_ERROR;
    Generator generator(udatpg_open(locale, &status), udatpg_close);
    Require(generator && U_SUCCESS(status), "native field generator opens");
    std::array<char16_t, 32> output{};
    const auto count = udatpg_getFieldDisplayName(generator.get(), UDATPG_YEAR_FIELD, UDATPG_WIDE, output.data(), output.size(), &status);
    Require(U_SUCCESS(status) && count == static_cast<std::int32_t>(expected.size()) &&
        std::u16string_view(output.data(), count) == expected, locale);
}

void Resources() {
    for (const auto* locale : {"root", "en", "de", "ja", "fr"}) {
        auto bundle = Open(nullptr, locale);
        for (const auto* key : {"calendar", "fields", "NumberElements"}) Key(bundle.get(), key);
        auto currency = Open(U_ICUDATA_CURR, locale);
        Key(currency.get(), "currencySpacing");
        auto zone = Open(U_ICUDATA_ZONE, locale);
        Key(zone.get(), "zoneStrings");
    }
    auto supplemental = Open(nullptr, "supplementalData", true);
    Key(supplemental.get(), "timeData");
    for (const auto* file : {"numberingSystems", "langInfo", "keyTypeData", "timezoneTypes", "zoneinfo64", "metaZones", "windowsZones"}) Open(nullptr, file, true);
    Open(U_ICUDATA_CURR, "supplementalData", true);
    Open(U_ICUDATA_ZONE, "tzdbNames", true);
    UErrorCode status = U_ZERO_ERROR;
    std::array<char, 64> maximized{};
    Require(uloc_addLikelySubtags("ja", maximized.data(), maximized.size(), &status) == 10 &&
        U_SUCCESS(status) && std::string_view(maximized.data()) == "ja_Jpan_JP", "complete source likely-subtags data");
    status = U_ZERO_ERROR;
    const auto* nfc = unorm2_getNFCInstance(&status);
    std::array<char16_t, 4> normalized{};
    Require(nfc && U_SUCCESS(status) && unorm2_normalize(nfc, u"e\u0301", 2, normalized.data(), normalized.size(), &status) == 1 &&
        U_SUCCESS(status) && normalized[0] == u'\u00e9', "compiled-in NFC data remains available");
}

} // namespace

int main() {
    try {
        Resources();
        Pattern("en_US", u"yMd", u"M/d/y");
        Pattern("de_DE", u"yMd", u"d.M.y");
        Pattern("ja_JP", u"yMd", u"y/M/d");
        Pattern("ja_JP@calendar=japanese", u"GyMd", u"GGGGGy/M/d");
        Field("en", u"year");
        Field("de", u"Jahr");
        Field("ja", u"年");
        Field("fr", u"année");
        std::puts("native pattern fixtures and complete calendar/currency/zone resources passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "native pattern data failure: %s\n", error.what());
        return 1;
    }
}
