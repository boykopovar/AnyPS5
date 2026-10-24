#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "Common.hpp"

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize);
int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, std::size_t size);
int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched);
int APS5_VABI sched_get_priority_max_nid_postfix(int policy);
int APS5_VABI sched_get_priority_min_nid_postfix(int policy);
}

static constexpr int GuestSchedFifo = 1;
static constexpr int GuestSchedRr = 3;

static bool Valid(const PthreadAttr* attr) {
    return attr && *attr;
}

extern "C" {

int APS5_VABI pthread_attr_destroy_nid_postfix(PthreadAttr* attr) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrDestroy(attr));
}

int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr) {
    if (!thread || !Valid(attr)) return PosixThread::GUEST_EINVAL;
    auto* p = *attr;
    p->_stacksize = thread->stackSize;
    p->stackAddress = thread->stackAddress;
    p->_guardsize = thread->guardSize;
    p->_detachstate = thread->_detached ? 1 : 0;
    p->_schedpriority = thread->priority.load(std::memory_order_relaxed);
    p->_schedpolicy = thread->schedPolicy.load(std::memory_order_relaxed);
    p->_affinity = thread->affinity.load(std::memory_order_relaxed);
    p->_affinityExplicit = true;
    p->_scope = thread->scope;
    return 0;
}

int APS5_VABI pthread_attr_getdetachstate_nid_postfix(const PthreadAttr* attr, int* state) {
    if (!Valid(attr) || !state) return PosixThread::GUEST_EINVAL;
    *state = (*attr)->_detachstate;
    return 0;
}

int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, size_t* guard_size) {
    if (!Valid(attr) || !guard_size) return PosixThread::GUEST_EINVAL;
    *guard_size = (*attr)->_guardsize;
    return 0;
}

int APS5_VABI pthread_attr_getinheritsched_nid_postfix(const PthreadAttr* attr, int* inherit_sched) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    if (!inherit_sched) throw std::runtime_error(std::string(__func__) + ": null output");
    *inherit_sched = (*attr)->_inheritsched;
    return 0;
}

int APS5_VABI pthread_attr_getschedparam_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
    if (!Valid(attr) || !param) return PosixThread::GUEST_EINVAL;
    param->sched_priority = (*attr)->_schedpriority;
    return 0;
}

int APS5_VABI pthread_attr_getschedpolicy_nid_postfix(const PthreadAttr* attr, int* policy) {
    if (!Valid(attr) || !policy) return PosixThread::GUEST_EINVAL;
    *policy = (*attr)->_schedpolicy;
    return 0;
}

int APS5_VABI pthread_attr_getscope_nid_postfix(const PthreadAttr* attr, int* scope) {
    if (!Valid(attr) || !scope) return PosixThread::GUEST_EINVAL;
    *scope = (*attr)->_scope;
    return 0;
}

int APS5_VABI pthread_attr_getstack_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr, size_t* __restrict stack_size) {
    if (!Valid(attr) || !stack_addr || !stack_size) return PosixThread::GUEST_EINVAL;
    *stack_addr = (*attr)->stackAddress;
    *stack_size = (*attr)->_stacksize;
    return 0;
}

int APS5_VABI pthread_attr_getstackaddr_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr) {
    if (!Valid(attr) || !stack_addr) return PosixThread::GUEST_EINVAL;
    *stack_addr = (*attr)->stackAddress;
    return 0;
}

int APS5_VABI pthread_attr_getstacksize_nid_postfix(const PthreadAttr* attr, size_t* stack_size) {
    if (!Valid(attr) || !stack_size) return PosixThread::GUEST_EINVAL;
    *stack_size = (*attr)->_stacksize;
    return 0;
}

int APS5_VABI pthread_attr_init_nid_postfix(PthreadAttr* attr) {
    if (!attr) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrInit(attr));
}

int APS5_VABI pthread_attr_setdetachstate_nid_postfix(PthreadAttr* attr, int state) {
    if (!Valid(attr) || (state != 0 && state != 1)) return PosixThread::GUEST_EINVAL;
    (*attr)->_detachstate = state;
    return 0;
}

int APS5_VABI pthread_attr_setguardsize_nid_postfix(PthreadAttr* attr, size_t guard_size) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    (*attr)->_guardsize = guard_size;
    return 0;
}

int APS5_VABI pthread_attr_setinheritsched_nid_postfix(PthreadAttr* attr, int inherit_sched) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    if (inherit_sched != GUEST_INHERIT_SCHED && inherit_sched != GUEST_EXPLICIT_SCHED) return PosixThread::GUEST_ENOTSUP;
    (*attr)->_inheritsched = inherit_sched;
    return 0;
}

int APS5_VABI pthread_attr_setschedparam_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    if (!param) return PosixThread::GUEST_ENOTSUP;
    const int policy = (*attr)->_schedpolicy;
    if ((policy == GuestSchedFifo || policy == GuestSchedRr) &&
        (param->sched_priority < sched_get_priority_min_nid_postfix(policy) || param->sched_priority > sched_get_priority_max_nid_postfix(policy)))
        return PosixThread::GUEST_ENOTSUP;
    (*attr)->_schedpriority = param->sched_priority;
    return 0;
}

int APS5_VABI pthread_attr_setschedpolicy_nid_postfix(PthreadAttr* attr, int policy) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    if (policy < GuestSchedFifo || policy > GuestSchedRr) return PosixThread::GUEST_ENOTSUP;
    (*attr)->_schedpolicy = policy;
    return 0;
}

int APS5_VABI pthread_attr_setscope_nid_postfix(PthreadAttr* attr, int scope) {
    if (!Valid(attr) || (scope != GUEST_SCOPE_PROCESS && scope != GUEST_SCOPE_SYSTEM)) return PosixThread::GUEST_EINVAL;
    (*attr)->_scope = scope;
    return 0;
}

int APS5_VABI pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, size_t stack_size) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrSetstacksize(attr, stack_size));
}

int APS5_VABI pthread_attr_setstack_nid_postfix(PthreadAttr* attr, void* stack_addr, size_t stack_size) {
    if (!Valid(attr) || !stack_addr || stack_size < 16384) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrSetstack(attr, stack_addr, stack_size));
}

int APS5_VABI pthread_attr_setsolosched_np_nid_postfix(PthreadAttr* attr, int solosched) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrSetsolosched(attr, solosched));
}

}
