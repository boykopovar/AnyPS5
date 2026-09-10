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
