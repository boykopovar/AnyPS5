#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr std::size_t WideInlineCapacity = 7;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide numpunct: %s\n", message);
    std::abort();
}

std::u16string WideText(const GuestLocale::String& string) {
    const char* data = string.capacity > WideInlineCapacity ? string.pointer : string.buffer;
    std::u16string text(string.size, u'\0');
    std::memcpy(text.data(), data, string.size * sizeof(char16_t));
    Require(data[string.size * sizeof(char16_t)] == 0 && data[string.size * sizeof(char16_t) + 1] == 0, "a wide string ends with a 16-bit terminator");
    return text;
}

std::u16string CallWide(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::WideNumpunctFacet*), const GuestLocale::WideNumpunctFacet* facet, std::size_t capacity) {
    GuestLocale::String result{};
    Require(method(&result, facet) == &result, "the wide string result is the caller's buffer");
    Require(result.capacity == capacity, "the wide string capacity counts 16-bit characters");
    std::u16string text = WideText(result);
    if (result.capacity > WideInlineCapacity) _ZdlPv_nid_postfix(result.pointer);
    return text;
}

std::string CallNarrow(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::WideNumpunctFacet*), const GuestLocale::WideNumpunctFacet* facet) {
    GuestLocale::String result{};
    Require(method(&result, facet) == &result, "the grouping result is the caller's buffer");
    Require(result.capacity == 15, "grouping is a short narrow string");
    return {result.buffer, result.size};
}

GuestLocale::String* AssignShortWide(GuestLocale::String* result, const char16_t* text) {
    *result = {};
    while (text[result->size] != u'\0') ++result->size;
    result->capacity = WideInlineCapacity;
    std::memcpy(result->buffer, text, result->size * sizeof(char16_t));
    return result;
}

GuestLocale::String* AssignShortNarrow(GuestLocale::String* result, const char* text) {
    *result = {};
    result->size = std::strlen(text);
    result->capacity = 15;
    std::memcpy(result->buffer, text, result->size);
    return result;
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

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }
std::uint16_t APS5_VABI DerivedDecimalPoint(const GuestLocale::WideNumpunctFacet*) { return 0x066b; }
std::uint16_t APS5_VABI DerivedThousandsSep(const GuestLocale::WideNumpunctFacet*) { return 0x066c; }
GuestLocale::String* APS5_VABI DerivedGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return AssignShortNarrow(result, "\3\2"); }
GuestLocale::String* APS5_VABI DerivedFalsename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return AssignShortWide(result, u"nein"); }
GuestLocale::String* APS5_VABI DerivedTruename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return AssignShortWide(result, u"ja"); }
const GuestLocale::WideNumpunctVtable g_derivedVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, DerivedDecimalPoint, DerivedThousandsSep, DerivedGrouping, DerivedFalsename, DerivedTruename};

const std::uint16_t g_offName[] = {'d', 'i', 's', 'a', 'b', 'l', 'e', 'd', 0};
const std::uint16_t g_onName[] = {0x00e9, 'i', 0};

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

    GuestLocale::Facet* created = nullptr;
    Require(_ZNSt8numpunctIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, nullptr) == 4 && created != nullptr, "_Getcat builds the C-locale wide facet");
    const auto* classic = reinterpret_cast<const GuestLocale::WideNumpunctFacet*>(created);

    Require(sceLibcInternalWideNumpunctDecimalPoint_nid_no_patch(classic) == u'.', "decimal_point of the C locale is L'.'");
    Require(sceLibcInternalWideNumpunctThousandsSep_nid_no_patch(classic) == u',', "thousands_sep of the C locale is L','");
    Require(CallNarrow(sceLibcInternalWideNumpunctGrouping_nid_no_patch, classic).empty(), "grouping of the C locale is empty");
    Require(CallWide(sceLibcInternalWideNumpunctFalsename_nid_no_patch, classic, WideInlineCapacity) == u"false", "falsename of the C locale is L\"false\"");
    Require(CallWide(sceLibcInternalWideNumpunctTruename_nid_no_patch, classic, WideInlineCapacity) == u"true", "truename of the C locale is L\"true\"");
    Require(sceLibcInternalWideNumpunctDoDecimalPoint_nid_no_patch(classic) == u'.', "do_decimal_point of the C locale is L'.'");
    Require(sceLibcInternalWideNumpunctDoThousandsSep_nid_no_patch(classic) == u',', "do_thousands_sep of the C locale is L','");
    Require(CallNarrow(sceLibcInternalWideNumpunctDoGrouping_nid_no_patch, classic).empty(), "do_grouping of the C locale is empty");
    Require(CallWide(sceLibcInternalWideNumpunctDoFalsename_nid_no_patch, classic, WideInlineCapacity) == u"false", "do_falsename of the C locale is L\"false\"");
    Require(CallWide(sceLibcInternalWideNumpunctDoTruename_nid_no_patch, classic, WideInlineCapacity) == u"true", "do_truename of the C locale is L\"true\"");

    const GuestLocale::WideNumpunctFacet derived{{&g_derivedVtable.facet, 1, 0}, "\4", 0x00b7, 0x2009, g_offName, g_onName};
    Require(sceLibcInternalWideNumpunctDecimalPoint_nid_no_patch(&derived) == 0x066b, "decimal_point calls the overriding do_decimal_point");
    Require(sceLibcInternalWideNumpunctThousandsSep_nid_no_patch(&derived) == 0x066c, "thousands_sep calls the overriding do_thousands_sep");
    Require(CallNarrow(sceLibcInternalWideNumpunctGrouping_nid_no_patch, &derived) == "\3\2", "grouping calls the overriding do_grouping");
    Require(CallWide(sceLibcInternalWideNumpunctFalsename_nid_no_patch, &derived, WideInlineCapacity) == u"nein", "falsename calls the overriding do_falsename");
    Require(CallWide(sceLibcInternalWideNumpunctTruename_nid_no_patch, &derived, WideInlineCapacity) == u"ja", "truename calls the overriding do_truename");
    Require(sceLibcInternalWideNumpunctDoDecimalPoint_nid_no_patch(&derived) == 0x00b7, "do_decimal_point reads the facet's 16-bit decimal point");
    Require(sceLibcInternalWideNumpunctDoThousandsSep_nid_no_patch(&derived) == 0x2009, "do_thousands_sep reads the facet's 16-bit separator");
    Require(CallNarrow(sceLibcInternalWideNumpunctDoGrouping_nid_no_patch, &derived) == "\4", "do_grouping copies the facet's narrow grouping");
    Require(CallWide(sceLibcInternalWideNumpunctDoFalsename_nid_no_patch, &derived, 15) == u"disabled", "do_falsename copies a false name longer than the inline buffer");
    Require(CallWide(sceLibcInternalWideNumpunctDoTruename_nid_no_patch, &derived, WideInlineCapacity) == u"éi", "do_truename copies the facet's 16-bit true name");

    created->vtable->deleteObject(created);
    return 0;
}
