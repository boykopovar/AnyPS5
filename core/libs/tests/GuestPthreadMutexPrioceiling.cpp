#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int GUEST_EINVAL = 22;
static constexpr int PRIO_NONE = 0;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr auto HoldTime = std::chrono::milliseconds(50);

static void Require(bool value) { if (!value) std::abort(); }

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct Holder {
    PthreadMutex* mutex;
    std::atomic<bool> held{false};
    std::atomic<bool> releasing{false};
    Pthread thread = nullptr;
};

static void* APS5_VABI Hold(void* arg) {
    auto& holder = *static_cast<Holder*>(arg);
    Require(pthread_mutex_lock_nid_postfix(holder.mutex) == 0);
    holder.held.store(true);
    std::this_thread::sleep_for(HoldTime);
    holder.releasing.store(true);
    Require(pthread_mutex_unlock_nid_postfix(holder.mutex) == 0);
    return nullptr;
}

static int Ceiling(PthreadMutex* mutex) {
    int ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(mutex, &ceiling) == 0);
    return ceiling;
}

static void RequireNoCeiling(PthreadMutex* mutex) {
    int ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(mutex, &ceiling) == GUEST_EINVAL && ceiling == -1);
    Require(pthread_mutex_getprioceiling_nid_postfix(mutex, nullptr) == GUEST_EINVAL);
    Require(pthread_mutex_setprioceiling_nid_postfix(mutex, 300, &ceiling) == GUEST_EINVAL && ceiling == -1);
    Require(scePthreadMutexGetprioceiling(mutex, &ceiling) == SCE_KERNEL_ERROR_EINVAL && ceiling == -1);
    Require(scePthreadMutexGetprioceiling(mutex, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexSetprioceiling(mutex, 300, &ceiling) == SCE_KERNEL_ERROR_EINVAL && ceiling == -1);
}

static void CheckWithoutPriorityProtection() {
    PthreadMutex mutex = nullptr;
    RequireNoCeiling(&mutex);
    Require(mutex == nullptr);
    mutex = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    RequireNoCeiling(&mutex);
    Require(mutex == reinterpret_cast<PthreadMutex>(std::uintptr_t{1}));
    mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    RequireNoCeiling(&mutex);
    Require(Throws([&] { pthread_mutex_getprioceiling_nid_postfix(nullptr, nullptr); }));
    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(nullptr, 300, nullptr); }));
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    RequireNoCeiling(&mutex);
}

static void CheckPriorityProtected() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == 0);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(Ceiling(&mutex) == 300);
    Require(Throws([&] { pthread_mutex_getprioceiling_nid_postfix(&mutex, nullptr); }));
    Require(Throws([&] { scePthreadMutexGetprioceiling(&mutex, nullptr); }));
    int ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&mutex, &ceiling) == SCE_OK && ceiling == 300);

    int previous = -1;
    Require(pthread_mutex_setprioceiling_nid_postfix(&mutex, 400, &previous) == 0 && previous == 300);
    Require(Ceiling(&mutex) == 400);
    Require(pthread_mutex_setprioceiling_nid_postfix(&mutex, 500, nullptr) == 0);
    Require(Ceiling(&mutex) == 500);
    Require(scePthreadMutexSetprioceiling(&mutex, 600, &previous) == SCE_OK && previous == 500);
    Require(Ceiling(&mutex) == 600);

    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(&mutex, 255, &previous); }));
    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(&mutex, 768, &previous); }));
    Require(previous == 500 && Ceiling(&mutex) == 600);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_setprioceiling_nid_postfix(&mutex, 256, &previous) == 0 && previous == 600);
    Require(Ceiling(&mutex) == 256);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Holder holder{&mutex};
    Require(scePthreadCreate(&holder.thread, nullptr, Hold, &holder, nullptr) == SCE_OK);
    while (!holder.held.load()) std::this_thread::yield();
    Require(pthread_mutex_setprioceiling_nid_postfix(&mutex, 767, &previous) == 0 && previous == 256);
    Require(holder.releasing.load());
    Require(scePthreadJoin(holder.thread, nullptr) == SCE_OK);
    Require(Ceiling(&mutex) == 767);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    RequireNoCeiling(&mutex);
}

static void CheckSceCreated() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_PROTECT) == SCE_OK);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_OK);
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, "prioceiling") == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    int ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&mutex, &ceiling) == SCE_OK && ceiling == 300);
    Require(Ceiling(&mutex) == 300);
    int previous = -1;
    Require(scePthreadMutexSetprioceiling(&mutex, 400, &previous) == SCE_OK && previous == 300);
    Require(Ceiling(&mutex) == 400);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    RequireNoCeiling(&mutex);

    for (const int protocol : {PRIO_NONE, PRIO_INHERIT}) {
        Require(scePthreadMutexattrInit(&attr) == SCE_OK);
        Require(scePthreadMutexattrSetprotocol(&attr, protocol) == SCE_OK);
        mutex = nullptr;
        Require(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
        Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
        RequireNoCeiling(&mutex);
        Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    }
}

static void CheckMixedPosixAndSce() {
    PthreadMutexattr posixAttr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&posixAttr) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&posixAttr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&posixAttr, 500) == 0);
    PthreadMutex sceMutex = nullptr;
    Require(scePthreadMutexInit(&sceMutex, &posixAttr, nullptr) == SCE_OK);
    Require(pthread_mutexattr_destroy_nid_postfix(&posixAttr) == 0);
    int ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&sceMutex, &ceiling) == SCE_OK && ceiling == 500);
    int previous = -1;
    Require(pthread_mutex_setprioceiling_nid_postfix(&sceMutex, 510, &previous) == 0 && previous == 500);
    Require(scePthreadMutexGetprioceiling(&sceMutex, &ceiling) == SCE_OK && ceiling == 510);
    Require(scePthreadMutexDestroy(&sceMutex) == SCE_OK);

    PthreadMutexattr sceAttr = nullptr;
    Require(scePthreadMutexattrInit(&sceAttr) == SCE_OK);
    Require(scePthreadMutexattrSetprotocol(&sceAttr, PRIO_PROTECT) == SCE_OK);
    Require(scePthreadMutexattrSetprioceiling(&sceAttr, 600) == SCE_OK);
    PthreadMutex posixMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&posixMutex, &sceAttr) == 0);
    Require(scePthreadMutexattrDestroy(&sceAttr) == SCE_OK);
    Require(Ceiling(&posixMutex) == 600);
    Require(scePthreadMutexSetprioceiling(&posixMutex, 700, &previous) == SCE_OK && previous == 600);
    Require(Ceiling(&posixMutex) == 700);
    Require(pthread_mutex_destroy_nid_postfix(&posixMutex) == 0);
}

int main() {
    CheckWithoutPriorityProtection();
    CheckPriorityProtected();
    CheckSceCreated();
    CheckMixedPosixAndSce();
}
