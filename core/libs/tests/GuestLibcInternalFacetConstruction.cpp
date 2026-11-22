#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/FacetConstruction.hpp"
#include "prx/libSceLibcInternal/include/FacetReferences.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libSceLibcInternal/include/WideCollate.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal facet construction: %s\n", message);
    std::abort();
}

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

template<typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct InputVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

template<typename Element> struct InputBuffer {
    GuestLocale::Streambuf base;
    Element* gnext;
    Element* gend;
};

int APS5_VABI InputUnderflow(GuestLocale::Streambuf*) { return -1; }

const InputVtable g_inputVtable{{}, InputUnderflow, InputUnderflow};

template<typename Element> InputBuffer<Element> MakeInput(Element* first, Element* last) {
    InputBuffer<Element> buffer{};
    buffer.base.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&g_inputVtable);
    buffer.gnext = first;
    buffer.gend = last;
    buffer.base.reserved[6] = &buffer.gnext;
    buffer.base.reservedCounts[2] = &buffer.gend;
    return buffer;
}

GuestLocale::InputIterator IteratorOver(GuestLocale::Streambuf* buffer) {
    GuestLocale::InputIterator iterator{};
    iterator.words[0] = reinterpret_cast<std::uintptr_t>(buffer);
    return iterator;
}

struct OutputVtable {
    const void* reserved[4];
    int (APS5_VABI *overflow)(GuestLocale::Streambuf* self, int character);
};

int g_overflowCalls = 0;

int APS5_VABI OutputOverflow(GuestLocale::Streambuf*, int) {
    ++g_overflowCalls;
    return -1;
}

const OutputVtable g_outputVtable{{}, OutputOverflow};

template<typename Element> struct OutputBuffer {
    GuestLocale::Streambuf base;
    Element* pnext;
    Element* pend;
    std::array<Element, 8> storage;
};

template<typename Element> void ResetOutput(OutputBuffer<Element>& buffer) {
    buffer.base = {};
    buffer.base.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&g_outputVtable);
    buffer.storage = {};
    buffer.pnext = buffer.storage.data();
    buffer.pend = buffer.storage.data() + buffer.storage.size();
    buffer.base.putNext = reinterpret_cast<char**>(&buffer.pnext);
    buffer.base.putEnd = reinterpret_cast<char**>(&buffer.pend);
    g_overflowCalls = 0;
}

GuestLocale::Facet* HeapFacet() {
    return static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(0x40));
}

void CheckLifetime(GuestLocale::Facet* facet, std::uint32_t references) {
    Require(facet->references == references, "the constructor stores the reference count");
    facet->vtable->retain(facet);
    Require(facet->references == references + 1, "retain increments the reference count");
    GuestLocale::Facet* released = facet->vtable->release(facet);
    Require(released == nullptr && facet->references == references, "release keeps a facet that still has references");
}

