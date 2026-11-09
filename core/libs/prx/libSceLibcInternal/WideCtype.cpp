#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

struct WideCtypeFacetPrefix {
    const std::uintptr_t* vtable;
    std::uint32_t references;
    std::uint32_t reserved;
    const short* classification;
    const void* lowerMap;
    const void* upperMap;
};

static_assert(sizeof(void*) == sizeof(std::uintptr_t));
static_assert(offsetof(WideCtypeFacetPrefix, classification) == 0x10);
static_assert(offsetof(WideCtypeFacetPrefix, lowerMap) == 0x18);
static_assert(offsetof(WideCtypeFacetPrefix, upperMap) == 0x20);
static_assert(sizeof(WideCtypeFacetPrefix) == 0x28);

constexpr std::size_t DoIsSlot = 4;
constexpr std::size_t WidenSlot = 12;
constexpr std::size_t NarrowSlot = 14;

using NarrowFunction = char (APS5_VABI*)(const GuestLocale::Facet*, char16_t, char);
using WidenFunction = char16_t (APS5_VABI*)(const GuestLocale::Facet*, char);
using DoIsFunction = bool (APS5_VABI*)(const GuestLocale::Facet*, short, char16_t);

const std::byte* Bytes(const void* object) {
    return static_cast<const std::byte*>(object);
}

template<typename T>
T Load(const void* object, std::size_t offset) {
    T value;
    std::memcpy(&value, Bytes(object) + offset, sizeof(value));
    return value;
}

template<typename T>
T LoadVtableFunction(const void* self, std::size_t slot) {
    const auto* vtable = Load<const std::uintptr_t*>(self, offsetof(WideCtypeFacetPrefix, vtable));
    static_assert(sizeof(T) == sizeof(std::uintptr_t));
    return Load<T>(vtable, slot * sizeof(std::uintptr_t));
}

unsigned char Narrow(const GuestLocale::Facet* self, char16_t character) {
    const auto narrow = LoadVtableFunction<NarrowFunction>(self, NarrowSlot);
    return static_cast<unsigned char>(narrow(self, character, 0));
}

short Classification(const short* table, unsigned char character) {
    return table[character];
}

char16_t Upper(const GuestLocale::Facet* self, unsigned char character) {
    const auto* table = Load<const std::byte*>(self, offsetof(WideCtypeFacetPrefix, upperMap));
    const auto mapped = Load<std::int8_t>(table, static_cast<std::size_t>(character) * 2);
    const auto widen = LoadVtableFunction<WidenFunction>(self, WidenSlot);
    return widen(self, static_cast<char>(mapped));
}

}

extern "C" {

bool APS5_VABI sceLibcInternalWideCtypeDoIs_nid_no_patch(const GuestLocale::Facet* self, short mask, char16_t character) {
    const auto* table = Load<const short*>(self, offsetof(WideCtypeFacetPrefix, classification));
    return (Classification(table, Narrow(self, character)) & mask) != 0;
}
APS5_EXPORT("rh7L-TliPoc", sceLibcInternalWideCtypeDoIs_nid_no_patch);

const char16_t* APS5_VABI sceLibcInternalWideCtypeDoIsRange_nid_no_patch(const GuestLocale::Facet* self, const char16_t* first, const char16_t* last, short* destination) {
    for (; first != last; ++first, ++destination) {
        const auto* table = Load<const short*>(self, offsetof(WideCtypeFacetPrefix, classification));
        *destination = Classification(table, Narrow(self, *first));
    }
    return first;
}
APS5_EXPORT("D0lUMKU+ELI", sceLibcInternalWideCtypeDoIsRange_nid_no_patch);

const char16_t* APS5_VABI sceLibcInternalWideCtypeDoScanNot_nid_no_patch(const GuestLocale::Facet* self, short mask, const char16_t* first, const char16_t* last) {
    for (; first != last; ++first) {
        const auto doIs = LoadVtableFunction<DoIsFunction>(self, DoIsSlot);
        if (!doIs(self, mask, *first)) break;
    }
    return first;
}
APS5_EXPORT("Kbe+LHOer9o", sceLibcInternalWideCtypeDoScanNot_nid_no_patch);

char16_t APS5_VABI sceLibcInternalWideCtypeDoToupper_nid_no_patch(const GuestLocale::Facet* self, char16_t character) {
    const auto narrowed = Narrow(self, character);
    return narrowed ? Upper(self, narrowed) : character;
}
APS5_EXPORT("JsZjB3TnZ8A", sceLibcInternalWideCtypeDoToupper_nid_no_patch);

const char16_t* APS5_VABI sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(const GuestLocale::Facet* self, char16_t* first, const char16_t* last) {
    for (; first != last; ++first) {
        const auto narrowed = Narrow(self, *first);
        if (narrowed) *first = Upper(self, narrowed);
    }
    return first;
}
APS5_EXPORT("r1W-NtOi6E8", sceLibcInternalWideCtypeDoToupperRange_nid_no_patch);

}
