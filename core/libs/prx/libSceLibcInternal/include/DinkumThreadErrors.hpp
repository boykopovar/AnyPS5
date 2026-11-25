#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

struct DinkumThreadError {
    const void* vtable;
    GuestLocale::String message;
};

extern "C" {

void APS5_VABI _ZN6Dinkum7threads10lock_errorD0Ev_nid_postfix(DinkumThreadError* self);
void APS5_VABI _ZN6Dinkum7threads10lock_errorD1Ev_nid_postfix(DinkumThreadError* self);
[[noreturn]] void APS5_VABI _ZN6Dinkum7threads17_Throw_lock_errorEv_nid_postfix();
[[noreturn]] void APS5_VABI _ZN6Dinkum7threads21_Throw_resource_errorEv_nid_postfix();
void APS5_VABI _ZN6Dinkum7threads21thread_resource_errorD0Ev_nid_postfix(DinkumThreadError* self);
void APS5_VABI _ZN6Dinkum7threads21thread_resource_errorD1Ev_nid_postfix(DinkumThreadError* self);

}
