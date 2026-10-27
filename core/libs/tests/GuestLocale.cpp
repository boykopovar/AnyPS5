#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <climits>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <limits>
#include <string>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;

extern std::uint64_t _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
extern std::uintptr_t _ZTVSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[17];
extern std::uintptr_t _ZTVSt7codecvtIwc9_MbstatetE_nid_postfix[13];
extern std::uintptr_t _ZTVSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[14];
extern std::uintptr_t _ZTVSt7collateIcE_nid_postfix[9];
extern std::uintptr_t _ZTVSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[13];
extern std::uintptr_t _ZTVSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[13];
extern std::uintptr_t _ZTVSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[17];
extern std::uintptr_t _ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[14];

}

static void Require(bool condition) {
    if (!condition) throw std::runtime_error("Guest locale check failed");
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

template<typename TAction>
static void RequireException(TAction action) {
    bool caught = false;
    try {
        action();
    } catch (const std::exception&) {
        caught = true;
    }
    Require(caught);
}

static void CheckInputFacetObjects() {
    Require(_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix == 0 && _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix == 0);
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 7;
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 9;
    Require(_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix == 7 && _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix == 9);
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
    for (const auto slot : _ZTVSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix) Require(slot == 0);
}

static void CheckCodecvtWideVtableObject() {
    for (std::uintptr_t& slot : _ZTVSt7codecvtIwc9_MbstatetE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
}

static void CheckCollateVtableObject() {
    Require(_ZTVSt7collateIcE_nid_postfix[0] == 0 && _ZTVSt7collateIcE_nid_postfix[1] == 0);
    for (std::size_t index = 2; index < std::size(_ZTVSt7collateIcE_nid_postfix); ++index) Require(_ZTVSt7collateIcE_nid_postfix[index] != 0);
    const auto* vtable = reinterpret_cast<const GuestLocale::CollateVtable*>(&_ZTVSt7collateIcE_nid_postfix[2]);
    GuestLocale::CollateFacet facet{};
    facet.base.references = 0;
    vtable->facet.destroy(&facet.base);
    vtable->facet.retain(&facet.base);
    Require(facet.base.references == 1);
    Require(vtable->facet.release(&facet.base) == &facet.base && facet.base.references == 0);
    Require(vtable->compare(&facet, "abc", "abc" + 3, "abd", "abd" + 3) < 0);
    Require(vtable->compare(&facet, "abc", "abc" + 3, "abc", "abc" + 3) == 0);
    Require(vtable->compare(&facet, "abd", "abd" + 3, "abc", "abc" + 3) > 0);
    GuestLocale::String result{};
    Require(vtable->transform(&result, &facet, "abc", "abc" + 3) == &result);
    Require(result.size == 3 && result.capacity == 15 && std::memcmp(result.buffer, "abc", 3) == 0);
    Require(vtable->hash(&facet, "abc", "abc" + 3) == -1792535898324117685LL);
}

static void CheckGuestCalls() {
    auto* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(locale != nullptr && locale == _ZNSt6locale16_GetgloballocaleEv_nid_postfix());
    Require(_ZNSt6locale5_InitEv_nid_postfix() == locale);
    Require(_ZNSt6locale5_InitEv_nid_postfix() == locale);
    auto* classic = _ZNSt6locale7classicEv_nid_postfix();
    Require(classic != nullptr && classic->implementation == locale);
    Require(_ZNSt6locale7classicEv_nid_postfix() == classic);
    const std::byte* table = nullptr;
    std::memcpy(&table, locale, sizeof(table));
    void (APS5_VABI *retain)(GuestLocale::Facet*) = nullptr;
    GuestLocale::Facet* (APS5_VABI *release)(GuestLocale::Facet*) = nullptr;
    std::memcpy(&retain, table + 0x10, sizeof(retain));
    std::memcpy(&release, table + 0x18, sizeof(release));
    const auto initial = locale->base.references;
    retain(&locale->base);
    Require(locale->base.references == initial + 1);
    Require(release(&locale->base) == nullptr);
    Require(locale->base.references == initial);
    Require(release(&locale->base) == nullptr && locale->base.references == initial);
    RequireException([&] { retain(nullptr); });
    RequireException([&] { locale->base.vtable->deleteObject(&locale->base); });
    Require(locale->base.references == initial);
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) {
        threads.emplace_back([&] {
            for (int iteration = 0; iteration < 10000; ++iteration) {
                retain(&locale->base);
                release(&locale->base);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(locale->base.references == initial);
    Require(locale->facetCount == 1 && locale->facets[0] == nullptr);
    Require(std::strcmp(locale->name, "C") == 0 && !locale->transparent);
}

static void CheckLocinfoAlignment() {
    alignas(16) std::array<std::byte, 80> bytes;
    bytes.fill(std::byte{0x5a});
    auto* storage = reinterpret_cast<GuestLocale::LocinfoStorage*>(bytes.data() + 8);
    _ZNSt8_LocinfoC1EPKc_nid_postfix(storage, "C");

    for (std::size_t index = 0; index < bytes.size(); ++index) Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}));
    _ZNSt8_LocinfoD1Ev_nid_postfix(storage);

    RequireException([&] { _ZNSt8_LocinfoC1EPKc_nid_postfix(storage, "unknown-locale"); });
    RequireException([] { _ZNSt8_LocinfoC1EPKc_nid_postfix(nullptr, "C"); });
}

static void CheckCharacterTables() {
    const auto* classification = _Getpctype_nid_postfix();
    const auto* lower = _Getptolower_nid_postfix();
    const auto* upper = _Getptoupper_nid_postfix();
    Require(classification[-1] == 0 && lower[-1] == -1 && upper[-1] == -1);
    Require(classification['A'] == 0x03 && classification['a'] == 0x11);
    Require(classification['G'] == 0x02 && classification['0'] == 0x21);
    Require(classification[' '] == 0x04 && classification['\t'] == 0x4c0);
    Require(classification['\n'] == 0xc0 && classification['!'] == 0x08);
    for (int value = 0; value < 256; ++value) {
        Require(lower[value] == (value >= 'A' && value <= 'Z' ? value + 32 : value));
        Require(upper[value] == (value >= 'a' && value <= 'z' ? value - 32 : value));
        if (value >= 128) Require(classification[value] == 0);
    }
}

static void CheckCharacterConversions() {
    const auto* classification = _Getpctype_nid_postfix();
    for (std::uint16_t value = 0; value < 128; ++value) {
        std::array<char, MB_LEN_MAX + 1> bytes{};
        bytes.fill('!');
        std::mbstate_t encodeState{};
        Require(_Wctombx_nid_postfix(bytes.data(), value, &encodeState) == 1);
        Require(bytes[0] == static_cast<char>(value) && bytes[1] == '!');
        std::array<std::uint16_t, 2> wide{0xffff, 0x1234};
        std::mbstate_t decodeState{};
        Require(_Mbtowcx_nid_postfix(wide.data(), bytes.data(), 2, &decodeState) == (value == 0 ? 0 : 1));
        Require(wide[0] == value && wide[1] == 0x1234);
        const bool alnum = (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
        Require(((classification[static_cast<unsigned char>(bytes[0])] & 0x232) != 0) == alnum);
    }
    std::mbstate_t state{};
    char byte{};
    std::uint16_t wide{};
    RequireException([&] { _Mbtowcx_nid_postfix(&wide, "]", 0, &state); });
    RequireException([&] { _Mbtowcx_nid_postfix(nullptr, "]", 1, &state); });
    RequireException([&] { _Mbtowcx_nid_postfix(&wide, nullptr, 1, &state); });
    RequireException([&] { _Mbtowcx_nid_postfix(&wide, "]", 1, nullptr); });
    RequireException([&] { _Wctombx_nid_postfix(nullptr, ']', &state); });
    RequireException([&] { _Wctombx_nid_postfix(&byte, ']', nullptr); });
}

static void CheckStreamDestruction() {
    struct GuardedStream {
        std::uint64_t before = 0x12345678;
        GuestLocale::IosBase stream{};
        std::uint64_t after = 0x87654321;
    } object;
    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    _ZNSt8ios_baseD2Ev_nid_postfix(&object.stream);

    Require(object.stream.locale == nullptr && object.before == 0x12345678 && object.after == 0x87654321);
    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    object.stream.callbacks = &object;
    RequireException([&] { _ZNSt8ios_baseD2Ev_nid_postfix(&object.stream); });
    Require(object.stream.locale == &_ZSt21_sceLibcClassicLocale_nid_postfix);
}

static void CheckStreamWidth() {
    struct GuardedStream {
        std::uint64_t before = 0x12345678;
        GuestLocale::IosBase stream{};
        std::uint64_t after = 0x87654321;
    } object;
    object.stream.flags = 0x4000;
    object.stream.precision = 6;
    const auto manipulator = _ZSt4setwi_nid_postfix(12);
    Require(manipulator.argument == 12 && manipulator.function != nullptr);
    Require(manipulator.function(&object.stream, manipulator.argument) == &object.stream);
    Require(object.stream.width == 12 && object.stream.flags == 0x4000 && object.stream.precision == 6);
    Require(object.before == 0x12345678 && object.after == 0x87654321);
    const auto reset = _ZSt4setwi_nid_postfix(0);
    reset.function(&object.stream, reset.argument);
    Require(object.stream.width == 0);
    RequireException([&] { manipulator.function(nullptr, manipulator.argument); });
}

static void CheckStreamPrecision() {
    struct GuardedStream {
        std::uint64_t before = 0x12345678;
        GuestLocale::IosBase stream{};
        std::uint64_t after = 0x87654321;
    } object;
    object.stream.flags = 0x4000;
    object.stream.width = 5;
    const auto manipulator = _ZSt12setprecisioni_nid_postfix(12);
    Require(manipulator.argument == 12 && manipulator.function != nullptr);
    Require(manipulator.function(&object.stream, manipulator.argument) == &object.stream);
    Require(object.stream.precision == 12 && object.stream.flags == 0x4000 && object.stream.width == 5);
    Require(object.before == 0x12345678 && object.after == 0x87654321);
    const auto zero = _ZSt12setprecisioni_nid_postfix(0);
    zero.function(&object.stream, zero.argument);
    Require(object.stream.precision == 0);
    const auto negative = _ZSt12setprecisioni_nid_postfix(-3);
    negative.function(&object.stream, negative.argument);
    Require(object.stream.precision == -3);
    RequireException([&] { manipulator.function(nullptr, manipulator.argument); });
}

static void CheckStandardStreams() {
    struct GuardedStream {
        std::uint64_t before = 0x12345678;
        GuestLocale::IosBase stream{};
        std::uint64_t after = 0x87654321;
    } streams[9];
    auto* classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    for (auto& object : streams) object.stream.locale = classic;
    RequireException([&] { _ZNSt8ios_base7_AddstdEPS__nid_postfix(nullptr); });
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[0].stream);
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[1].stream);

    Require(streams[0].stream.standardStream == 1 && streams[1].stream.standardStream == 2);
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[0].stream);

    Require(streams[0].stream.standardStream == 1);
    _ZNSt8ios_baseD2Ev_nid_postfix(&streams[0].stream);

    Require(streams[0].stream.locale == classic && streams[0].stream.standardStream == 1);
    _ZNSt8ios_baseD2Ev_nid_postfix(&streams[0].stream);

    Require(streams[0].stream.locale == nullptr && streams[0].stream.standardStream == 1);
    streams[0].stream.locale = classic;
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[0].stream);

    Require(streams[0].stream.standardStream == 1);
    _ZNSt8ios_baseD2Ev_nid_postfix(&streams[1].stream);

    Require(streams[1].stream.locale == nullptr && streams[1].stream.standardStream == 2);
    for (int index = 2; index < 7; ++index) {
        _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[index].stream);

        Require(streams[index].stream.standardStream == index + 1);
    }
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[7].stream);
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&streams[8].stream);

    Require(streams[7].stream.standardStream == 8 && streams[8].stream.standardStream == 8);
    _ZNSt8ios_baseD2Ev_nid_postfix(&streams[7].stream);

    Require(streams[7].stream.locale == classic);
    _ZNSt8ios_baseD2Ev_nid_postfix(&streams[8].stream);

    Require(streams[8].stream.locale == nullptr);
    for (int index = 2; index < 7; ++index) {
        _ZNSt8ios_baseD2Ev_nid_postfix(&streams[index].stream);

        Require(streams[index].stream.locale == nullptr && streams[index].stream.standardStream == index + 1);
    }
    for (auto& object : streams) Require(object.before == 0x12345678 && object.after == 0x87654321);
}

