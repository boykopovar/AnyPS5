#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static constexpr int PosixSuccess = 0;
static constexpr int PosixEinval = 22;

static pthread_key_destructor_func_t AsDtor(void (APS5_VABI* function)(void*)) {
    return reinterpret_cast<pthread_key_destructor_func_t>(function);
}

struct KeyOrderItem {
    int id = 0;
    int observedOrder = 0;
    bool clearedInDestructor = false;
};

static std::array<PthreadKey, 4> g_orderKeys{};
static std::vector<int> g_executionOrder;
static std::atomic<int> g_orderCounter{0};

static void APS5_VABI OrderDtor(void* arg) {
    auto* item = static_cast<KeyOrderItem*>(arg);
    item->observedOrder = ++g_orderCounter;
    item->clearedInDestructor = (pthread_getspecific_nid_postfix(g_orderKeys[item->id]) == nullptr);
    g_executionOrder.push_back(item->id);
}

static void* APS5_VABI OrderWorker(void* arg) {
    auto* items = static_cast<KeyOrderItem*>(arg);
    for (int i = 0; i < 4; ++i) {
        Require(pthread_setspecific_nid_postfix(g_orderKeys[i], &items[i]) == PosixSuccess);
        Require(pthread_getspecific_nid_postfix(g_orderKeys[i]) == &items[i]);
    }
    return nullptr;
}

static void* APS5_VABI ExplicitExitWorker(void* arg) {
    auto* items = static_cast<KeyOrderItem*>(arg);
    for (int i = 0; i < 4; ++i) {
        Require(pthread_setspecific_nid_postfix(g_orderKeys[i], &items[i]) == PosixSuccess);
    }
    pthread_exit_nid_postfix(nullptr);
    return nullptr;
}

static std::atomic<int> g_nullDtorCalls{0};
static std::atomic<int> g_activeDtorCalls{0};
static PthreadKey g_nullKey = 0;
static PthreadKey g_activeKey = 0;

static void APS5_VABI NullDtor(void*) {
    ++g_nullDtorCalls;
}

static void APS5_VABI ActiveDtor(void*) {
    ++g_activeDtorCalls;
}

