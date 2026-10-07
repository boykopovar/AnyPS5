#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <future>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param);
int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask);
int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, std::size_t* stackSize);
int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state);
int APS5_VABI pthread_attr_init_nid_postfix(PthreadAttr* attr);
int APS5_VABI pthread_attr_destroy_nid_postfix(PthreadAttr* attr);
int APS5_VABI pthread_attr_setstack_nid_postfix(PthreadAttr* attr, void* stack_addr, std::size_t stack_size);
int APS5_VABI pthread_attr_getstack_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr, std::size_t* __restrict stack_size);
int APS5_VABI pthread_attr_setstackaddr_nid_postfix(PthreadAttr* attr, void* stack_addr);
int APS5_VABI pthread_attr_getstackaddr_nid_postfix(const PthreadAttr* attr, void** stack_addr);
int APS5_VABI pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, std::size_t stack_size);
int APS5_VABI pthread_attr_getstacksize_nid_postfix(const PthreadAttr* attr, std::size_t* stack_size);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int EXPLICIT_SCHED = 0;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int CREATION_PRIORITY = 256;
static constexpr int UPDATED_PRIORITY = 767;
static constexpr KernelCpumask CREATION_AFFINITY = 0x3;
static constexpr KernelCpumask UPDATED_AFFINITY = 0x1000;
static constexpr std::size_t STACK_SIZE = 2u << 20;

static void Require(bool value) { if (!value) std::abort(); }

struct ReportedAttributes {
    int priority = 0;
    KernelCpumask affinity = 0;
    std::size_t stackSize = 0;
    int detachState = -1;
};

static ReportedAttributes Query(Pthread thread) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(thread, &attr) == SCE_OK);
    ReportedAttributes reported;
    KernelSchedParam param{};
    Require(scePthreadAttrGetschedparam(&attr, &param) == SCE_OK);
    reported.priority = param.sched_priority;
    Require(scePthreadAttrGetaffinity(&attr, &reported.affinity) == SCE_OK);
    Require(scePthreadAttrGetstacksize(&attr, &reported.stackSize) == SCE_OK);
    Require(scePthreadAttrGetdetachstate(&attr, &reported.detachState) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return reported;
}

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

int main() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_OK);
    const KernelSchedParam requested{CREATION_PRIORITY};
    Require(scePthreadAttrSetschedparam(&attr, &requested) == SCE_OK);
    Require(scePthreadAttrSetaffinity(&attr, CREATION_AFFINITY) == SCE_OK);
    Require(scePthreadAttrSetstacksize(&attr, STACK_SIZE) == SCE_OK);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &released, nullptr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    auto reported = Query(thread);
    Require(reported.priority == CREATION_PRIORITY);
    Require(reported.affinity == CREATION_AFFINITY);
    Require(reported.stackSize == STACK_SIZE);
    Require(reported.detachState == DETACH_JOINABLE);

    Require(scePthreadSetprio(thread, UPDATED_PRIORITY) == SCE_OK);
    Require(scePthreadSetaffinity(thread, UPDATED_AFFINITY) == SCE_OK);
    reported = Query(thread);
    Require(reported.priority == UPDATED_PRIORITY);
    Require(reported.affinity == UPDATED_AFFINITY);
    Require(reported.stackSize == STACK_SIZE);
    Require(reported.detachState == DETACH_JOINABLE);

    PthreadAttr destroyed = nullptr;
    Require(scePthreadAttrGet(thread, &destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGet(thread, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(nullptr, &attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    PthreadAttr posixAttr = nullptr;
    Require(pthread_attr_init_nid_postfix(&posixAttr) == 0);
    alignas(16) unsigned char dummyStack[65536] = {};
    alignas(16) unsigned char dummyStack2[65536] = {};
    Require(pthread_attr_setstack_nid_postfix(&posixAttr, dummyStack, sizeof(dummyStack)) == 0);
    void* queriedAddr = nullptr;
    std::size_t queriedSize = 0;
    Require(pthread_attr_getstack_nid_postfix(&posixAttr, &queriedAddr, &queriedSize) == 0);
    Require(queriedAddr == dummyStack && queriedSize == sizeof(dummyStack));
    Require(pthread_attr_getstackaddr_nid_postfix(&posixAttr, &queriedAddr) == 0 && queriedAddr == dummyStack);
    Require(pthread_attr_getstacksize_nid_postfix(&posixAttr, &queriedSize) == 0 && queriedSize == sizeof(dummyStack));
    Require(pthread_attr_setstackaddr_nid_postfix(&posixAttr, dummyStack2) == 0);
    Require(pthread_attr_getstackaddr_nid_postfix(&posixAttr, &queriedAddr) == 0 && queriedAddr == dummyStack2);
    Require(pthread_attr_setstacksize_nid_postfix(&posixAttr, 32768) == 0);
    Require(pthread_attr_getstacksize_nid_postfix(&posixAttr, &queriedSize) == 0 && queriedSize == 32768);
    Require(pthread_attr_setstack_nid_postfix(nullptr, dummyStack, sizeof(dummyStack)) == 22);
    Require(pthread_attr_setstack_nid_postfix(&posixAttr, nullptr, sizeof(dummyStack)) == 22);
    Require(pthread_attr_setstack_nid_postfix(&posixAttr, dummyStack, 1024) == 22);
    Require(pthread_attr_setstackaddr_nid_postfix(nullptr, dummyStack) == 22);
    Require(pthread_attr_setstackaddr_nid_postfix(&posixAttr, nullptr) == 22);
    Require(pthread_attr_getstackaddr_nid_postfix(nullptr, &queriedAddr) == 22);
    Require(pthread_attr_getstackaddr_nid_postfix(&posixAttr, nullptr) == 22);
    Require(pthread_attr_getstack_nid_postfix(nullptr, &queriedAddr, &queriedSize) == 22);
    Require(pthread_attr_getstack_nid_postfix(&posixAttr, nullptr, &queriedSize) == 22);
    Require(pthread_attr_getstack_nid_postfix(&posixAttr, &queriedAddr, nullptr) == 22);
    Require(pthread_attr_destroy_nid_postfix(&posixAttr) == 0);
}