static void CheckWideCaseTranslation() {
    const auto* lower = _Getptolower_nid_postfix();
    const auto* upper = _Getptoupper_nid_postfix();
    Require(_Towctrans_nid_postfix(u'A', 1) == u'a' && _Towctrans_nid_postfix(u'a', 2) == u'A');
    *__error_nid_postfix() = 0;
    for (std::uint32_t value = 0; value < 256; ++value) {
        Require(_Towctrans_nid_postfix(value, 1) == static_cast<std::uint32_t>(lower[value]));
        Require(_Towctrans_nid_postfix(value, 2) == static_cast<std::uint32_t>(upper[value]));
    }
    const std::uint32_t uncased[] = {0x100u, 0x391u, 0x3b1u, 0xffffu, 0xffffffffu};
    for (const std::uint32_t value : uncased) {
        Require(_Towctrans_nid_postfix(value, 1) == value && _Towctrans_nid_postfix(value, 2) == value);
    }
    Require(*__error_nid_postfix() == 0);
    const std::uint64_t invalidDescriptors[] = {0, 3};
    for (const std::uint64_t descriptor : invalidDescriptors) {
        *__error_nid_postfix() = 0;
        Require(_Towctrans_nid_postfix(u'A', descriptor) == u'A' && _Towctrans_nid_postfix(u'7', descriptor) == u'7');
        Require(*__error_nid_postfix() == 22);
    }
}

