#include "Unwind.cpp"
#include "Runtime.cpp"
#include "StandardExceptions.cpp"
#include "ArraySupport.cpp"
#include "GuardSupport.cpp"
#include "Personality.cpp"

#include "prx/libc/include/specifics/gcc/SymbolAlias.hpp"

extern "C" [[noreturn]] GCC_HIDDEN_FN void APS5_VABI LibcRuntimeAssertion(const char*, int, const char*, const char*) { std::abort(); }

GCC_LOCAL_ALIAS(_ZSt21__glibcxx_assert_failPKciS0_S0_, LibcRuntimeAssertion);
GCC_GLOBAL_ALIAS(__cxa_allocate_exception_nid_no_patch_cut, __cxa_allocate_exception_nid_postfix);
GCC_GLOBAL_ALIAS(__cxa_throw_nid_no_patch_cut, __cxa_throw_nid_postfix);
GCC_GLOBAL_ALIAS(__cxa_begin_catch_nid_no_patch_cut, __cxa_begin_catch_nid_postfix);
GCC_GLOBAL_ALIAS(__cxa_end_catch_nid_no_patch_cut, __cxa_end_catch_nid_postfix);
GCC_GLOBAL_ALIAS(__cxa_rethrow_nid_no_patch_cut, __cxa_rethrow_nid_postfix);
GCC_GLOBAL_ALIAS(__gxx_personality_v0_nid_no_patch_cut, __gxx_personality_v0_nid_postfix);
GCC_GLOBAL_ALIAS(_Unwind_Resume_nid_no_patch_cut, _Unwind_Resume_nid_postfix);
GCC_LOCAL_ALIAS(__cxa_free_exception, __cxa_free_exception_nid_postfix);
GCC_LOCAL_ALIAS(_ZSt9terminatev, _ZSt9terminatev_nid_postfix);
GCC_LOCAL_ALIAS(__cxa_call_terminate, __cxa_call_terminate_nid_postfix);

#ifdef _WIN32
extern "C" {
void* NativeAllocateException(std::size_t size) asm("__cxa_allocate_exception");
void* NativeAllocateException(std::size_t size) {
    void* object = __cxa_allocate_exception_nid_postfix(size);
    LibcException::FromObject(object)->_pad = 1;
    return object;
}
void NativeFreeException(void* object) asm("__cxa_free_exception");
void NativeFreeException(void* object) { __cxa_free_exception_nid_postfix(object); }
[[noreturn]] void NativeThrow(void* object, std::type_info* type, void (*destructor)(void*)) asm("__cxa_throw");
[[noreturn]] void NativeThrow(void* object, std::type_info* type, void (*destructor)(void*)) {
    __cxa_throw_nid_postfix(object, type, destructor);
}
void* NativeBeginCatch(void* exception) asm("__cxa_begin_catch");
void* NativeBeginCatch(void* exception) { return __cxa_begin_catch_nid_postfix(exception); }
void NativeEndCatch() asm("__cxa_end_catch");
void NativeEndCatch() { __cxa_end_catch_nid_postfix(); }
[[noreturn]] void NativeRethrow() asm("__cxa_rethrow");
[[noreturn]] void NativeRethrow() { __cxa_rethrow_nid_postfix(); }
[[noreturn]] void NativeResume(_Unwind_Exception* exception) asm("_Unwind_Resume");
[[noreturn]] void NativeResume(_Unwind_Exception* exception) { _Unwind_Resume_nid_postfix(exception); }
[[noreturn]] void NativeTerminate() asm("_ZSt9terminatev");
[[noreturn]] void NativeTerminate() { LibcException::Terminate(); }
[[noreturn]] void NativeCallTerminate(void* exception) asm("__cxa_call_terminate");
[[noreturn]] void NativeCallTerminate(void* exception) { __cxa_call_terminate_nid_postfix(exception); }
void* NativeExceptionPointer(void* exception) asm("__cxa_get_exception_ptr");
void* NativeExceptionPointer(void* exception) { return __cxa_get_exception_ptr_nid_postfix(exception); }
void* NativeGlobals() asm("__cxa_get_globals");
void* NativeGlobals() { return &LibcException::globals; }
void* NativeGlobalsFast() asm("__cxa_get_globals_fast");
void* NativeGlobalsFast() { return &LibcException::globals; }
unsigned NativeUncaughtExceptions() asm("__cxa_uncaught_exceptions");
unsigned NativeUncaughtExceptions() { return LibcException::globals.uncaught; }
bool NativeUncaughtException() asm("__cxa_uncaught_exception");
bool NativeUncaughtException() { return LibcException::globals.uncaught != 0; }
}
#endif
