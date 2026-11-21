#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v25ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27codecvtIcc9_MbstatetE2idE_nid_postfix;
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

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

struct GuestString {
    union {
        char buffer[16];
        char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

using CaseChar = char (APS5_VABI *)(const void*, char);
using CaseRange = const char* (APS5_VABI *)(const void*, char*, const char*);
using WidenRange = const char* (APS5_VABI *)(const void*, const char*, const char*, char*);
using NarrowChar = char (APS5_VABI *)(const void*, char, char);
using NarrowRange = const char* (APS5_VABI *)(const void*, const char*, const char*, char, char*);
using CharGet = char (APS5_VABI *)(const void*);
using StringGet = GuestString* (APS5_VABI *)(GuestString*, const void*);
using NoConv = bool (APS5_VABI *)(const void*);
using IntGet = int (APS5_VABI *)(const void*);
using Convert = int (APS5_VABI *)(void*, void*, const char*, const char*, const char**, char*, char*, char**);
using Unshift = int (APS5_VABI *)(void*, void*, char*, char*, char**);
using Length = std::uint64_t (APS5_VABI *)(void*, void*, const char*, const char*, std::uint64_t);

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

static std::string Take(GuestString& text) {
    if (text.capacity <= 15) return std::string(text.buffer, text.size);
    std::string copy(text.pointer, text.size);
    void* storage = text.pointer;
    if (text.capacity + 1 >= 0x1000) storage = static_cast<void**>(storage)[-1];
    _ZdlPv_nid_postfix(storage);
    return copy;
}

static void CheckStringSlot(GuestLocale::NumpunctFacet* facet, int slot, const char*& field, const char* name) {
    const char* saved = field;
    for (std::size_t length : {0u, 1u, 15u, 16u, 31u, 32u, 0xffeu, 0xfffu, 0x1000u}) {
        const std::string expected(length, 'x');
        field = expected.c_str();
        GuestString result;
        std::memset(&result, 0xa5, sizeof(result));
        Require(Slot<StringGet>(facet, slot)(&result, facet) == &result, name);
        Require(result.size == length, name);
        Require(result.capacity == (length <= 15 ? 15 : (length | 15)), name);
        if (result.capacity + 1 >= 0x1000) Require((reinterpret_cast<std::uintptr_t>(result.pointer) & 31) == 0, name);
        if (length <= 15) {
            for (std::size_t index = length; index < 16; ++index) Require(result.buffer[index] == 0, name);
        } else {
            Require(result.pointer[length] == 0, name);
        }
        Require(Take(result) == expected, name);
    }
    field = saved;
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
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);
    const void* ctype = locale->facets[_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix];
    auto* numpunct = reinterpret_cast<GuestLocale::NumpunctFacet*>(locale->facets[_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix]);
    void* codecvt = locale->facets[_ZNSt8__sce_v27codecvtIcc9_MbstatetE2idE_nid_postfix];
    Require(ctype != nullptr && numpunct != nullptr && codecvt != nullptr, "_Makeloc installs the char facets");

    for (int value = 0; value < 256; ++value) {
        const char c = static_cast<char>(value);
        const char lower = value >= 'A' && value <= 'Z' ? static_cast<char>(value + 32) : c;
        const char upper = value >= 'a' && value <= 'z' ? static_cast<char>(value - 32) : c;
        Require(Slot<CaseChar>(ctype, 4)(ctype, c) == lower, "do_tolower changes only ASCII capitals");
        Require(Slot<CaseChar>(ctype, 6)(ctype, c) == upper, "do_toupper changes only ASCII small letters");
        Require(Slot<CaseChar>(ctype, 8)(ctype, c) == c, "do_widen returns the character");
        Require(Slot<NarrowChar>(ctype, 10)(ctype, c, '?') == c, "do_narrow ignores the fallback");
    }

