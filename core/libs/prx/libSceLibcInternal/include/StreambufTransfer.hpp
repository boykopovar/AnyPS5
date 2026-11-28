#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI _ZNSt15basic_streambufIcSt11char_traitsIcEE6xsgetnEPci_nid_postfix(GuestLocale::Streambuf* self, char* destination, int count);
int APS5_VABI _ZNSt15basic_streambufIcSt11char_traitsIcEE6xsputnEPKci_nid_postfix(GuestLocale::Streambuf* self, const char* source, int count);
int APS5_VABI _ZNSt15basic_streambufIwSt11char_traitsIwEE6xsgetnEPwi_nid_postfix(GuestLocale::Streambuf* self, std::uint16_t* destination, int count);
int APS5_VABI _ZNSt15basic_streambufIwSt11char_traitsIwEE6xsputnEPKwi_nid_postfix(GuestLocale::Streambuf* self, const std::uint16_t* source, int count);

}
