#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal numpunct: %s\n", message);
    std::abort();
}

std::string Text(const GuestLocale::String& string) {
    return {string.capacity > 15 ? string.pointer : string.buffer, string.size};
}

std::string Call(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::NumpunctFacet*), const GuestLocale::NumpunctFacet* facet) {
    GuestLocale::String result{};
    Require(method(&result, facet) == &result, "the string result is the caller's buffer");
    Require(result.capacity == 15, "a short string stays inline");
    return Text(result);
}

GuestLocale::String* AssignShortString(GuestLocale::String* result, const char* text) {
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
char APS5_VABI DerivedDecimalPoint(const GuestLocale::NumpunctFacet*) { return ','; }
char APS5_VABI DerivedThousandsSep(const GuestLocale::NumpunctFacet*) { return '.'; }
GuestLocale::String* APS5_VABI DerivedGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "\3"); }
GuestLocale::String* APS5_VABI DerivedFalsename(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "no"); }
GuestLocale::String* APS5_VABI DerivedTruename(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "yes"); }
const GuestLocale::NumpunctVtable g_derivedVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, DerivedDecimalPoint, DerivedThousandsSep, DerivedGrouping, DerivedFalsename, DerivedTruename};

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
    Require(_ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, nullptr) == 4 && created != nullptr, "_Getcat builds the C-locale facet");
    const auto* classic = reinterpret_cast<const GuestLocale::NumpunctFacet*>(created);

    Require(sceLibcInternalNumpunctDecimalPoint_nid_no_patch(classic) == '.', "decimal_point of the C locale is '.'");
    Require(sceLibcInternalNumpunctThousandsSep_nid_no_patch(classic) == ',', "thousands_sep of the C locale is ','");
    Require(Call(sceLibcInternalNumpunctGrouping_nid_no_patch, classic).empty(), "grouping of the C locale is empty");
    Require(Call(sceLibcInternalNumpunctFalsename_nid_no_patch, classic) == "false", "falsename of the C locale is \"false\"");
    Require(Call(sceLibcInternalNumpunctTruename_nid_no_patch, classic) == "true", "truename of the C locale is \"true\"");
    Require(sceLibcInternalNumpunctDoDecimalPoint_nid_no_patch(classic) == '.', "do_decimal_point of the C locale is '.'");
    Require(sceLibcInternalNumpunctDoThousandsSep_nid_no_patch(classic) == ',', "do_thousands_sep of the C locale is ','");
    Require(Call(sceLibcInternalNumpunctDoGrouping_nid_no_patch, classic).empty(), "do_grouping of the C locale is empty");
    Require(Call(sceLibcInternalNumpunctDoFalsename_nid_no_patch, classic) == "false", "do_falsename of the C locale is \"false\"");
    Require(Call(sceLibcInternalNumpunctDoTruename_nid_no_patch, classic) == "true", "do_truename of the C locale is \"true\"");

    const GuestLocale::NumpunctFacet derived{{&g_derivedVtable.facet, 1, 0}, "\2", ':', ';', "off", "on"};
    Require(sceLibcInternalNumpunctDecimalPoint_nid_no_patch(&derived) == ',', "decimal_point calls the overriding do_decimal_point");
    Require(sceLibcInternalNumpunctThousandsSep_nid_no_patch(&derived) == '.', "thousands_sep calls the overriding do_thousands_sep");
    Require(Call(sceLibcInternalNumpunctGrouping_nid_no_patch, &derived) == "\3", "grouping calls the overriding do_grouping");
    Require(Call(sceLibcInternalNumpunctFalsename_nid_no_patch, &derived) == "no", "falsename calls the overriding do_falsename");
    Require(Call(sceLibcInternalNumpunctTruename_nid_no_patch, &derived) == "yes", "truename calls the overriding do_truename");
    Require(sceLibcInternalNumpunctDoDecimalPoint_nid_no_patch(&derived) == ':', "do_decimal_point reads the facet's decimal point");
    Require(sceLibcInternalNumpunctDoThousandsSep_nid_no_patch(&derived) == ';', "do_thousands_sep reads the facet's separator");
    Require(Call(sceLibcInternalNumpunctDoGrouping_nid_no_patch, &derived) == "\2", "do_grouping copies the facet's grouping");
    Require(Call(sceLibcInternalNumpunctDoFalsename_nid_no_patch, &derived) == "off", "do_falsename copies the facet's false name");
    Require(Call(sceLibcInternalNumpunctDoTruename_nid_no_patch, &derived) == "on", "do_truename copies the facet's true name");

    created->vtable->deleteObject(created);
    return 0;
}
