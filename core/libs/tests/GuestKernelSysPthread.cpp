#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include <cstdlib>

static constexpr int SCOPE_PROCESS = 0;
static constexpr int SCOPE_SYSTEM = 2;

static void Require(bool value) { if (!value) std::abort(); }

static void APS5_VABI Prepare() {}
static void APS5_VABI Parent() {}
static void APS5_VABI Child() {}

static void TestScope() {
    PthreadAttr attr{};
    Require(scePthreadAttrInit(&attr) == 0);
    int scope = -1;
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrSetscope(&attr, SCOPE_PROCESS) == 0);
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    for (const int invalid : {1, 3, -1}) {
        Require(scePthreadAttrSetscope(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
        scope = -1;
        Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    }
    Require(scePthreadAttrSetscope(&attr, SCOPE_SYSTEM) == 0);
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrGetscope(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetscope(nullptr, &scope) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetscope(nullptr, SCOPE_PROCESS) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == 0);
}

static void TestMutexAttributeGeneration() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    int generation = -1;
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, &generation) == 0 && generation == 0);
    Require(pthread_mutexattr_setgen_np_nid_postfix(&attr, 1) == 0);
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, &generation) == 0 && generation == 1);
    Require(pthread_mutexattr_setgen_np_nid_postfix(&attr, 2) == 45);
    Require(pthread_mutexattr_getgen_np_nid_postfix(nullptr, &generation) == 22);
    Require(pthread_mutexattr_getgen_np_nid_postfix(&attr, nullptr) == 22);
    Require(pthread_mutexattr_setgen_np_nid_postfix(nullptr, 0) == 22);
    PthreadMutexattr destroyedAttr = nullptr;
    Require(pthread_mutexattr_getgen_np_nid_postfix(&destroyedAttr, &generation) == 22);
    Require(pthread_mutexattr_setgen_np_nid_postfix(&destroyedAttr, 0) == 22);

    Require(scePthreadMutexattrSetgen(&attr, 0) == 0);
    Require(scePthreadMutexattrGetgen(&attr, &generation) == 0 && generation == 0);
    Require(scePthreadMutexattrSetgen(&attr, 2) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    Require(scePthreadMutexattrGetgen(nullptr, &generation) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestConcurrency() {
    Require(scePthreadSetconcurrency(3) == 0 && scePthreadGetconcurrency() == 3);
    Require(scePthreadSetconcurrency(-1) == SCE_KERNEL_ERROR_EINVAL && scePthreadGetconcurrency() == 3);
    Require(scePthreadSetconcurrency(0) == 0 && scePthreadGetconcurrency() == 0);
}

static void TestAtfork() {
    const auto before = PthreadAtforkHandlers_nid_no_patch().size();
    Require(scePthreadAtfork(Prepare, Parent, Child) == 0);
    Require(scePthreadAtfork(nullptr, nullptr, nullptr) == 0);
    const auto handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == before + 2);
    Require(handlers[before].prepare == Prepare && handlers[before].parent == Parent && handlers[before].child == Child);
    Require(handlers[before + 1].prepare == nullptr && handlers[before + 1].parent == nullptr && handlers[before + 1].child == nullptr);
}

int main() {
    TestScope();
    TestMutexAttributeGeneration();
    TestConcurrency();
    TestAtfork();
    return 0;
}
