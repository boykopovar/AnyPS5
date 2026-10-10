#include <exception>

#ifndef _LIBCPP_VERSION
#include <future>

extern "C" {
void ExceptionPointerAddref(std::exception_ptr* self) noexcept asm("_ZNSt15__exception_ptr13exception_ptr9_M_addrefEv");
void ExceptionPointerRelease(std::exception_ptr* self) noexcept asm("_ZNSt15__exception_ptr13exception_ptr10_M_releaseEv");
void* ExceptionPointerGet(const std::exception_ptr* self) noexcept asm("_ZNKSt15__exception_ptr13exception_ptr6_M_getEv");
void ExceptionPointerConstruct(std::exception_ptr* self, void* exception) noexcept asm("_ZNSt15__exception_ptr13exception_ptrC1EPv");
const std::type_info* ExceptionPointerType(const std::exception_ptr* self) noexcept asm("_ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv");
void FutureResultDestroy(std::__future_base::_Result_base* self) noexcept asm("_ZNSt13__future_base12_Result_baseD2Ev");

void FutureResultDestroy(std::__future_base::_Result_base* self) noexcept {
    self->_M_error.~exception_ptr();
}

void APS5_VABI _ZNSt13__future_base12_Result_baseD2Ev_nid_postfix(std::__future_base::_Result_base* self) noexcept {
    FutureResultDestroy(self);
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptr9_M_addrefEv_nid_postfix(std::exception_ptr* self) noexcept {
    ExceptionPointerAddref(self);
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(std::exception_ptr* self) noexcept {
    ExceptionPointerRelease(self);
}

void* APS5_VABI _ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(const std::exception_ptr* self) noexcept {
    return ExceptionPointerGet(self);
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(std::exception_ptr* self, void* exception) noexcept {
    ExceptionPointerConstruct(self, exception);
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptrC2EPv_nid_postfix(std::exception_ptr* self, void* exception) noexcept {
    ExceptionPointerConstruct(self, exception);
}

const std::type_info* APS5_VABI _ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(const std::exception_ptr* self) noexcept {
    return ExceptionPointerType(self);
}

std::exception_ptr APS5_VABI _ZSt17current_exceptionv_nid_postfix() noexcept {
    return std::current_exception();
}

[[noreturn]] void APS5_VABI _ZSt17rethrow_exceptionNSt15__exception_ptr13exception_ptrE_nid_postfix(std::exception_ptr exception) {
    std::rethrow_exception(exception);
}
}

namespace std {

exception_ptr current_exception() noexcept {
    exception_ptr result;
    result._M_exception_object = __cxa_current_primary_exception_nid_postfix();
    return result;
}

void rethrow_exception(exception_ptr exception) {
    __cxa_rethrow_primary_exception_nid_postfix(exception._M_exception_object);
    LibcException::Terminate();
}

namespace __exception_ptr {

exception_ptr::exception_ptr(void* exception) noexcept : _M_exception_object(exception) {
    _M_addref();
}

void exception_ptr::_M_addref() noexcept {
    __cxa_increment_exception_refcount_nid_postfix(_M_exception_object);
}

void exception_ptr::_M_release() noexcept {
    __cxa_decrement_exception_refcount_nid_postfix(_M_exception_object);
    _M_exception_object = nullptr;
}

void* exception_ptr::_M_get() const noexcept {
    return _M_exception_object;
}

const type_info* exception_ptr::__cxa_exception_type() const noexcept {
    return LibcException::FromObject(_M_exception_object)->type;
}

}

}

#else

namespace {
struct GuestExceptionPointer {
    void* object {};
    explicit GuestExceptionPointer(void* primary) noexcept : object(primary) {}
    GuestExceptionPointer(const GuestExceptionPointer&) = delete;
    ~GuestExceptionPointer() {}
};
}

extern "C" {
void APS5_VABI _ZNSt15__exception_ptr13exception_ptr9_M_addrefEv_nid_postfix(GuestExceptionPointer* self) noexcept {
    __cxa_increment_exception_refcount_nid_postfix(self->object);
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(GuestExceptionPointer* self) noexcept {
    __cxa_decrement_exception_refcount_nid_postfix(self->object);
    self->object = nullptr;
}

void* APS5_VABI _ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(const GuestExceptionPointer* self) noexcept {
    return self->object;
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(GuestExceptionPointer* self, void* exception) noexcept {
    self->object = exception;
    __cxa_increment_exception_refcount_nid_postfix(exception);
}

void APS5_VABI _ZNSt15__exception_ptr13exception_ptrC2EPv_nid_postfix(GuestExceptionPointer* self, void* exception) noexcept {
    _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(self, exception);
}

const std::type_info* APS5_VABI _ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(const GuestExceptionPointer* self) noexcept {
    return LibcException::FromObject(self->object)->type;
}

GuestExceptionPointer APS5_VABI _ZSt17current_exceptionv_nid_postfix() noexcept {
    return GuestExceptionPointer(__cxa_current_primary_exception_nid_postfix());
}

[[noreturn]] void APS5_VABI _ZSt17rethrow_exceptionNSt15__exception_ptr13exception_ptrE_nid_postfix(GuestExceptionPointer& exception) {
    __cxa_rethrow_primary_exception_nid_postfix(exception.object);
    LibcException::Terminate();
}
}

#endif
