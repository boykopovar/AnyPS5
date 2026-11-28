#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

constexpr std::int32_t EofBit = 0x1;
constexpr std::int32_t FailBit = 0x2;
constexpr int DateTime = 5;
constexpr int DateDate = 6;
constexpr int DateWeekday = 7;
constexpr int DateMonthname = 8;
constexpr int DateYear = 9;
constexpr int DateField = 10;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

template<typename Text> struct InputIterator {
    GuestLocale::Streambuf* buffer;
    bool got;
    Text value;
};

struct InputVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

template<typename Text> struct Source {
    GuestLocale::Streambuf buffer{};
    InputVtable vtable{};
    std::array<Text, 256> storage{};
    std::basic_string<Text> text;
    Text* next = nullptr;
    std::int32_t count = 0;
    std::size_t direct = 0;
    std::size_t slow = 0;
};

template<typename Text> static Source<Text>* g_source = nullptr;

template<typename Text> static int APS5_VABI SourceUnderflow(GuestLocale::Streambuf*) {
    const auto& source = *g_source<Text>;
    return source.slow < source.text.size() ? static_cast<int>(static_cast<std::make_unsigned_t<Text>>(source.text[source.slow])) : -1;
}

template<typename Text> static int APS5_VABI SourceUflow(GuestLocale::Streambuf*) {
    auto& source = *g_source<Text>;
    return source.slow < source.text.size() ? static_cast<int>(static_cast<std::make_unsigned_t<Text>>(source.text[source.slow++])) : -1;
}

template<typename Text> struct Bench {
    void* facet = nullptr;
    GuestLocale::SceV2IosBase stream{};
    Source<Text> source;
};

template<typename Text> using Get = InputIterator<Text> (APS5_VABI *)(const void*, InputIterator<Text>, InputIterator<Text>, GuestLocale::SceV2IosBase*, std::int32_t*, GuestTm*);
template<typename Text> using GetField = InputIterator<Text> (APS5_VABI *)(const void*, InputIterator<Text>, InputIterator<Text>, GuestLocale::SceV2IosBase*, std::int32_t*, GuestTm*, char, char);
using DateOrder = std::int32_t (APS5_VABI *)(const void*);

template<typename Text> static void Load(Bench<Text>& bench, const std::basic_string<Text>& text, std::size_t direct) {
    auto& source = bench.source;
    source.vtable.underflow = SourceUnderflow<Text>;
    source.vtable.uflow = SourceUflow<Text>;
    source.buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&source.vtable);
    source.text = text;
    std::memcpy(source.storage.data(), text.data(), text.size() * sizeof(Text));
    source.direct = direct < text.size() ? direct : text.size();
    source.next = source.storage.data();
    source.count = static_cast<std::int32_t>(source.direct);
    source.slow = source.direct;
    source.buffer.reserved[6] = &source.next;
    source.buffer.reservedCounts[1] = &source.count;
    g_source<Text> = &source;
}

template<typename Text> static std::size_t Consumed(const Source<Text>& source) {
    return static_cast<std::size_t>(source.next - source.storage.data()) + source.slow - source.direct;
}

struct Expect {
    int sec = -7, min = -7, hour = -7, mday = -7, mon = -7, year = -7, wday = -7, yday = -7;
};

static GuestTm Blank() {
    GuestTm time{};
    time.tm_sec = time.tm_min = time.tm_hour = time.tm_mday = time.tm_mon = time.tm_year = time.tm_wday = time.tm_yday = -7;
    return time;
}

