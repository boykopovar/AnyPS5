#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal facet put: %s\n", message);
    std::exit(1);
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

#ifdef _WIN32
HMODULE g_library = nullptr;
#else
void* g_library = nullptr;
#endif

template<typename TFunction>
TFunction Export(const char* nid) {
    const std::string name = std::string(nid) + "_nid_no_patch_cut";
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(g_library, name.c_str()));
#else
    void* address = dlsym(g_library, name.c_str());
#endif
    Require(address != nullptr, "missing export");
    return reinterpret_cast<TFunction>(address);
}

struct TestStreambuf {
    GuestLocale::Streambuf buffer;
    std::string accepted;
    int offered;
    int capacity;
};

int APS5_VABI TestOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<TestStreambuf*>(self);
    ++stream->offered;
    if (static_cast<int>(stream->accepted.size()) >= stream->capacity) return -1;
    stream->accepted += static_cast<char>(character);
    return character;
}

const GuestLocale::StreambufVtable g_streambufVtable{{}, TestOverflow};
char* g_noBuffer = nullptr;

TestStreambuf MakeStream(int capacity) {
    return TestStreambuf{{&g_streambufVtable, {}, &g_noBuffer, {}, &g_noBuffer}, {}, 0, capacity};
}

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }
const GuestLocale::FacetVtable g_testFacet{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease};

struct Call {
    int slot;
    const GuestLocale::Facet* self;
    GuestLocale::Streambuf* buffer;
    bool failed;
    GuestLocale::IosBase* stream;
    char fill;
    std::uint64_t integer;
    long double floating;
    const void* pointer;
};

Call g_call{};

GuestLocale::OutputIterator Record(int slot, const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill) {
    g_call = Call{slot, self, out.buffer, out.failed, stream, fill, 0, 0, nullptr};
    return GuestLocale::OutputIterator{!out.failed, out.buffer};
}

GuestLocale::OutputIterator APS5_VABI RecordBool(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, bool value) {
    const auto result = Record(0, self, out, stream, fill);
    g_call.integer = value ? 1 : 0;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    const auto result = Record(1, self, out, stream, fill);
    g_call.integer = static_cast<std::uint64_t>(value);
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordUnsignedLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    const auto result = Record(2, self, out, stream, fill);
    g_call.integer = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordLongLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    const auto result = Record(3, self, out, stream, fill);
    g_call.integer = static_cast<std::uint64_t>(value);
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordUnsignedLongLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    const auto result = Record(4, self, out, stream, fill);
    g_call.integer = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordDouble(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, double value) {
    const auto result = Record(5, self, out, stream, fill);
    g_call.floating = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordLongDouble(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, long double value) {
    const auto result = Record(6, self, out, stream, fill);
    g_call.floating = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordPointer(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const void* value) {
    const auto result = Record(7, self, out, stream, fill);
    g_call.pointer = value;
    return result;
}

const GuestLocale::NumPutVtable g_recordingNumPut{g_testFacet, RecordBool, RecordLong, RecordUnsignedLong, RecordLongLong, RecordUnsignedLongLong, RecordDouble, RecordLongDouble, RecordPointer};

template<typename TValue>
using NumPutExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, TValue);
using TimePutExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, const GuestTm*, char, char);
using TimePutPatternExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, const GuestTm*, const char*, const char*);

template<typename TValue>
void CheckNumPutForward(const char* nid, int slot, TValue value) {
    const auto put = Export<NumPutExport<TValue>>(nid);
    GuestLocale::Facet facet{&g_recordingNumPut.facet, 1, 0};
    GuestLocale::IosBase ios{};
    auto stream = MakeStream(0);
    const GuestLocale::OutputIterator out = put(&facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '#', value);
    Require(g_call.slot == slot, "num_put::put calls the do_put slot of its value type");
    Require(g_call.self == &facet && g_call.stream == &ios && g_call.fill == '#', "num_put::put passes the facet, stream and fill");
    Require(g_call.buffer == &stream.buffer && !g_call.failed, "num_put::put passes the output iterator");
    Require(out.failed && out.buffer == &stream.buffer, "num_put::put returns what do_put returns");
}

