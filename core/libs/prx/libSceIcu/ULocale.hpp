#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct ULocale;

extern "C" {
ULocale* APS5_VABI ulocale_openForLocaleID_nid_postfix(const char*, std::int32_t, std::int32_t*);
void APS5_VABI ulocale_close_nid_postfix(ULocale*);
const char* APS5_VABI ulocale_getLocaleID_nid_postfix(const ULocale*);
bool APS5_VABI ulocale_isBogus_nid_postfix(const ULocale*);
}