void CheckCollate() {
    const auto* vtable = &GuestWideCollateVtable_nid_no_patch()->facet;
    GuestLocale::CollateFacet facet{};
    _ZNSt7collateIwEC2Em_nid_postfix(&facet, 5);
    Require(facet.base.vtable == vtable && facet.base.references == 5, "collate<wchar_t>(size_t) installs the wide vtable");
    Require(facet.collation == _Getpcostate_nid_postfix() && facet.wideCollation == _Getpwcostate_nid_postfix(), "collate<wchar_t> reads the collation states");
    facet = {};
    _ZNSt7collateIwEC1Em_nid_postfix(&facet, 6);
    Require(facet.base.vtable == vtable && facet.base.references == 6 && facet.collation != nullptr, "collate<wchar_t>(size_t) C1 matches C2");
    facet = {};
    _ZNSt7collateIwEC1EPKcm_nid_postfix(&facet, "C", 7);
    Require(facet.base.references == 7 && facet.wideCollation == _Getpwcostate_nid_postfix(), "a named collate<wchar_t> reads the collation states");
    facet = {};
    _ZNSt7collateIwEC2EPKcm_nid_postfix(&facet, "", 8);
    Require(facet.base.references == 8 && facet.collation == _Getpcostate_nid_postfix(), "an unnamed collate<wchar_t> is the classic locale");
    Require(Throws([&] { _ZNSt7collateIwEC2EPKcm_nid_postfix(&facet, "xx_YY", 0); }), "an unknown locale name is rejected");
    Require(Throws([&] { _ZNSt7collateIwEC2EPKcm_nid_postfix(&facet, nullptr, 0); }), "a null locale name is rejected");
    GuestLocale::LocinfoStorage locinfo{};
    facet = {};
    _ZNSt7collateIwEC1ERKSt8_Locinfom_nid_postfix(&facet, &locinfo, 9);
    Require(facet.base.references == 9 && facet.collation == _Getpcostate_nid_postfix(), "collate<wchar_t>(_Locinfo) C1");
    facet = {};
    _ZNSt7collateIwEC2ERKSt8_Locinfom_nid_postfix(&facet, &locinfo, 10);
    Require(facet.base.references == 10 && facet.wideCollation == _Getpwcostate_nid_postfix(), "collate<wchar_t>(_Locinfo) C2");
    facet = {};
    _ZNSt7collateIwE5_InitERKSt8_Locinfo_nid_postfix(&facet, &locinfo);
    Require(facet.collation == _Getpcostate_nid_postfix() && facet.wideCollation == _Getpwcostate_nid_postfix(), "collate<wchar_t>::_Init stores both states");
    facet = {};
    _ZNSt7collateIcE5_InitERKSt8_Locinfo_nid_postfix(&facet, &locinfo);
    Require(facet.collation == _Getpcostate_nid_postfix() && facet.wideCollation == _Getpwcostate_nid_postfix(), "collate<char>::_Init stores both states");
    Require(Throws([&] { _ZNSt7collateIwEC2Em_nid_postfix(nullptr, 0); }), "a null object is rejected");

    const char16_t first[] = u"abc";
    const char16_t second[] = u"abd";
    _ZNSt7collateIwEC2Em_nid_postfix(&facet, 1);
    Require(sceLibcInternalWideCollateCompare_nid_no_patch(&facet, first, first + 3, second, second + 3) == -1, "the constructed facet compares wide text");

}

