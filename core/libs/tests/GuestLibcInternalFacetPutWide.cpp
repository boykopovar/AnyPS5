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
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide facet put: %s\n", message);
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

using WideString = std::u16string;

const std::uint16_t* Units(const WideString& text) {
    return reinterpret_cast<const std::uint16_t*>(text.data());
}

struct TestStreambuf {
    GuestLocale::Streambuf buffer;
    WideString accepted;
    int offered;
    int capacity;
    std::uint16_t* next;
    std::uint16_t* end;
};

int APS5_VABI TestOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<TestStreambuf*>(self);
    ++stream->offered;
    if (static_cast<int>(stream->accepted.size()) >= stream->capacity) return -1;
    stream->accepted += static_cast<char16_t>(character);
    return character;
}

const GuestLocale::StreambufVtable g_streambufVtable{{}, TestOverflow};

struct StreamHolder {
    TestStreambuf stream;

    explicit StreamHolder(int capacity, std::uint16_t* first = nullptr, std::uint16_t* last = nullptr)
        : stream{{&g_streambufVtable, {}, nullptr, {}, nullptr}, {}, 0, capacity, first, last} {
        stream.buffer.putNext = reinterpret_cast<char**>(&stream.next);
        stream.buffer.putEnd = reinterpret_cast<char**>(&stream.end);
    }

    StreamHolder(const StreamHolder&) = delete;
    StreamHolder& operator=(const StreamHolder&) = delete;
};

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }
const GuestLocale::FacetVtable g_testFacet{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease};

struct Call {
    int slot;
    const GuestLocale::Facet* self;
    GuestLocale::Streambuf* buffer;
    bool failed;
    GuestLocale::IosBase* stream;
    std::uint16_t fill;
    std::uint64_t integer;
    long double floating;
    const void* pointer;
};

Call g_call{};

GuestLocale::OutputIterator Record(int slot, const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill) {
    g_call = Call{slot, self, out.buffer, out.failed, stream, fill, 0, 0, nullptr};
    return GuestLocale::OutputIterator{!out.failed, out.buffer};
}

GuestLocale::OutputIterator APS5_VABI RecordBool(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, bool value) {
    const auto result = Record(0, self, out, stream, fill);
    g_call.integer = value ? 1 : 0;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    const auto result = Record(1, self, out, stream, fill);
    g_call.integer = static_cast<std::uint64_t>(value);
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordUnsignedLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    const auto result = Record(2, self, out, stream, fill);
    g_call.integer = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordLongLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    const auto result = Record(3, self, out, stream, fill);
    g_call.integer = static_cast<std::uint64_t>(value);
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordUnsignedLongLong(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    const auto result = Record(4, self, out, stream, fill);
    g_call.integer = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordDouble(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, double value) {
    const auto result = Record(5, self, out, stream, fill);
    g_call.floating = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordLongDouble(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, long double value) {
    const auto result = Record(6, self, out, stream, fill);
    g_call.floating = value;
    return result;
}

GuestLocale::OutputIterator APS5_VABI RecordPointer(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const void* value) {
    const auto result = Record(7, self, out, stream, fill);
    g_call.pointer = value;
    return result;
}

const GuestLocale::WideNumPutVtable g_recordingNumPut{g_testFacet, RecordBool, RecordLong, RecordUnsignedLong, RecordLongLong, RecordUnsignedLongLong, RecordDouble, RecordLongDouble, RecordPointer};

constexpr std::uint16_t WideFill = 0x263a;

template<typename TValue>
using NumPutExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, TValue);
using TimePutExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const GuestTm*, char, char);
using TimePutPatternExport = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const GuestTm*, const std::uint16_t*, const std::uint16_t*);

