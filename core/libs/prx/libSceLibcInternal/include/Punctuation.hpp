#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

void APS5_VABI _ZNSt8numpunctIwE5_InitERKSt8_Locinfob_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, bool isDefault);
void APS5_VABI _ZNSt8numpunctIwEC1Em_nid_postfix(GuestLocale::WideNumpunctFacet* self, std::size_t references);
void APS5_VABI _ZNSt8numpunctIwEC2Em_nid_postfix(GuestLocale::WideNumpunctFacet* self, std::size_t references);
void APS5_VABI _ZNSt8numpunctIwEC1EPKcmb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool isDefault);
void APS5_VABI _ZNSt8numpunctIwEC2EPKcmb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool isDefault);
void APS5_VABI _ZNSt8numpunctIwEC1ERKSt8_Locinfomb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool isDefault);
void APS5_VABI _ZNSt8numpunctIwEC2ERKSt8_Locinfomb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool isDefault);
void APS5_VABI _ZNSt8numpunctIwED1Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self);
void APS5_VABI _ZNSt8numpunctIwED2Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self);
void APS5_VABI _ZNSt8numpunctIwED0Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self);

}