void CheckNumGet() {
    GuestLocale::Facet narrow{};
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&narrow, 2);
    Require(narrow.vtable == LibcInternalCharNumGetVtable_nid_no_patch(), "num_get<char>(size_t) installs the narrow vtable");
    Require(narrow.references == 2, "num_get<char>(size_t) stores the reference count");
    GuestLocale::LocinfoStorage locinfo{};
    narrow = {};
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(&narrow, 3);
    Require(narrow.vtable == LibcInternalCharNumGetVtable_nid_no_patch() && narrow.references == 3, "num_get<char> C1");
    narrow = {};
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(&narrow, &locinfo, 4);
    Require(narrow.vtable == LibcInternalCharNumGetVtable_nid_no_patch() && narrow.references == 4, "num_get<char>(_Locinfo) C2");
    narrow = {};
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&narrow, &locinfo, 5);
    Require(narrow.vtable == LibcInternalCharNumGetVtable_nid_no_patch() && narrow.references == 5, "num_get<char>(_Locinfo) C1");

    GuestLocale::Facet wide{};
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(&wide, 2);
    CheckLifetime(&wide, 2);
    const GuestLocale::FacetVtable* wideVtable = wide.vtable;
    wide = {};
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&wide, 3);
    Require(wide.vtable == wideVtable && wide.references == 3, "num_get<wchar_t> C1 and C2 share a vtable");
    wide = {};
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(&wide, &locinfo, 4);
    Require(wide.vtable == wideVtable && wide.references == 4, "num_get<wchar_t>(_Locinfo) C2");
    wide = {};
    _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(&wide, &locinfo, 5);
    Require(wide.vtable == wideVtable && wide.references == 5, "num_get<wchar_t>(_Locinfo) C1");
    Require(Throws([&] { _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(&wide, nullptr, 0); }), "a null _Locinfo is rejected");

    GuestLocale::Implementation* classic = _ZNSt6locale7classicEv_nid_postfix()->implementation;
    GuestLocale::Facet* created = nullptr;
    Require(_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&created, &classic) == 4 && created != nullptr, "num_get<wchar_t>::_Getcat builds a facet in category 4");
    Require(created->vtable == wideVtable && created->references == 0, "_Getcat constructs with no references");
    GuestLocale::Facet* kept = created;
    Require(_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&created, &classic) == 4 && created == kept, "_Getcat keeps an existing facet");

    char16_t digits[] = u"123x";
    auto input = MakeInput<char16_t>(digits, digits + 4);
    GuestLocale::IosBase stream{};
    stream.locale = &classic;
    stream.flags = 0x2;
    std::int32_t state = 0;
    std::int64_t value = 0;
    const auto* vtable = reinterpret_cast<const GuestLocale::NumGetVtable*>(created->vtable);
    const GuestLocale::InputIterator end{};
    const GuestLocale::InputIterator after = vtable->getLong(created, IteratorOver(&input.base), end, &stream, &state, &value);
    Require(value == 123 && state == 0, "a wide num_get facet parses a decimal long");
    Require(reinterpret_cast<char16_t*>(input.gnext) - digits == 3 && after.words[0] == reinterpret_cast<std::uintptr_t>(&input.base), "the parse stops at the first non-digit");
    state = 0;
    bool flag = false;
    char16_t one[] = u"1";
    auto boolInput = MakeInput<char16_t>(one, one + 1);
    vtable->getBool(created, IteratorOver(&boolInput.base), end, &stream, &state, &flag);
    Require(flag && state == 1, "a wide num_get facet parses a numeric true");
    char16_t zero[] = u"0";
    boolInput = MakeInput<char16_t>(zero, zero + 1);
    state = 0;
    vtable->getBool(created, IteratorOver(&boolInput.base), end, &stream, &state, &flag);
    Require(!flag && state == 1, "a wide num_get facet parses a numeric false");
    char16_t letters[] = u"x";
    boolInput = MakeInput<char16_t>(letters, letters + 1);
    state = 0;
    vtable->getBool(created, IteratorOver(&boolInput.base), end, &stream, &state, &flag);
    Require(state == 2, "a wide num_get facet rejects text for a numeric bool");
    stream.flags |= 0x8000;
    Require(Throws([&] { vtable->getBool(created, IteratorOver(&boolInput.base), end, &stream, &state, &flag); }), "an alphabetic bool needs a numpunct facet");
    stream.flags = 0x2;

    state = 0;
    void* pointer = nullptr;
    char16_t hexadecimal[] = u"1f";
    auto pointerInput = MakeInput<char16_t>(hexadecimal, hexadecimal + 2);
    vtable->getPointer(created, IteratorOver(&pointerInput.base), end, &stream, &state, &pointer);
    Require(pointer == reinterpret_cast<void*>(0x1f) && state == 1, "a wide num_get facet parses a hexadecimal pointer");
    char16_t nonsense[] = u"zz";
    state = 0;
    pointerInput = MakeInput<char16_t>(nonsense, nonsense + 2);
    vtable->getPointer(created, IteratorOver(&pointerInput.base), end, &stream, &state, &pointer);
    Require(pointer == nullptr && state == 2, "a wide num_get facet fails on a non-hexadecimal pointer");

    GuestLocaleFacade localeView{classic};
    char field[32]{};
    char16_t prefixed[] = u"0x1Fz";
    auto fieldInput = MakeInput<char16_t>(prefixed, prefixed + 5);
    GuestLocale::InputIterator fieldFirst = IteratorOver(&fieldInput.base);
    GuestLocale::InputIterator fieldLast{};
    Require(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(created, field, &fieldFirst, &fieldLast, 0, &localeView) == 16, "_Getifld reads a hexadecimal prefix when the base field is empty");
    Require(std::strcmp(field, "1F") == 0 && fieldInput.gnext - prefixed == 4 && *fieldInput.gnext == u'z', "_Getifld leaves the digits and stops at the first other element");
    char16_t plain[] = u"-42";
    fieldInput = MakeInput<char16_t>(plain, plain + 3);
    fieldFirst = IteratorOver(&fieldInput.base);
    Require(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(created, field, &fieldFirst, &fieldLast, 0x10, &localeView) == 10 && std::strcmp(field, "-42") == 0, "_Getifld keeps the sign and reports base ten");
    char16_t none[] = u"qq";
    fieldInput = MakeInput<char16_t>(none, none + 2);
    fieldFirst = IteratorOver(&fieldInput.base);
    Require(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(created, field, &fieldFirst, &fieldLast, 0x10, &localeView) == 10 && field[0] == '\0', "_Getifld clears the digits when there are none");
    Require(Throws([&] { _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(created, nullptr, &fieldFirst, &fieldLast, 0x10, &localeView); }), "_Getifld rejects a null digit buffer");

    GuestLocale::Facet narrowFacet{};
    _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&narrowFacet, 1);
    char narrowDigits[] = "7";
    auto narrowBoolInput = MakeInput<char>(narrowDigits, narrowDigits + 1);
    flag = false;
    state = 0;
    _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb_nid_postfix(&narrowFacet, IteratorOver(&narrowBoolInput.base), end, &stream, &state, &flag);
    Require(state == 3, "the narrow do_get(bool) export rejects a number above one");
    char narrowPlain[] = "12";
    auto narrowFieldInput = MakeInput<char>(narrowPlain, narrowPlain + 2);
    GuestLocale::InputIterator narrowFirst = IteratorOver(&narrowFieldInput.base);
    Require(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(&narrowFacet, field, &narrowFirst, &fieldLast, 0x20, &localeView) == 8 && std::strcmp(field, "12") == 0, "the narrow _Getifld reports base eight for the octal field");
    created->vtable->deleteObject(created);

    GuestLocale::Facet* unnamed = nullptr;
    Require(_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&unnamed, nullptr) == 4 && unnamed != nullptr, "_Getcat without a locale builds the classic facet");
    unnamed->vtable->deleteObject(unnamed);
}