template<typename TValue>
void CheckNumPutForward(const char* nid, int slot, TValue value) {
    const auto put = Export<NumPutExport<TValue>>(nid);
    GuestLocale::Facet facet{&g_recordingNumPut.facet, 1, 0};
    GuestLocale::IosBase ios{};
    StreamHolder holder(0);
    g_call = Call{-1};
    const GuestLocale::OutputIterator out = put(&facet, GuestLocale::OutputIterator{false, &holder.stream.buffer}, &ios, WideFill, value);
    Require(g_call.slot == slot, "num_put<wchar_t>::put calls the do_put slot of its value type");
    Require(g_call.self == &facet && g_call.stream == &ios && g_call.fill == WideFill, "num_put<wchar_t>::put passes the facet, stream and wide fill");
    Require(g_call.buffer == &holder.stream.buffer && !g_call.failed, "num_put<wchar_t>::put passes the output iterator");
    Require(out.failed && out.buffer == &holder.stream.buffer, "num_put<wchar_t>::put returns what do_put returns");
    Require(holder.stream.offered == 0, "num_put<wchar_t>::put writes nothing itself");
}

void CheckNumPutForwarding() {
    CheckNumPutForward<bool>("mY9FWooxqJY", 0, true);
    Require(g_call.integer == 1, "put(bool) passes the value");
    CheckNumPutForward<std::int64_t>("USLhWp7sZoU", 1, std::numeric_limits<std::int64_t>::min());
    Require(g_call.integer == static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::min()), "put(long) passes the value");
    CheckNumPutForward<std::uint64_t>("qtpzdwMMCPc", 2, std::numeric_limits<std::uint64_t>::max());
    Require(g_call.integer == std::numeric_limits<std::uint64_t>::max(), "put(unsigned long) passes the value");
    CheckNumPutForward<std::int64_t>("ryykbHJ04Cw", 3, -7);
    Require(g_call.integer == static_cast<std::uint64_t>(-7), "put(long long) passes the value");
    CheckNumPutForward<std::uint64_t>("lmb3oBpMNPU", 4, 0x8000000000000001ull);
    Require(g_call.integer == 0x8000000000000001ull, "put(unsigned long long) passes the value");
    CheckNumPutForward<double>("V7aIsVIsIIA", 5, -2.5);
    Require(g_call.floating == -2.5L, "put(double) passes the value");
    CheckNumPutForward<long double>("vCIFGeI6adI", 6, 1.0L / 3.0L);
    Require(g_call.floating == 1.0L / 3.0L, "put(long double) passes the value");
    int object = 0;
    CheckNumPutForward<const void*>("xfOSCbCiY44", 7, &object);
    Require(g_call.pointer == &object, "put(const void*) passes the value");
}

struct TimeCall {
    const GuestLocale::Facet* self;
    GuestLocale::IosBase* stream;
    std::uint16_t fill;
    const GuestTm* time;
};

TimeCall g_timeCall{};
int g_timeCalls = 0;

GuestLocale::OutputIterator APS5_VABI RecordTimePut(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    ++g_timeCalls;
    g_timeCall = TimeCall{self, stream, fill, time};
    auto* buffer = reinterpret_cast<TestStreambuf*>(out.buffer);
    buffer->accepted += u'<';
    buffer->accepted += static_cast<char16_t>(static_cast<unsigned char>(modifier != '\0' ? modifier : '_'));
    buffer->accepted += static_cast<char16_t>(static_cast<unsigned char>(format != '\0' ? format : '0'));
    buffer->accepted += u'>';
    return GuestLocale::OutputIterator{out.failed || format == 'F', out.buffer};
}

const GuestLocale::WideTimePutVtable g_recordingTimePut{g_testFacet, RecordTimePut};

