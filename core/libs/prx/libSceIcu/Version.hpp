#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void APS5_VABI u_versionFromString_nid_postfix(std::uint8_t*, const char*);
void APS5_VABI u_versionFromUString_nid_postfix(std::uint8_t*, const char16_t*);
void APS5_VABI u_versionToString_nid_postfix(const std::uint8_t*, char*);
}
