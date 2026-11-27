#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc numpunct char: %s\n", message);
    std::abort();
}

std::string Call(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::NumpunctFacet*), const GuestLocale::NumpunctFacet* facet) {
    GuestLocale::String result{};
    Require(method(&result, facet) == &result, "the string result is the caller's buffer");
    Require(result.capacity == 15, "a short string stays inline");
    return {result.capacity > 15 ? result.pointer : result.buffer, result.size};
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

GuestLocale::String* AssignShortString(GuestLocale::String* result, const char* text) {
    *result = {};
    result->size = std::strlen(text);
    result->capacity = 15;
    std::memcpy(result->buffer, text, result->size);
    return result;
}

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }
char APS5_VABI DerivedDecimalPoint(const GuestLocale::NumpunctFacet*) { return ','; }
char APS5_VABI DerivedThousandsSep(const GuestLocale::NumpunctFacet*) { return '.'; }
GuestLocale::String* APS5_VABI DerivedGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "\3"); }
GuestLocale::String* APS5_VABI DerivedFalsename(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "no"); }
GuestLocale::String* APS5_VABI DerivedTruename(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return AssignShortString(result, "yes"); }
const GuestLocale::NumpunctVtable g_derivedVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, DerivedDecimalPoint, DerivedThousandsSep, DerivedGrouping, DerivedFalsename, DerivedTruename};

template<typename TAction>
void RequireException(TAction action, const char* expectedMessage) {
    bool caught = false;
    try {
        action();
    } catch (const std::exception& e) {
        caught = true;
        Require(std::strcmp(e.what(), expectedMessage) == 0, "exception message matches");
    }
    Require(caught, "exception was thrown");
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

    GuestLocale::Facet* created = nullptr;
    Require(_ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, &_ZSt21_sceLibcClassicLocale_nid_postfix) == 4 && created != nullptr, "_Getcat builds the C-locale facet");
    const auto* classic = reinterpret_cast<const GuestLocale::NumpunctFacet*>(created);

    Require(classic->decimalPoint == '.', "facet decimalPoint is '.'");
    Require(classic->thousandsSep == ',', "facet thousandsSep is ','");
    Require(classic->grouping != nullptr && classic->grouping[0] == '\0', "facet grouping is empty C-string");

    Require(_ZNKSt8numpunctIcE13decimal_pointEv_nid_postfix(classic) == '.', "decimal_point accessor returns '.'");
    Require(_ZNKSt8numpunctIcE13thousands_sepEv_nid_postfix(classic) == ',', "thousands_sep accessor returns ','");
    const std::string grouping = Call(_ZNKSt8numpunctIcE8groupingEv_nid_postfix, classic);
    const std::string falsename = Call(_ZNKSt8numpunctIcE9falsenameEv_nid_postfix, classic);
    const std::string truename = Call(_ZNKSt8numpunctIcE8truenameEv_nid_postfix, classic);
    Require(grouping.empty(), "grouping accessor returns empty string");
    Require(falsename.size() == 5 && std::memcmp(falsename.data(), "false", 5) == 0, "falsename accessor returns \"false\"");
    Require(truename.size() == 4 && std::memcmp(truename.data(), "true", 4) == 0, "truename accessor returns \"true\"");

    Require(_ZNKSt8numpunctIcE16do_decimal_pointEv_nid_postfix(classic) == '.', "do_decimal_point accessor returns '.'");
    Require(_ZNKSt8numpunctIcE16do_thousands_sepEv_nid_postfix(classic) == ',', "do_thousands_sep accessor returns ','");
    Require(Call(_ZNKSt8numpunctIcE11do_groupingEv_nid_postfix, classic) == grouping, "do_grouping matches grouping");
    const std::string doFalse = Call(_ZNKSt8numpunctIcE12do_falsenameEv_nid_postfix, classic);
    const std::string doTrue = Call(_ZNKSt8numpunctIcE11do_truenameEv_nid_postfix, classic);
    Require(doFalse.size() == 5 && std::memcmp(doFalse.data(), "false", 5) == 0, "do_falsename matches \"false\"");
    Require(doTrue.size() == 4 && std::memcmp(doTrue.data(), "true", 4) == 0, "do_truename matches \"true\"");

    const auto* vtable = reinterpret_cast<const GuestLocale::NumpunctVtable*>(classic->base.vtable);
    Require(vtable->decimalPoint(classic) == '.', "vtable decimalPoint returns '.'");
    Require(vtable->thousandsSep(classic) == ',', "vtable thousandsSep returns ','");
    GuestLocale::String vtableGrouping{};
    Require(vtable->grouping(&vtableGrouping, classic)->size == 0, "vtable grouping returns empty");
    GuestLocale::String vtableFalse{};
    Require(vtable->falsename(&vtableFalse, classic)->size == 5 && std::memcmp(vtableFalse.buffer, "false", 5) == 0, "vtable falsename returns \"false\"");
    GuestLocale::String vtableTrue{};
    Require(vtable->truename(&vtableTrue, classic)->size == 4 && std::memcmp(vtableTrue.buffer, "true", 4) == 0, "vtable truename returns \"true\"");

    RequireException([] { _ZNSt6locale5facet9_RegisterEv_nid_postfix(nullptr); }, "locale register: invalid facet");
    GuestLocale::Facet nullVtableFacet{nullptr, 1, 0};
    RequireException([&] { _ZNSt6locale5facet9_RegisterEv_nid_postfix(&nullVtableFacet); }, "locale register: invalid facet");

    static GuestLocale::Facet testFacet{&g_derivedVtable.facet, 1, 0};
    _ZNSt6locale5facet9_RegisterEv_nid_postfix(&testFacet);
    RequireException([] { _ZNSt6locale5facet9_RegisterEv_nid_postfix(&testFacet); }, "locale register: duplicate facet");

    created->vtable->deleteObject(created);
    return 0;
}