static void* APS5_VABI NullValueWorker(void*) {
    static int val = 123;
    Require(pthread_setspecific_nid_postfix(g_activeKey, &val) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_deletedDtorCalls{0};
static std::atomic<int> g_survivingDtorCalls{0};
static PthreadKey g_deletedKey = 0;
static PthreadKey g_survivingKey = 0;

static void APS5_VABI DeletedDtor(void*) {
    ++g_deletedDtorCalls;
}

static void APS5_VABI SurvivingDtor(void*) {
    ++g_survivingDtorCalls;
}

static void* APS5_VABI DeleteBeforeExitWorker(void*) {
    static int val1 = 11;
    static int val2 = 22;
    Require(pthread_setspecific_nid_postfix(g_deletedKey, &val1) == PosixSuccess);
    Require(pthread_setspecific_nid_postfix(g_survivingKey, &val2) == PosixSuccess);
    Require(pthread_key_delete_nid_postfix(g_deletedKey) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_initiatorCalls{0};
static std::atomic<int> g_victimCalls{0};
static PthreadKey g_initiatorKey = 0;
static PthreadKey g_victimKey = 0;

static void APS5_VABI InitiatorDtor(void*) {
    ++g_initiatorCalls;
    Require(pthread_key_delete_nid_postfix(g_victimKey) == PosixSuccess);
}

static void APS5_VABI VictimDtor(void*) {
    ++g_victimCalls;
}

static void* APS5_VABI DeleteDuringDtorWorker(void*) {
    static int val1 = 101;
    static int val2 = 202;
    Require(pthread_setspecific_nid_postfix(g_initiatorKey, &val1) == PosixSuccess);
    Require(pthread_setspecific_nid_postfix(g_victimKey, &val2) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_selfDeleteCalls{0};
static PthreadKey g_selfDeleteKey = 0;

static void APS5_VABI SelfDeleteDtor(void*) {
    ++g_selfDeleteCalls;
    Require(pthread_key_delete_nid_postfix(g_selfDeleteKey) == PosixSuccess);
    static int dummy = 7;
    Require(pthread_setspecific_nid_postfix(g_selfDeleteKey, &dummy) == PosixEinval);
    Require(pthread_key_delete_nid_postfix(g_selfDeleteKey) == PosixEinval);
}

static void* APS5_VABI SelfDeleteWorker(void*) {
    static int val = 55;
    Require(pthread_setspecific_nid_postfix(g_selfDeleteKey, &val) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_iterationCount{0};
static PthreadKey g_iterationKey = 0;
static int g_iterationVal = 999;

static void APS5_VABI IterationDtor(void*) {
    const int count = ++g_iterationCount;
    if (count < 10) {
        Require(pthread_setspecific_nid_postfix(g_iterationKey, &g_iterationVal) == PosixSuccess);
    }
}

static void* APS5_VABI IterationWorker(void*) {
    Require(pthread_setspecific_nid_postfix(g_iterationKey, &g_iterationVal) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_pingCalls{0};
static std::atomic<int> g_pongCalls{0};
static PthreadKey g_pingKey = 0;
static PthreadKey g_pongKey = 0;
static int g_pingData = 1;
static int g_pongData = 2;

static void APS5_VABI PingDtor(void*) {
    ++g_pingCalls;
    Require(pthread_setspecific_nid_postfix(g_pongKey, &g_pongData) == PosixSuccess);
}

static void APS5_VABI PongDtor(void*) {
    ++g_pongCalls;
    Require(pthread_setspecific_nid_postfix(g_pingKey, &g_pingData) == PosixSuccess);
}

static void* APS5_VABI PingPongWorker(void*) {
    Require(pthread_setspecific_nid_postfix(g_pingKey, &g_pingData) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_threadOneValue{0};
static std::atomic<int> g_threadTwoValue{0};
static PthreadKey g_multiThreadKey = 0;

static void APS5_VABI MultiThreadDtor(void* val) {
    const int num = *static_cast<int*>(val);
    if (num == 1000) {
        g_threadOneValue = 1000;
    } else if (num == 2000) {
        g_threadTwoValue = 2000;
    }
}

static void* APS5_VABI ThreadOneWorker(void*) {
    static int val1 = 1000;
    Require(pthread_setspecific_nid_postfix(g_multiThreadKey, &val1) == PosixSuccess);
    return nullptr;
}

static void* APS5_VABI ThreadTwoWorker(void*) {
    static int val2 = 2000;
    Require(pthread_setspecific_nid_postfix(g_multiThreadKey, &val2) == PosixSuccess);
    return nullptr;
}

static std::atomic<int> g_interopCalls{0};
static void* g_interopObservedVal = nullptr;

static void APS5_VABI InteropDtor(void* val) {
    ++g_interopCalls;
    g_interopObservedVal = val;
}

static void* APS5_VABI InteropWorker(void* arg) {
    auto* keyPtr = static_cast<PthreadKey*>(arg);
    static int payload = 888;
    Require(scePthreadSetspecific(*keyPtr, &payload) == PosixSuccess);
    Require(pthread_getspecific_nid_postfix(*keyPtr) == &payload);
    return nullptr;
}

static void TestKeyLifecycleAndValidation() {
    PthreadKey key = 0;
    Require(pthread_key_create_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(pthread_key_create_nid_postfix(&key, nullptr) == PosixSuccess);
    Require(key > 0);
    Require(pthread_getspecific_nid_postfix(key) == nullptr);

    int testVal = 42;
    Require(pthread_setspecific_nid_postfix(key, &testVal) == PosixSuccess);
    Require(pthread_getspecific_nid_postfix(key) == &testVal);

    Require(pthread_setspecific_nid_postfix(key, nullptr) == PosixSuccess);
    Require(pthread_getspecific_nid_postfix(key) == nullptr);

    Require(pthread_key_delete_nid_postfix(key) == PosixSuccess);
    Require(pthread_key_delete_nid_postfix(key) == PosixEinval);
    Require(pthread_setspecific_nid_postfix(key, &testVal) == PosixEinval);
    Require(pthread_getspecific_nid_postfix(key) == nullptr);

    Require(pthread_key_delete_nid_postfix(0) == PosixEinval);
    Require(pthread_key_delete_nid_postfix(-1) == PosixEinval);
    Require(pthread_key_delete_nid_postfix(513) == PosixEinval);
    Require(pthread_key_delete_nid_postfix(99999) == PosixEinval);

    Require(scePthreadKeyDelete(0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadKeyDelete(513) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadKeyDelete(key) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadSetspecific(key, &testVal) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetspecific(key) == nullptr);
}

static void TestDestructionOrderOnThreadExit() {
    g_executionOrder.clear();
    g_orderCounter = 0;

    for (int i = 0; i < 4; ++i) {
        Require(pthread_key_create_nid_postfix(&g_orderKeys[i], AsDtor(OrderDtor)) == PosixSuccess);
        Require(g_orderKeys[i] > 0);
    }

    std::array<KeyOrderItem, 4> items{};
    for (int i = 0; i < 4; ++i) {
        items[i].id = i;
    }

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, OrderWorker, items.data()) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_executionOrder.size() == 4);
    for (int i = 0; i < 4; ++i) {
        Require(g_executionOrder[i] == i);
        Require(items[i].observedOrder == i + 1);
        Require(items[i].clearedInDestructor);
    }

    for (int i = 0; i < 4; ++i) {
        Require(pthread_key_delete_nid_postfix(g_orderKeys[i]) == PosixSuccess);
    }
}

static void TestDestructionOnExplicitExit() {
    g_executionOrder.clear();
    g_orderCounter = 0;

    for (int i = 0; i < 4; ++i) {
        Require(pthread_key_create_nid_postfix(&g_orderKeys[i], AsDtor(OrderDtor)) == PosixSuccess);
    }

    std::array<KeyOrderItem, 4> items{};
    for (int i = 0; i < 4; ++i) {
        items[i].id = i;
    }

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, ExplicitExitWorker, items.data()) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_executionOrder.size() == 4);
    for (int i = 0; i < 4; ++i) {
        Require(g_executionOrder[i] == i);
        Require(items[i].observedOrder == i + 1);
        Require(items[i].clearedInDestructor);
    }

    for (int i = 0; i < 4; ++i) {
        Require(pthread_key_delete_nid_postfix(g_orderKeys[i]) == PosixSuccess);
    }
}

static void TestNullValueSkipped() {
    g_nullDtorCalls = 0;
    g_activeDtorCalls = 0;

    Require(pthread_key_create_nid_postfix(&g_nullKey, AsDtor(NullDtor)) == PosixSuccess);
    Require(pthread_key_create_nid_postfix(&g_activeKey, AsDtor(ActiveDtor)) == PosixSuccess);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, NullValueWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_activeDtorCalls == 1);
    Require(g_nullDtorCalls == 0);

    Require(pthread_key_delete_nid_postfix(g_nullKey) == PosixSuccess);
    Require(pthread_key_delete_nid_postfix(g_activeKey) == PosixSuccess);
}

static void TestKeyDeleteBeforeThreadExit() {
    g_deletedDtorCalls = 0;
    g_survivingDtorCalls = 0;

    Require(pthread_key_create_nid_postfix(&g_deletedKey, AsDtor(DeletedDtor)) == PosixSuccess);
    Require(pthread_key_create_nid_postfix(&g_survivingKey, AsDtor(SurvivingDtor)) == PosixSuccess);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, DeleteBeforeExitWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_deletedDtorCalls == 0);
    Require(g_survivingDtorCalls == 1);

    Require(pthread_key_delete_nid_postfix(g_survivingKey) == PosixSuccess);
}

static void TestKeyDeleteDuringDestructorExecution() {
    g_initiatorCalls = 0;
    g_victimCalls = 0;

    Require(pthread_key_create_nid_postfix(&g_initiatorKey, AsDtor(InitiatorDtor)) == PosixSuccess);
    Require(pthread_key_create_nid_postfix(&g_victimKey, AsDtor(VictimDtor)) == PosixSuccess);
    Require(g_initiatorKey < g_victimKey);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, DeleteDuringDtorWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_initiatorCalls == 1);
    Require(g_victimCalls == 0);

    Require(pthread_key_delete_nid_postfix(g_initiatorKey) == PosixSuccess);
    Require(pthread_key_delete_nid_postfix(g_victimKey) == PosixEinval);
}

static void TestSelfDeleteInsideDestructor() {
    g_selfDeleteCalls = 0;

    Require(pthread_key_create_nid_postfix(&g_selfDeleteKey, AsDtor(SelfDeleteDtor)) == PosixSuccess);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, SelfDeleteWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_selfDeleteCalls == 1);
    Require(pthread_key_delete_nid_postfix(g_selfDeleteKey) == PosixEinval);
}

static void TestDestructorIterationRounds() {
    g_iterationCount = 0;

    Require(pthread_key_create_nid_postfix(&g_iterationKey, AsDtor(IterationDtor)) == PosixSuccess);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, IterationWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_iterationCount == 4);
    Require(pthread_key_delete_nid_postfix(g_iterationKey) == PosixSuccess);
}

static void TestPingPongIterations() {
    g_pingCalls = 0;
    g_pongCalls = 0;

    Require(pthread_key_create_nid_postfix(&g_pingKey, AsDtor(PingDtor)) == PosixSuccess);
    Require(pthread_key_create_nid_postfix(&g_pongKey, AsDtor(PongDtor)) == PosixSuccess);
    Require(g_pingKey < g_pongKey);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, PingPongWorker, nullptr) == PosixSuccess);
    void* result = nullptr;
    Require(pthread_join_nid_postfix(thread, &result) == PosixSuccess);

    Require(g_pingCalls == 4);
    Require(g_pongCalls == 4);

    Require(pthread_key_delete_nid_postfix(g_pingKey) == PosixSuccess);
    Require(pthread_key_delete_nid_postfix(g_pongKey) == PosixSuccess);
}

static void TestMultipleThreadsIndependence() {
    g_threadOneValue = 0;
    g_threadTwoValue = 0;

    Require(pthread_key_create_nid_postfix(&g_multiThreadKey, AsDtor(MultiThreadDtor)) == PosixSuccess);

    Pthread thread1 = nullptr;
    Require(pthread_create_nid_postfix(&thread1, nullptr, ThreadOneWorker, nullptr) == PosixSuccess);
    void* result1 = nullptr;
    Require(pthread_join_nid_postfix(thread1, &result1) == PosixSuccess);

    Pthread thread2 = nullptr;
    Require(pthread_create_nid_postfix(&thread2, nullptr, ThreadTwoWorker, nullptr) == PosixSuccess);
    void* result2 = nullptr;
    Require(pthread_join_nid_postfix(thread2, &result2) == PosixSuccess);

    Require(g_threadOneValue == 1000);
    Require(g_threadTwoValue == 2000);

    Require(pthread_key_delete_nid_postfix(g_multiThreadKey) == PosixSuccess);
}

static void TestSceAndPosixInteroperability() {
    g_interopCalls = 0;
    g_interopObservedVal = nullptr;

    PthreadKey interopKey = 0;
    Require(scePthreadKeyCreate(&interopKey, AsDtor(InteropDtor)) == PosixSuccess);
    Require(interopKey > 0);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, InteropWorker, &interopKey, "Interop") == PosixSuccess);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == PosixSuccess);

    Require(g_interopCalls == 1);
    Require(g_interopObservedVal != nullptr);
    Require(*static_cast<int*>(g_interopObservedVal) == 888);

    Require(pthread_key_delete_nid_postfix(interopKey) == PosixSuccess);
}

int main() {
    TestKeyLifecycleAndValidation();
    TestDestructionOrderOnThreadExit();
    TestDestructionOnExplicitExit();
    TestNullValueSkipped();
    TestKeyDeleteBeforeThreadExit();
    TestKeyDeleteDuringDestructorExecution();
    TestSelfDeleteInsideDestructor();
    TestDestructorIterationRounds();
    TestPingPongIterations();
    TestMultipleThreadsIndependence();
    TestSceAndPosixInteroperability();
    return 0;
}