void CheckNumPutForwarding() {
    CheckNumPutForward<bool>("nftirmo6hBg", 0, true);
    Require(g_call.integer == 1, "put(bool) passes the value");
    CheckNumPutForward<std::int64_t>("ffnhh0HcxJ4", 1, std::numeric_limits<std::int64_t>::min());
    Require(g_call.integer == static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::min()), "put(long) passes the value");
    CheckNumPutForward<std::uint64_t>("uODuM76vS4U", 2, std::numeric_limits<std::uint64_t>::max());
    Require(g_call.integer == std::numeric_limits<std::uint64_t>::max(), "put(unsigned long) passes the value");
    CheckNumPutForward<std::int64_t>("NJtKruu9qOs", 3, -7);
    Require(g_call.integer == static_cast<std::uint64_t>(-7), "put(long long) passes the value");
    CheckNumPutForward<std::uint64_t>("dep6W2Ix35s", 4, 0x8000000000000001ull);
    Require(g_call.integer == 0x8000000000000001ull, "put(unsigned long long) passes the value");
    CheckNumPutForward<double>("w9NzCYAjEpQ", 5, -2.5);
    Require(g_call.floating == -2.5L, "put(double) passes the value");
    CheckNumPutForward<long double>("VPcTGA-LwSo", 6, 1.0L / 3.0L);
    Require(g_call.floating == 1.0L / 3.0L, "put(long double) passes the value");
    int object = 0;
    CheckNumPutForward<const void*>("8NVUcufbklM", 7, &object);
    Require(g_call.pointer == &object, "put(const void*) passes the value");
}

std::string PutLibcNumber(GuestLocale::Facet* facet, std::int32_t flags, std::int32_t width, std::int64_t value) {
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.precision = 6;
    ios.width = width;
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    auto stream = MakeStream(64);
    const GuestLocale::OutputIterator out = Export<NumPutExport<std::int64_t>>("ffnhh0HcxJ4")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', value);
    Require(!out.failed && out.buffer == &stream.buffer && ios.width == 0, "the libc num_put facet writes the field");
    return stream.accepted;
}

void CheckLibcNumPut() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &locale) == 4 && facet != nullptr, "num_put<char>::_Getcat creates the facet");
    Require(PutLibcNumber(facet, 0, 6, -42) == "***-42", "put(long) formats through the libc facet");
    Require(PutLibcNumber(facet, 0x2c8, 8, 255) == "0X****FF", "put(long) honours the stream flags");
    GuestLocale::IosBase ios{};
    ios.precision = 6;
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    auto stream = MakeStream(64);
    Export<NumPutExport<double>>("w9NzCYAjEpQ")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', 0.125);
    Export<NumPutExport<bool>>("nftirmo6hBg")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', true);
    Require(stream.accepted == "0.1251", "put(double) and put(bool) format through the libc facet");
    facet->vtable->deleteObject(facet);
}

struct TimeCall {
    const GuestLocale::Facet* self;
    GuestLocale::IosBase* stream;
    char fill;
    const GuestTm* time;
};

TimeCall g_timeCall{};
int g_timeCalls = 0;

GuestLocale::OutputIterator APS5_VABI RecordTimePut(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, char format, char modifier) {
    ++g_timeCalls;
    g_timeCall = TimeCall{self, stream, fill, time};
    auto* buffer = reinterpret_cast<TestStreambuf*>(out.buffer);
    buffer->accepted += '<';
    buffer->accepted += modifier != '\0' ? modifier : '_';
    buffer->accepted += format != '\0' ? format : '0';
    buffer->accepted += '>';
    return GuestLocale::OutputIterator{out.failed || format == 'F', out.buffer};
}

const GuestLocale::TimePutVtable g_recordingTimePut{g_testFacet, RecordTimePut};

