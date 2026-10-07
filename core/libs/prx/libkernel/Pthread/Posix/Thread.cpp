#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "Common.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <chrono>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadRename(Pthread thread, const char* name);
int APS5_VABI scePthreadGetname(Pthread thread, char* name);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCancel(Pthread thread);
int APS5_VABI scePthreadSetcancelstate(int state, int* old_state);
int APS5_VABI scePthreadSetcanceltype(int type, int* old_type);
void APS5_VABI scePthreadTestcancel();
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
int APS5_VABI scePthreadGetprio(Pthread thread, int* prio);
}

static constexpr int GUEST_SCHED_FIFO = 1;

extern "C" {

int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg) {
    return PosixThread::ToErrno(scePthreadCreate(thread, attr, entry, arg, nullptr));
}

int APS5_VABI pthread_create_name_np_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return PosixThread::ToErrno(scePthreadCreate(thread, attr, entry, arg, name));
}

int APS5_VABI pthread_detach_nid_postfix(Pthread thread) {
    return PosixThread::ToErrno(scePthreadDetach(thread));
}

void APS5_VABI pthread_exit_nid_postfix(void* value) {
    scePthreadExit(value);
}

int APS5_VABI scePthreadGetschedparam(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!policy || !param) return SCE_KERNEL_ERROR_EINVAL;
    *policy = GUEST_SCHED_FIFO;
    return scePthreadGetprio(thread, &param->sched_priority);
}

int APS5_VABI scePthreadSetschedparam(Pthread thread, int policy, const KernelSchedParam* param) {
    (void)policy;
    if (!param) return SCE_KERNEL_ERROR_EINVAL;
    return scePthreadSetprio(thread, param->sched_priority);
}

int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!policy || !param) return PosixThread::GUEST_EINVAL;
    *policy = GUEST_SCHED_FIFO;
    return PosixThread::ToErrno(scePthreadGetprio(thread, &param->sched_priority));
}

int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** value) {
    return PosixThread::ToErrno(scePthreadJoin(thread, value));
}

int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name) {
    if (thread && name && std::strlen(name) >= 32) return PosixThread::ToErrno(SCE_KERNEL_ERROR_ENAMETOOLONG);
    return PosixThread::ToErrno(scePthreadRename(thread, name));
}

int APS5_VABI pthread_getname_np_nid_postfix(Pthread thread, char* name) {
    if (!thread) return PosixThread::GUEST_ESRCH;
    if (!name) return PosixThread::GUEST_EFAULT;
    return PosixThread::ToErrno(scePthreadGetname(thread, name));
}

Pthread APS5_VABI pthread_self_nid_postfix(void) {
    return scePthreadSelf();
}

int APS5_VABI pthread_equal_nid_postfix(Pthread first, Pthread second) {
    return first == second;
}

int APS5_VABI pthread_getcpuclockid_nid_postfix(Pthread thread, int* clockId) {
    constexpr int guestFault = 14;
    if (!thread) return PosixThread::GUEST_EINVAL;
    if (!clockId) return guestFault;
    *clockId = GuestThreadCpuClockId(thread);
    return 0;
}

int APS5_VABI sched_yield_nid_postfix(void) {
    std::this_thread::yield();
    return 0;
}

// Canonical lib is libScePosix (dead import of Cyberpunk 2077): 35 of its
// 36 sibling imports resolve to libkernel, and libScePosix is not in the
// game's NEEDED list so only a NEEDED module can satisfy the loader here.
int APS5_VABI pthread_cancel_nid_postfix(Pthread thread) {
    return PosixThread::ToErrno(scePthreadCancel(thread));
}

int APS5_VABI pthread_setcancelstate_nid_postfix(int state, int* old_state) {
    return PosixThread::ToErrno(scePthreadSetcancelstate(state, old_state));
}

int APS5_VABI pthread_setcanceltype_nid_postfix(int type, int* old_type) {
    return PosixThread::ToErrno(scePthreadSetcanceltype(type, old_type));
}

int APS5_VABI pthread_setprio_nid_postfix(Pthread thread, int prio) {
    return PosixThread::ToErrno(scePthreadSetprio(thread, prio));
}

int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    (void)policy;
    if (!param) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadSetprio(thread, param->sched_priority));
}

void APS5_VABI pthread_testcancel_nid_postfix(void) {
    scePthreadTestcancel();
}

void APS5_VABI pthread_yield_nid_postfix(void) {
    std::this_thread::yield();
}

unsigned int APS5_VABI sleep_nid_postfix(unsigned int seconds) {
    ThreadCancel::Check();
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    ThreadCancel::Check();
    return 0;
}

}