static void CheckWideNumPutVtableObject() {
    for (std::uintptr_t& slot : _ZTVSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
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

static void CheckTimePut() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &locale) == 5);
    const auto put = reinterpret_cast<const GuestLocale::TimePutVtable*>(facet->vtable)->put;
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
    GuestLocale::OutputIterator out = put(facet, {false, &stream.buffer}, nullptr, '*', &time, 'Y', 0);
    Require(!out.failed && out.buffer == &stream.buffer && next == storage + 4 && std::memcmp(storage, "2026", 4) == 0 && stream.offered == 0);
    out = put(facet, out, nullptr, '*', &time, 'c', 0);
    Require(!out.failed && next == end && std::memcmp(storage + 4, "Fri ", 4) == 0 && stream.accepted == "Oct  2 13:05:09 2026");
    stream.accepted.clear();
    out = put(facet, out, nullptr, '*', &time, 'Y', 'E');
    out = put(facet, out, nullptr, '*', &time, 'y', 'O');
    out = put(facet, out, nullptr, '*', &time, '\0', 0);
    out = put(facet, out, nullptr, '*', &time, 'k', 0);
    Require(!out.failed && stream.accepted == "202626%%k");
    stream.capacity = static_cast<int>(stream.accepted.size()) + 1;
    stream.offered = 0;
    out = put(facet, out, nullptr, '*', &time, 'T', 0);
    Require(out.failed && stream.offered == 8 && stream.accepted == "202626%%k1");
    next = nullptr;
    stream.offered = 0;
    out = put(facet, {false, &stream.buffer}, nullptr, '*', &time, 'H', 0);
    Require(out.failed && stream.offered == 2);
    out = put(facet, {false, nullptr}, nullptr, '*', &time, 'H', 0);
    Require(out.failed && out.buffer == nullptr);
    facet->vtable->deleteObject(facet);
}