std::string PutPattern(const GuestLocale::Facet* facet, GuestLocale::Implementation** locale, const std::string& pattern, bool* failed = nullptr) {
    GuestLocale::IosBase ios{};
    ios.locale = locale;
    GuestTm time{};
    auto stream = MakeStream(64);
    g_timeCalls = 0;
    g_timeCall = {};
    const GuestLocale::OutputIterator out = Export<TimePutPatternExport>("j9LU8GsuEGw")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '@', &time, pattern.data(), pattern.data() + pattern.size());
    Require(out.buffer == &stream.buffer, "time_put::put returns the output iterator");
    if (g_timeCalls != 0) Require(g_timeCall.self == facet && g_timeCall.stream == &ios && g_timeCall.fill == '@' && g_timeCall.time == &time, "time_put::put passes the facet, stream, fill and time to do_put");
    if (failed != nullptr) *failed = out.failed;
    else Require(!out.failed, "time_put::put keeps a good iterator");
    return stream.accepted;
}

void CheckTimePutForwarding() {
    const auto put = Export<TimePutExport>("qkuA-unH7PU");
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    GuestLocale::IosBase ios{};
    GuestTm time{};
    auto stream = MakeStream(0);
    g_timeCalls = 0;
    GuestLocale::OutputIterator out = put(&facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '!', &time, 'Y', 'E');
    Require(g_timeCalls == 1 && g_timeCall.self == &facet && g_timeCall.stream == &ios && g_timeCall.fill == '!' && g_timeCall.time == &time, "time_put::put(spec, mod) calls do_put once with its arguments");
    Require(stream.accepted == "<EY>" && !out.failed && out.buffer == &stream.buffer, "time_put::put(spec, mod) passes the specifier and modifier");
    out = put(&facet, out, &ios, '!', &time, 'F', '\0');
    Require(stream.accepted == "<EY><_F>" && out.failed, "time_put::put(spec, mod) returns what do_put returns");
}

void CheckPatternWalk() {
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutPattern(&facet, classic, "") == "" && g_timeCalls == 0, "an empty pattern writes nothing");
    Require(PutPattern(&facet, classic, "plain text") == "plain text" && g_timeCalls == 0, "ordinary characters are copied");
    Require(PutPattern(&facet, classic, "a%Yb%Ec%Od%%e") == "a<_Y>b<Ec><Od><_%>e" && g_timeCalls == 4, "each conversion calls do_put in pattern order");
    Require(PutPattern(&facet, classic, "%Qx%#y") == "<Qx><#y>", "Q and # are modifiers");
    Require(PutPattern(&facet, classic, "%k%z") == "<_k><_z>", "any specifier goes to do_put");
    Require(PutPattern(&facet, classic, std::string("%\0%E\0", 5)) == "<_0><E0>", "a null specifier goes to do_put");
    Require(PutPattern(&facet, classic, "x%") == "x%" && g_timeCalls == 0, "a trailing percent is copied");
    Require(PutPattern(&facet, classic, "x%E") == "x%E" && g_timeCalls == 0, "a trailing modifier is copied with its percent");
    Require(PutPattern(&facet, classic, "%O") == "%O", "a lone modifier is copied");
    Require(PutPattern(&facet, classic, "%EE") == "<EE>", "a modifier can be the specifier");
    bool failed = false;
    Require(PutPattern(&facet, classic, "%Fz", &failed) == "<_F>z" && failed, "the iterator do_put returns is used for the rest of the pattern");
}

std::string g_narrowFallbacks;

char APS5_VABI TestNarrow(const GuestLocale::Facet*, char character, char fallback) {
    g_narrowFallbacks += fallback == '\0' ? '0' : 'x';
    switch (character) {
    case '!': return '%';
    case 'q': return 'E';
    case 'z': return 'd';
    case '%': return 'p';
    default: return character;
    }
}

struct TestCtypeVtable {
    GuestLocale::FacetVtable facet;
    const void* caseAndWidenMethods[6];
    char (APS5_VABI *narrow)(const GuestLocale::Facet* self, char character, char fallback);
    const void* narrowRange;
};

static_assert(offsetof(TestCtypeVtable, narrow) == 0x50);

const TestCtypeVtable g_testCtype{g_testFacet, {}, TestNarrow, nullptr};

