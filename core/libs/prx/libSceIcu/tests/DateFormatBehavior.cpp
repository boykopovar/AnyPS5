#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/DateFormat.hpp"
#include "prx/libSceIcu/FieldPositionIterator.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "unicode/udat.h"
#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using GuestFormat = std::unique_ptr<UDateFormat, decltype(&udat_close_nid_postfix)>;
using ReferenceFormat = std::unique_ptr<UDateFormat, decltype(&udat_close)>;

struct AllocatorCounts {
    mutable unsigned allocations = 0;
    mutable unsigned releases = 0;
};

void* APS5_VABI Allocate_nid_no_patch(const void* context, std::size_t size) {
    ++static_cast<const AllocatorCounts*>(context)->allocations;
    return std::malloc(size);
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void* pointer, std::size_t size) {
    return std::realloc(pointer, size);
}

void APS5_VABI Release_nid_no_patch(const void* context, void* pointer) {
    if (pointer) ++static_cast<const AllocatorCounts*>(context)->releases;
    std::free(pointer);
}

void GuestAllocator() {
    u_cleanup_nid_postfix();
    AllocatorCounts counts;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&counts, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0);
    const auto beforeOpen = counts.allocations;
    GuestFormat format(udat_open_nid_postfix(UDAT_SHORT, UDAT_SHORT, "en_US", u"UTC", 3, nullptr, 0, &status),
                       &udat_close_nid_postfix);
    Require(format && status <= 0 && counts.allocations > beforeOpen);
    const auto beforeClose = counts.releases;
    format.reset();
    Require(counts.releases > beforeClose);
    u_cleanup_nid_postfix();
}

struct Pair {
    GuestFormat guest;
    ReferenceFormat reference;
};

Pair Open(const char* locale, const char16_t* zone, int zoneLength, const char16_t* pattern, int patternLength,
          UDateFormatStyle time = UDAT_PATTERN, UDateFormatStyle date = UDAT_PATTERN) {
    std::int32_t status = 0;
    UErrorCode error = U_ZERO_ERROR;
    GuestFormat guest(udat_open_nid_postfix(time, date, locale, zone, zoneLength, pattern, patternLength, &status),
                      &udat_close_nid_postfix);
    ReferenceFormat reference(udat_open(time, date, locale, zone, zoneLength, pattern, patternLength, &error), &udat_close);
    Require(guest && reference && U_SUCCESS(error) && status == error);
    return {std::move(guest), std::move(reference)};
}

template <typename Guest, typename Reference>
void Compare(int capacity, int incoming, Guest guest, Reference reference) {
    std::array<char16_t, 512> actual;
    actual.fill(0xa55a);
    auto expected = actual;
    auto status = static_cast<std::int32_t>(incoming);
    auto error = static_cast<UErrorCode>(incoming);
    const int length = guest(capacity < 0 ? nullptr : actual.data(), capacity < 0 ? 0 : capacity, &status);
    Require(length == reference(capacity < 0 ? nullptr : expected.data(), capacity < 0 ? 0 : capacity, &error));
    Require(status == error);
    Require(actual == expected);
}

void CompareFormats(const Pair& pair, double date) {
    for (int capacity : {-1, 0, 1, 4, 9, 10, 11, 128, 512}) {
        for (int incoming : {-128, -124, 0, 15}) {
            UFieldPosition pos{UDAT_YEAR_FIELD, -43, -17};
            auto expected = pos;
            Compare(capacity, incoming,
                [&](auto* out, int size, auto* status) {
                    return udat_format_nid_postfix(pair.guest.get(), date, out, size, &pos, status);
                },
                [&](auto* out, int size, auto* error) {
                    return udat_format(pair.reference.get(), date, out, size, &expected, error);
                });
            Require(pos.field == expected.field && pos.beginIndex == expected.beginIndex && pos.endIndex == expected.endIndex);
        }
        for (bool localized : {false, true}) {
            Compare(capacity, 0,
                [&](auto* out, int size, auto* status) {
                    return udat_toPattern_nid_postfix(pair.guest.get(), localized, out, size, status);
                },
                [&](auto* out, int size, auto* error) {
                    return udat_toPattern(pair.reference.get(), localized, out, size, error);
                });
        }
    }
}

void Symbols(const Pair& pair) {
    for (int type = UDAT_ERAS; type <= UDAT_AM_PMS_WIDE; ++type) {
        const auto symbol = static_cast<UDateFormatSymbolType>(type);
        const auto count = udat_countSymbols(pair.reference.get(), symbol);
        Require(udat_countSymbols_nid_postfix(pair.guest.get(), type) == count);
        for (int index : {0, count - 1, count, count + 1}) {
            if (index < 0 && symbol != UDAT_LOCALIZED_CHARS) continue;
            for (int capacity : {-1, 0, 1, 512}) {
                for (int incoming : {0, -124, 15}) {
                    Compare(capacity, incoming,
                        [&](auto* out, int size, auto* status) {
                            return udat_getSymbols_nid_postfix(pair.guest.get(), type, index, out, size, status);
                        },
                        [&](auto* out, int size, auto* error) {
                            return udat_getSymbols(pair.reference.get(), symbol, index, out, size, error);
                        });
                }
            }
        }
    }
}