void CheckNumPut() {
    const auto* narrowVtable = reinterpret_cast<const GuestLocale::FacetVtable*>(&_ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[2]);
    GuestLocale::Facet narrow{};
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&narrow, 2);
    Require(narrow.vtable == narrowVtable && narrow.references == 2, "num_put<char>(size_t) installs the narrow vtable");
    GuestLocale::LocinfoStorage locinfo{};
    narrow = {};
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(&narrow, 3);
    Require(narrow.vtable == narrowVtable && narrow.references == 3, "num_put<char> C1");
    narrow = {};
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&narrow, &locinfo, 4);
    Require(narrow.vtable == narrowVtable && narrow.references == 4, "num_put<char>(_Locinfo) C1");
    narrow = {};
    _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(&narrow, &locinfo, 5);
    Require(narrow.vtable == narrowVtable && narrow.references == 5, "num_put<char>(_Locinfo) C2");

    OutputBuffer<char> output{};
    ResetOutput(output);
    GuestLocale::OutputIterator out{false, &output.base};
    out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(&narrow, out, 'x', 3);
    out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm_nid_postfix(&narrow, out, "yz", 2);
    Require(!out.failed && out.buffer == &output.base && std::string(output.storage.data(), 5) == "xxxyz", "num_put<char>::_Rep and _Put write through the put area");
    out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(&narrow, out, 'q', 6);
    Require(out.failed && g_overflowCalls == 3 && output.storage[7] == 'q', "an overflowing put area marks the iterator failed");
    ResetOutput(output);
    out = {false, nullptr};
    out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm_nid_postfix(&narrow, out, "ab", 2);
    Require(out.failed && out.buffer == nullptr, "an iterator without a buffer is failed");
    out = {false, nullptr};
    out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm_nid_postfix(&narrow, out, "ab", 0);
    Require(!out.failed, "an empty put leaves the iterator untouched");
}