void CheckStreamCtype() {
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    Require(PutPattern(&facet, &_ZSt21_sceLibcClassicLocale_nid_postfix, "%Y") == "<_Y>", "the classic locale narrows without a ctype<char> facet");
    Require(_ZNSt5ctypeIcE2idE_nid_postfix != 0, "time_put::put looks up ctype<char> through its locale id");
    GuestLocale::Facet ctype{&g_testCtype.facet, 1, 0};
    std::vector<GuestLocale::Facet*> facets(_ZNSt5ctypeIcE2idE_nid_postfix + 1);
    facets.back() = &ctype;
    GuestLocale::Implementation custom{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* customPointer = &custom;
    g_narrowFallbacks.clear();
    Require(PutPattern(&facet, &customPointer, "1!qz2%Y!") == "1<Ed>2%Y!", "the stream's ctype<char> narrows the pattern and the literal characters keep their value");
    Require(g_narrowFallbacks == "00000000", "narrow is called once per examined character with a null default");
    Require(PutPattern(&facet, &customPointer, "a!q") == "a!E", "a trailing modifier is copied narrowed after the original percent");
}

void CheckLibcTimePut() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &locale) == 5 && facet != nullptr, "time_put<char>::_Getcat creates the facet");
    GuestTm time{};
    time.tm_year = 126;
    time.tm_mon = 9;
    time.tm_mday = 2;
    time.tm_hour = 13;
    time.tm_min = 5;
    time.tm_sec = 9;
    time.tm_wday = 5;
    time.tm_yday = 274;
    GuestLocale::IosBase ios{};
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    auto stream = MakeStream(64);
    const std::string pattern = "%Y-%m-%d %H:%M:%S %% %Ey %Oy %";
    GuestLocale::OutputIterator out = Export<TimePutPatternExport>("j9LU8GsuEGw")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, ' ', &time, pattern.data(), pattern.data() + pattern.size());
    Require(!out.failed && stream.accepted == "2026-10-02 13:05:09 % 26 26 %", "the pattern formats through the libc time_put facet");
    out = Export<TimePutExport>("qkuA-unH7PU")(facet, out, &ios, ' ', &time, 'a', '\0');
    Require(!out.failed && stream.accepted == "2026-10-02 13:05:09 % 26 26 %Fri", "put(spec, mod) formats through the libc time_put facet");
    auto shortStream = MakeStream(3);
    out = Export<TimePutPatternExport>("j9LU8GsuEGw")(facet, GuestLocale::OutputIterator{false, &shortStream.buffer}, &ios, ' ', &time, pattern.data(), pattern.data() + 8);
    Require(out.failed && shortStream.accepted == "202" && shortStream.offered == 10, "a failed write marks the iterator and later characters are still offered");
    facet->vtable->deleteObject(facet);
}

struct WideTestStreambuf {
    GuestLocale::Streambuf buffer;
    std::u16string accepted;
    std::vector<int> offered;
    int capacity;
};

int APS5_VABI WideTestOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<WideTestStreambuf*>(self);
    stream->offered.push_back(character);
    if (static_cast<int>(stream->accepted.size()) >= stream->capacity) return -1;
    stream->accepted += static_cast<char16_t>(character);
    return character;
}

const GuestLocale::StreambufVtable g_wideStreambufVtable{{}, WideTestOverflow};

WideTestStreambuf MakeWideStream(int capacity) {
    return WideTestStreambuf{{&g_wideStreambufVtable, {}, &g_noBuffer, {}, &g_noBuffer}, {}, {}, capacity};
}

struct WideTimeCall {
    const GuestLocale::Facet* self;
    GuestLocale::IosBase* stream;
    std::uint16_t fill;
    const GuestTm* time;
};

WideTimeCall g_wideTimeCall{};

GuestLocale::OutputIterator APS5_VABI RecordWideTimePut(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    ++g_timeCalls;
    g_wideTimeCall = WideTimeCall{self, stream, fill, time};
    auto* buffer = reinterpret_cast<WideTestStreambuf*>(out.buffer);
    buffer->accepted += u'<';
    buffer->accepted += modifier != '\0' ? static_cast<char16_t>(static_cast<unsigned char>(modifier)) : u'_';
    buffer->accepted += format != '\0' ? static_cast<char16_t>(static_cast<unsigned char>(format)) : u'0';
    buffer->accepted += u'>';
    return GuestLocale::OutputIterator{out.failed || format == 'F', out.buffer};
}

