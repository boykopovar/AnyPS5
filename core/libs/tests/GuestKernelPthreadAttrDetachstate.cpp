#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int PTHREAD_CREATE_JOINABLE = 0;
static constexpr int PTHREAD_CREATE_DETACHED = 1;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EINVAL = 22;

static void Require(bool value) {
    if (!value) std::abort();
}

struct TestWorker {
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
};

static void* APS5_VABI WorkerFunc(void* arg) {
    auto* worker = static_cast<TestWorker*>(arg);
    worker->started.store(true, std::memory_order_release);
    while (!worker->release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    worker->finished.store(true, std::memory_order_release);
    return arg;
}

static void TestPosixAttrDefault() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);
    int state = -1;
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
    Require(state == PTHREAD_CREATE_JOINABLE);
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

static void TestPosixAttrSetValid() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    int state = -1;

    Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_DETACHED) == 0);
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
    Require(state == PTHREAD_CREATE_DETACHED);

    Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_JOINABLE) == 0);
    state = -1;
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
    Require(state == PTHREAD_CREATE_JOINABLE);

    Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_JOINABLE) == 0);
    state = -1;
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
    Require(state == PTHREAD_CREATE_JOINABLE);

    Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_DETACHED) == 0);
    state = -1;
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
    Require(state == PTHREAD_CREATE_DETACHED);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
}

static void TestPosixAttrSetInvalidState() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    int state = -1;

    for (const int invalidState : {-1000, -50, -2, -1, 2, 3, 4, 10, 100, 1000, 0x7fffffff, -0x7fffffff}) {
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, invalidState) == GUEST_EINVAL);
        state = -1;
        Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
        Require(state == PTHREAD_CREATE_JOINABLE);
    }

    Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_DETACHED) == 0);
    for (const int invalidState : {-100, -5, -1, 2, 3, 99, 500}) {
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, invalidState) == GUEST_EINVAL);
        state = -1;
        Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == 0);
        Require(state == PTHREAD_CREATE_DETACHED);
    }

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
}

static void TestPosixAttrNullPointers() {
    PthreadAttr attr = nullptr;
    PthreadAttr unset = nullptr;
    int state = -1;

    Require(pthread_attr_setdetachstate_nid_postfix(nullptr, PTHREAD_CREATE_JOINABLE) == GUEST_EINVAL);
    Require(pthread_attr_setdetachstate_nid_postfix(nullptr, PTHREAD_CREATE_DETACHED) == GUEST_EINVAL);
    Require(pthread_attr_setdetachstate_nid_postfix(nullptr, -1) == GUEST_EINVAL);
    Require(pthread_attr_setdetachstate_nid_postfix(&unset, PTHREAD_CREATE_JOINABLE) == GUEST_EINVAL);
    Require(pthread_attr_setdetachstate_nid_postfix(&unset, PTHREAD_CREATE_DETACHED) == GUEST_EINVAL);
    Require(pthread_attr_setdetachstate_nid_postfix(&unset, 2) == GUEST_EINVAL);

    Require(pthread_attr_getdetachstate_nid_postfix(nullptr, &state) == GUEST_EINVAL);
    Require(pthread_attr_getdetachstate_nid_postfix(&unset, &state) == GUEST_EINVAL);

    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, nullptr) == GUEST_EINVAL);
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);

    state = -1;
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &state) == GUEST_EINVAL);
    Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_JOINABLE) == GUEST_EINVAL);
}

