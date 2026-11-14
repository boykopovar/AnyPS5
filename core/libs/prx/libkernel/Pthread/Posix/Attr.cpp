#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "Common.hpp"

static constexpr int GuestSchedFifo = 1;
static constexpr int GuestSchedRr = 3;

static bool Valid(const PthreadAttr* attr) {
    return attr && *attr;
}

static int LoadAttribute(const PthreadAttr* attr, PthreadAttr& handle) {
    if (!attr) return PosixThread::GUEST_EINVAL;
    if (!GuestCopyIn_nid_no_patch(&handle, attr, sizeof(handle))) return PosixThread::GUEST_EFAULT;
    return handle ? 0 : PosixThread::GUEST_EINVAL;
}

template <typename Value>
static int StoreOutput(Value* destination, const Value& value) {
    if (!destination) return PosixThread::GUEST_EINVAL;
    return GuestCopyOut_nid_no_patch(destination, &value, sizeof(value)) ? 0 : PosixThread::GUEST_EFAULT;
}

extern "C" {

int APS5_VABI pthread_attr_destroy_nid_postfix(PthreadAttr* attr) {
    if (!Valid(attr)) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrDestroy(attr));
}

int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr) {
    if (!thread || !attr) return PosixThread::GUEST_EINVAL;
    PthreadAttr handle = nullptr;
    if (!GuestCopyIn_nid_no_patch(&handle, attr, sizeof(handle))) return PosixThread::GUEST_EFAULT;
    if (!handle) return PosixThread::GUEST_EINVAL;
    const auto live = LockLiveGuestThread(thread);
    if (!live) return PosixThread::GUEST_ESRCH;
    auto* p = handle;
    p->_stacksize = thread->stackSize;
    p->stackAddress = thread->stackAddress;
    p->_guardsize = thread->guardSize;
    p->_detachstate = thread->_detached ? 1 : 0;
    p->_schedpriority = thread->priority.load(std::memory_order_relaxed);
    p->_schedpolicy = thread->schedPolicy.load(std::memory_order_relaxed);
    p->_inheritsched = thread->inheritSched;
    p->_createSuspended = thread->createdSuspended;
    p->_affinity = thread->affinity.load(std::memory_order_relaxed);
    p->_affinityExplicit = true;
    p->_scope = thread->scope;
    return 0;
}

int APS5_VABI pthread_attr_getdetachstate_nid_postfix(const PthreadAttr* attr, int* state) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    return StoreOutput<int>(state, handle->_detachstate);
}

int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, size_t* guard_size) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    return StoreOutput<size_t>(guard_size, handle->_guardsize);
}

int APS5_VABI pthread_attr_getinheritsched_nid_postfix(const PthreadAttr* attr, int* inherit_sched) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    return StoreOutput<int>(inherit_sched, handle->_inheritsched);
}

int APS5_VABI pthread_attr_getschedparam_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    if (!param) return PosixThread::GUEST_EINVAL;
    const int priority = handle->_schedpriority;
    return StoreOutput<int>(&param->sched_priority, priority);
}

int APS5_VABI pthread_attr_getschedpolicy_nid_postfix(const PthreadAttr* attr, int* policy) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    return StoreOutput<int>(policy, handle->_schedpolicy);
}

int APS5_VABI pthread_attr_getscope_nid_postfix(const PthreadAttr* attr, int* scope) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    return StoreOutput<int>(scope, handle->_scope);
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
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    return StoreOutput<size_t>(stack_size, handle->_stacksize);
}

int APS5_VABI pthread_attr_init_nid_postfix(PthreadAttr* attr) {
    if (!attr) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadAttrInit(attr));
}

int APS5_VABI pthread_attr_setdetachstate_nid_postfix(PthreadAttr* attr, int state) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    if (state != 0 && state != 1) return PosixThread::GUEST_EINVAL;
    handle->_detachstate = state;
    return 0;
}

int APS5_VABI pthread_attr_setguardsize_nid_postfix(PthreadAttr* attr, size_t guard_size) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    handle->_guardsize = guard_size;
    return 0;
}

int APS5_VABI pthread_attr_setinheritsched_nid_postfix(PthreadAttr* attr, int inherit_sched) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    if (inherit_sched != GUEST_INHERIT_SCHED && inherit_sched != GUEST_EXPLICIT_SCHED) return PosixThread::GUEST_ENOTSUP;
    handle->_inheritsched = inherit_sched;
    return 0;
}

int APS5_VABI pthread_attr_setschedparam_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    if (!param) return PosixThread::GUEST_ENOTSUP;
    KernelSchedParam requested{};
    if (!GuestCopyIn_nid_no_patch(&requested, param, sizeof(requested))) return PosixThread::GUEST_EFAULT;
    const int policy = handle->_schedpolicy;
    if ((policy == GuestSchedFifo || policy == GuestSchedRr) &&
        (requested.sched_priority < sched_get_priority_min_nid_postfix(policy) || requested.sched_priority > sched_get_priority_max_nid_postfix(policy)))
        return PosixThread::GUEST_ENOTSUP;
    handle->_schedpriority = requested.sched_priority;
    return 0;
}

int APS5_VABI pthread_attr_setschedpolicy_nid_postfix(PthreadAttr* attr, int policy) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    if (policy < GuestSchedFifo || policy > GuestSchedRr) return PosixThread::GUEST_ENOTSUP;
    handle->_schedpolicy = policy;
    return 0;
}

int APS5_VABI pthread_attr_setscope_nid_postfix(PthreadAttr* attr, int scope) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
    if (scope != GUEST_SCOPE_PROCESS && scope != GUEST_SCOPE_SYSTEM) return PosixThread::GUEST_EINVAL;
    handle->_scope = scope;
    return 0;
}

int APS5_VABI pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, size_t stack_size) {
    PthreadAttr handle = nullptr;
    if (const int error = LoadAttribute(attr, handle)) return error;
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