const GuestLocale::WideTimePutVtable g_recordingWideTimePut{g_testFacet, RecordWideTimePut};

using WideTimePutExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const GuestTm*, char, char);
using WideTimePutPatternExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const GuestTm*, const std::uint16_t*, const std::uint16_t*);

constexpr std::uint16_t g_wideFill = 0x263a;

GuestLocale::OutputIterator PutWidePatternInto(WideTestStreambuf& stream, const GuestLocale::Facet* facet, GuestLocale::Implementation** locale, const std::u16string& pattern) {
    GuestLocale::IosBase ios{};
    ios.locale = locale;
    GuestTm time{};
    g_timeCalls = 0;
    g_wideTimeCall = {};
    const auto* first = reinterpret_cast<const std::uint16_t*>(pattern.data());
    const GuestLocale::OutputIterator out = Export<WideTimePutPatternExport>("Sc0lXhQG5Ko")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, g_wideFill, &time, first, first + pattern.size());
    Require(out.buffer == &stream.buffer, "time_put<wchar_t>::put returns the output iterator");
    if (g_timeCalls != 0) Require(g_wideTimeCall.self == facet && g_wideTimeCall.stream == &ios && g_wideTimeCall.fill == g_wideFill && g_wideTimeCall.time == &time, "time_put<wchar_t>::put passes the facet, stream, fill and time to do_put");
    return out;
}

std::u16string PutWidePattern(const GuestLocale::Facet* facet, GuestLocale::Implementation** locale, const std::u16string& pattern, bool* failed = nullptr) {
    auto stream = MakeWideStream(64);
    const GuestLocale::OutputIterator out = PutWidePatternInto(stream, facet, locale, pattern);
    if (failed != nullptr) *failed = out.failed;
    else Require(!out.failed, "time_put<wchar_t>::put keeps a good iterator");
    return stream.accepted;
}

void CheckWideTimePutForwarding() {
    const auto put = Export<WideTimePutExport>("Nt6eyVKm+Z4");
    GuestLocale::Facet facet{&g_recordingWideTimePut.facet, 1, 0};
    GuestLocale::IosBase ios{};
    GuestTm time{};
    auto stream = MakeWideStream(0);
    g_timeCalls = 0;
    GuestLocale::OutputIterator out = put(&facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, g_wideFill, &time, 'Y', 'E');
    Require(g_timeCalls == 1 && g_wideTimeCall.self == &facet && g_wideTimeCall.stream == &ios && g_wideTimeCall.fill == g_wideFill && g_wideTimeCall.time == &time, "time_put<wchar_t>::put(spec, mod) calls do_put once with its arguments and the 16-bit fill");
    Require(stream.accepted == u"<EY>" && !out.failed && out.buffer == &stream.buffer, "time_put<wchar_t>::put(spec, mod) passes the specifier and modifier");
    out = put(&facet, out, &ios, g_wideFill, &time, 'F', '\0');
    Require(stream.accepted == u"<EY><_F>" && out.failed, "time_put<wchar_t>::put(spec, mod) returns what do_put returns");
}

void CheckWidePatternWalk() {
    GuestLocale::Facet facet{&g_recordingWideTimePut.facet, 1, 0};
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutWidePattern(&facet, classic, u"") == u"" && g_timeCalls == 0, "an empty wide pattern writes nothing");
    Require(PutWidePattern(&facet, classic, u"plain é中Ａ") == u"plain é中Ａ" && g_timeCalls == 0, "wide literal elements are copied with their 16-bit value");
    Require(PutWidePattern(&facet, classic, u"a%Yb%Ec%Od%%e") == u"a<_Y>b<Ec><Od><_%>e" && g_timeCalls == 4, "each wide conversion calls do_put in pattern order");
    Require(PutWidePattern(&facet, classic, u"%Qx%#y") == u"<Qx><#y>", "Q and # are modifiers in a wide pattern");
    Require(PutWidePattern(&facet, classic, u"x%") == u"x%" && g_timeCalls == 0, "a trailing wide percent is copied");
    Require(PutWidePattern(&facet, classic, u"x%E") == u"x%E" && g_timeCalls == 0, "a trailing wide modifier is copied with its percent");
    Require(PutWidePattern(&facet, classic, u"%EE") == u"<EE>", "a modifier can be the specifier in a wide pattern");
    bool failed = false;
    Require(PutWidePattern(&facet, classic, u"%Fz", &failed) == u"<_F>z" && failed, "the iterator do_put returns is used for the rest of the wide pattern");
}

