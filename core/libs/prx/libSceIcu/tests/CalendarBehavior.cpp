#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/CalendarOracle.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>

namespace {

using IcuRecoveredTest::Require;

struct Handle {
    UCalendar* guest;
    void* native;
    Handle(const char16_t* zone, std::int32_t length, const char* locale, std::int32_t type) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        native = CalendarOracleOpen(zone, length, locale, type, &expected);
        guest = ucal_open_nid_postfix(zone, length, locale, type, &status);
        Require(guest && native && status == expected && status <= 0, "calendar open follows explicit zone/locale/type");
    }
    Handle(UCalendar* guestValue, void* nativeValue) : guest(guestValue), native(nativeValue) {}
    ~Handle() { ucal_close_nid_postfix(guest); CalendarOracleClose(native); }
    void Set(double time, bool cutover = false, std::int32_t incoming = 0) {
        auto status = incoming;
        auto expected = incoming;
        CalendarOracleSet(native, time, cutover, &expected);
        if (cutover) ucal_setGregorianChange_nid_postfix(guest, time, &status);
        else ucal_setMillis_nid_postfix(guest, time, &status);
        Require(status == expected, "time/cutover setter preserves native errors and warnings");
    }
    void Fields(std::int32_t incoming = 0) {
        for (std::int32_t field = 0; field < 24; ++field) {
            auto status = incoming;
            auto expected = incoming;
            const auto wanted = CalendarOracleGet(native, field, &expected);
            const auto actual = ucal_get_nid_postfix(guest, field, &status);
            Require(actual == wanted && status == expected, "all native calendar fields and statuses match");
        }
    }
    std::int32_t Get(std::int32_t field) {
        std::int32_t status = 0;
        const auto result = ucal_get_nid_postfix(guest, field, &status);
        Require(status <= 0, "fixed field extraction succeeds");
        return result;
    }
};

template<class Function>
void Refuses(Function&& function) {
    bool refused = false;
    try { function(); } catch (const std::exception&) { refused = true; }
    Require(refused, "foreign/stale handle, unknown guest default or bad memory is refused");
}