static void TestSceAttrDetachstate() {
    PthreadAttr attr = nullptr;
    PthreadAttr unset = nullptr;
    int state = -1;

    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_OK);
    Require(state == PTHREAD_CREATE_JOINABLE);

    Require(scePthreadAttrSetdetachstate(&attr, PTHREAD_CREATE_DETACHED) == SCE_OK);
    state = -1;
    Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_OK);
    Require(state == PTHREAD_CREATE_DETACHED);

    Require(scePthreadAttrSetdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == SCE_OK);
    state = -1;
    Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_OK);
    Require(state == PTHREAD_CREATE_JOINABLE);

    for (const int invalidState : {-100, -1, 2, 3, 50, 1000}) {
        Require(scePthreadAttrSetdetachstate(&attr, invalidState) == SCE_KERNEL_ERROR_EINVAL);
        state = -1;
        Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_OK);
        Require(state == PTHREAD_CREATE_JOINABLE);
    }

    Require(scePthreadAttrSetdetachstate(nullptr, PTHREAD_CREATE_JOINABLE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetdetachstate(&unset, PTHREAD_CREATE_JOINABLE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetdetachstate(nullptr, &state) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetdetachstate(&unset, &state) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetdetachstate(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(scePthreadAttrSetdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_KERNEL_ERROR_EINVAL);
}

static void TestThreadJoinableVsDetached() {
    {
        PthreadAttr attr = nullptr;
        Require(pthread_attr_init_nid_postfix(&attr) == 0);
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_JOINABLE) == 0);

        TestWorker worker;
        Pthread thread = nullptr;
        Require(pthread_create_nid_postfix(&thread, &attr, WorkerFunc, &worker) == 0);
        Require(thread != nullptr);

        while (!worker.started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        PthreadAttr queryAttr = nullptr;
        Require(scePthreadAttrInit(&queryAttr) == SCE_OK);
        Require(scePthreadAttrGet(thread, &queryAttr) == SCE_OK);
        int reported = -1;
        Require(pthread_attr_getdetachstate_nid_postfix(&queryAttr, &reported) == 0);
        Require(reported == PTHREAD_CREATE_JOINABLE);
        Require(scePthreadAttrDestroy(&queryAttr) == SCE_OK);

        worker.release.store(true, std::memory_order_release);
        void* ret = nullptr;
        Require(pthread_join_nid_postfix(thread, &ret) == 0);
        Require(ret == &worker);
        Require(pthread_join_nid_postfix(thread, &ret) == GUEST_ESRCH);

        Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    }

    {
        PthreadAttr attr = nullptr;
        Require(pthread_attr_init_nid_postfix(&attr) == 0);
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_DETACHED) == 0);

        TestWorker worker;
        Pthread thread = nullptr;
        Require(pthread_create_nid_postfix(&thread, &attr, WorkerFunc, &worker) == 0);
        Require(thread != nullptr);

        while (!worker.started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        PthreadAttr queryAttr = nullptr;
        Require(scePthreadAttrInit(&queryAttr) == SCE_OK);
        Require(scePthreadAttrGet(thread, &queryAttr) == SCE_OK);
        int reported = -1;
        Require(pthread_attr_getdetachstate_nid_postfix(&queryAttr, &reported) == 0);
        Require(reported == PTHREAD_CREATE_DETACHED);
        Require(scePthreadAttrDestroy(&queryAttr) == SCE_OK);

        void* ret = nullptr;
        Require(pthread_join_nid_postfix(thread, &ret) == GUEST_EINVAL);
        Require(scePthreadJoin(thread, &ret) == SCE_KERNEL_ERROR_EINVAL);
        Require(pthread_detach_nid_postfix(thread) == GUEST_EINVAL);
        Require(scePthreadDetach(thread) == SCE_KERNEL_ERROR_EINVAL);

        worker.release.store(true, std::memory_order_release);
        while (!worker.finished.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    }

    {
        PthreadAttr attr = nullptr;
        Require(pthread_attr_init_nid_postfix(&attr) == 0);
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, PTHREAD_CREATE_JOINABLE) == 0);

        TestWorker worker;
        Pthread thread = nullptr;
        Require(pthread_create_nid_postfix(&thread, &attr, WorkerFunc, &worker) == 0);
        Require(thread != nullptr);

        while (!worker.started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        Require(pthread_detach_nid_postfix(thread) == 0);

        PthreadAttr queryAttr = nullptr;
        Require(scePthreadAttrInit(&queryAttr) == SCE_OK);
        Require(scePthreadAttrGet(thread, &queryAttr) == SCE_OK);
        int reported = -1;
        Require(pthread_attr_getdetachstate_nid_postfix(&queryAttr, &reported) == 0);
        Require(reported == PTHREAD_CREATE_DETACHED);
        Require(scePthreadAttrDestroy(&queryAttr) == SCE_OK);

        void* ret = nullptr;
        Require(pthread_join_nid_postfix(thread, &ret) == GUEST_EINVAL);
        Require(pthread_detach_nid_postfix(thread) == GUEST_EINVAL);

        worker.release.store(true, std::memory_order_release);
        while (!worker.finished.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    }
}

int main() {
    TestPosixAttrDefault();
    TestPosixAttrSetValid();
    TestPosixAttrSetInvalidState();
    TestPosixAttrNullPointers();
    TestSceAttrDetachstate();
    TestThreadJoinableVsDetached();
    return 0;
}