void CheckWideClassicNarrow() {
    GuestLocale::Facet facet{&g_recordingWideTimePut.facet, 1, 0};
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutWidePattern(&facet, classic, u"ĥY") == u"ĥY" && g_timeCalls == 0, "without a ctype<wchar_t> an element above ASCII is not a percent");
    Require(PutWidePattern(&facet, classic, u"%ř") == u"<_0>" && g_timeCalls == 1, "without a ctype<wchar_t> a specifier above ASCII narrows to the null default");
}

std::u16string g_wideNarrowCalls;

char APS5_VABI TestWideNarrow(const GuestLocale::Facet*, std::uint16_t character, char fallback) {
    g_wideNarrowCalls += fallback == '\0' ? static_cast<char16_t>(character) : u'?';
    switch (character) {
    case u'!': return '%';
    case u'q': return 'E';
    case u'z': return 'd';
    case u'%': return 'p';
    default: return static_cast<char>(character);
    }
}

const GuestLocale::WideCtypeVtable g_testWideCtype{g_testFacet, {}, {}, {}, TestWideNarrow, nullptr};

void CheckWideStreamCtype() {
    GuestLocale::Facet facet{&g_recordingWideTimePut.facet, 1, 0};
    Require(_ZNSt5ctypeIwE2idE_nid_postfix != 0, "time_put<wchar_t>::put looks up ctype<wchar_t> through its locale id");
    GuestLocale::Facet ctype{&g_testWideCtype.facet, 1, 0};
    std::vector<GuestLocale::Facet*> facets(_ZNSt5ctypeIwE2idE_nid_postfix + 1);
    facets.back() = &ctype;
    GuestLocale::Implementation custom{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* customPointer = &custom;
    g_wideNarrowCalls.clear();
    Require(PutWidePattern(&facet, &customPointer, u"1!qz2%Y!") == u"1<Ed>2%Y!", "the stream's ctype<wchar_t> narrows the pattern and literal elements keep their value");
    Require(g_wideNarrowCalls == u"1!qz2%Y!", "narrow is called once per examined element with a null default");
    Require(PutWidePattern(&facet, &customPointer, u"a!q") == u"a!q", "a trailing modifier is copied as its original elements");
}

void CheckWideStreamWrite() {
    GuestLocale::Facet facet{&g_recordingWideTimePut.facet, 1, 0};
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    std::array<std::uint16_t, 3> area{0, 0, 0x5a5a};
    char* next = reinterpret_cast<char*>(area.data());
    char* end = reinterpret_cast<char*>(area.data() + 2);
    auto stream = MakeWideStream(64);
    stream.buffer.putNext = &next;
    stream.buffer.putEnd = &end;
    GuestLocale::OutputIterator out = PutWidePatternInto(stream, &facet, classic, u"éＡ耀");
    Require(!out.failed && area[0] == 0x00e9 && area[1] == 0xff21 && area[2] == 0x5a5a && next == end, "wide literal elements fill the stream's put area while it has room");
    Require(stream.offered == std::vector<int>{0x8000} && stream.accepted == u"耀", "an element that does not fit goes to overflow zero-extended");
    auto shortStream = MakeWideStream(2);
    out = PutWidePatternInto(shortStream, &facet, classic, u"abcd");
    Require(out.failed && shortStream.accepted == u"ab" && shortStream.offered == std::vector<int>{'a', 'b', 'c', 'd'}, "an overflow of -1 marks the iterator failed and later elements are still offered");
    GuestLocale::IosBase ios{};
    ios.locale = classic;
    GuestTm time{};
    const std::u16string pattern = u"x";
    const auto* first = reinterpret_cast<const std::uint16_t*>(pattern.data());
    out = Export<WideTimePutPatternExport>("Sc0lXhQG5Ko")(&facet, GuestLocale::OutputIterator{false, nullptr}, &ios, g_wideFill, &time, first, first + pattern.size());
    Require(out.failed && out.buffer == nullptr, "a null stream buffer marks the iterator failed");
}

GuestTm MakeWideDoPutTime() {
    GuestTm time{};
    time.tm_year = 126;
    time.tm_mon = 10;
    time.tm_mday = 14;
    time.tm_hour = 9;
    time.tm_min = 5;
    time.tm_sec = 7;
    time.tm_wday = 6;
    time.tm_yday = 317;
    return time;
}

std::u16string WideDoPut(const GuestTm& time, char format, char modifier) {
    auto stream = MakeWideStream(256);
    const GuestLocale::OutputIterator out = Export<WideTimePutExport>("Fr7j8dMsy4s")(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, g_wideFill, &time, format, modifier);
    Require(!out.failed && out.buffer == &stream.buffer, "time_put<wchar_t>::do_put returns a good iterator over the same buffer");
    return stream.accepted;
}

void RequireWideDoPutThrows(char format, char modifier) {
    const GuestTm time = MakeWideDoPutTime();
    auto stream = MakeWideStream(256);
    try {
        Export<WideTimePutExport>("Fr7j8dMsy4s")(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, g_wideFill, &time, format, modifier);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("LibcWideTimePutDoPut") != std::string::npos && stream.accepted.empty(), "an unsettled do_put throws before writing");
        return;
    }
    Require(false, "an unsettled do_put must throw");
}

