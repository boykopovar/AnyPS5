#pragma once

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI _ZNSt24_Iostream_error_categoryD1Ev_nid_postfix(LibcException::CategoryObject* self);
GuestLocale::String* APS5_VABI _ZNKSt24_Iostream_error_category7messageEi_nid_postfix(GuestLocale::String* result, const LibcException::CategoryObject* self, std::int32_t value);
GuestLocale::String* APS5_VABI _ZNKSt22_System_error_category7messageEi_nid_postfix(GuestLocale::String* result, const LibcException::CategoryObject* self, std::int32_t value);
LibcException::ErrorCode APS5_VABI _ZNKSt22_System_error_category23default_error_conditionEi_nid_postfix(const LibcException::CategoryObject* self, std::int32_t value);

[[noreturn]] void APS5_VABI _ZSt14_Debug_messagePKcS0_j_nid_postfix(const char* message, const char* file, std::uint32_t line);

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE4syncEv_nid_postfix(void* self);
int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE9pbackfailEi_nid_postfix(void* self, std::int32_t character);
int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE9pbackfailEi_nid_postfix(void* self, std::int32_t character);
bool APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE9_EndwriteEv_nid_postfix(void* self);
bool APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE9_EndwriteEv_nid_postfix(void* self);

}
