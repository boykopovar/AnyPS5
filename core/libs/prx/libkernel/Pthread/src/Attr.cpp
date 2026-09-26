#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int DETACH_DETACHED = 1;
static constexpr int SCHED_FIFO_PS5 = 1;

#ifdef _WIN32
#include <windows.h>
#include <limits>
#endif

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr) {
    if (!attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    auto* p = new (std::nothrow) PthreadAttrPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_stacksize = DEFAULT_STACK_SIZE;
    p->_detachstate = DETACH_JOINABLE;
    p->_schedpriority = 700;
    p->_schedpolicy = SCHED_FIFO_PS5;
    p->_inheritsched = 4;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr) {
    if (!attr || !*attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate) {
    if (!attr || !*attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    if (detachstate != DETACH_JOINABLE && detachstate != DETACH_DETACHED)
        throw std::runtime_error(std::string(__func__) + ": invalid state");
    (*attr)->_detachstate = detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!attr || !*attr || !param)
        throw std::runtime_error(std::string(__func__) + ": null arg");
    (*attr)->_schedpriority = param->sched_priority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize) {
    if (!attr || !*attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    if (stacksize < 16384) throw std::runtime_error(std::string(__func__) + ": too small");
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (stacksize % system.dwPageSize != 0 || stacksize > std::numeric_limits<unsigned>::max())
        throw std::runtime_error(std::string(__func__) + ": invalid Windows stack size");
#endif
    (*attr)->_stacksize = stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    if (!attr || !*attr || !stackaddr || !stacksize)
        throw std::runtime_error(std::string(__func__) + ": null arg");
    *stackaddr = (*attr)->stackAddress;
    *stacksize = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    if (!thread || !attr || !*attr)
        return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_stacksize = thread->stackSize;
    (*attr)->stackAddress = thread->stackAddress;
    (*attr)->_detachstate = thread->_detached ? DETACH_DETACHED : DETACH_JOINABLE;
    (*attr)->_schedpriority = 700;
    (*attr)->_schedpolicy = SCHED_FIFO_PS5;
    (*attr)->_inheritsched = 4;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
    if (!attr || !*attr || !state) throw std::runtime_error(std::string(__func__) + ": null arg");
    *state = (*attr)->_detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
    if (!attr || !*attr || !param) throw std::runtime_error(std::string(__func__) + ": null arg");
    param->sched_priority = (*attr)->_schedpriority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
    if (!attr || !*attr || !stack_addr) throw std::runtime_error(std::string(__func__) + ": null arg");
    *stack_addr = (*attr)->stackAddress;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
    if (!attr || !*attr || !stack_size) throw std::runtime_error(std::string(__func__) + ": null arg");
    *stack_size = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
 (void)attr;
 (void)mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
 (void)attr;
 (void)guard_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
    if (!attr || !*attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    (*attr)->_inheritsched = inherit_sched;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
    if (!attr || !*attr) throw std::runtime_error(std::string(__func__) + ": null attr");
    (*attr)->_schedpolicy = policy;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
 (void)attr;
 (void)solosched;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
    if (!attr || !*attr || !addr) throw std::runtime_error(std::string(__func__) + ": null arg");
    if (size < 16384) throw std::runtime_error(std::string(__func__) + ": too small");
    (*attr)->stackAddress = addr;
    (*attr)->_stacksize = size;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
    if (!attr || !*attr || !addr) throw std::runtime_error(std::string(__func__) + ": null arg");
    (*attr)->stackAddress = addr;
    return SCE_OK;
}

}