WideString PutPattern(const GuestLocale::Facet* facet, GuestLocale::Implementation** locale, const WideString& pattern, bool* failed = nullptr) {
    GuestLocale::IosBase ios{};
    ios.locale = locale;
    GuestTm time{};
    StreamHolder holder(64);
    g_timeCalls = 0;
    g_timeCall = {};
    const GuestLocale::OutputIterator out = Export<TimePutPatternExport>("Sc0lXhQG5Ko")(facet, GuestLocale::OutputIterator{false, &holder.stream.buffer}, &ios, WideFill, &time, Units(pattern), Units(pattern) + pattern.size());
    Require(out.buffer == &holder.stream.buffer, "time_put<wchar_t>::put returns the output iterator");
    if (g_timeCalls != 0) Require(g_timeCall.self == facet && g_timeCall.stream == &ios && g_timeCall.fill == WideFill && g_timeCall.time == &time, "time_put<wchar_t>::put passes the facet, stream, wide fill and time to do_put");
    if (failed != nullptr) *failed = out.failed;
    else Require(!out.failed, "time_put<wchar_t>::put keeps a good iterator");
    return holder.stream.accepted;
}

void CheckTimePutForwarding() {
    const auto put = Export<TimePutExport>("Nt6eyVKm+Z4");
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    GuestLocale::IosBase ios{};
    GuestTm time{};
    StreamHolder holder(0);
    g_timeCalls = 0;
    GuestLocale::OutputIterator out = put(&facet, GuestLocale::OutputIterator{false, &holder.stream.buffer}, &ios, WideFill, &time, 'Y', 'E');
    Require(g_timeCalls == 1 && g_timeCall.self == &facet && g_timeCall.stream == &ios && g_timeCall.fill == WideFill && g_timeCall.time == &time, "time_put<wchar_t>::put(spec, mod) calls do_put once with its arguments");
    Require(holder.stream.accepted == u"<EY>" && !out.failed && out.buffer == &holder.stream.buffer, "time_put<wchar_t>::put(spec, mod) passes the specifier and modifier");
    out = put(&facet, out, &ios, WideFill, &time, 'F', '\0');
    Require(holder.stream.accepted == u"<EY><_F>" && out.failed, "time_put<wchar_t>::put(spec, mod) returns what do_put returns");
}

void CheckPatternWalk() {
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutPattern(&facet, classic, WideString()).empty() && g_timeCalls == 0, "an empty pattern writes nothing");
    Require(PutPattern(&facet, classic, u"plain ☺ text") == u"plain ☺ text" && g_timeCalls == 0, "ordinary wide characters are copied");
    Require(PutPattern(&facet, classic, u"a%Yb%Ec%Od%%e") == u"a<_Y>b<Ec><Od><_%>e" && g_timeCalls == 4, "each conversion calls do_put in pattern order");
    Require(PutPattern(&facet, classic, u"%Qx%#y") == u"<Qx><#y>", "Q and # are modifiers");
    Require(PutPattern(&facet, classic, u"%k%z") == u"<_k><_z>", "any specifier goes to do_put");
    Require(PutPattern(&facet, classic, u"x%") == u"x%" && g_timeCalls == 0, "a trailing percent is copied");
    Require(PutPattern(&facet, classic, u"x%E") == u"x%E" && g_timeCalls == 0, "a trailing modifier is copied with its percent");
    Require(PutPattern(&facet, classic, u"%EE") == u"<EE>", "a modifier can be the specifier");
    Require(PutPattern(&facet, classic, u"％Yĥ") == u"％Yĥ" && g_timeCalls == 0, "characters that do not narrow to a percent are copied");
    Require(PutPattern(&facet, classic, u"%ĥ%Eé") == u"<_0><E0>", "a specifier without a narrow form reaches do_put as the null default");
    WideString nulls{u'%', u'\0', u'%', u'E', u'\0'};
    Require(PutPattern(&facet, classic, nulls) == u"<_0><E0>", "a null specifier goes to do_put");
    bool failed = false;
    Require(PutPattern(&facet, classic, u"%Fz", &failed) == u"<_F>z" && failed, "the iterator do_put returns is used for the rest of the pattern");
}

std::string g_narrowFallbacks;

