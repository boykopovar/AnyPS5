#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
int APS5_VABI ucnv_compareNames_nid_postfix(const char*, const char*);
char* APS5_VABI ucnv_io_stripASCIIForCompare_nid_postfix(char*, const char*);
char* APS5_VABI ucnv_io_stripEBCDICForCompare_nid_postfix(char*, const char*);
const char* APS5_VABI ucnv_detectUnicodeSignature_nid_postfix(const char*, std::int32_t, std::int32_t*, std::int32_t*);
}
