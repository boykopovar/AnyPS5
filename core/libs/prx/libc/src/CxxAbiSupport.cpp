#include <cstddef>
#include <cxxabi.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <new>
#include <ios>
#include <locale>
#include <regex>
#include <functional>
#include <unwind.h>

extern "C" {

void* __dynamic_cast_nid_postfix(
    const void* source,
    const __cxxabiv1::__class_type_info* sourceType,
    const __cxxabiv1::__class_type_info* destinationType,
    std::ptrdiff_t sourceToDestinationOffset
) {
    (void)source;
    (void)sourceType;
    (void)destinationType;
    (void)sourceToDestinationOffset;
    return nullptr;
}

void* __cxa_allocate_exception_nid_postfix(std::size_t thrownSize) {
    return std::malloc(thrownSize + 128);
}

void __cxa_free_exception_nid_postfix(void* thrownException) {
    std::free(thrownException);
}

[[noreturn]] void __cxa_throw_nid_postfix(void* thrownException, std::type_info* tinfo, void (*dest)(void*)) {
    (void)thrownException; (void)tinfo; (void)dest;
    std::terminate();
}

void* __cxa_begin_catch_nid_postfix(void* exceptionObject) {
    (void)exceptionObject;
    return nullptr;
}

void __cxa_end_catch_nid_postfix() {}

void __cxa_rethrow_nid_postfix() {}

[[noreturn]] void __cxa_bad_cast_nid_postfix() { throw std::bad_cast(); }
[[noreturn]] void __cxa_bad_typeid_nid_postfix() { throw std::bad_typeid(); }

void* __cxa_get_exception_ptr_nid_postfix(void* exceptionObject) {
    (void)exceptionObject;
    return nullptr;
}

std::type_info* __cxa_current_exception_type_nid_postfix() { return nullptr; }

int __cxa_guard_acquire_nid_postfix(std::uint64_t* guardObject) {
    (void)guardObject;
    return 0;
}

void __cxa_guard_release_nid_postfix(std::uint64_t* guardObject) { (void)guardObject; }
void __cxa_guard_abort_nid_postfix(std::uint64_t* guardObject) { (void)guardObject; }

void* __cxa_vec_new_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*)) {
    (void)n; (void)s; (void)p; (void)ctor; (void)dtor;
    return nullptr;
}

void* __cxa_vec_new2_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*), void* (*alloc)(std::size_t), void (*dealloc)(void*)) {
    (void)n; (void)s; (void)p; (void)ctor; (void)dtor; (void)alloc; (void)dealloc;
    return nullptr;
}

void* __cxa_vec_new3_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*), void* (*alloc)(std::size_t), void (*dealloc)(void*, std::size_t)) {
    (void)n; (void)s; (void)p; (void)ctor; (void)dtor; (void)alloc; (void)dealloc;
    return nullptr;
}

void __cxa_vec_ctor_nid_postfix(void* arr, std::size_t n, std::size_t s, void (*ctor)(void*), void (*dtor)(void*)) {
    (void)arr; (void)n; (void)s; (void)ctor; (void)dtor;
}

void __cxa_vec_dtor_nid_postfix(void* arr, std::size_t n, std::size_t s, void (*dtor)(void*)) {
    (void)arr; (void)n; (void)s; (void)dtor;
}

void __cxa_vec_cleanup_nid_postfix(void* arr, std::size_t n, std::size_t s, void (*dtor)(void*)) {
    (void)arr; (void)n; (void)s; (void)dtor;
}

void __cxa_vec_delete_nid_postfix(void* arr, std::size_t s, std::size_t p, void (*dtor)(void*)) {
    (void)arr; (void)s; (void)p; (void)dtor;
}

void __cxa_vec_delete2_nid_postfix(void* arr, std::size_t s, std::size_t p, void (*dtor)(void*), void (*dealloc)(void*)) {
    (void)arr; (void)s; (void)p; (void)dtor; (void)dealloc;
}

void __cxa_vec_delete3_nid_postfix(void* arr, std::size_t s, std::size_t p, void (*dtor)(void*), void (*dealloc)(void*, std::size_t)) {
    (void)arr; (void)s; (void)p; (void)dtor; (void)dealloc;
}

void __cxa_vec_cctor_nid_postfix(void* dst, void* src, std::size_t n, std::size_t s, void (*cctor)(void*, void*), void (*dtor)(void*)) {
    (void)dst; (void)src; (void)n; (void)s; (void)cctor; (void)dtor;
}

void __cxa_pure_virtual_nid_postfix() { std::terminate(); }
void __cxa_deleted_virtual_nid_postfix() { std::terminate(); }

void* __cxa_demangle_nid_postfix(const char* mangled, char* buf, std::size_t* len, int* status) {
    (void)mangled; (void)buf; (void)len;
    if (status) *status = -2;
    return nullptr;
}

int __cxa_thread_atexit_impl_nid_postfix(void (*func)(void*), void* arg, void* dso) {
    (void)func; (void)arg; (void)dso;
    return 0;
}

void* __cxa_current_primary_exception_nid_postfix() { return nullptr; }
void __cxa_decrement_exception_refcount_nid_postfix(void* p) { (void)p; }
void __cxa_increment_exception_refcount_nid_postfix(void* p) { (void)p; }
void __cxa_rethrow_primary_exception_nid_postfix(void* p) { (void)p; }
bool __cxa_uncaught_exception_nid_postfix() { return false; }
unsigned int __cxa_uncaught_exceptions_nid_postfix() { return 0; }