void CheckWideTimePutDoPut() {
    const GuestTm time = MakeWideDoPutTime();
    Require(WideDoPut(time, 'Y', '\0') == u"2026", "do_put writes one conversion without its leading marker and reads neither the facet, the stream nor the fill");
    Require(WideDoPut(time, 'y', 'E') == u"26" && WideDoPut(time, 'd', 'O') == u"14", "do_put puts the modifier before the specifier");
    std::array<std::uint16_t, 2> area{};
    char* next = reinterpret_cast<char*>(area.data());
    char* end = reinterpret_cast<char*>(area.data() + area.size());
    auto stream = MakeWideStream(1);
    stream.buffer.putNext = &next;
    stream.buffer.putEnd = &end;
    const GuestLocale::OutputIterator out = Export<WideTimePutExport>("Fr7j8dMsy4s")(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, g_wideFill, &time, 'Y', '\0');
    Require(out.failed && area[0] == u'2' && area[1] == u'0' && stream.accepted == u"2" && stream.offered == std::vector<int>{'2', '6'}, "do_put writes through the put area and overflow, and a failed write marks the iterator");
    RequireWideDoPutThrows('\xe9', '\0');
    RequireWideDoPutThrows('d', '\x80');
    RequireWideDoPutThrows('c', '\0');
    const GuestLocale::WideTimePutVtable vtable{g_testFacet, Export<WideTimePutExport>("Fr7j8dMsy4s")};
    const GuestLocale::Facet facet{&vtable.facet, 1, 0};
    GuestLocale::IosBase ios{};
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    auto patternStream = MakeWideStream(64);
    const std::u16string pattern = u"%Y-%m-%d %H:%M:%S %Ey 中 %";
    const auto* first = reinterpret_cast<const std::uint16_t*>(pattern.data());
    const GuestLocale::OutputIterator patternOut = Export<WideTimePutPatternExport>("Sc0lXhQG5Ko")(&facet, GuestLocale::OutputIterator{false, &patternStream.buffer}, &ios, g_wideFill, &time, first, first + pattern.size());
    Require(!patternOut.failed && patternStream.accepted == u"2026-11-14 09:05:07 26 中 %", "the wide pattern formats through do_put");
#ifndef _WIN32
    Require(setenv_nid_postfix("TZ", "ABCDEFGHIJKLMNO0", 1) == 0, "set a zone with a 15-letter name");
    Require(WideDoPut(time, 'Z', '\0') == u"ABCDEFGHIJKLMNO", "do_put retries with a larger buffer when the first leaves no room for the terminator");
    Require(setenv_nid_postfix("TZ", "ABCDEFGHIJKLMNOP0", 1) == 0, "set a zone with a 16-letter name");
    RequireWideDoPutThrows('Z', '\0');
#endif
}