void Fields() {
    std::fputs("calendar fixture: valid explicit UTC ucal_open\n", stderr);
    Handle utc(u"UTC", -1, "en_US", 1);
    utc.Set(1577934245000.0);
    Require(utc.Get(1) == 2020 && utc.Get(2) == 0 && utc.Get(5) == 2 && utc.Get(11) == 3 && utc.Get(12) == 4 && utc.Get(13) == 5,
        "fixed UTC 2020-01-02 03:04:05 calendar fields");
    utc.Set(1583019000000.0);
    Require(utc.Get(2) == 1 && utc.Get(5) == 29, "fixed leap day");
    Handle ny(u"America/New_York", -1, "en_US", 1);
    ny.Set(1583650740000.0);
    Require(ny.Get(11) == 1 && ny.Get(12) == 59 && ny.Get(16) == 0, "NY time before spring DST transition");
    ny.Set(1583650800000.0);
    Require(ny.Get(11) == 3 && ny.Get(12) == 0 && ny.Get(16) == 3600000, "NY time after spring DST transition");
    Handle thai(u"UTC", -1, "th_TH@calendar=buddhist", 0);
    thai.Set(1577934245000.0);
    Require(thai.Get(1) == 2563, "fixed Buddhist year uses locale calendar");
    Handle japanese(u"UTC", -1, "ja_JP@calendar=japanese", 0);
    japanese.Set(1577934245000.0);
    Require(japanese.Get(1) == 2, "fixed Japanese era year uses real calendar data");
    for (const auto* zone : {u"UTC", u"America/New_York", u"Asia/Kolkata", u"GMT+05:30", u"not-a-time-zone", u""}) {
        for (const auto* locale : {"en_US", "de_DE", "th_TH@calendar=buddhist", "ja_JP@calendar=japanese", "en@calendar=iso8601", "ar_SA@calendar=islamic-umalqura"}) {
            for (auto type : {0, 1, 2, -1}) {
                Handle value(zone, -1, locale, type);
                for (double time : {0.0, -1.0, 1577934245000.0, 1583019000000.0, 1583650800000.0, -12219724800000.0}) {
                    value.Set(time);
                    value.Fields();
                }
            }
        }
    }
    for (double time : {-8640000000000000.0, 8640000000000000.0, -1.0e20, 1.0e20,
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Handle value(u"UTC", -1, "en_US", 1);
        value.Set(0.0);
        value.Set(time);
        value.Fields();
    }
    const std::array<char16_t, 7> embedded{u'U', u'T', u'C', 0, u'X', u'Y', 0};
    for (auto length : {-99, -1, 0, 3, 6}) {
        Handle value(embedded.data(), length, "en_US", 1);
        value.Set(1577934245000.0);
        value.Fields();
    }
}

void Clone() {
    Handle original(u"UTC", -1, "en_US", 1);
    original.Set(-12219724800000.0);
    std::int32_t status = 0;
    std::int32_t expected = 0;
    Handle copy(ucal_clone_nid_postfix(original.guest, &status), CalendarOracleClone(original.native, &expected));
    Require(copy.guest && copy.native && status == expected && status <= 0, "clone owns a usable independent calendar");
    original.Fields();
    copy.Fields();
    const auto day = original.Get(5);
    copy.Set(-8640000000000000.0, true);
    copy.Fields();
    Require(original.Get(5) == day, "clone cutover mutation leaves original unchanged");
    copy.Set(1577934245000.0);
    copy.Fields();
    Require(copy.Get(1) == 2020 && original.Get(1) == 1582, "clone time mutation is independent");
    original.Set(8640000000000000.0, true);
    original.Fields();
    copy.Fields();
    Handle buddhist(u"UTC", -1, "th@calendar=buddhist", 0);
    buddhist.Set(0.0);
    buddhist.Set(0.0, true);
    buddhist.Fields();
}

void Errors() {
    for (auto incoming : {1, 7, 15, 0x7fffffff}) {
        auto status = incoming;
        auto* foreign = reinterpret_cast<UCalendar*>(std::uintptr_t{1});
        Require(ucal_open_nid_postfix(reinterpret_cast<const char16_t*>(std::uintptr_t{1}), -99,
            reinterpret_cast<const char*>(std::uintptr_t{1}), -1, &status) == nullptr && status == incoming, "failed status before all open inputs");
        Require(ucal_clone_nid_postfix(foreign, &status) == nullptr && status == incoming, "failed status before clone handle");
        Require(ucal_get_nid_postfix(foreign, -1, &status) == -1 && status == incoming, "failed status before field/handle checks");
        ucal_setMillis_nid_postfix(foreign, 0.0, &status);
        ucal_setGregorianChange_nid_postfix(foreign, 0.0, &status);
        Require(status == incoming, "failed setters do not access handle");
    }
    for (auto field : {-1, 24, 0x7fffffff}) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        Require(ucal_get_nid_postfix(nullptr, field, &status) == CalendarOracleGet(nullptr, field, &expected) && status == expected,
            "native invalid-field error precedes null-handle access");
    }
    for (auto incoming : {0, -1, -124, -128}) {
        Handle value(u"UTC", -1, "en_US", 1);
        value.Set(0.0, false, incoming);
        value.Fields(incoming);
        value.Set(0.0, true, incoming);
        value.Fields();
        auto status = incoming;
        auto expected = incoming;
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, &status);
        CalendarOracleSet(nullptr, 0.0, true, &expected);
        Require(status == expected && status == 1, "null Gregorian target preserves native argument error");
    }
    std::int32_t status = 0;
    Refuses([&] { ucal_open_nid_postfix(nullptr, 0, "en", 1, &status); });
    Refuses([&] { ucal_open_nid_postfix(u"UTC", -1, nullptr, 1, &status); });
    Refuses([] { ucal_open_nid_postfix(u"UTC", -1, "en", 1, nullptr); });
    Handle value(u"UTC", -1, "en", 1);
    auto* closed = ucal_clone_nid_postfix(value.guest, &status);
    for (int phase = 0; phase < 2; ++phase) {
        auto* enumeration = reinterpret_cast<UEnumeration*>(closed);
        Refuses([&] { uenum_count_nid_postfix(enumeration, &status); });
        Refuses([&] { uenum_close_nid_postfix(enumeration); });
        if (phase == 0) ucal_close_nid_postfix(closed);
    }
    for (int index = 0; index < 64; ++index) {
        auto* fresh = ucal_clone_nid_postfix(value.guest, &status);
        Require(fresh != closed, "calendar identities do not revive closed tokens");
        Refuses([&] { ucal_get_nid_postfix(closed, 1, &status); });
        ucal_close_nid_postfix(fresh);
    }
    auto* numbering = unumsys_openByName_nid_postfix("latn", &status);
    Refuses([&] { ucal_get_nid_postfix(reinterpret_cast<UCalendar*>(numbering), 1, &status); });
    Refuses([&] { unumsys_getName_nid_postfix(reinterpret_cast<UNumberingSystem*>(value.guest)); });
    unumsys_close_nid_postfix(numbering);
    ucal_close_nid_postfix(nullptr);
}

void Memory() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0);
    auto* data = static_cast<char*>(address);
    Require(sceKernelMprotect(data, page, 0) == 0);
    Require(sceKernelMprotect(data + page * 2, page, 0) == 0);
    auto* zone = reinterpret_cast<char16_t*>(data + page * 2 - 8);
    std::memcpy(zone, u"UTC", 8);
    auto* status = reinterpret_cast<std::int32_t*>(data + page);
    *status = 0;
    auto* handle = ucal_open_nid_postfix(zone, -1, "en", 1, status);
    Require(handle && *status <= 0, "NUL-terminated UTF16 ends at readable page boundary");
    ucal_setMillis_nid_postfix(handle, 0.0, status);
    Require(ucal_get_nid_postfix(handle, 1, status) == 1970 && *status <= 0, "mapped status works with state operations");
    std::fill_n(zone, 4, u'X');
    Refuses([&] { ucal_open_nid_postfix(zone, -1, "en", 1, status); });
    Refuses([&] { ucal_get_nid_postfix(handle, 1, reinterpret_cast<std::int32_t*>(data)); });
    *status = 7;
    Require(sceKernelMprotect(data + page, page, 1) == 0);
    Require(ucal_open_nid_postfix(reinterpret_cast<const char16_t*>(data), -1, reinterpret_cast<const char*>(data), 1, status) == nullptr && *status == 7,
        "read-only failed status returns before guarded inputs");
    Require(ucal_get_nid_postfix(reinterpret_cast<UCalendar*>(std::uintptr_t{1}), 1, status) == -1 && *status == 7,
        "read-only failed status returns before foreign handle");
    Require(sceKernelMprotect(data + page, page, 3) == 0);
    ucal_close_nid_postfix(handle);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "fields";
        if (mode == "fields") Fields();
        else if (mode == "clone") Clone();
        else if (mode == "errors") Errors();
        else if (mode == "memory") Memory();
        else Require(false, "unknown calendar test mode");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "calendar fixture failed: %s\n", error.what());
        return 1;
    }
}