void Fields(const Pair& pair) {
    std::int32_t status = 0;
    UErrorCode error = U_ZERO_ERROR;
    auto* fields = ufieldpositer_open_nid_postfix(&status);
    auto* reference = ufieldpositer_open(&error);
    Require(fields && reference);
    for (int capacity : {-1, 4, 512}) {
        Compare(capacity, 0,
            [&](auto* out, int size, auto* ec) {
                return udat_formatForFields_nid_postfix(pair.guest.get(), 0, out, size, fields, ec);
            },
            [&](auto* out, int size, auto* ec) {
                return udat_formatForFields(pair.reference.get(), 0, out, size, reference, ec);
            });
        int count = 0;
        for (;;) {
            int begin = -1, end = -1, rb = -1, re = -1;
            const int field = ufieldpositer_next_nid_postfix(fields, &begin, &end);
            Require(field == ufieldpositer_next(reference, &rb, &re) && begin == rb && end == re);
            if (field < 0) break;
            ++count;
        }
        Require(count >= 3);
    }
    ufieldpositer_close(reference);
    ufieldpositer_close_nid_postfix(fields);
    RequireThrows([&] { udat_formatForFields_nid_postfix(pair.guest.get(), 0, nullptr, 0, fields, &status); });
}

void GuestPointers(const Pair& pair) {
    auto* bad = reinterpret_cast<char16_t*>(std::uintptr_t{1});
    std::int32_t status = 0;
    RequireThrows([&] { udat_open_nid_postfix(-2, -2, "en_US", bad, 3, u"y", 1, &status); });
    RequireThrows([&] { udat_open_nid_postfix(-2, -2, "en_US", u"UTC", 3, bad, -1, &status); });
    RequireThrows([&] { udat_open_nid_postfix(-2, -2, reinterpret_cast<const char*>(bad), nullptr, 0, nullptr, 0, &status); });
    GuestFormat ignored(udat_open_nid_postfix(UDAT_SHORT, UDAT_SHORT, "en_US", u"UTC", 3, bad, INT32_MAX, &status),
                        &udat_close_nid_postfix);
    Require(ignored && status <= 0);
    status = 0;
    RequireThrows([&] { udat_format_nid_postfix(pair.guest.get(), 0, bad, 64, nullptr, &status); });
    RequireThrows([&] { udat_format_nid_postfix(pair.guest.get(), 0, nullptr, 0,
                                               reinterpret_cast<UFieldPosition*>(bad), &status); });
    status = 0;
    Require(udat_getSymbols_nid_postfix(pair.guest.get(), UDAT_MONTHS, 0, bad, 1, &status) == 7);
    Require(status == U_BUFFER_OVERFLOW_ERROR);
    status = 0;
    RequireThrows([&] { udat_open_nid_postfix(INT32_MAX, UDAT_LONG, nullptr, nullptr, 0, nullptr, 0, &status); });
    Require(udat_countSymbols_nid_postfix(pair.guest.get(), INT32_MAX) == 0);
    Require(udat_getSymbols_nid_postfix(pair.guest.get(), UDAT_MONTHS, 100, bad, 32, nullptr) == 0);
    RequireThrows([&] { udat_getSymbols_nid_postfix(pair.guest.get(), UDAT_MONTHS, -1, nullptr, 0, &status); });
}

void KnownDate(const char* locale, const char16_t* zone, const char16_t* pattern, double instant,
               std::u16string_view expected) {
    auto pair = Open(locale, zone, -1, pattern, -1);
    std::array<char16_t, 64> out{};
    std::int32_t status = 0;
    const int length = udat_format_nid_postfix(pair.guest.get(), instant, out.data(), INT32_MAX, nullptr, &status);
    Require(status <= 0 && length == static_cast<int>(expected.size()));
    Require(std::u16string_view(out.data(), length) == expected && out[length] == 0);
}

}

void CheckIcuDateFormatBehavior() {
    GuestAllocator();
    KnownDate("en_US", u"UTC", u"yyyy-MM-dd", 0, u"1970-01-01");
    KnownDate("fr_FR", u"UTC", u"MMMM", 0, u"janvier");
    KnownDate("th_TH@calendar=buddhist", u"Asia/Bangkok", u"yyyy-MM-dd", 0, u"2513-01-01");
    KnownDate("en_US", u"America/New_York", u"HH:mm XXX", 1710053940000., u"01:59 -05:00");
    KnownDate("en_US", u"America/New_York", u"HH:mm XXX", 1710054000000., u"03:00 -04:00");

    constexpr char16_t zone[]{u'U', u'T', u'C'};
    constexpr char16_t pattern[]{u'y', u'y', u'y', u'y', u'-', u'M', u'M', u'-', u'd', u'd'};
    const auto iso = Open("en_US", zone, 3, pattern, 10);
    CompareFormats(iso, -1.);
    Fields(iso);
    GuestPointers(iso);
    const auto* calendar = udat_getCalendar_nid_postfix(iso.guest.get());
    Require(calendar && calendar == udat_getCalendar_nid_postfix(iso.guest.get()));
    UErrorCode error = U_ZERO_ERROR;
    Require(ucal_getType(calendar, &error) == std::string_view("gregorian") && U_SUCCESS(error));

    for (auto locale : {"fr_CA", "ar_EG", "ja_JP@calendar=japanese", "fa_IR"}) {
        auto pair = Open(locale, u"Europe/Paris", -1, nullptr, 0, UDAT_LONG, UDAT_FULL);
        CompareFormats(pair, 1704067200000.);
        Symbols(pair);
    }
    auto relative = Open("de_DE", u"UTC", -1, nullptr, 0, UDAT_NONE, UDAT_SHORT_RELATIVE);
    CompareFormats(relative, 0.);
    auto longPattern = std::u16string(u"'") + std::u16string(200, u'x') + u"' yyyy";
    CompareFormats(Open("en_US", u"UTC", -1, longPattern.c_str(), -1), 0.);
    constexpr char16_t embedded[]{u'y', u'\'', 0, u'\'', u'M'};
    CompareFormats(Open("en_US", nullptr, 0, embedded, 5), 0.);
    CompareFormats(Open("en_US", u"UTC", -1, u"'\uffff' yyyy/MM/dd", -1), 0.);
}
