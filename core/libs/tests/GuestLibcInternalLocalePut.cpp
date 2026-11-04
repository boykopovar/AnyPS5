#include "prx/libSceLibcInternal/include/Api.hpp"
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

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale put: %s\n", message);
    std::abort();
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

const GuestLocale::StreambufVtable g_testStreambufVtable{{}, TestOverflow};

template<typename TPut>
std::string Put(TPut put, std::int32_t flags, std::int32_t precision, std::int32_t width) {
    char storage[64];
    char* next = storage;
    char* end = storage + sizeof(storage);
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 0};
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.precision = precision;
    ios.width = width;
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    const GuestLocale::OutputIterator out = put(GuestLocale::OutputIterator{false, &stream.buffer}, &ios);
    Require(!out.failed && out.buffer == &stream.buffer, "the iterator keeps its buffer and does not fail");
    Require(ios.width == 0 && stream.offered == 0, "the width is consumed and the put area takes every character");
    return std::string(storage, next);
}

GuestLocale::OutputIterator APS5_VABI DerivedPutLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char, std::int64_t value) {
    char* next = *out.buffer->putNext;
    *next = value == 1 ? 'T' : 'F';
    *out.buffer->putNext = next + 1;
    stream->width = 0;
    return out;
}

void CheckNumPut() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &locale) == 4 && facet != nullptr, "num_put facet created");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutLong_nid_no_patch(facet, out, ios, '*', -42); }, 0x408, 6, 8) == "-*****42", "long with showpos and internal fill");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutLong_nid_no_patch(facet, out, ios, '*', 255); }, 0x2c8, 6, 10) == "0X******FF", "long in uppercase hex with showbase");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutUnsignedLong_nid_no_patch(facet, out, ios, '*', std::numeric_limits<std::uint64_t>::max()); }, 0x20, 6, 0) == "1777777777777777777777", "unsigned long in octal");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutLongLong_nid_no_patch(facet, out, ios, '*', std::numeric_limits<std::int64_t>::min()); }, 0x2, 6, 25) == "-9223372036854775808*****", "long long left adjusted");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutUnsignedLongLong_nid_no_patch(facet, out, ios, '*', 0xabcdefULL); }, 0x40, 6, 0) == "abcdef", "unsigned long long in hex");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutBool_nid_no_patch(facet, out, ios, '*', true); }, 0x8002, 6, 7) == "true***", "bool with boolalpha");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutBool_nid_no_patch(facet, out, ios, '*', false); }, 0, 6, 0) == "0", "bool without boolalpha");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutPointer_nid_no_patch(facet, out, ios, '*', reinterpret_cast<const void*>(0x1234)); }, 0, 6, 6) == "**1234", "pointer right adjusted");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutDouble_nid_no_patch(facet, out, ios, '*', 1234567.891); }, 0, 6, 0) == "1.23457e+06", "double in general notation");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutDouble_nid_no_patch(facet, out, ios, '*', -0.000123); }, 0xa00, 3, 12) == "**-1.230E-04", "double in uppercase scientific notation");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutLongDouble_nid_no_patch(facet, out, ios, '*', -2.25L); }, 0, 3, 0) == "-2.25", "long double");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutLongDouble_nid_no_patch(facet, out, ios, '*', 0.5L); }, 0x1000, 2, 0) == "0.50", "long double in fixed notation");
    facet->vtable->deleteObject(facet);
}

void CheckDerivedFacet() {
    GuestLocale::NumPutVtable derived = *LibcNumPutMethods_nid_no_patch();
    derived.putLong = DerivedPutLong;
    GuestLocale::Facet facet{&derived.facet, 1, 0};
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutBool_nid_no_patch(&facet, out, ios, '*', true); }, 0, 6, 0) == "T", "true without boolalpha calls the derived long do_put");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutBool_nid_no_patch(&facet, out, ios, '*', false); }, 0, 6, 0) == "F", "false without boolalpha calls the derived long do_put");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutBool_nid_no_patch(&facet, out, ios, '*', false); }, 0x8000, 6, 0) == "false", "bool with boolalpha uses numpunct");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalNumPutDoPutLong_nid_no_patch(&facet, out, ios, '*', 7); }, 0, 6, 0) == "7", "the base long do_put does not dispatch to the override");
}

void CheckTimePut() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &locale) == 5 && facet != nullptr, "time_put facet created");
    GuestTm time{};
    time.tm_year = 126;
    time.tm_mon = 9;
    time.tm_mday = 2;
    time.tm_hour = 13;
    time.tm_min = 5;
    time.tm_sec = 9;
    time.tm_wday = 5;
    time.tm_yday = 274;
    char storage[8];
    char* next = storage;
    char* end = storage + sizeof(storage);
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 100};
    GuestLocale::OutputIterator out = sceLibcInternalTimePutDoPut_nid_no_patch(facet, {false, &stream.buffer}, nullptr, '*', &time, 'Y', 0);
    Require(!out.failed && out.buffer == &stream.buffer && next == storage + 4 && std::memcmp(storage, "2026", 4) == 0, "%Y fills the put area");
    out = sceLibcInternalTimePutDoPut_nid_no_patch(facet, out, nullptr, '*', &time, 'c', 0);
    Require(!out.failed && next == end && std::memcmp(storage + 4, "Fri ", 4) == 0 && stream.accepted == "Oct  2 13:05:09 2026", "%c overflows into the stream buffer");
    stream.accepted.clear();
    out = sceLibcInternalTimePutDoPut_nid_no_patch(facet, out, nullptr, '*', &time, 'Y', 'E');
    out = sceLibcInternalTimePutDoPut_nid_no_patch(facet, out, nullptr, '*', &time, 'd', 'O');
    Require(!out.failed && stream.accepted == "202602", "%EY and %Od use the C locale forms");
    stream.capacity = static_cast<int>(stream.accepted.size()) + 1;
    stream.offered = 0;
    out = sceLibcInternalTimePutDoPut_nid_no_patch(facet, out, nullptr, '*', &time, 'T', 0);
    Require(out.failed && stream.offered == 8 && stream.accepted == "2026021", "a refused character fails the iterator");
    out = sceLibcInternalTimePutDoPut_nid_no_patch(facet, {false, nullptr}, nullptr, '*', &time, 'H', 0);
    Require(out.failed && out.buffer == nullptr, "a null buffer fails the iterator");
    facet->vtable->deleteObject(facet);
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckNumPut();
    CheckDerivedFacet();
    CheckTimePut();

    std::puts("libc internal locale put checks passed");
    return 0;
}
