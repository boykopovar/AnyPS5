#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
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
extern std::uint64_t _ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
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

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

template<typename Text> struct Sink {
    GuestLocale::Streambuf buffer{};
    GuestLocale::StreambufVtable vtable{};
    std::array<Text, 256> storage{};
    Text* next = nullptr;
    Text* end = nullptr;
    std::int32_t count = 0;
};

template<typename Text> struct Bench {
    const void* facet = nullptr;
    GuestLocale::SceV2IosBase stream{};
    Sink<Text> sink;
};

template<typename Text> using Put = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, Text, const GuestTm*, char, char);

static std::string g_overflowed;

static int APS5_VABI Overflow(GuestLocale::Streambuf*, int character) {
    g_overflowed.push_back(static_cast<char>(character));
    return character;
}

template<typename Text> static void Arm(Bench<Text>& bench, std::int32_t room) {
    bench.sink.buffer.vtable = &bench.sink.vtable;
    bench.sink.vtable.overflow = Overflow;
    bench.sink.next = bench.sink.storage.data();
    bench.sink.end = bench.sink.storage.data() + bench.sink.storage.size();
    bench.sink.count = room;
    bench.sink.buffer.putNext = reinterpret_cast<char**>(&bench.sink.next);
    bench.sink.buffer.putEnd = reinterpret_cast<char**>(&bench.sink.end);
    bench.sink.buffer.reservedCounts[2] = &bench.sink.count;
    g_overflowed.clear();
}

static GuestTm Sample() {
    GuestTm time{};
    time.tm_sec = 6;
    time.tm_min = 5;
    time.tm_hour = 4;
    time.tm_mday = 3;
    time.tm_mon = 1;
    time.tm_year = 124;
    time.tm_wday = 6;
    time.tm_yday = 33;
    return time;
}

template<typename Text> static void Check(Bench<Text>& bench, const std::string& name, char specifier, char modifier, const std::basic_string<Text>& expected) {
    Arm(bench, static_cast<std::int32_t>(bench.sink.storage.size()));
    const GuestTm time = Sample();
    *__error_nid_postfix() = 7;
    const auto out = Slot<Put<Text>>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, Text('*'), &time, specifier, modifier);
    const std::basic_string<Text> written(bench.sink.storage.data(), bench.sink.next);
    Require(written == expected, name + ": wrote other text");
    Require(!out.failed && out.buffer == &bench.sink.buffer, name + ": iterator stays good");
    Require(*__error_nid_postfix() == 7, name + ": errno is kept");
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
    narrow.facet = locale->facets[_ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix];
    narrow.stream.locale = &locale;
    static Bench<char16_t> wide;
    wide.facet = locale->facets[_ZNSt8__sce_v28time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix];
    wide.stream.locale = &locale;
    Require(narrow.facet != nullptr && wide.facet != nullptr, "_Makeloc installs time_put");
    if (failures != 0) std::_Exit(1);

    Check<char>(narrow, "%Y", 'Y', 0, "2024");
    Check<char>(narrow, "%d", 'd', 0, "03");
    Check<char>(narrow, "a modifier goes before the specifier", 'd', 'O', "03");
    Check<char>(narrow, "%%", '%', 0, "%");
    Check<char>(narrow, "%n", 'n', 0, "\n");
    Check<char>(narrow, "%c grows the buffer past 16 characters", 'c', 0, "Sat Feb  3 04:05:06 2024");
    Check<char>(narrow, "%A", 'A', 0, "Saturday");

    Arm(narrow, 2);
    const GuestTm time = Sample();
    const auto overflowed = Slot<Put<char>>(narrow.facet, 4)(narrow.facet, {false, &narrow.sink.buffer}, &narrow.stream, '*', &time, 'Y', 0);
    Require(std::string(narrow.sink.storage.data(), narrow.sink.next) == "20" && g_overflowed == "24" && !overflowed.failed, "text beyond the buffer's room goes through overflow");
    const auto missing = Slot<Put<char>>(narrow.facet, 4)(narrow.facet, {false, nullptr}, &narrow.stream, '*', &time, 'Y', 0);
    Require(missing.failed && missing.buffer == nullptr, "no buffer fails the iterator");

    Check<char16_t>(wide, "wide %Y", 'Y', 0, u"2024");
    Check<char16_t>(wide, "wide %B", 'B', 0, u"February");
    Check<char16_t>(wide, "wide %c writes nothing, as wcsftime fills a 16-unit buffer with only the leading mark", 'c', 0, u"");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
