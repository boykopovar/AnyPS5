#include "prx/libSceIcu/Converter.hpp"
#include "unicode/ucnv.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucnv_open_nid_postfix), UConverter* (APS5_VABI*)(const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ucnv_setFallback_nid_postfix), void (APS5_VABI*)(UConverter*, std::int8_t)>);
static_assert(std::is_same_v<decltype(&ucnv_convertEx_nid_postfix),
    void (APS5_VABI*)(UConverter*, UConverter*, char**, const char*, const char**, const char*,
                       char16_t*, char16_t**, char16_t**, const char16_t*, std::int8_t, std::int8_t, std::int32_t*)>);

namespace {

constexpr char Utf8[] = "A\xe2\x82\xac\xf0\x9f\x98\x80";
constexpr char16_t Utf16[] = u"A\u20ac\U0001f600";

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

void ExpectStatus(std::int32_t guest, UErrorCode native, const char* message) {
    if (guest == static_cast<std::int32_t>(native)) return;
    std::fprintf(stderr, "%s: guest status %d, native status %d\n", message, guest, static_cast<int>(native));
    std::abort();
}

struct Converters {
    UConverter* guest = nullptr;
    UConverter* native = nullptr;

    explicit Converters(const char* name) {
        std::int32_t guestStatus = 0;
        UErrorCode nativeStatus = U_ZERO_ERROR;
        guest = ucnv_open_nid_postfix(name, &guestStatus);
        native = ucnv_open(name, &nativeStatus);
        ExpectStatus(guestStatus, nativeStatus, "open");
        Expect(guest && native, "a built-in converter opens");
        const char* guestName = ucnv_getName_nid_postfix(guest, &guestStatus);
        const char* nativeName = ucnv_getName(native, &nativeStatus);
        ExpectStatus(guestStatus, nativeStatus, "getName");
        Expect(guestName && nativeName && std::strcmp(guestName, nativeName) == 0, "canonical converter name");
    }

    ~Converters() {
        ucnv_close_nid_postfix(guest);
        ucnv_close(native);
    }

