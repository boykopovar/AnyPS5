#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v25ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix;
extern std::int32_t _ZNSt8__sce_v26locale2id7_Id_cntE_nid_postfix;
extern GuestLocale::Implementation* _ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix;
extern unsigned char _ZNSt8__sce_v24coutE_nid_postfix[];
extern unsigned char _ZNSt8__sce_v24cerrE_nid_postfix[];
extern unsigned char _ZNSt8__sce_v25wcoutE_nid_postfix[];
extern unsigned char _ZNSt8__sce_v25wcerrE_nid_postfix[];
extern unsigned char _ZNSt8__sce_v23cinE_nid_postfix[];
extern unsigned char _ZNSt8__sce_v24wcinE_nid_postfix[];
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

static void Require(bool value) { if (!value) std::abort(); }

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

    Require(_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix == 0);
    Require(_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix == 0);
    Require(_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix == 0);
    Require(_ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix == 0);
    Require(_ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix == 0);
    Require(_ZNSt8__sce_v26locale2id7_Id_cntE_nid_postfix == 0);

    unsigned char* streams[] {
        _ZNSt8__sce_v24coutE_nid_postfix,
        _ZNSt8__sce_v24cerrE_nid_postfix,
        _ZNSt8__sce_v25wcoutE_nid_postfix,
        _ZNSt8__sce_v25wcerrE_nid_postfix,
        _ZNSt8__sce_v23cinE_nid_postfix,
        _ZNSt8__sce_v24wcinE_nid_postfix,
    };
    for (std::size_t index = 0; index < 6; ++index) {
        Require(streams[index][0] == 0);
        for (std::size_t other = 0; other < index; ++other) Require(streams[index] != streams[other]);
    }

    auto* locale = _ZNSt8__sce_v26locale5_InitEb_nid_postfix(true);
    Require(locale != nullptr);
    Require(locale == _ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix);
    Require(locale == _ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix());
    Require(_ZNSt8__sce_v26locale5_InitEb_nid_postfix(false) == locale);

    GuestLocale::Facet facet{};
    _ZNSt8__sce_v215_Facet_RegisterEPNS_11_Facet_baseE_nid_postfix(&facet);

    auto* copy = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(locale);
    Require(copy != nullptr && copy != locale);
    Require(copy->facetCount == locale->facetCount);

    GuestLocale::Facet added{};
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(copy, &added, 41);
    Require(copy->facetCount > 41);
    Require(copy->facets[41] == &added);

    alignas(16) unsigned char classicStorage[sizeof(GuestLocale::SceV2IosBase)]{};
    auto* classic = reinterpret_cast<GuestLocale::SceV2IosBase*>(classicStorage);
    classic->locale = &_ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix;
    _ZNSt8__sce_v28ios_base7_AddstdEPS0__nid_postfix(classic);
    Require(classic->standardStream >= 1 && classic->standardStream <= 8);
    _ZNSt8__sce_v28ios_base14_Ios_base_dtorEPS0__nid_postfix(classic);
    Require(classic->locale == nullptr);

    copy->base.vtable->retain(&copy->base);
    Require(copy->base.references == 2);

    GuestLocale::Implementation* firstSlot = copy;
    GuestLocale::Implementation* secondSlot = copy;
    auto** firstLocale = &firstSlot;
    auto** secondLocale = &secondSlot;

    alignas(16) unsigned char firstStorage[sizeof(GuestLocale::SceV2IosBase)]{};
    alignas(16) unsigned char secondStorage[sizeof(GuestLocale::SceV2IosBase)]{};
    auto* first = reinterpret_cast<GuestLocale::SceV2IosBase*>(firstStorage);
    auto* second = reinterpret_cast<GuestLocale::SceV2IosBase*>(secondStorage);
    first->locale = firstLocale;
    second->locale = secondLocale;
    _ZNSt8__sce_v28ios_base7_AddstdEPS0__nid_postfix(first);
    _ZNSt8__sce_v28ios_base7_AddstdEPS0__nid_postfix(second);
    Require(first->standardStream >= 1 && first->standardStream <= 8);
    Require(second->standardStream >= 1 && second->standardStream <= 8);
    Require(first->standardStream != second->standardStream);

    _ZNSt8__sce_v28ios_base14_Ios_base_dtorEPS0__nid_postfix(first);
    Require(first->locale == nullptr);
    Require(copy->base.references == 1);
    Require(copy->facets[41] == &added);

    _ZNSt8__sce_v28ios_base14_Ios_base_dtorEPS0__nid_postfix(second);
    Require(second->locale == nullptr);
    Require(copy->base.references == 0);
    Require(copy->facets[41] == &added);
}