void CheckMoney() {
    GuestLocale::LocinfoStorage locinfo{};
    GuestLocale::Facet charGet{};
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&charGet, 2);
    CheckLifetime(&charGet, 2);
    const GuestLocale::FacetVtable* charGetVtable = charGet.vtable;
    charGet = {};
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(&charGet, 3);
    Require(charGet.vtable == charGetVtable && charGet.references == 3, "money_get<char> C1 and C2 share a vtable");
    charGet = {};
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&charGet, &locinfo, 4);
    Require(charGet.vtable == charGetVtable && charGet.references == 4, "money_get<char>(_Locinfo) C1");
    charGet = {};
    _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(&charGet, &locinfo, 5);
    Require(charGet.vtable == charGetVtable && charGet.references == 5, "money_get<char>(_Locinfo) C2");

    GuestLocale::Facet wideGet{};
    _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&wideGet, 2);
    CheckLifetime(&wideGet, 2);
    const GuestLocale::FacetVtable* wideGetVtable = wideGet.vtable;
    Require(wideGetVtable != charGetVtable, "money_get<wchar_t> has its own vtable");
    wideGet = {};
    _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(&wideGet, &locinfo, 4);
    Require(wideGet.vtable == wideGetVtable && wideGet.references == 4, "money_get<wchar_t>(_Locinfo) C1");
    wideGet = {};
    _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(&wideGet, &locinfo, 5);
    Require(wideGet.vtable == wideGetVtable && wideGet.references == 5, "money_get<wchar_t>(_Locinfo) C2");

    GuestLocale::Facet charPut{};
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&charPut, 2);
    CheckLifetime(&charPut, 2);
    const GuestLocale::FacetVtable* charPutVtable = charPut.vtable;
    charPut = {};
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(&charPut, 3);
    Require(charPut.vtable == charPutVtable && charPut.references == 3, "money_put<char> C1 and C2 share a vtable");
    charPut = {};
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&charPut, &locinfo, 4);
    Require(charPut.vtable == charPutVtable && charPut.references == 4, "money_put<char>(_Locinfo) C1");
    charPut = {};
    _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(&charPut, &locinfo, 5);
    Require(charPut.vtable == charPutVtable && charPut.references == 5, "money_put<char>(_Locinfo) C2");

    GuestLocale::Facet widePut{};
    _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(&widePut, 2);
    CheckLifetime(&widePut, 2);
    const GuestLocale::FacetVtable* widePutVtable = widePut.vtable;
    Require(widePutVtable != charPutVtable, "money_put<wchar_t> has its own vtable");
    widePut = {};
    _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&widePut, 3);
    Require(widePut.vtable == widePutVtable && widePut.references == 3, "money_put<wchar_t> C1 and C2 share a vtable");
    widePut = {};
    _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(&widePut, &locinfo, 4);
    Require(widePut.vtable == widePutVtable && widePut.references == 4, "money_put<wchar_t>(_Locinfo) C1");

    using Getcat = std::size_t (APS5_VABI*)(GuestLocale::Facet**, const GuestLocale::Implementation* const*);
    const std::array<std::pair<Getcat, const GuestLocale::FacetVtable*>, 3> getcats{{
        {_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix, charGetVtable},
        {_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix, charPutVtable},
        {_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix, widePutVtable}}};
    GuestLocale::Implementation* classic = _ZNSt6locale7classicEv_nid_postfix()->implementation;
    for (const auto& [getcat, vtable] : getcats) {
        GuestLocale::Facet* created = nullptr;
        Require(getcat(&created, &classic) == 3 && created != nullptr, "the monetary facets belong to category 3");
        Require(created->vtable == vtable && created->references == 0, "_Getcat constructs with no references");
        GuestLocale::Facet* kept = created;
        Require(getcat(&created, &classic) == 3 && created == kept, "_Getcat keeps an existing facet");
        created->vtable->deleteObject(created);
    }


    OutputBuffer<char> narrowOutput{};
    ResetOutput(narrowOutput);
    GuestLocale::OutputIterator out{false, &narrowOutput.base};
    out = _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(out, '$', 2);
    const GuestStringConstIterator<char> text{nullptr, nullptr, "1.5"};
    out = _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIcEEEm_nid_postfix(out, text, 3);
    Require(!out.failed && std::string(narrowOutput.storage.data(), 5) == "$$1.5", "money_put<char>::_Rep and _Put write through the put area");
    out = _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(out, '0', 4);
    Require(out.failed, "money_put<char> overflow marks the iterator failed");

    OutputBuffer<std::uint16_t> wideOutput{};
    ResetOutput(wideOutput);
    out = {false, &wideOutput.base};
    out = _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix(out, u'$', 2);
    Require(!out.failed && wideOutput.storage[0] == '$' && wideOutput.storage[1] == '$', "money_put<wchar_t>::_Rep writes through the put area");
    out = _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix(out, u'0', 7);
    Require(out.failed && g_overflowCalls == 1, "money_put<wchar_t> overflow marks the iterator failed");
}

