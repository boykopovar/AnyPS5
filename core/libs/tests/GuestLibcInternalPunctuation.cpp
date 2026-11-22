#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/FacetReferences.hpp"
#include "prx/libSceLibcInternal/include/Punctuation.hpp"
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
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal punctuation: %s\n", message);
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

std::u16string WideText(const std::uint16_t* text) {
    std::u16string out;
    while (*text != 0) out.push_back(static_cast<char16_t>(*text++));
    return out;
}

void CheckNumpunct() {
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");
    GuestLocale::WideNumpunctFacet wide{};
    _ZNSt8numpunctIwEC2Em_nid_postfix(&wide, 1);
    Require(wide.base.vtable == LibcWideNumpunctVtable_nid_no_patch(), "numpunct<wchar_t>(size_t) installs the wide numpunct vtable");
    Require(wide.decimalPoint == u'.', "numpunct<wchar_t> widens the decimal point");
    Require(wide.thousandsSep == u',', "numpunct<wchar_t>(size_t) replaces an empty separator with L','");
    Require(WideText(wide.falsename) == u"false" && WideText(wide.truename) == u"true" && std::strcmp(wide.grouping, "") == 0, "numpunct<wchar_t> names are the module's constants");
    _ZNSt8numpunctIwEC1ERKSt8_Locinfomb_nid_postfix(&wide, &locinfo, 2, true);
    Require(wide.base.references == 2 && wide.decimalPoint == u'.' && wide.thousandsSep == u',', "numpunct<wchar_t>(_Locinfo) honours isDefault");
    _ZNSt8numpunctIwEC1ERKSt8_Locinfomb_nid_postfix(&wide, &locinfo, 2, false);
    Require(wide.decimalPoint == u'.' && wide.thousandsSep == 0, "numpunct<wchar_t>(_Locinfo) takes the separators of the C locale when isDefault is false");
    _ZNSt8numpunctIwE5_InitERKSt8_Locinfob_nid_postfix(&wide, &locinfo, true);
    Require(wide.decimalPoint == u'.' && wide.thousandsSep == u',', "numpunct<wchar_t>::_Init honours isDefault");
    bool threw = false;
    try {
        _ZNSt8numpunctIwE5_InitERKSt8_Locinfob_nid_postfix(&wide, nullptr, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "numpunct<wchar_t>::_Init rejects a null _Locinfo");
    _ZNSt8numpunctIwEC1EPKcmb_nid_postfix(&wide, "C", 0, false);
    Require(wide.thousandsSep == 0, "a named numpunct<wchar_t> takes the separators of the C locale");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);

    auto* heapWide = static_cast<GuestLocale::WideNumpunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::WideNumpunctFacet)));
    _ZNSt8numpunctIwEC1Em_nid_postfix(heapWide, 0);
    _ZNSt8numpunctIwED2Ev_nid_postfix(heapWide);
    _ZNSt8numpunctIwED1Ev_nid_postfix(heapWide);
    _ZNSt8numpunctIwED0Ev_nid_postfix(heapWide);
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

    CheckNumpunct();
    return 0;
}
