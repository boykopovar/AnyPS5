#include "prx/libSceLibcInternal/include/ExceptionVirtuals.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
#include <cstddef>
#include <stdexcept>
#include <typeinfo>

extern "C" {
extern const void* const _ZTVSt12future_error_nid_postfix[6];
extern const void* const _ZTVSt18bad_variant_access_nid_postfix[5];
extern const void* const _ZTVSt22_Future_error_category_nid_postfix[9];
extern const unsigned char _ZTISt12system_error_nid_postfix[];
}

namespace {

using PlainWhat = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);
using FutureErrorWhat = const char* (APS5_VABI *)(const LibcException::FutureErrorObject*);
using FutureErrorRaise = void (APS5_VABI *)(const LibcException::FutureErrorObject*);
using CategoryMessage = GuestLocale::String* (APS5_VABI *)(GuestLocale::String*, const LibcException::CategoryObject*, int);

constexpr std::size_t WhatSlot = 4;
constexpr std::size_t DoraiseSlot = 5;
constexpr std::size_t MessageSlot = 5;

template <typename TFunction>
TFunction Slot(const void* const* table, std::size_t index) {
    return std::bit_cast<TFunction>(table[index]);
}

void APS5_VABI DestroySystemError_nid_no_patch(void* object) {
    _ZNSt12system_errorD1Ev_nid_postfix(static_cast<LibcException::ExceptionObject*>(object));
}

}

extern "C" {

[[noreturn]] void APS5_VABI sceLibcInternalBadAllocDoraise_nid_no_patch(const LibcException::ExceptionObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    _ZSt11_Xbad_allocv_nid_postfix();
}
APS5_EXPORT("pS-t9AJblSM", sceLibcInternalBadAllocDoraise_nid_no_patch);

[[noreturn]] void APS5_VABI sceLibcInternalBadTypeidDoraise_nid_no_patch(const LibcException::ExceptionObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    __cxa_bad_typeid_nid_postfix();
}
APS5_EXPORT("WMw8eIs0kjM", sceLibcInternalBadTypeidDoraise_nid_no_patch);

const char* APS5_VABI sceLibcInternalBadVariantAccessWhat_nid_no_patch(const LibcException::ExceptionObject* self) {
    return Slot<PlainWhat>(_ZTVSt18bad_variant_access_nid_postfix, WhatSlot)(self);
}
APS5_EXPORT("+5IOQncui3c", sceLibcInternalBadVariantAccessWhat_nid_no_patch);

const char* APS5_VABI sceLibcInternalFutureErrorWhat_nid_no_patch(const LibcException::FutureErrorObject* self) {
    return Slot<FutureErrorWhat>(_ZTVSt12future_error_nid_postfix, WhatSlot)(self);
}
APS5_EXPORT("5sfbtNAdt-M", sceLibcInternalFutureErrorWhat_nid_no_patch);

void APS5_VABI sceLibcInternalFutureErrorDoraise_nid_no_patch(const LibcException::FutureErrorObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    Slot<FutureErrorRaise>(_ZTVSt12future_error_nid_postfix, DoraiseSlot)(self);
}
APS5_EXPORT("-syPONaWjqw", sceLibcInternalFutureErrorDoraise_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalFutureErrorCategoryMessage_nid_no_patch(GuestLocale::String* result, const LibcException::CategoryObject* self, int value) {
    return Slot<CategoryMessage>(_ZTVSt22_Future_error_category_nid_postfix, MessageSlot)(result, self, value);
}
APS5_EXPORT("nWfZplDjbxQ", sceLibcInternalFutureErrorCategoryMessage_nid_no_patch);

[[noreturn]] void APS5_VABI sceLibcInternalSystemErrorDoraise_nid_no_patch(const LibcException::SystemErrorObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    auto* object = static_cast<LibcException::SystemErrorObject*>(__cxa_allocate_exception_nid_postfix(sizeof(LibcException::SystemErrorObject)));
    _ZNSt12system_errorC1ERKS__nid_postfix(object, self);
    __cxa_throw_nid_postfix(object, std::bit_cast<std::type_info*>(static_cast<const void*>(_ZTISt12system_error_nid_postfix)),
        reinterpret_cast<void (*)(void*)>(DestroySystemError_nid_no_patch));
}
APS5_EXPORT("uWZBRxLAvEg", sceLibcInternalSystemErrorDoraise_nid_no_patch);

}
