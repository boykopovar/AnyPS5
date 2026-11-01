#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

constexpr int SCE_OK = 0;
constexpr int POSIX_EPERM = 1;
constexpr int POSIX_EINVAL = 22;
constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001u);
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

struct MutexHolder {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI HolderThread(void* arg) {
    auto& holder = *static_cast<MutexHolder*>(arg);
    Require(scePthreadMutexLock(&holder.mutex) == SCE_OK);
    holder.held.store(true);
    while (!holder.release.load()) {
        std::this_thread::yield();
    }
    Require(scePthreadMutexUnlock(&holder.mutex) == SCE_OK);
    return nullptr;
}

void TestPthreadCondSignalNullAndErrors() {
    bool threw = false;
    try {
        pthread_cond_signal_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        scePthreadCondSignal(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondDestroy(&cond) == SCE_OK);

    Require(pthread_cond_signal_nid_postfix(&cond) == POSIX_EINVAL);
    Require(scePthreadCondSignal(&cond) == SCE_KERNEL_ERROR_EINVAL);
}

void TestPthreadCondBroadcastNullAndErrors() {
    bool threw = false;
    try {
        pthread_cond_broadcast_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        scePthreadCondBroadcast(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondDestroy(&cond) == SCE_OK);

    Require(pthread_cond_broadcast_nid_postfix(&cond) == POSIX_EINVAL);
    Require(scePthreadCondBroadcast(&cond) == SCE_KERNEL_ERROR_EINVAL);
}

void TestPthreadCondWaitUnownedMutex() {
    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);

    bool threw = false;
    try {
        pthread_cond_wait_nid_postfix(nullptr, &mutex);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        scePthreadCondWait(nullptr, &mutex);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        pthread_cond_wait_nid_postfix(&cond, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        scePthreadCondWait(&cond, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == POSIX_EPERM);
    Require(scePthreadCondWait(&cond, &mutex) == SCE_KERNEL_ERROR_EPERM);

    PthreadMutex uninit = nullptr;
    Require(pthread_cond_wait_nid_postfix(&cond, &uninit) == POSIX_EPERM);
    Require(scePthreadCondWait(&cond, &uninit) == SCE_KERNEL_ERROR_EPERM);

    PthreadMutex destroyed = nullptr;
    Require(scePthreadMutexInit(&destroyed, nullptr, nullptr) == SCE_OK);
    Require(scePthreadMutexDestroy(&destroyed) == SCE_OK);
    Require(pthread_cond_wait_nid_postfix(&cond, &destroyed) == POSIX_EINVAL);
    Require(scePthreadCondWait(&cond, &destroyed) == SCE_KERNEL_ERROR_EINVAL);

    MutexHolder holder;
    Require(scePthreadMutexInit(&holder.mutex, nullptr, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, HolderThread, &holder, nullptr) == SCE_OK);
    while (!holder.held.load()) {
        std::this_thread::yield();
    }
    Require(pthread_cond_wait_nid_postfix(&cond, &holder.mutex) == POSIX_EPERM);
    Require(scePthreadCondWait(&cond, &holder.mutex) == SCE_KERNEL_ERROR_EPERM);
    holder.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadMutexDestroy(&holder.mutex) == SCE_OK);

    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    Require(scePthreadCondDestroy(&cond) == SCE_OK);
}

}

int main() {
    TestPthreadCondSignalNullAndErrors();
    TestPthreadCondBroadcastNullAndErrors();
    TestPthreadCondWaitUnownedMutex();
    return 0;
}