    Converters(const Converters&) = delete;
    Converters& operator=(const Converters&) = delete;
};

void TestOpenAndStatus() {
    Converters utf8("UTF-8");
    Converters utf16("UTF-16LE");
    std::int32_t guestStatus = 0;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    auto* guest = ucnv_open_nid_postfix("x-unknown-icu-converter-8941", &guestStatus);
    auto* native = ucnv_open("x-unknown-icu-converter-8941", &nativeStatus);
    Expect(!guest && !native, "unknown encoding not silently accepted");
    ExpectStatus(guestStatus, nativeStatus, "unknown encoding status");
    Expect(guestStatus > 0, "unknown encoding fails");
    guestStatus = U_BUFFER_OVERFLOW_ERROR;
    nativeStatus = U_BUFFER_OVERFLOW_ERROR;
    Expect(!ucnv_open_nid_postfix("UTF-8", &guestStatus), "preexisting failure rejects open");
    Expect(!ucnv_open("UTF-8", &nativeStatus), "native preexisting failure rejects open");
    ExpectStatus(guestStatus, nativeStatus, "preexisting failure preservation");
    bool threw = false;
    try {
        (void)ucnv_open_nid_postfix("UTF-8", nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Expect(threw, "null status throws instead of dereferencing");
}

void TestWholeBuffer() {
    Converters converters("UTF-8");
    std::array<char16_t, 16> guest{};
    std::array<char16_t, 16> native{};
    std::int32_t guestStatus = 0;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    const auto guestLength = ucnv_toUChars_nid_postfix(converters.guest, guest.data(), static_cast<std::int32_t>(guest.size()), Utf8, -1, &guestStatus);
    const auto nativeLength = ucnv_toUChars(converters.native, native.data(), static_cast<std::int32_t>(native.size()), Utf8, -1, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "toUChars");
    Expect(guestLength == nativeLength && guestLength == 4, "UTF-8 surrogate-pair result length");
    Expect(guest == native && std::u16string(guest.data()) == Utf16, "UTF-8 to Unicode content");

    std::array<char, 32> guestBytes{};
    std::array<char, 32> nativeBytes{};
    guestStatus = 0;
    nativeStatus = U_ZERO_ERROR;
    const auto guestByteLength = ucnv_fromUChars_nid_postfix(converters.guest, guestBytes.data(), static_cast<std::int32_t>(guestBytes.size()), Utf16, -1, &guestStatus);
    const auto nativeByteLength = ucnv_fromUChars(converters.native, nativeBytes.data(), static_cast<std::int32_t>(nativeBytes.size()), Utf16, -1, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "fromUChars");
    Expect(guestByteLength == nativeByteLength && guestByteLength == 8, "UTF-16 supplementary conversion length");
    Expect(guestBytes == nativeBytes && std::strcmp(guestBytes.data(), Utf8) == 0, "Unicode to UTF-8 content");

    guestStatus = 0;
    nativeStatus = U_ZERO_ERROR;
    const auto guestRequired = ucnv_toUChars_nid_postfix(converters.guest, nullptr, 0, Utf8, -1, &guestStatus);
    const auto nativeRequired = ucnv_toUChars(converters.native, nullptr, 0, Utf8, -1, &nativeStatus);
    Expect(guestRequired == nativeRequired && guestRequired == 4, "preflight length");
    ExpectStatus(guestStatus, nativeStatus, "preflight status");

    guestStatus = 0;
    nativeStatus = U_ZERO_ERROR;
    const auto shortGuest = ucnv_fromUChars_nid_postfix(converters.guest, guestBytes.data(), 2, Utf16, -1, &guestStatus);
    const auto shortNative = ucnv_fromUChars(converters.native, nativeBytes.data(), 2, Utf16, -1, &nativeStatus);
    Expect(shortGuest == shortNative, "short buffer result");
    ExpectStatus(guestStatus, nativeStatus, "short buffer status");
    Expect(guestStatus == U_BUFFER_OVERFLOW_ERROR, "short buffer overflows");
}

void TestStreamToUnicode() {
    Converters converters("UTF-8");
    const char* guestSource = Utf8;
    const char* nativeSource = Utf8;
    const char* limit = Utf8 + sizeof(Utf8) - 1;
    std::array<char16_t, 16> guest{};
    std::array<char16_t, 16> native{};
    std::array<std::int32_t, 16> guestOffsets{};
    std::array<std::int32_t, 16> nativeOffsets{};
    char16_t* guestDestination = guest.data();
    char16_t* nativeDestination = native.data();
    std::int32_t guestStatus = 0;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    ucnv_toUnicode_nid_postfix(converters.guest, &guestDestination, guest.data() + 2, &guestSource, limit,
                              guestOffsets.data(), 0, &guestStatus);
    ucnv_toUnicode(converters.native, &nativeDestination, native.data() + 2, &nativeSource, limit,
                   nativeOffsets.data(), false, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "stream decode partial status");
    Expect(guestSource - Utf8 == nativeSource - Utf8, "stream decode consumed input");
    Expect(guestDestination - guest.data() == nativeDestination - native.data(), "stream decode produced output");
    Expect(guest == native && guestOffsets == nativeOffsets, "stream decode partial data and offsets");

    guestStatus = 0;
    nativeStatus = U_ZERO_ERROR;
    ucnv_toUnicode_nid_postfix(converters.guest, &guestDestination, guest.data() + guest.size(), &guestSource, limit,
                              guestOffsets.data() + (guestDestination - guest.data()), 1, &guestStatus);
    ucnv_toUnicode(converters.native, &nativeDestination, native.data() + native.size(), &nativeSource, limit,
                   nativeOffsets.data() + (nativeDestination - native.data()), true, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "stream decode completion");
    Expect(guestSource == limit && nativeSource == limit, "stream decode completes input");
    Expect(guestDestination - guest.data() == 4, "stream decode complete length");
    Expect(guest == native && guestOffsets == nativeOffsets, "stream decode complete data");
    Expect(std::u16string(guest.data(), 4) == Utf16, "stream decode Unicode content");
    ucnv_resetToUnicode_nid_postfix(converters.guest);
    ucnv_resetToUnicode(converters.native);
}

void TestStreamFromUnicode() {
    Converters converters("UTF-8");
    const char16_t* guestSource = Utf16;
    const char16_t* nativeSource = Utf16;
    const char16_t* limit = Utf16 + 4;
    std::array<char, 16> guest{};
    std::array<char, 16> native{};
    char* guestDestination = guest.data();
    char* nativeDestination = native.data();
    std::int32_t guestStatus = 0;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    ucnv_fromUnicode_nid_postfix(converters.guest, &guestDestination, guest.data() + 3, &guestSource, limit,
                                nullptr, 0, &guestStatus);
    ucnv_fromUnicode(converters.native, &nativeDestination, native.data() + 3, &nativeSource, limit,
                     nullptr, false, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "stream encode partial status");
    Expect(guestSource - Utf16 == nativeSource - Utf16, "stream encode consumed input");
    Expect(guestDestination - guest.data() == nativeDestination - native.data(), "stream encode produced output");
    Expect(guest == native, "stream encode partial data");
    guestStatus = 0;
    nativeStatus = U_ZERO_ERROR;
    ucnv_fromUnicode_nid_postfix(converters.guest, &guestDestination, guest.data() + guest.size(), &guestSource, limit,
                                nullptr, 1, &guestStatus);
    ucnv_fromUnicode(converters.native, &nativeDestination, native.data() + native.size(), &nativeSource, limit,
                     nullptr, true, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "stream encode completion");
    Expect(guestSource == limit && nativeSource == limit, "stream encode completes input");
    Expect(guestDestination - guest.data() == 8, "stream encode complete length");
    Expect(guest == native && std::memcmp(guest.data(), Utf8, 8) == 0, "stream encode complete content");
    ucnv_resetFromUnicode_nid_postfix(converters.guest);
    ucnv_resetFromUnicode(converters.native);
}

void TestTranscodeAndOptions() {
    Converters source("UTF-8");
    Converters destination("UTF-16LE");
    std::array<char, 32> guestBytes{};
    std::array<char, 32> nativeBytes{};
    std::array<char16_t, 16> guestPivot{};
    std::array<char16_t, 16> nativePivot{};
    const char* guestSource = Utf8;
    const char* nativeSource = Utf8;
    char* guestTarget = guestBytes.data();
    char* nativeTarget = nativeBytes.data();
    char16_t* guestPivotSource = guestPivot.data();
    char16_t* guestPivotTarget = guestPivot.data();
    char16_t* nativePivotSource = nativePivot.data();
    char16_t* nativePivotTarget = nativePivot.data();
    std::int32_t guestStatus = 0;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    ucnv_convertEx_nid_postfix(destination.guest, source.guest, &guestTarget, guestBytes.data() + guestBytes.size(),
                              &guestSource, Utf8 + sizeof(Utf8) - 1, guestPivot.data(), &guestPivotSource,
                              &guestPivotTarget, guestPivot.data() + guestPivot.size(), 1, 1, &guestStatus);
    ucnv_convertEx(destination.native, source.native, &nativeTarget, nativeBytes.data() + nativeBytes.size(),
                   &nativeSource, Utf8 + sizeof(Utf8) - 1, nativePivot.data(), &nativePivotSource,
                   &nativePivotTarget, nativePivot.data() + nativePivot.size(), true, true, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "convertEx");
    Expect(guestSource - Utf8 == nativeSource - Utf8, "convertEx source position");
    Expect(guestTarget - guestBytes.data() == nativeTarget - nativeBytes.data(), "convertEx output length");
    Expect(guestBytes == nativeBytes && guestTarget - guestBytes.data() == 8, "UTF-16LE cross-encoding bytes");
    constexpr std::array<unsigned char, 8> expected = {0x41, 0, 0xac, 0x20, 0x3d, 0xd8, 0, 0xde};
    Expect(std::memcmp(guestBytes.data(), expected.data(), expected.size()) == 0, "UTF-16LE canonical bytes");

    ucnv_setFallback_nid_postfix(source.guest, 1);
    ucnv_setFallback(source.native, true);
    Expect(ucnv_usesFallback(source.guest) && ucnv_usesFallback(source.native), "fallback enabled");
    ucnv_setFallback_nid_postfix(source.guest, 0);
    ucnv_setFallback(source.native, false);
    Expect(!ucnv_usesFallback(source.guest) && !ucnv_usesFallback(source.native), "fallback disabled");

    guestStatus = 0;
    nativeStatus = U_ZERO_ERROR;
    ucnv_setSubstChars_nid_postfix(source.guest, "?", 1, &guestStatus);
    ucnv_setSubstChars(source.native, "?", 1, &nativeStatus);
    ExpectStatus(guestStatus, nativeStatus, "substitution setting");
    ucnv_reset_nid_postfix(source.guest);
    ucnv_reset(source.native);
}

}

int main() {
    TestOpenAndStatus();
    TestWholeBuffer();
    TestStreamToUnicode();
    TestStreamFromUnicode();
    TestTranscodeAndOptions();
}