GuestLocale::String* AssignShortString(GuestLocale::String* result, const char* text) {
    *result = {};
    result->size = std::strlen(text);
    result->capacity = 15;
    std::memcpy(result->buffer, text, result->size);
    return result;
}

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }
char APS5_VABI TestDecimalPoint(const GuestLocale::NumpunctFacet*) { return ','; }
char APS5_VABI TestThousandsSep(const GuestLocale::NumpunctFacet*) { return '.'; }
GuestLocale::String* APS5_VABI TestGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "\1\2"); }
GuestLocale::String* APS5_VABI TestFalsename(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "no"); }
GuestLocale::String* APS5_VABI TestTruename(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "yes"); }
const GuestLocale::NumpunctVtable g_testNumpunctVtable{{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease}, TestDecimalPoint, TestThousandsSep, TestGrouping, TestFalsename, TestTruename};

template<typename TPut>
static std::string NumPut(TPut put, GuestLocale::Implementation** locale, std::int32_t flags, std::int32_t precision, std::int32_t width) {
    char storage[64];
    char* next = storage;
    char* end = storage + sizeof(storage);
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 0};
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.precision = precision;
    ios.width = width;
    ios.locale = locale;
    const GuestLocale::OutputIterator out = put(GuestLocale::OutputIterator{false, &stream.buffer}, &ios);
    Require(!out.failed && out.buffer == &stream.buffer && ios.width == 0 && stream.offered == 0);
    return std::string(storage, next);
}

