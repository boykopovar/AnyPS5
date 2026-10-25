#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <stdexcept>
#include <string>

static constexpr int SCE_OK = 0;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr std::size_t MIN_STACK_SIZE = 16384;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int DETACH_DETACHED = 1;
static constexpr int SCHED_FIFO_PS5 = 1;

static std::atomic<std::size_t> defaultStackSize{DEFAULT_STACK_SIZE};

std::size_t GetDefaultStackSize_nid_no_patch() {
    return defaultStackSize.load(std::memory_order_relaxed);
}

bool SetDefaultStackSize_nid_no_patch(std::size_t size) {
    if (size <= MIN_STACK_SIZE) return false;
    defaultStackSize.store(size, std::memory_order_relaxed);
    return true;
}

#ifdef _WIN32
#include <windows.h>
#include <limits>
#endif

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr) {
    if (!attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    auto* p = new (std::nothrow) PthreadAttrPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_stacksize = GetDefaultStackSize_nid_no_patch();
    p->_detachstate = DETACH_JOINABLE;
    p->_schedpriority = 700;
    p->_schedpolicy = SCHED_FIFO_PS5;
    p->_inheritsched = 4;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (detachstate != DETACH_JOINABLE && detachstate != DETACH_DETACHED) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_detachstate = detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int HighestPriority = 256;
    constexpr int LowestPriority = 767;
    if (!param || param->sched_priority < HighestPriority || param->sched_priority > LowestPriority)
        return SCE_KERNEL_ERROR_EOPNOTSUPP;
    (*attr)->_schedpriority = param->sched_priority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (stacksize < MIN_STACK_SIZE) return SCE_KERNEL_ERROR_EINVAL;
#ifdef _WIN32
    if (stacksize > std::numeric_limits<unsigned>::max() - 0xffffu)
        throw std::runtime_error(std::string(__func__) + ": stack size exceeds the Windows limit");
#endif
    (*attr)->_stacksize = stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    if (!attr || !*attr || !stackaddr || !stacksize) return SCE_KERNEL_ERROR_EINVAL;
    *stackaddr = (*attr)->stackAddress;
    *stacksize = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    if (!thread || !attr || !*attr)
        return SCE_KERNEL_ERROR_EINVAL;
    const auto live = LockLiveGuestThread(thread);
    if (!live) return SCE_KERNEL_ERROR_ESRCH;
    (*attr)->_stacksize = thread->stackSize;
    (*attr)->stackAddress = thread->stackAddress;
    (*attr)->_detachstate = thread->_detached ? DETACH_DETACHED : DETACH_JOINABLE;
    (*attr)->_schedpriority = thread->priority.load(std::memory_order_relaxed);
    (*attr)->_schedpolicy = thread->schedPolicy.load(std::memory_order_relaxed);
    (*attr)->_inheritsched = 4;
    (*attr)->_affinity = thread->affinity.load(std::memory_order_relaxed);
    (*attr)->_affinityExplicit = true;
    (*attr)->_scope = thread->scope;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (!mask) throw std::runtime_error("scePthreadAttrGetaffinity: null argument");
    *mask = (*attr)->_affinity;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
    if (!attr || !*attr || !state) return SCE_KERNEL_ERROR_EINVAL;
    *state = (*attr)->_detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
    if (!attr || !*attr || !guard_size) return SCE_KERNEL_ERROR_EINVAL;
    *guard_size = (*attr)->_guardsize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetinheritsched(const PthreadAttr* attr, int* inheritSched) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (!inheritSched) throw std::runtime_error(std::string(__func__) + ": null output");
    *inheritSched = (*attr)->_inheritsched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
    if (!attr || !*attr || !param) return SCE_KERNEL_ERROR_EINVAL;
    param->sched_priority = (*attr)->_schedpriority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetschedpolicy(const PthreadAttr* attr, int* policy) {
    if (!attr || !*attr || !policy) return SCE_KERNEL_ERROR_EINVAL;
    *policy = (*attr)->_schedpolicy;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (!solosched) throw std::runtime_error(std::string(__func__) + ": null output");
    *solosched = (*attr)->_solosched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
    if (!attr || !*attr || !stack_addr) return SCE_KERNEL_ERROR_EINVAL;
    *stack_addr = (*attr)->stackAddress;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
    if (!attr || !*attr || !stack_size) return SCE_KERNEL_ERROR_EINVAL;
    *stack_size = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_affinity = mask;
    (*attr)->_affinityExplicit = true;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_guardsize = guard_size;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int ExplicitSched = 0;
    constexpr int InheritSched = 4;
    if (inherit_sched != ExplicitSched && inherit_sched != InheritSched) return SCE_KERNEL_ERROR_EOPNOTSUPP;
    (*attr)->_inheritsched = inherit_sched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int SchedRr = 3;
    if (policy != SCHED_FIFO_PS5 && policy != SchedRr) return SCE_KERNEL_ERROR_EOPNOTSUPP;
    (*attr)->_schedpolicy = policy;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int SoloSchedOff = 0;
    constexpr int SoloSchedOn = 16;
    if (solosched != SoloSchedOff && solosched != SoloSchedOn) return SCE_KERNEL_ERROR_EOPNOTSUPP;
    (*attr)->_solosched = solosched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
    if (!attr || !*attr || !addr || size < MIN_STACK_SIZE) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->stackAddress = addr;
    (*attr)->_stacksize = size;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
    if (!attr || !*attr || !addr) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->stackAddress = addr;
    return SCE_OK;
}

}
