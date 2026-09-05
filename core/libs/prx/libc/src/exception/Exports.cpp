#include "Unwind.cpp"
#include "Runtime.cpp"
#include "Personality.cpp"
#include "StandardExceptions.cpp"
#include "ArraySupport.cpp"

asm(
".local __cxa_allocate_exception\n.set __cxa_allocate_exception,__cxa_allocate_exception_nid_postfix\n"
".local __cxa_free_exception\n.set __cxa_free_exception,__cxa_free_exception_nid_postfix\n"
".local __cxa_throw\n.set __cxa_throw,__cxa_throw_nid_postfix\n"
".local __cxa_begin_catch\n.set __cxa_begin_catch,__cxa_begin_catch_nid_postfix\n"
".local __cxa_end_catch\n.set __cxa_end_catch,__cxa_end_catch_nid_postfix\n"
".local __cxa_rethrow\n.set __cxa_rethrow,__cxa_rethrow_nid_postfix\n"
".local __gxx_personality_v0\n.set __gxx_personality_v0,__gxx_personality_v0_nid_postfix\n"
".local _Unwind_Resume\n.set _Unwind_Resume,_Unwind_Resume_nid_postfix\n"
".local _ZSt9terminatev\n.set _ZSt9terminatev,_ZSt9terminatev_nid_postfix\n"
".local __cxa_call_terminate\n.set __cxa_call_terminate,__cxa_call_terminate_nid_postfix\n"
);