static void CheckNumPut() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &locale) == 4);
    const auto* put = reinterpret_cast<const GuestLocale::NumPutVtable*>(facet->vtable);
    const double infinity = std::numeric_limits<double>::infinity();
    Require(NumPut([&](auto out, auto* ios) { return put->putLong(facet, out, ios, '*', -42); }, classic, 0x408, 6, 8) == "-*****42");
    Require(NumPut([&](auto out, auto* ios) { return put->putLong(facet, out, ios, '*', 255); }, classic, 0x2c8, 6, 10) == "0X******FF");
    Require(NumPut([&](auto out, auto* ios) { return put->putUnsignedLong(facet, out, ios, '*', std::numeric_limits<std::uint64_t>::max()); }, classic, 0x20, 6, 0) == "1777777777777777777777");
    Require(NumPut([&](auto out, auto* ios) { return put->putLongLong(facet, out, ios, '*', std::numeric_limits<std::int64_t>::min()); }, classic, 0x2, 6, 25) == "-9223372036854775808*****");
    Require(NumPut([&](auto out, auto* ios) { return put->putBool(facet, out, ios, '*', true); }, classic, 0x8002, 6, 7) == "true***");
    Require(NumPut([&](auto out, auto* ios) { return put->putBool(facet, out, ios, '*', false); }, classic, 0, 6, 0) == "0");
    Require(NumPut([&](auto out, auto* ios) { return put->putPointer(facet, out, ios, '*', reinterpret_cast<const void*>(0x1234)); }, classic, 0, 6, 6) == "**1234");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', 1234567.891); }, classic, 0, 6, 0) == "1.23457e+06");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', 1234567.891); }, classic, 0x1000, 2, 0) == "1234567.89");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', -0.000123); }, classic, 0xa00, 3, 12) == "**-1.230E-04");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', -1.5); }, classic, 0x1808, 6, 12) == "-***0x1.8p+0");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', 12345678901234.0); }, classic, 0x1000, 0, 0) == "12345678901234");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', -infinity); }, classic, 0x400, 6, 0) == "-inf");
    Require(NumPut([&](auto out, auto* ios) { return put->putLongDouble(facet, out, ios, '*', -2.25L); }, classic, 0, 3, 0) == "-2.25");
    RequireException([&] { NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', 1.0); }, classic, 0x1000, -45, 0); });
    GuestLocale::Facet punct{&g_testNumpunctVtable.facet, 1, 0};
    std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIcE2idE_nid_postfix + 1);
    facets.back() = &punct;
    GuestLocale::Implementation custom{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* customPointer = &custom;
    Require(NumPut([&](auto out, auto* ios) { return put->putLong(facet, out, ios, '*', 1234567); }, &customPointer, 0, 6, 0) == "12.34.56.7");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', 1234567.5); }, &customPointer, 0x1000, 1, 0) == "12.34.56.7,5");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', infinity); }, &customPointer, 0x1000, 1, 0) == "inf");
    Require(NumPut([&](auto out, auto* ios) { return put->putDouble(facet, out, ios, '*', -infinity); }, &customPointer, 0, 6, 0) == "-inf");
    Require(NumPut([&](auto out, auto* ios) { return put->putBool(facet, out, ios, '*', true); }, &customPointer, 0x8000, 6, 5) == "**yes");
    facet->vtable->deleteObject(facet);
}