using WideTimePutGetcatExport = std::size_t (APS5_VABI*)(GuestLocale::Facet**, const GuestLocale::Implementation* const*);

std::u16string PutThroughBuiltWideFacet(const GuestLocale::Facet* facet, const std::u16string& pattern) {
    const GuestTm time = MakeWideDoPutTime();
    GuestLocale::IosBase ios{};
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    auto stream = MakeWideStream(64);
    const auto* first = reinterpret_cast<const std::uint16_t*>(pattern.data());
    const GuestLocale::OutputIterator out = Export<WideTimePutPatternExport>("Sc0lXhQG5Ko")(facet, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, g_wideFill, &time, first, first + pattern.size());
    Require(!out.failed && out.buffer == &stream.buffer, "time_put<wchar_t>::put through the facet _Getcat built keeps a good iterator");
    return stream.accepted;
}

void CheckWideTimePutGetcat() {
    const auto getcat = Export<WideTimePutGetcatExport>("bF2uVCqVhBY");
    const GuestLocale::Implementation* const* classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(getcat(nullptr, nullptr) == 5 && getcat(nullptr, classic) == 5, "_Getcat without a facet pointer returns the time category");
    GuestLocale::Facet existing{&g_testFacet, 7, 0};
    GuestLocale::Facet* facet = &existing;
    Require(getcat(&facet, nullptr) == 5 && facet == &existing && existing.references == 7, "_Getcat leaves an existing facet alone without reading the locale");
    facet = nullptr;
    Require(getcat(&facet, classic) == 5 && facet != nullptr && facet->references == 0, "_Getcat builds a facet with no references when the facet pointer holds null");
    Require(PutThroughBuiltWideFacet(facet, u"%Y-%m-%d %H:%M:%S %Ey 中 %") == u"2026-11-14 09:05:07 26 中 %", "the facet _Getcat builds formats through time_put<wchar_t>::do_put");
    facet->vtable->retain(facet);
    facet->vtable->retain(facet);
    Require(facet->references == 2 && facet->vtable->release(facet) == nullptr && facet->vtable->release(facet) == facet && facet->references == 0, "the built facet's _Incref and _Decref count references and _Decref returns the facet when none remain");
    facet->vtable->deleteObject(facet);
    GuestLocale::Implementation named{{nullptr, 1, 0}, nullptr, 0, 0, false, "fr_FR.UTF-8"};
    const GuestLocale::Implementation* namedPointer = &named;
    const GuestLocale::Implementation* noImplementation = nullptr;
    GuestLocale::Facet* namedFacet = nullptr;
    GuestLocale::Facet* emptyFacet = nullptr;
    Require(getcat(&namedFacet, &namedPointer) == 5 && getcat(&emptyFacet, &noImplementation) == 5 && PutThroughBuiltWideFacet(namedFacet, u"%A %B %p") == u"Saturday November AM" && PutThroughBuiltWideFacet(emptyFacet, u"%A %B %p") == u"Saturday November AM", "a locale with another name or no implementation gets the C locale's facet, as setlocale accepts only C");
    namedFacet->vtable->deleteObject(namedFacet);
    emptyFacet->vtable->deleteObject(emptyFacet);
    facet = nullptr;
    try {
        getcat(&facet, nullptr);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_Getcat") != std::string::npos && facet == nullptr, "_Getcat with a null locale throws before building a facet");
        return;
    }
    Require(false, "_Getcat with a null locale must throw");
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    Require(argc == 2, "expected the libSceLibcInternal path");
#ifdef _WIN32
    g_library = LoadLibraryA(argv[1]);
#else
    g_library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(g_library != nullptr, "load libSceLibcInternal");
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckNumPutForwarding();
    CheckLibcNumPut();
    CheckTimePutForwarding();
    CheckPatternWalk();
    CheckStreamCtype();
    CheckLibcTimePut();
    CheckWideTimePutForwarding();
    CheckWidePatternWalk();
    CheckWideClassicNarrow();
    CheckWideStreamCtype();
    CheckWideStreamWrite();
    CheckWideTimePutDoPut();
    CheckWideTimePutGetcat();

    std::puts("libc internal num_put and time_put put checks passed");
    return 0;
}