void CheckTime() {
    GuestLocale::LocinfoStorage locinfo{};
    alignas(16) std::array<unsigned char, 0x40> storage{};
    auto* facet = reinterpret_cast<GuestLocale::Facet*>(storage.data());
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(facet, 2);
    CheckLifetime(facet, 2);
    const GuestLocale::FacetVtable* vtable = facet->vtable;
    const auto* get = reinterpret_cast<const GuestLocale::TimeGetVtable*>(vtable);
    Require(get->dateOrder(facet) == 2, "a time_get<char> starts with the month-day-year order");
    Require(std::strcmp(*reinterpret_cast<const char* const*>(storage.data() + 0x10), ":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday") == 0, "time_get<char> keeps the weekday table");
    Require(*reinterpret_cast<void* const*>(storage.data() + 0x30) == _Getpmbstate_nid_postfix() && *reinterpret_cast<void* const*>(storage.data() + 0x38) == _Getpwcstate_nid_postfix(), "time_get<char> keeps the conversion states");

    storage.fill(0);
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(facet, 3);
    Require(facet->vtable == vtable && facet->references == 3 && get->dateOrder(facet) == 2, "time_get<char>(size_t) C1");
    storage.fill(0);
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1EPKcm_nid_postfix(facet, "C", 4);
    Require(facet->vtable == vtable && facet->references == 4 && get->dateOrder(facet) == 2, "time_get<char>(const char*) C1");
    storage.fill(0);
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2EPKcm_nid_postfix(facet, "", 5);
    Require(facet->vtable == vtable && facet->references == 5, "time_get<char>(const char*) C2");
    Require(Throws([&] { _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2EPKcm_nid_postfix(facet, "xx_YY", 0); }), "an unknown locale name is rejected");
    storage.fill(0);
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(facet, &locinfo, 6);
    Require(facet->vtable == vtable && facet->references == 6, "time_get<char>(_Locinfo) C1");
    storage.fill(0);
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(facet, &locinfo, 7);
    Require(facet->vtable == vtable && facet->references == 7, "time_get<char>(_Locinfo) C2");
    libSceLibcInternalInventory_184_nid_no_patch(facet);
    libSceLibcInternalInventory_192_nid_no_patch(facet);

    GuestLocale::Implementation* classic = _ZNSt6locale7classicEv_nid_postfix()->implementation;
    GuestLocale::IosBase stream{};
    stream.locale = &classic;
    char text[] = "tuesday";
    auto input = MakeInput<char>(text, text + 7);
    std::int32_t state = 0;
    GuestTm time{};
    const GuestLocale::InputIterator end{};
    get->getWeekday(facet, IteratorOver(&input.base), end, &stream, &state, &time);
    Require(time.tm_wday == 2 && state == 0, "the constructed time_get<char> parses a weekday name");

    auto* heap = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(0x40));
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(heap, 0);
    libSceLibcInternalInventory_183_nid_no_patch(heap);

    alignas(16) std::array<unsigned char, 0xb8> putStorage{};
    auto* put = reinterpret_cast<GuestLocale::Facet*>(putStorage.data());
    const GuestLocale::TimeInfo* names = _Getptimes_nid_postfix();
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(put, &locinfo, 2);
    Require(put->references == 2 && std::memcmp(putStorage.data() + 0x10, names, sizeof(GuestLocale::TimeInfo)) == 0, "time_put<char>(_Locinfo) C1 copies the time names");
    const GuestLocale::FacetVtable* charPutVtable = put->vtable;
    putStorage.fill(0);
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(put, 3);
    Require(put->references == 3, "time_put<wchar_t>(size_t) C2 sets the reference count");
    const GuestLocale::FacetVtable* widePutVtable = put->vtable;
    Require(widePutVtable != charPutVtable, "time_put<wchar_t> has its own vtable");
    putStorage.fill(0);
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(put, &locinfo, 4);
    Require(put->vtable == widePutVtable && put->references == 4 && std::memcmp(putStorage.data() + 0x10, names, sizeof(GuestLocale::TimeInfo)) == 0, "time_put<wchar_t>(_Locinfo) C1");
    putStorage.fill(0);
    put->vtable = widePutVtable;
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(put, &locinfo, 5);
    Require(put->vtable == widePutVtable && put->references == 5 && std::memcmp(putStorage.data() + 0x10, names, sizeof(GuestLocale::TimeInfo)) == 0, "time_put<wchar_t>(_Locinfo) C2");
}