template<typename Text> static void Check(Bench<Text>& bench, const std::string& name, int slot, char specifier, const std::basic_string<Text>& text, const Expect& expected, std::int32_t state, std::size_t consumed, std::int32_t initialState = 0, std::size_t direct = 256) {
    Load(bench, text, direct);
    GuestTm time = Blank();
    std::int32_t outState = initialState;
    const InputIterator<Text> first{&bench.source.buffer, false, 0};
    const InputIterator<Text> last{nullptr, false, 0};
    if (slot == DateField) Slot<GetField<Text>>(bench.facet, slot)(bench.facet, first, last, &bench.stream, &outState, &time, specifier, 0);
    else Slot<Get<Text>>(bench.facet, slot)(bench.facet, first, last, &bench.stream, &outState, &time);
    const int got[] {time.tm_sec, time.tm_min, time.tm_hour, time.tm_mday, time.tm_mon, time.tm_year, time.tm_wday, time.tm_yday};
    const int want[] {expected.sec, expected.min, expected.hour, expected.mday, expected.mon, expected.year, expected.wday, expected.yday};
    static const char* const fields[] {"sec", "min", "hour", "mday", "mon", "year", "wday", "yday"};
    for (int i = 0; i < 8; ++i) Require(got[i] == want[i], name + ": " + fields[i] + " " + std::to_string(got[i]) + ", expected " + std::to_string(want[i]));
    Require(outState == state, name + ": state " + std::to_string(outState) + ", expected " + std::to_string(state));
    Require(Consumed(bench.source) == consumed, name + ": consumed " + std::to_string(Consumed(bench.source)) + ", expected " + std::to_string(consumed));
}

