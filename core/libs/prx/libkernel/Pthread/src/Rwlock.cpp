#include "../include/Pthread.hpp"
#include "../include/Rwlock.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
 (void)rwlock;
 (void)attr;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
 (void)rwlock;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
 (void)attr;
 (void)type;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