void CheckCtype() {
    auto* table = static_cast<std::int16_t*>(_Znwm_nid_postfix(32));
    GuestLocale::CtypeFacet kept{};
    kept.classification = table;
    kept.deleteTable = 0;
    _ZNSt5ctypeIwED1Ev_nid_postfix(&kept);
    kept.deleteTable = 1;
    _ZNSt5ctypeIwED1Ev_nid_postfix(&kept);

    auto* heap = static_cast<GuestLocale::CtypeFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CtypeFacet)));
    *heap = {};
    heap->classification = static_cast<std::int16_t*>(std::malloc(32));
    heap->deleteTable = 1;
    _ZNSt5ctypeIwED0Ev_nid_postfix(heap);
    _ZN6Dinkum7codecvt10_Cvt_checkEmm_nid_postfix(1, 2);
}

void CheckLocinfo() {
    alignas(16) GuestLocinfoLayout info{};
    std::memset(&info, 0x5a, sizeof(info));
    _ZNSt8_LocinfoC2EiPKc_nid_postfix(&info, 0x3f, "C");
    Require(std::memcmp(&info.times, _Getptimes_nid_postfix(), sizeof(info.times)) == 0, "_Locinfo copies the time names");
    Require(info.name != nullptr && info.classicName == setlocale_nid_postfix(0x3f, nullptr), "_Locinfo records the current locale name");
    std::memset(&info, 0x5a, sizeof(info));
    _ZNSt8_LocinfoC2EiPKc_nid_postfix(&info, 1, "");
    Require(std::memcmp(&info.times, _Getptimes_nid_postfix(), sizeof(info.times)) == 0 && info.classicName != nullptr, "_Locinfo accepts the empty name");
    Require(Throws([&] { _ZNSt8_LocinfoC2EiPKc_nid_postfix(&info, 1, "xx_YY"); }), "_Locinfo rejects an unknown name");
    Require(Throws([&] { _ZNSt8_LocinfoC2EiPKc_nid_postfix(&info, 1, nullptr); }), "_Locinfo rejects a null name");
    info.classicName = nullptr;
    Require(_ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(&info, 2, "C") == &info && info.classicName == setlocale_nid_postfix(0x3f, nullptr), "_Addcats refreshes the locale name and returns the object");
    Require(Throws([&] { _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(&info, 2, "zz"); }), "_Addcats rejects an unknown name");

    GuestLocale::String name{};
    LibcAssignCString_nid_no_patch(&name, "C");
    std::memset(&info, 0x5a, sizeof(info));
    _ZNSt8_LocinfoC1ERKSs_nid_postfix(&info, &name);
    Require(std::memcmp(&info.times, _Getptimes_nid_postfix(), sizeof(info.times)) == 0 && info.name == info.classicName, "_Locinfo(const string&) C1");
    std::memset(&info, 0x5a, sizeof(info));
    _ZNSt8_LocinfoC2ERKSs_nid_postfix(&info, &name);
    Require(info.name == info.classicName && info.name != nullptr, "_Locinfo(const string&) C2");
    GuestLocale::String bad{};
    LibcAssignCString_nid_no_patch(&bad, "a locale name that is longer than the inline buffer");
    Require(Throws([&] { _ZNSt8_LocinfoC1ERKSs_nid_postfix(&info, &bad); }), "_Locinfo(const string&) rejects an unknown heap name");
}