static void CheckFacetGetcat() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* codecvt = nullptr;
    Require(_ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix(&codecvt, &locale) == 2 && codecvt != nullptr);
    auto* codecvtVtable = reinterpret_cast<const GuestLocale::CodecvtVtable*>(codecvt->vtable);
    Require(codecvtVtable->alwaysNoconv(codecvt) && codecvtVtable->encoding(codecvt) == 1 && codecvtVtable->maxLength(codecvt) == 1);
    const char* nextFrom = nullptr;
    char target[4]{};
    char* nextTo = nullptr;
    Require(codecvtVtable->out(codecvt, nullptr, "ab", "ab" + 2, &nextFrom, target, target + 4, &nextTo) == 3);
    Require(nextFrom == "ab" && nextTo == target);
    Require(codecvtVtable->in(codecvt, nullptr, "ab", "ab" + 2, &nextFrom, target, target + 4, &nextTo) == 3);
    nextTo = reinterpret_cast<char*>(0x10);
    Require(codecvtVtable->unshift(codecvt, nullptr, nullptr, nullptr, &nextTo) == 0 && nextTo == nullptr);
    Require(codecvtVtable->length(codecvt, nullptr, "ab", "ab" + 2, 5) == 2);
    Require(codecvtVtable->length(codecvt, nullptr, "ab", "ab" + 2, 1) == 1);
    GuestLocale::Facet* punct = nullptr;
    Require(_ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&punct, &locale) == 4 && punct != nullptr);
    auto* punctVtable = reinterpret_cast<const GuestLocale::NumpunctVtable*>(punct->vtable);
    auto* punctFacet = reinterpret_cast<const GuestLocale::NumpunctFacet*>(punct);
    Require(punctVtable->decimalPoint(punctFacet) == '.' && punctVtable->thousandsSep(punctFacet) == ',');
    GuestLocale::String name{};
    Require(punctVtable->grouping(&name, punctFacet) == &name && name.size == 0);
    const char* contents = name.size <= 15 ? name.buffer : name.pointer;
    Require(punctVtable->falsename(&name, punctFacet)->size == 5 && std::memcmp(contents, "false", 5) == 0);
    contents = name.size <= 15 ? name.buffer : name.pointer;
    Require(punctVtable->truename(&name, punctFacet)->size == 4 && std::memcmp(contents, "true", 4) == 0);
    GuestLocale::Facet* numPut = nullptr;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&numPut, &locale) == 4 && numPut != nullptr);
    GuestLocale::Facet* timePut = nullptr;
    Require(_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&timePut, &locale) == 5 && timePut != nullptr);
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 11;
    Require(_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix == 11);
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
    for (GuestLocale::Facet* facet : {codecvt, punct, numPut, timePut}) {
        facet->vtable->retain(facet);
        facet->vtable->retain(facet);
        Require(facet->references == 2);
        facet->vtable->deleteObject(facet);
    }
    Require(_ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix(nullptr, &locale) == 2);
    Require(_ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&punct, &locale) == 4);
}

static void CheckNumPutVtableObject() {
    Require(sizeof(_ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix) == kNumPutCharVtableSlots * sizeof(std::uintptr_t));
    for (std::uintptr_t& slot : _ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
}

static void CheckNumGetWideVtableObject() {
    for (std::uintptr_t& slot : _ZTVSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
}

static void CheckTimeGetVtableObject() {
    for (std::uintptr_t& slot : _ZTVSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
}

static void CheckTimeGetWideVtableObject() {
    for (std::uintptr_t& slot : _ZTVSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
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
    CheckGuestCalls();
    CheckLocinfoAlignment();
    CheckCharacterTables();
    CheckCharacterConversions();
    CheckStreamDestruction();
    CheckStreamWidth();
    CheckStreamPrecision();
    CheckStandardStreams();
    CheckWideCaseTranslation();
    CheckInputFacetObjects();
    CheckCodecvtWideVtableObject();
    CheckWideNumPutVtableObject();
    CheckFacetGetcat();
    CheckCollateVtableObject();
    CheckTimeGetVtableObject();
    CheckTimeGetWideVtableObject();
    CheckNumGetWideVtableObject();
    CheckNumPutVtableObject();
    _Locksyslock_nid_postfix();
    _Locksyslock_nid_postfix();
    _Unlocksyslock_nid_postfix();
    _Unlocksyslock_nid_postfix();
    CheckTimePut();
    CheckNumPut();

    std::cout << "Guest locale checks passed\n";
}
