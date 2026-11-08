#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static constexpr int PosixSuccess = 0;
static constexpr int PosixEinval = 22;
static constexpr int PosixEfault = 14;
static constexpr int PosixEagain = 35;
static constexpr std::size_t MinExpectedKeys = 128;

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Assertion failed at line %d\n", line);
        std::abort();
    }
}

#define Check(condition) Require((condition), __LINE__)

static pthread_key_destructor_func_t AsDtor(void (APS5_VABI* function)(void*)) {
    return reinterpret_cast<pthread_key_destructor_func_t>(function);
}

static std::atomic<int> g_destructorCalls{0};
static std::atomic<void*> g_destructorObservedArg{nullptr};

static void APS5_VABI RecordDestructor(void* arg) {
    ++g_destructorCalls;
    g_destructorObservedArg = arg;
}

static void TestKeyCreateAndDestroyValidation() {
    Check(pthread_key_create_nid_postfix(nullptr, nullptr) == PosixEinval);
    Check(pthread_key_create_nid_postfix(reinterpret_cast<PthreadKey*>(0x10), nullptr) == PosixEfault);

    PthreadKey key = 0;
    Check(pthread_key_create_nid_postfix(&key, nullptr) == PosixSuccess);
    Check(key > 0);
    Check(pthread_getspecific_nid_postfix(key) == nullptr);

    int dummy = 42;
    Check(pthread_setspecific_nid_postfix(key, &dummy) == PosixSuccess);
    Check(pthread_getspecific_nid_postfix(key) == &dummy);

    Check(pthread_key_delete_nid_postfix(0) == PosixEinval);
    Check(pthread_key_delete_nid_postfix(0x7fffffff) == PosixEinval);
    Check(pthread_key_delete_nid_postfix(key) == PosixSuccess);
    Check(pthread_key_delete_nid_postfix(key) == PosixEinval);

    Check(pthread_setspecific_nid_postfix(key, &dummy) == PosixEinval);
    Check(pthread_getspecific_nid_postfix(key) == nullptr);
}

struct ThreadIsolationContext {
    PthreadKey key = 0;
    int expectedValue = 0;
    int observedValueBeforeSet = -1;
    int observedValueAfterSet = -1;
};

static void* APS5_VABI ThreadIsolationWorker(void* arg) {
    auto* context = static_cast<ThreadIsolationContext*>(arg);
    void* initial = pthread_getspecific_nid_postfix(context->key);
    context->observedValueBeforeSet = initial ? *static_cast<int*>(initial) : 0;
    Check(pthread_setspecific_nid_postfix(context->key, &context->expectedValue) == PosixSuccess);
    void* after = pthread_getspecific_nid_postfix(context->key);
    context->observedValueAfterSet = after ? *static_cast<int*>(after) : 0;
    return nullptr;
}

static void TestPerThreadValueIsolation() {
    PthreadKey key = 0;
    Check(pthread_key_create_nid_postfix(&key, nullptr) == PosixSuccess);

    int mainVal = 100;
    Check(pthread_setspecific_nid_postfix(key, &mainVal) == PosixSuccess);
    Check(pthread_getspecific_nid_postfix(key) == &mainVal);

    ThreadIsolationContext ctx1{key, 201, -1, -1};
    ThreadIsolationContext ctx2{key, 302, -1, -1};

    Pthread t1 = nullptr;
    Pthread t2 = nullptr;
    Check(pthread_create_nid_postfix(&t1, nullptr, ThreadIsolationWorker, &ctx1) == PosixSuccess);
    Check(pthread_create_nid_postfix(&t2, nullptr, ThreadIsolationWorker, &ctx2) == PosixSuccess);

    void* res1 = nullptr;
    void* res2 = nullptr;
    Check(pthread_join_nid_postfix(t1, &res1) == PosixSuccess);
    Check(pthread_join_nid_postfix(t2, &res2) == PosixSuccess);

    Check(ctx1.observedValueBeforeSet == 0);
    Check(ctx1.observedValueAfterSet == 201);
    Check(ctx2.observedValueBeforeSet == 0);
    Check(ctx2.observedValueAfterSet == 302);

    Check(pthread_getspecific_nid_postfix(key) == &mainVal);
    Check(*static_cast<int*>(pthread_getspecific_nid_postfix(key)) == 100);

    Check(pthread_key_delete_nid_postfix(key) == PosixSuccess);
}

static PthreadKey g_dtorKey = 0;
static int g_dtorPayload = 777;

static void* APS5_VABI DtorWorkerReturn(void*) {
    Check(pthread_setspecific_nid_postfix(g_dtorKey, &g_dtorPayload) == PosixSuccess);
    Check(pthread_getspecific_nid_postfix(g_dtorKey) == &g_dtorPayload);
    return nullptr;
}