    char mixed[] = "aZ1\xc3\xa9-";
    char* mixedEnd = mixed + 6;
    Require(Slot<CaseRange>(ctype, 5)(ctype, mixed, mixedEnd) == mixedEnd && std::strcmp(mixed, "az1\xc3\xa9-") == 0, "do_tolower converts a range in place");
    Require(Slot<CaseRange>(ctype, 7)(ctype, mixed, mixedEnd) == mixedEnd && std::strcmp(mixed, "AZ1\xc3\xa9-") == 0, "do_toupper converts a range in place");
    Require(Slot<CaseRange>(ctype, 5)(ctype, mixed, mixed) == mixed, "an empty case range returns its end");
    char copy[8];
    std::memset(copy, 'q', sizeof(copy));
    Require(Slot<WidenRange>(ctype, 9)(ctype, mixed, mixedEnd, copy) == mixedEnd && std::memcmp(copy, mixed, 6) == 0 && copy[6] == 'q', "do_widen copies the range");
    std::memset(copy, 'q', sizeof(copy));
    Require(Slot<NarrowRange>(ctype, 11)(ctype, mixed, mixedEnd, '?', copy) == mixedEnd && std::memcmp(copy, mixed, 6) == 0 && copy[6] == 'q', "do_narrow copies the range");

    Require(Slot<CharGet>(numpunct, 4)(numpunct) == '.', "do_decimal_point reads the facet");
    numpunct->decimalPoint = ';';
    numpunct->thousandsSep = '_';
    Require(Slot<CharGet>(numpunct, 4)(numpunct) == ';', "do_decimal_point follows the field");
    Require(Slot<CharGet>(numpunct, 5)(numpunct) == '_', "do_thousands_sep follows the field");
    CheckStringSlot(numpunct, 6, numpunct->grouping, "do_grouping copies the field");
    CheckStringSlot(numpunct, 7, numpunct->falsename, "do_falsename copies the field");
    CheckStringSlot(numpunct, 8, numpunct->truename, "do_truename copies the field");

    Require(Slot<NoConv>(codecvt, 4)(codecvt), "do_always_noconv is true");
    Require(Slot<IntGet>(codecvt, 5)(codecvt) == 1, "do_max_length is one");
    Require(Slot<IntGet>(codecvt, 6)(codecvt) == 1, "do_encoding is one");
    std::uint64_t state[2]{0x1122334455667788, 0x99aabbccddeeff00};
    char source[4] = "abc";
    char target[4] = "xyz";
    for (int slot : {7, 8}) {
        const char* next1 = nullptr;
        char* next3 = nullptr;
        Require(Slot<Convert>(codecvt, slot)(codecvt, state, source, source + 3, &next1, target, target + 3, &next3) == 3, "do_in and do_out report noconv");
        Require(next1 == source && next3 == target, "do_in and do_out consume nothing");
    }
    char* next = nullptr;
    Require(Slot<Unshift>(codecvt, 9)(codecvt, state, target, target + 3, &next) == 3 && next == target, "do_unshift reports noconv");
    Require(std::strcmp(target, "xyz") == 0 && state[0] == 0x1122334455667788 && state[1] == 0x99aabbccddeeff00, "the conversions leave their buffers and state alone");
    const auto length = Slot<Length>(codecvt, 10);
    const auto at = [](std::uintptr_t address) { return reinterpret_cast<const char*>(address); };
    Require(length(codecvt, nullptr, at(0x1000), at(0x100a), 3) == 3, "do_length stops at the count");
    Require(length(codecvt, nullptr, at(0x1000), at(0x100a), 30) == 10, "do_length stops at the range");
    Require(length(codecvt, nullptr, at(0x1000), at(0x80001000), ~0ull) == 0x7fffffff, "do_length caps the range at INT_MAX");
    Require(length(codecvt, nullptr, at(0x100a), at(0x1000), 7) == 7, "a reversed range reads as a large unsigned distance");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    return failures == 0 ? 0 : 1;
}
