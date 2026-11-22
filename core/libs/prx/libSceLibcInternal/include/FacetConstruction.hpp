#pragma once

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

struct GuestLocinfoLayout {
    GuestLocale::TimeInfo times;
    const char* name;
    const char* classicName;
};

static_assert(sizeof(GuestLocinfoLayout) == 0xb8);
static_assert(offsetof(GuestLocinfoLayout, name) == 0xa8);
static_assert(offsetof(GuestLocinfoLayout, classicName) == 0xb0);

template<typename Element> struct GuestStringConstIterator {
    const void* container;
    const void* next;
    const Element* pointer;
};

static_assert(sizeof(GuestStringConstIterator<char>) == 0x18);

extern "C" {

const GuestLocale::FacetVtable* LibcInternalCharNumGetVtable_nid_no_patch();

void APS5_VABI _ZNSt7collateIwE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info);
void APS5_VABI _ZNSt7collateIwEC1EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references);
void APS5_VABI _ZNSt7collateIwEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7collateIwEC2EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references);
void APS5_VABI _ZNSt7collateIwEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7collateIwEC2Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references);
void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references);
void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
std::size_t APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
std::size_t APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
std::size_t APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7collateIcE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info);
void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references);
void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
void APS5_VABI _ZNSt7collateIwEC1Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references);
void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references);
std::size_t APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references);
void APS5_VABI _ZNSt5ctypeIwED1Ev_nid_postfix(GuestLocale::CtypeFacet* self);
void APS5_VABI _ZNSt5ctypeIwED0Ev_nid_postfix(GuestLocale::CtypeFacet* self);
void APS5_VABI _ZNSt8_LocinfoC2EiPKc_nid_postfix(GuestLocinfoLayout* self, int category, const char* localeName);
GuestLocinfoLayout* APS5_VABI _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(GuestLocinfoLayout* self, int category, const char* localeName);
void APS5_VABI _ZNSt8_LocinfoC1ERKSs_nid_postfix(GuestLocinfoLayout* self, const GuestLocale::String* name);
void APS5_VABI _ZNSt8_LocinfoC2ERKSs_nid_postfix(GuestLocinfoLayout* self, const GuestLocale::String* name);
void APS5_VABI _ZNSt6locale7_LocimpD1Ev_nid_postfix(GuestLocale::Implementation* self);
void APS5_VABI _ZNSt6locale7_LocimpD2Ev_nid_postfix(GuestLocale::Implementation* self);
void APS5_VABI _ZNSt6locale7_LocimpD0Ev_nid_postfix(GuestLocale::Implementation* self);
void APS5_VABI _ZNSt6locale7_LocimpC1Eb_nid_postfix(GuestLocale::Implementation* self, bool transparent);
void APS5_VABI _ZNSt6localeD1Ev_nid_postfix(GuestLocaleFacade* self);
GuestLocaleFacade* APS5_VABI _ZNSt6locale5emptyEv_nid_postfix(GuestLocaleFacade* result);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, const char* elements, std::size_t count);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, char element, std::size_t count);
GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIcEEEm_nid_postfix(GuestLocale::OutputIterator out, GuestStringConstIterator<char> first, std::size_t count);
GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(GuestLocale::OutputIterator out, char element, std::size_t count);
GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix(GuestLocale::OutputIterator out, std::uint16_t element, std::size_t count);
void APS5_VABI _ZN6Dinkum7codecvt10_Cvt_checkEmm_nid_postfix(std::size_t first, std::size_t second);
GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value);
GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value);
GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value);
GuestLocale::InputIterator APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value);
int APS5_VABI _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(const GuestLocale::Facet* self, char* digits, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, std::int32_t basefield, const GuestLocaleFacade* locale);
int APS5_VABI _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetifldEPcRS3_S6_NSt5_IosbIiE9_FmtflagsERKSt6locale_nid_postfix(const GuestLocale::Facet* self, char* digits, GuestLocale::InputIterator* first, GuestLocale::InputIterator* last, std::int32_t basefield, const GuestLocaleFacade* locale);

}
