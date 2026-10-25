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
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;

extern std::uint64_t _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
extern std::uintptr_t _ZTVSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[17];
extern std::uintptr_t _ZTVSt7codecvtIwc9_MbstatetE_nid_postfix[13];
extern std::uintptr_t _ZTVSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[14];
extern std::uintptr_t _ZTVSt7collateIcE_nid_postfix[9];

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
    for (std::uintptr_t& slot : _ZTVSt7collateIcE_nid_postfix) {
        Require(slot == 0);
        slot = 0x1111;
        Require(slot == 0x1111);
        slot = 0;
    }
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
    Require(classification[' '] == 0x04 && classification['\t'] == 0xc0);
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
    CheckStandardStreams();
    CheckWideCaseTranslation();
    CheckInputFacetObjects();
    CheckCodecvtWideVtableObject();
    CheckWideNumPutVtableObject();
    CheckFacetGetcat();
    CheckCollateVtableObject();
    _Locksyslock_nid_postfix();
    _Locksyslock_nid_postfix();
    _Unlocksyslock_nid_postfix();
    _Unlocksyslock_nid_postfix();

    std::cout << "Guest locale checks passed\n";
}