void CheckLocale() {
    alignas(16) std::array<unsigned char, sizeof(GuestLocale::Implementation)> storage{};
    auto* implementation = reinterpret_cast<GuestLocale::Implementation*>(storage.data());
    std::memset(storage.data(), 0x5a, storage.size());
    _ZNSt6locale7_LocimpC1Eb_nid_postfix(implementation, true);
    Require(implementation->base.references == 1 && implementation->facets == nullptr && implementation->facetCount == 0 && implementation->category == 0, "_Locimp(bool) starts empty");
    Require(implementation->transparent && std::strcmp(implementation->name, "*") == 0, "_Locimp(bool) is unnamed and keeps the transparent flag");
    _ZNSt6locale7_LocimpC1Eb_nid_postfix(implementation, false);
    Require(!implementation->transparent && implementation->base.vtable != nullptr, "_Locimp(bool) C1 matches C2");

    GuestLocale::LocinfoStorage locinfo{};
    GuestLocale::Facet* facets[3]{};
    for (auto& facet : facets) {
        facet = HeapFacet();
        _ZNSt8messagesIcEC1ERKSt8_Locinfom_nid_postfix(facet, &locinfo, 1);
    }
    auto* vector = static_cast<GuestLocale::Facet**>(std::malloc(sizeof(facets)));
    std::memcpy(vector, facets, sizeof(facets));
    implementation->facets = vector;
    implementation->facetCount = 3;
    GuestLocale::Facet* shared = facets[1];
    shared->vtable->retain(shared);
    vector[1] = shared;
    _ZNSt6locale7_LocimpD2Ev_nid_postfix(implementation);
    Require(shared->references == 1, "~_Locimp releases each facet once");
    shared->vtable->deleteObject(shared);

    auto* heap = static_cast<GuestLocale::Implementation*>(_Znwm_nid_postfix(sizeof(GuestLocale::Implementation)));
    _ZNSt6locale7_LocimpC1Eb_nid_postfix(heap, false);
    _ZNSt6locale7_LocimpD0Ev_nid_postfix(heap);

    GuestLocaleFacade empty{};
    Require(_ZNSt6locale5emptyEv_nid_postfix(&empty) == &empty && empty.implementation != nullptr, "locale::empty returns its result");
    Require(empty.implementation->transparent && empty.implementation->base.references == 1 && std::strcmp(empty.implementation->name, "*") == 0, "locale::empty is a transparent unnamed locale");
    _ZNSt6localeD1Ev_nid_postfix(&empty);
    GuestLocaleFacade none{};
    _ZNSt6localeD1Ev_nid_postfix(&none);
    Require(Throws([&] { _ZNSt6localeD1Ev_nid_postfix(nullptr); }), "a null locale is rejected");
}

}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&HeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&HeapFree);
    heapApi[2] = reinterpret_cast<void*>(&HeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&HeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&HeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&HeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    CheckCollate();
    CheckNumGet();
    CheckNumPut();
    CheckMoney();
    CheckTime();
    CheckCtype();
    CheckLocinfo();
    CheckLocale();
    return 0;
}
