#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexGetspinloops(PthreadMutex* mutex, int* count);
int APS5_VABI scePthreadMutexSetspinloops(PthreadMutex* mutex, int count);
int APS5_VABI scePthreadMutexGetyieldloops(PthreadMutex* mutex, int* count);
int APS5_VABI scePthreadMutexSetyieldloops(PthreadMutex* mutex, int count);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_reltimedlock_np_nid_postfix(PthreadMutex* mutex, KernelUseconds usec);
int APS5_VABI pthread_mutex_isowned_np_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_getspinloops_np_nid_postfix(PthreadMutex* mutex, int* count);
int APS5_VABI pthread_mutex_setspinloops_np_nid_postfix(PthreadMutex* mutex, int count);
int APS5_VABI pthread_mutex_getyieldloops_np_nid_postfix(PthreadMutex* mutex, int* count);
int APS5_VABI pthread_mutex_setyieldloops_np_nid_postfix(PthreadMutex* mutex, int count);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010u);

static constexpr int POSIX_EDEADLK = 11;
static constexpr int POSIX_EINVAL = 22;
static constexpr int POSIX_ETIMEDOUT = 60;
static const PthreadMutex AdaptiveInitializer = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
static const PthreadMutex DestroyedMutex = reinterpret_cast<PthreadMutex>(std::uintptr_t{2});

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

struct OtherThread {
    PthreadMutex* mutex;
    int isowned = -1;
    int reltimedlock = -1;
};

static void* APS5_VABI CheckFromOtherThread(void* arg) {
    auto& other = *static_cast<OtherThread*>(arg);
    other.isowned = pthread_mutex_isowned_np_nid_postfix(other.mutex);
    other.reltimedlock = pthread_mutex_reltimedlock_np_nid_postfix(other.mutex, 1000);
    return nullptr;
}

static void CheckOwnershipAndRelativeLock() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_isowned_np_nid_postfix(&mutex) == 0 && mutex == nullptr);
    PthreadMutex adaptive = AdaptiveInitializer;
    Require(pthread_mutex_isowned_np_nid_postfix(&adaptive) == 0 && adaptive == AdaptiveInitializer);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 1000) == 0 && mutex != nullptr);
    Require(pthread_mutex_isowned_np_nid_postfix(&mutex) == 1);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 1000) == POSIX_ETIMEDOUT);
    OtherThread other{&mutex};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, CheckFromOtherThread, &other, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(other.isowned == 0 && other.reltimedlock == POSIX_ETIMEDOUT);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_isowned_np_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 0) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(mutex == DestroyedMutex);
    Require(pthread_mutex_isowned_np_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 1000) == POSIX_EINVAL);
    Require(Throws([] { pthread_mutex_isowned_np_nid_postfix(nullptr); }));
}

static void CheckLoops(PthreadMutexattr* attr) {
    PthreadMutex mutex = nullptr;
    int count = -1;
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &count) == 0 && count == 0 && mutex != nullptr);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &count) == 0 && count == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);

    PthreadMutex adaptive = AdaptiveInitializer;
    Require(scePthreadMutexGetspinloops(&adaptive, &count) == SCE_OK && count == 2000 && adaptive != AdaptiveInitializer);
    Require(scePthreadMutexGetyieldloops(&adaptive, &count) == SCE_OK && count == 0);
    Require(scePthreadMutexDestroy(&adaptive) == SCE_OK);

    Require(scePthreadMutexattrSettype(attr, 4) == SCE_OK);
    Require(scePthreadMutexInit(&mutex, attr, "adaptive") == SCE_OK);
    Require(scePthreadMutexGetspinloops(&mutex, &count) == SCE_OK && count == 2000);
    Require(scePthreadMutexSetspinloops(&mutex, 7) == SCE_OK);
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &count) == 0 && count == 7);
    Require(pthread_mutex_setspinloops_np_nid_postfix(&mutex, -3) == 0);
    Require(scePthreadMutexGetspinloops(&mutex, &count) == SCE_OK && count == -3);
    Require(scePthreadMutexSetyieldloops(&mutex, 9) == SCE_OK);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &count) == 0 && count == 9);
    Require(pthread_mutex_setyieldloops_np_nid_postfix(&mutex, 11) == 0);
    Require(scePthreadMutexGetyieldloops(&mutex, &count) == SCE_OK && count == 11);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == POSIX_EDEADLK);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(Throws([&] { scePthreadMutexGetspinloops(&mutex, nullptr); }));
    Require(Throws([&] { scePthreadMutexGetyieldloops(&mutex, nullptr); }));
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);

    count = -1;
    Require(scePthreadMutexGetspinloops(&mutex, &count) == SCE_KERNEL_ERROR_EINVAL && count == -1);
    Require(scePthreadMutexSetspinloops(&mutex, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexGetyieldloops(&mutex, &count) == SCE_KERNEL_ERROR_EINVAL && count == -1);
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &count) == POSIX_EINVAL);
    Require(pthread_mutex_setspinloops_np_nid_postfix(&mutex, 1) == POSIX_EINVAL);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &count) == POSIX_EINVAL && count == -1);
    Require(scePthreadMutexSetyieldloops(&mutex, 1) == SCE_OK);
    Require(pthread_mutex_setyieldloops_np_nid_postfix(&mutex, 1) == 0);
    Require(mutex == DestroyedMutex);

    for (const int type : {1, 2, 3}) {
        Require(scePthreadMutexattrSettype(attr, type) == SCE_OK);
        Require(scePthreadMutexInit(&mutex, attr, nullptr) == SCE_OK);
        Require(scePthreadMutexGetspinloops(&mutex, &count) == SCE_OK && count == 0);
        Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    }
}

int main() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    for (int type : {-1, 0, 5}) Require(scePthreadMutexattrSettype(&attr, type) == SCE_KERNEL_ERROR_EINVAL);
    for (int type : {1, 2, 3, 4}) Require(scePthreadMutexattrSettype(&attr, type) == SCE_OK);
    for (int protocol : {-1, 3}) Require(scePthreadMutexattrSetprotocol(&attr, protocol) == SCE_KERNEL_ERROR_EINVAL);
    for (int protocol : {0, 1, 2}) Require(scePthreadMutexattrSetprotocol(&attr, protocol) == SCE_OK);

    PthreadMutexattr unset = nullptr;
    Require(scePthreadMutexattrSettype(&unset, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&unset, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(&unset) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSettype(nullptr, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);

    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &unset, "unset") == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    Require(mutex == reinterpret_cast<PthreadMutex>(std::uintptr_t{2}));
    Require(scePthreadMutexDestroy(&mutex) == SCE_KERNEL_ERROR_EINVAL);

    Require(scePthreadMutexInit(&mutex, &attr, "attr") == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    CheckLoops(&attr);
    CheckOwnershipAndRelativeLock();
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
}