void* __cxa_allocate_dependent_exception_nid_postfix() { return std::malloc(256); }
void __cxa_free_dependent_exception_nid_postfix(void* p) { std::free(p); }

[[noreturn]] void __cxa_throw_bad_array_new_length_nid_postfix() { throw std::bad_array_new_length(); }

_Unwind_Reason_Code _Unwind_RaiseException_nid_postfix(_Unwind_Exception* e) {
    (void)e;
    return _URC_END_OF_STACK;
}

[[noreturn]] void _Unwind_Resume_nid_postfix(_Unwind_Exception* e) {
    (void)e;
    std::terminate();
}

void _Unwind_DeleteException_nid_postfix(_Unwind_Exception* e) { (void)e; }

std::uintptr_t _Unwind_GetGR_nid_postfix(_Unwind_Context* ctx, int idx) { (void)ctx; (void)idx; return 0; }
void _Unwind_SetGR_nid_postfix(_Unwind_Context* ctx, int idx, std::uintptr_t v) { (void)ctx; (void)idx; (void)v; }
std::uintptr_t _Unwind_GetIP_nid_postfix(_Unwind_Context* ctx) { (void)ctx; return 0; }
void _Unwind_SetIP_nid_postfix(_Unwind_Context* ctx, std::uintptr_t v) { (void)ctx; (void)v; }

std::uintptr_t _Unwind_GetIPInfo_nid_postfix(_Unwind_Context* ctx, int* ipBefore) {
    (void)ctx;
    if (ipBefore) *ipBefore = 0;
    return 0;
}

std::uintptr_t _Unwind_GetCFA_nid_postfix(_Unwind_Context* ctx) { (void)ctx; return 0; }
std::uintptr_t _Unwind_GetLanguageSpecificData_nid_postfix(_Unwind_Context* ctx) { (void)ctx; return 0; }
std::uintptr_t _Unwind_GetRegionStart_nid_postfix(_Unwind_Context* ctx) { (void)ctx; return 0; }
std::uintptr_t _Unwind_GetDataRelBase_nid_postfix(_Unwind_Context* ctx) { (void)ctx; return 0; }
std::uintptr_t _Unwind_GetTextRelBase_nid_postfix(_Unwind_Context* ctx) { (void)ctx; return 0; }

_Unwind_Reason_Code _Unwind_ForcedUnwind_nid_postfix(_Unwind_Exception* e, _Unwind_Stop_Fn stop, void* arg) {
    (void)e; (void)stop; (void)arg;
    return _URC_END_OF_STACK;
}

_Unwind_Reason_Code _Unwind_Resume_or_Rethrow_nid_postfix(_Unwind_Exception* e) {
    (void)e;
    return _URC_END_OF_STACK;
}

_Unwind_Reason_Code _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn trace, void* arg) {
    (void)trace; (void)arg;
    return _URC_END_OF_STACK;
}

int dl_iterate_phdr_nid_postfix(int (*cb)(void*, std::size_t, void*), void* data) {
    (void)cb; (void)data;
    return 0;
}

void _ZNSt9exceptionD1Ev_nid_postfix() {}
void _ZNSt9exceptionD2Ev_nid_postfix() {}
void _ZNSt8bad_castD1Ev_nid_postfix() {}
void _ZNSt12out_of_rangeD1Ev_nid_postfix() {}
void _ZNSt12domain_errorD1Ev_nid_postfix() {}
void _ZNSt16invalid_argumentD1Ev_nid_postfix() {}
void _ZNSt13runtime_errorD2Ev_nid_postfix() {}
void _ZNSt8ios_base7failureD1Ev_nid_postfix() {}
void _ZNSt8ios_baseD2Ev_nid_postfix() {}

const char* _ZNKSt9exception4whatEv_nid_postfix() { return ""; }

[[noreturn]] void _ZSt9terminatev_nid_postfix() { std::terminate(); }
bool _ZSt18uncaught_exceptionv_nid_postfix() { return false; }

const std::error_category* _ZSt17iostream_categoryv_nid_postfix() { return &std::iostream_category(); }

std::uintptr_t _ZTVSt13runtime_error_nid_postfix[5] {};
std::uintptr_t _ZTVSt12system_error_nid_postfix[5] {};
std::uintptr_t _ZTVSt9exception_nid_postfix[5] {};
std::uintptr_t _ZTVSt16invalid_argument_nid_postfix[5] {};
std::uintptr_t _ZTVSt12out_of_range_nid_postfix[5] {};
std::uintptr_t _ZTVSt12domain_error_nid_postfix[5] {};
std::uintptr_t _ZTVSt8bad_cast_nid_postfix[5] {};
std::uintptr_t _ZTVSt11logic_error_nid_postfix[5] {};

void* _ZTVNSt8ios_base7failureE_nid_postfix = nullptr;
void* _ZTISt8bad_cast_nid_postfix = nullptr;
void* _ZTISt12out_of_range_nid_postfix = nullptr;
void* _ZTISt12domain_error_nid_postfix = nullptr;
void* _ZTISt16invalid_argument_nid_postfix = nullptr;
void* _ZTINSt8ios_base7failureE_nid_postfix = nullptr;

}
