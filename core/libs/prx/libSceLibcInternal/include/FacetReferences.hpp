#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

void APS5_VABI _ZNSt6locale5facet7_IncrefEv_nid_postfix(GuestLocale::Facet* self);
GuestLocale::Facet* APS5_VABI _ZNSt6locale5facet7_DecrefEv_nid_postfix(GuestLocale::Facet* self);

}