static void SetOrder(void* facet, std::int32_t order) {
    std::memcpy(static_cast<unsigned char*>(facet) + 0x28, &order, sizeof(order));
}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    static const GuestLocale::SceV2Locinfo info{0, "C", "C"};
    static auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);

    static Bench<char> narrow;
    narrow.facet = locale->facets[_ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix];
    narrow.stream.locale = &locale;
    static Bench<char16_t> wide;
    wide.facet = locale->facets[_ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix];
    wide.stream.locale = &locale;
    Require(narrow.facet != nullptr && wide.facet != nullptr, "_Makeloc installs time_get");
    if (failures != 0) std::_Exit(1);

    SetOrder(narrow.facet, 3);
    Require(Slot<DateOrder>(narrow.facet, 4)(narrow.facet) == 3, "date_order reads the facet's order");

    Expect time;
    time.hour = 12; time.min = 34; time.sec = 56;
    Check<char>(narrow, "get_time", DateTime, 0, "12:34:56", time, EofBit, 8);
    Check<char>(narrow, "get_time through underflow", DateTime, 0, "12:34:56", time, EofBit, 8, 0, 3);
    Expect hourOnly;
    hourOnly.hour = 7;
    Check<char>(narrow, "get_time fails on any incoming state", DateTime, 0, "7:05:00", hourOnly, EofBit | FailBit, 1, EofBit);
    Expect hourMinute;
    hourMinute.hour = 7; hourMinute.min = 5;
    Check<char>(narrow, "get_time needs the seconds", DateTime, 0, " 7:05", hourMinute, EofBit | FailBit, 5);

    SetOrder(narrow.facet, 2);
    Expect date;
    date.mon = 11; date.mday = 25; date.year = 99;
    Check<char>(narrow, "mdy date", DateDate, 0, "12/25/99", date, EofBit, 8);
    Check<char>(narrow, "a month name starts an mdy date", DateDate, 0, "dEc 25, 99", date, EofBit, 10);
    SetOrder(narrow.facet, 1);
    Expect dmy;
    dmy.mday = 25; dmy.mon = 11; dmy.year = 124;
    Check<char>(narrow, "dmy date", DateDate, 0, "25 : 12 , 2024", dmy, EofBit, 14);
    SetOrder(narrow.facet, 0);
    Check<char>(narrow, "no order reads mdy", DateDate, 0, "12/25/99", date, EofBit, 8);

    Expect tuesday;
    tuesday.wday = 2;
    Check<char>(narrow, "weekday ignores case", DateWeekday, 0, "tUESDAY", tuesday, 0, 7);
    Check<char>(narrow, "weekday short name", DateWeekday, 0, "TUE x", tuesday, 0, 3);
    Check<char>(narrow, "unknown weekday", DateWeekday, 0, "Xyz", Expect{}, FailBit, 0);
    Expect february;
    february.mon = 1;
    Check<char>(narrow, "month name", DateMonthname, 0, "february", february, 0, 8);
    Check<char>(narrow, "a longer name cut short fails", DateMonthname, 0, "Febr", Expect{}, FailBit, 4);

    Expect y2005;
    y2005.year = 105;
    Check<char>(narrow, "a two-digit year below 69 is in the 2000s", DateYear, 0, "5", y2005, EofBit, 1);
    Expect y1969;
    y1969.year = 69;
    Check<char>(narrow, "a two-digit year of 69 is in the 1900s", DateYear, 0, "69 ", y1969, 0, 2);
    Expect y123;
    y123.year = 123 - 1900;
    Check<char>(narrow, "three digits are literal", DateYear, 0, "123", y123, EofBit, 3);
    Expect y99;
    y99.year = 99 - 1900;
    Check<char>(narrow, "leading zeros count as digits", DateYear, 0, "0099", y99, EofBit, 4);

    Expect hour;
    hour.hour = 23;
    Check<char>(narrow, "%H", DateField, 'H', "23", hour, EofBit, 2);
    Check<char>(narrow, "%H keeps at most two digits", DateField, 'H', "234", hour, 0, 2);
    Check<char>(narrow, "%H rejects a negative value", DateField, 'H', "-5", Expect{}, EofBit | FailBit, 2);
    Expect zeroHour;
    zeroHour.hour = 0;
    Check<char>(narrow, "%H counts leading spaces as digits", DateField, 'H', "  5", zeroHour, 0, 2);
    Check<char>(narrow, "%H keeps one of its leading zeros", DateField, 'H', "00", zeroHour, EofBit, 2);
    Expect plusFive;
    plusFive.hour = 5;
    Check<char>(narrow, "%H takes a plus sign", DateField, 'H', "+5", plusFive, EofBit, 2);
    Check<char>(narrow, "do_get clears the incoming state", DateField, 'H', "5 ", plusFive, 0, 1, EofBit);
    Check<char>(narrow, "a format on empty input fails", DateField, 'n', "", Expect{}, EofBit | FailBit, 0);
    Expect noon;
    noon.hour = 0;
    Check<char>(narrow, "%I turns 12 into 0", DateField, 'I', "12", noon, EofBit, 2);
    Expect century;
    century.year = 100;
    Check<char>(narrow, "%C", DateField, 'C', "20", century, EofBit, 2);
    Expect shortYear;
    shortYear.year = 168;
    Check<char>(narrow, "%y below 69", DateField, 'y', "68", shortYear, EofBit, 2);
    Expect x;
    x.mday = 25; x.mon = 11; x.year = 99;
    Check<char>(narrow, "%x reads day, month and year", DateField, 'x', "25/12/99", x, EofBit, 8);
    Expect pm;
    pm.hour = -7 + 12;
    Check<char>(narrow, "%p adds 12 for PM", DateField, 'p', "pm", pm, EofBit, 2);
    Check<char>(narrow, "%% never fails", DateField, '%', "x", Expect{}, 0, 0);
    Check<char>(narrow, "%% takes a percent sign", DateField, '%', "%x", Expect{}, 0, 1);
    Check<char>(narrow, "an unknown specifier fails", DateField, 'q', "1", Expect{}, FailBit, 0);
    Expect full;
    full.wday = 2; full.mon = 1; full.mday = 3; full.hour = 4; full.min = 5; full.sec = 6; full.year = 124;
    Check<char>(narrow, "%c", DateField, 'c', "Tue Feb 3 04:05:06 2024", full, EofBit, 23);

    Expect widePm;
    widePm.hour = -7 + 12;
    Check<char16_t>(wide, "wide %p compares the low byte without case", DateField, 'p', u"\x0150\x014d", widePm, EofBit, 2);
    Expect wideTime;
    wideTime.hour = 1; wideTime.min = 2; wideTime.sec = 3;
    Check<char16_t>(wide, "wide get_time", DateTime, 0, u"01:02:03", wideTime, EofBit, 8);
    Expect wideMonth;
    wideMonth.mon = 11;
    Check<char16_t>(wide, "wide month name", DateMonthname, 0, u"DECEMBER", wideMonth, 0, 8);

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
