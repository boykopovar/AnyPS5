#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadCancel(Pthread thread);
int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name);
int APS5_VABI scePthreadSemDestroy(PthreadSem* sem);
int APS5_VABI scePthreadSemPost(PthreadSem* sem);
int APS5_VABI scePthreadSemWait(PthreadSem* sem);
int APS5_VABI scePthreadSemTrywait(PthreadSem* sem);
int APS5_VABI scePthreadSemTimedwait(PthreadSem* sem, unsigned int usec);
int APS5_VABI scePthreadSemGetvalue(PthreadSem* sem, int* value);
int APS5_Test_GetPthreadSemWaiterCount(PthreadSem* sem);
int APS5_Test_GetPthreadSemApiPauseCount();
void APS5_Test_SetPthreadSemApiPause(int kind);
void APS5_Test_ReleasePthreadSemApiPause();
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int SCE_KERNEL_ERROR_EOVERFLOW = static_cast<int>(0x80020054u);
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);

static void Require(bool value) { if (!value) std::abort(); }

static void WaitForSemaphoreWaiters(PthreadSem* sem, int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (APS5_Test_GetPthreadSemWaiterCount(sem) != expected) {
        if (std::chrono::steady_clock::now() >= deadline) Require(false);
        std::this_thread::yield();
    }
}

static void WaitForSemaphoreApiPause() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (APS5_Test_GetPthreadSemApiPauseCount() != 1) {
        if (std::chrono::steady_clock::now() >= deadline) Require(false);
        std::this_thread::yield();
    }
}

static void CheckValueLimit() {
    constexpr unsigned int maximum = 0x7fffffffu;
    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 7, nullptr) == SCE_OK);
    const PthreadSem original = sem;
    const unsigned int invalidValues[] = {maximum + 1u, std::numeric_limits<unsigned int>::max()};
    int value = -1;
    for (const unsigned int initial : invalidValues) {
        Require(scePthreadSemInit(&sem, 0, initial, nullptr) == SCE_KERNEL_ERROR_EINVAL);
        Require(sem == original);
        Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 7);
    }
    Require(scePthreadSemDestroy(&sem) == SCE_OK);

    Require(scePthreadSemInit(&sem, 0, maximum, nullptr) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemPost(&sem) == SCE_KERNEL_ERROR_EOVERFLOW);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum - 1));
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemPost(&sem) == SCE_KERNEL_ERROR_EOVERFLOW);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}

struct Context {
    PthreadSem sem = nullptr;
};

static void* APS5_VABI Poster(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    return nullptr;
}

struct CancellableWaitContext {
    PthreadSem* sem;
};

static void* APS5_VABI CancellableWaiter(void* arg) {
    auto& context = *static_cast<CancellableWaitContext*>(arg);
    (void)scePthreadSemWait(context.sem);
    return nullptr;
}

int main() {
    CheckValueLimit();

    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 2, nullptr) == SCE_OK);
    int value = -1;
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 2);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 0);
    Require(scePthreadSemTimedwait(&sem, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 1);

    Context context;
    Require(scePthreadSemInit(&context.sem, 0, 0, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Poster, &context, nullptr) == SCE_OK);
    Require(scePthreadSemTimedwait(&context.sem, 5000000) == SCE_OK);
    Require(scePthreadSemWait(&context.sem) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadSemDestroy(&context.sem) == SCE_OK);

    Require(scePthreadSemDestroy(&context.sem) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadSemDestroy(&sem) == SCE_OK);

    PthreadSem busySem = nullptr;
    Require(scePthreadSemInit(&busySem, 0, 0, nullptr) == SCE_OK);
    std::atomic<int> waitResult{-1};
    std::thread waiterThread([&] {
        waitResult = scePthreadSemWait(&busySem);
    });
    WaitForSemaphoreWaiters(&busySem, 1);
    Require(scePthreadSemDestroy(&busySem) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadSemPost(&busySem) == SCE_OK);
    waiterThread.join();
    Require(waitResult == SCE_OK);
    Require(scePthreadSemDestroy(&busySem) == SCE_OK);

    PthreadSem cancelSem = nullptr;
    Require(scePthreadSemInit(&cancelSem, 0, 0, nullptr) == SCE_OK);
    CancellableWaitContext cancelContext{&cancelSem};
    Pthread cancelThread = nullptr;
    Require(scePthreadCreate(&cancelThread, nullptr, CancellableWaiter, &cancelContext, nullptr) == SCE_OK);
    WaitForSemaphoreWaiters(&cancelSem, 1);
    Require(scePthreadCancel(cancelThread) == SCE_OK);
    Require(scePthreadJoin(cancelThread, nullptr) == SCE_OK);
    Require(APS5_Test_GetPthreadSemWaiterCount(&cancelSem) == 0);
    Require(scePthreadSemDestroy(&cancelSem) == SCE_OK);

    for (int iter = 0; iter < 50; ++iter) {
        PthreadSem raceSem = nullptr;
        Require(scePthreadSemInit(&raceSem, 0, 0, nullptr) == SCE_OK);
        std::thread racer([&] {
            Require(scePthreadSemWait(&raceSem) == SCE_OK);
            Require(scePthreadSemDestroy(&raceSem) == SCE_OK);
        });
        WaitForSemaphoreWaiters(&raceSem, 1);
        Require(scePthreadSemPost(&raceSem) == SCE_OK);
        racer.join();
    }

    PthreadSem queuedSem = nullptr;
    Require(scePthreadSemInit(&queuedSem, 0, 0, nullptr) == SCE_OK);
    APS5_Test_SetPthreadSemApiPause(1);
    std::atomic<int> postResult{-1};
    std::thread concurrentPost([&] {
        try {
            postResult = scePthreadSemPost(&queuedSem);
        } catch (...) {
            postResult = -2;
        }
    });
    WaitForSemaphoreApiPause();
    Require(scePthreadSemDestroy(&queuedSem) == SCE_OK);
    APS5_Test_ReleasePthreadSemApiPause();
    concurrentPost.join();
    Require(postResult == SCE_KERNEL_ERROR_EINVAL);
}