char APS5_VABI TestNarrow(const GuestLocale::Facet*, std::uint16_t character, char fallback) {
    g_narrowFallbacks += fallback == '\0' ? '0' : 'x';
    switch (character) {
    case 0xff05: return '%';
    case 0x00c9: return 'E';
    case 0x00f0: return 'd';
    case u'%': return 'p';
    default: return character < 0x80 ? static_cast<char>(character) : '?';
    }
}

struct TestCtypeVtable {
    GuestLocale::FacetVtable facet;
    const void* classifyCaseAndWidenMethods[10];
    char (APS5_VABI *narrow)(const GuestLocale::Facet* self, std::uint16_t character, char fallback);
    const void* narrowRange;
};

static_assert(offsetof(TestCtypeVtable, narrow) == 0x70);

const TestCtypeVtable g_testCtype{g_testFacet, {}, TestNarrow, nullptr};

void CheckStreamCtype() {
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    Require(PutPattern(&facet, &_ZSt21_sceLibcClassicLocale_nid_postfix, u"%Y") == u"<_Y>", "the classic locale narrows without a ctype<wchar_t> facet");
    Require(_ZNSt5ctypeIwE2idE_nid_postfix != 0, "time_put<wchar_t>::put looks up ctype<wchar_t> through its locale id");
    GuestLocale::Facet ctype{&g_testCtype.facet, 1, 0};
    std::vector<GuestLocale::Facet*> facets(_ZNSt5ctypeIwE2idE_nid_postfix + 1);
    facets.back() = &ctype;
    GuestLocale::Implementation custom{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* customPointer = &custom;
    g_narrowFallbacks.clear();
    Require(PutPattern(&facet, &customPointer, u"1％Éð2%Y％") == u"1<Ed>2%Y％", "the stream's ctype<wchar_t> narrows the pattern and the literal characters keep their wide value");
    Require(g_narrowFallbacks == "00000000", "narrow is called once per examined character with a null default");
    Require(PutPattern(&facet, &customPointer, u"a％É") == u"a％E", "a trailing modifier is copied narrowed after the original percent");
}

void CheckStreamBuffer() {
    GuestLocale::Facet facet{&g_recordingTimePut.facet, 1, 0};
    GuestLocale::IosBase ios{};
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestTm time{};
    const WideString pattern = u"ab☺cd";
    std::array<std::uint16_t, 4> storage{};
    StreamHolder direct(8, storage.data(), storage.data() + 3);
    GuestLocale::OutputIterator out = Export<TimePutPatternExport>("Sc0lXhQG5Ko")(&facet, GuestLocale::OutputIterator{false, &direct.stream.buffer}, &ios, WideFill, &time, Units(pattern), Units(pattern) + pattern.size());
    Require(!out.failed && storage[0] == u'a' && storage[1] == u'b' && storage[2] == 0x263a && storage[3] == 0, "wide characters fill the put area first");
    Require(direct.stream.next == storage.data() + 3 && direct.stream.accepted == u"cd" && direct.stream.offered == 2, "a full put area sends the rest through overflow");
    StreamHolder failing(2);
    out = Export<TimePutPatternExport>("Sc0lXhQG5Ko")(&facet, GuestLocale::OutputIterator{false, &failing.stream.buffer}, &ios, WideFill, &time, Units(pattern), Units(pattern) + pattern.size());
    Require(out.failed && failing.stream.accepted == u"ab" && failing.stream.offered == 5, "a failed overflow marks the iterator and later characters are still offered");
    out = Export<TimePutPatternExport>("Sc0lXhQG5Ko")(&facet, GuestLocale::OutputIterator{false, nullptr}, &ios, WideFill, &time, Units(pattern), Units(pattern) + 1);
    Require(out.failed && out.buffer == nullptr, "a null stream buffer marks the iterator failed");
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
    CheckTimePutForwarding();
    CheckPatternWalk();
    CheckStreamCtype();
    CheckStreamBuffer();

    std::puts("libc internal num_put<wchar_t> and time_put<wchar_t> put checks passed");
    return 0;
}
