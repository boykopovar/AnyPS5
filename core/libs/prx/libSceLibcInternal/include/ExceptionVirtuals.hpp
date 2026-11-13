#pragma once

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {

[[noreturn]] void APS5_VABI sceLibcInternalBadAllocDoraise_nid_no_patch(const LibcException::ExceptionObject* self);
[[noreturn]] void APS5_VABI sceLibcInternalBadTypeidDoraise_nid_no_patch(const LibcException::ExceptionObject* self);
const char* APS5_VABI sceLibcInternalBadVariantAccessWhat_nid_no_patch(const LibcException::ExceptionObject* self);
const char* APS5_VABI sceLibcInternalFutureErrorWhat_nid_no_patch(const LibcException::FutureErrorObject* self);
void APS5_VABI sceLibcInternalFutureErrorDoraise_nid_no_patch(const LibcException::FutureErrorObject* self);
GuestLocale::String* APS5_VABI sceLibcInternalFutureErrorCategoryMessage_nid_no_patch(GuestLocale::String* result, const LibcException::CategoryObject* self, int value);
[[noreturn]] void APS5_VABI sceLibcInternalSystemErrorDoraise_nid_no_patch(const LibcException::SystemErrorObject* self);
LibcRaiseHandler APS5_VABI _ZNSt9exception18_Set_raise_handlerEPFvRKS_E_nid_postfix(LibcRaiseHandler handler);

}