static void TestDestructorExecutionOnThreadExit() {
    g_destructorCalls = 0;
    g_destructorObservedArg = nullptr;

    Check(pthread_key_create_nid_postfix(&g_dtorKey, AsDtor(RecordDestructor)) == PosixSuccess);

    Pthread thread = nullptr;
    Check(pthread_create_nid_postfix(&thread, nullptr, DtorWorkerReturn, nullptr) == PosixSuccess);
    void* result = nullptr;
    Check(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Check(g_destructorCalls == 1);
    Check(g_destructorObservedArg == &g_dtorPayload);

    Check(pthread_key_delete_nid_postfix(g_dtorKey) == PosixSuccess);
}

static void* APS5_VABI DtorWorkerExplicitExit(void*) {
    Check(pthread_setspecific_nid_postfix(g_dtorKey, &g_dtorPayload) == PosixSuccess);
    pthread_exit_nid_postfix(nullptr);
    return nullptr;
}

static void TestDestructorExecutionOnExplicitExit() {
    g_destructorCalls = 0;
    g_destructorObservedArg = nullptr;

    Check(pthread_key_create_nid_postfix(&g_dtorKey, AsDtor(RecordDestructor)) == PosixSuccess);

    Pthread thread = nullptr;
    Check(pthread_create_nid_postfix(&thread, nullptr, DtorWorkerExplicitExit, nullptr) == PosixSuccess);
    void* result = nullptr;
    Check(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Check(g_destructorCalls == 1);
    Check(g_destructorObservedArg == &g_dtorPayload);

    Check(pthread_key_delete_nid_postfix(g_dtorKey) == PosixSuccess);
}

static void* APS5_VABI NullValueWorker(void*) {
    Check(pthread_setspecific_nid_postfix(g_dtorKey, nullptr) == PosixSuccess);
    return nullptr;
}

static void TestNullValueDoesNotTriggerDestructor() {
    g_destructorCalls = 0;
    g_destructorObservedArg = nullptr;

    Check(pthread_key_create_nid_postfix(&g_dtorKey, AsDtor(RecordDestructor)) == PosixSuccess);

    Pthread thread = nullptr;
    Check(pthread_create_nid_postfix(&thread, nullptr, NullValueWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Check(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Check(g_destructorCalls == 0);
    Check(g_destructorObservedArg == nullptr);

    Check(pthread_key_delete_nid_postfix(g_dtorKey) == PosixSuccess);
}

static void* APS5_VABI DeleteBeforeExitWorker(void*) {
    Check(pthread_setspecific_nid_postfix(g_dtorKey, &g_dtorPayload) == PosixSuccess);
    Check(pthread_key_delete_nid_postfix(g_dtorKey) == PosixSuccess);
    return nullptr;
}

static void TestKeyDeletedBeforeThreadExitNoDestructor() {
    g_destructorCalls = 0;
    g_destructorObservedArg = nullptr;

    Check(pthread_key_create_nid_postfix(&g_dtorKey, AsDtor(RecordDestructor)) == PosixSuccess);

    Pthread thread = nullptr;
    Check(pthread_create_nid_postfix(&thread, nullptr, DeleteBeforeExitWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Check(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Check(g_destructorCalls == 0);
    Check(g_destructorObservedArg == nullptr);
}

static void TestKeySlotReuse() {
    PthreadKey keyA = 0;
    Check(pthread_key_create_nid_postfix(&keyA, nullptr) == PosixSuccess);
    Check(keyA > 0);
    Check(pthread_key_delete_nid_postfix(keyA) == PosixSuccess);

    PthreadKey keyB = 0;
    Check(pthread_key_create_nid_postfix(&keyB, nullptr) == PosixSuccess);
    Check(keyB == keyA);
    Check(pthread_getspecific_nid_postfix(keyB) == nullptr);

    int testVal = 999;
    Check(pthread_setspecific_nid_postfix(keyB, &testVal) == PosixSuccess);
    Check(pthread_getspecific_nid_postfix(keyB) == &testVal);
    Check(pthread_key_delete_nid_postfix(keyB) == PosixSuccess);
}

static void TestKeysMaxCapacityBounds() {
    std::vector<PthreadKey> keys;
    PthreadKey key = 0;
    int res = PosixSuccess;

    while (keys.size() < 10000 && (res = pthread_key_create_nid_postfix(&key, nullptr)) == PosixSuccess) {
        keys.push_back(key);
    }

    Check(res == PosixEagain);
    Check(keys.size() >= MinExpectedKeys);

    PthreadKey freedKey = keys.back();
    keys.pop_back();
    Check(pthread_key_delete_nid_postfix(freedKey) == PosixSuccess);
    Check(pthread_key_delete_nid_postfix(freedKey) == PosixEinval);

    PthreadKey newKey = 0;
    Check(pthread_key_create_nid_postfix(&newKey, nullptr) == PosixSuccess);
    Check(newKey == freedKey);
    keys.push_back(newKey);

    Check(pthread_key_create_nid_postfix(&key, nullptr) == PosixEagain);

    for (PthreadKey allocatedKey : keys) {
        Check(pthread_key_delete_nid_postfix(allocatedKey) == PosixSuccess);
    }

    Check(pthread_key_create_nid_postfix(&key, nullptr) == PosixSuccess);
    Check(pthread_key_delete_nid_postfix(key) == PosixSuccess);
}

static void TestScePthreadKeyInteroperability() {
    Check(scePthreadSetspecific(0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Check(scePthreadGetspecific(0) == nullptr);
    Check(scePthreadKeyDelete(0) == SCE_KERNEL_ERROR_EINVAL);

    PthreadKey sceKey = 0;
    Check(scePthreadKeyCreate(&sceKey, nullptr) == 0);
    Check(sceKey > 0);

    int interopVal = 555;
    Check(pthread_setspecific_nid_postfix(sceKey, &interopVal) == PosixSuccess);
    Check(scePthreadGetspecific(sceKey) == &interopVal);
    Check(pthread_getspecific_nid_postfix(sceKey) == &interopVal);

    Check(scePthreadKeyDelete(sceKey) == 0);
    Check(scePthreadGetspecific(sceKey) == nullptr);
    Check(pthread_getspecific_nid_postfix(sceKey) == nullptr);
}

int main() {
    TestKeyCreateAndDestroyValidation();
    TestPerThreadValueIsolation();
    TestDestructorExecutionOnThreadExit();
    TestDestructorExecutionOnExplicitExit();
    TestNullValueDoesNotTriggerDestructor();
    TestKeyDeletedBeforeThreadExitNoDestructor();
    TestKeySlotReuse();
    TestKeysMaxCapacityBounds();
    TestScePthreadKeyInteroperability();
    return 0;
}
