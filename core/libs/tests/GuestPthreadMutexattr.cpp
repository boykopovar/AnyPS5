#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int GUEST_EINVAL = 22;
static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int PRIO_NONE = 0;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr int GuestSched_Rr = 3;

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int GUEST_EFAULT = 14;
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000Eu);

class GuardPage {
public:
    GuardPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * page);
#endif
    }

    GuardPage(const GuardPage&) = delete;
    GuardPage& operator=(const GuardPage&) = delete;

    void* Unreadable() const { return memory + page; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void CheckBadPointers() {
    GuardPage guard;
    void* const bad[] = {nullptr, reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : bad) {
        const bool null = pointer == nullptr;
        auto* attrs = static_cast<PthreadMutexattr*>(pointer);
        auto* out = static_cast<int*>(pointer);
        const int attrError = null ? GUEST_EINVAL : GUEST_EFAULT;
        const int sceAttrError = null ? SCE_KERNEL_ERROR_EINVAL : SCE_KERNEL_ERROR_EFAULT;
        *__error_nid_postfix() = 0;
        Require(pthread_mutexattr_destroy_nid_postfix(attrs) == attrError);
        Require(pthread_mutexattr_settype_nid_postfix(attrs, 2) == attrError);
        Require(pthread_mutexattr_setpshared_nid_postfix(attrs, 0) == attrError);
        Require(pthread_mutexattr_setprotocol_nid_postfix(attrs, PRIO_NONE) == attrError);
        Require(pthread_mutexattr_setprioceiling_nid_postfix(attrs, 300) == attrError);
        int value = -1;
        Require(pthread_mutexattr_gettype_nid_postfix(attrs, &value) == attrError);
        Require(pthread_mutexattr_getpshared_nid_postfix(attrs, &value) == attrError);
        Require(pthread_mutexattr_getprotocol_nid_postfix(attrs, &value) == attrError);
        Require(pthread_mutexattr_getprioceiling_nid_postfix(attrs, &value) == attrError);
        Require(value == -1);
        Require(pthread_mutexattr_setkind_np_nid_postfix(attrs, 2) == -1 && *__error_nid_postfix() == attrError);
        Require(scePthreadMutexattrDestroy(attrs) == sceAttrError);
        Require(scePthreadMutexattrSettype(attrs, 2) == sceAttrError);
        Require(scePthreadMutexattrGettype(attrs, &value) == sceAttrError);
        Require(scePthreadMutexattrSetprotocol(attrs, PRIO_NONE) == sceAttrError);
        Require(scePthreadMutexattrGetprotocol(attrs, &value) == sceAttrError);
        Require(scePthreadMutexattrSetprioceiling(attrs, 300) == sceAttrError);
        Require(scePthreadMutexattrGetprioceiling(attrs, &value) == sceAttrError);
        Require(scePthreadMutexattrSetpshared(attrs, 0) == sceAttrError);
        Require(scePthreadMutexattrGetpshared(attrs, &value) == sceAttrError);
        Require(value == -1);
        Require(pthread_mutexattr_init_nid_postfix(attrs) == GUEST_EFAULT);
        Require(scePthreadMutexattrInit(attrs) == SCE_KERNEL_ERROR_EFAULT);

        PthreadMutexattr attr = nullptr;
        Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
        Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, out) == GUEST_EFAULT);
        Require(pthread_mutexattr_getpshared_nid_postfix(&attr, out) == GUEST_EFAULT);
        Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, out) == GUEST_EFAULT);
        Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, out) == GUEST_EFAULT);
        Require(scePthreadMutexattrGettype(&attr, out) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadMutexattrGetpshared(&attr, out) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadMutexattrGetprotocol(&attr, out) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadMutexattrGetprioceiling(&attr, out) == SCE_KERNEL_ERROR_EFAULT);
        Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    }
}

static constexpr KernelTimespec INVALID_DEADLINE{0, 1000000000};

static void* APS5_VABI TimedlockInvalidDeadline(void* arg) {
    auto* mutex = static_cast<PthreadMutex*>(arg);
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(pthread_mutex_timedlock_nid_postfix(mutex, &INVALID_DEADLINE)));
}

static void CheckTimedlockInvalidDeadline() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &INVALID_DEADLINE) == 0);
    Pthread thread{};
    void* result = nullptr;
    Require(scePthreadCreate(&thread, nullptr, TimedlockInvalidDeadline, &mutex, nullptr) == 0);
    Require(scePthreadJoin(thread, &result) == 0 && reinterpret_cast<std::intptr_t>(result) == GUEST_EINVAL);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

static void CheckType() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    int type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 1);
    for (const int set : {2, 3, 4, 1}) {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, set) == 0);
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == set);
    }
    Require(pthread_mutexattr_settype_nid_postfix(nullptr, 1) == GUEST_EINVAL);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 5) == GUEST_EINVAL);
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 1);
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, nullptr) == GUEST_EFAULT);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == GUEST_EINVAL && type == -1);
    Require(pthread_mutexattr_gettype_nid_postfix(nullptr, &type) == GUEST_EINVAL && type == -1);
}

static void CheckKind() {
    PthreadMutexattr attr = nullptr;
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_getkind_np_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_setkind_np_nid_postfix(nullptr, 2) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 2) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 1);
    int type = -1;
    for (const int kind : {2, 3, 4, 1}) {
        Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, kind) == 0);
        Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == kind);
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == kind);
    }
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 2) == 0);
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 2);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 0) == 0);
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 0);
    for (const int invalid : {0, 5, -1}) {
        Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, invalid) == 0);
        Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == invalid);
        type = -1;
        if (invalid == 5) Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == GUEST_EINVAL && type == -1);
        if (invalid != 5) Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == invalid);
        Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == GUEST_EINVAL && mutex == nullptr);
    }
    for (const int negative : {-2, -32768}) {
        Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, negative) == 0);
        type = 0;
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == negative);
    }
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 0x8000) == 0);
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == -32768);
    for (const int large : {32767, 0x12345}) {
        Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, large) == 0);
        type = -1;
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == GUEST_EINVAL && type == -1);
    }
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 0x2345);
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 0x10001) == 0);
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 1);
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 2) == 0);
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 2) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
}

static void CheckSceAccessors() {
    PthreadMutexattr attr = nullptr;
    int value = -1;
    Require(scePthreadMutexattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGettype(nullptr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetpshared(nullptr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetpshared(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(value == -1);
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_OK && value == 1);
    Require(scePthreadMutexattrSettype(&attr, 3) == SCE_OK);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_OK && value == 3);
    Require(scePthreadMutexattrGettype(&attr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_OK && value == PRIO_NONE);
    for (const int invalid : {-1, 3})
        Require(scePthreadMutexattrSetprotocol(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_OK && value == PRIO_NONE);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_INHERIT) == SCE_OK);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_OK && value == PRIO_INHERIT);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_PROTECT) == SCE_OK);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_OK && value == sched_get_priority_max_nid_postfix(GuestSched_Rr));
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_OK);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_OK && value == 300);
    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_PROTECT);
    Require(scePthreadMutexattrGetpshared(&attr, &value) == SCE_OK && value == 0);
    Require(scePthreadMutexattrSetpshared(&attr, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetpshared(&attr, 0) == SCE_OK);
    Require(scePthreadMutexattrGetpshared(&attr, &value) == SCE_OK && value == 0);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
}

static void CheckProcessSharing() {
    PthreadMutexattr attr = nullptr;
    int sharing = -1;
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    Require(pthread_mutexattr_getpshared_nid_postfix(nullptr, &sharing) == GUEST_EINVAL && sharing == -1);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == GUEST_EINVAL && sharing == -1);
    Require(pthread_mutexattr_setpshared_nid_postfix(nullptr, 0) == GUEST_EINVAL);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == GUEST_EINVAL);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 2) == 0);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == 0);
    for (const int invalid : {1, -1, 2, 2147483647})
        Require(pthread_mutexattr_setpshared_nid_postfix(&attr, invalid) == GUEST_EINVAL);
    sharing = -1;
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    int type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 2);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, nullptr) == GUEST_EFAULT);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == 0);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    sharing = -1;
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == GUEST_EINVAL && sharing == -1);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == GUEST_EINVAL);
}

static void CheckInvalidPriorityAttribute(PthreadMutexattr* attr) {
    int ceiling = -1;
    int protocol = -1;
    Require(pthread_mutexattr_destroy_nid_postfix(attr) == GUEST_EINVAL);
    Require(scePthreadMutexattrDestroy(attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(attr, PRIO_PROTECT) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(attr, -1) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(attr, &protocol) == GUEST_EINVAL && protocol == -1);
    Require(pthread_mutexattr_getprotocol_nid_postfix(attr, nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(attr, &ceiling) == GUEST_EINVAL && ceiling == -1);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(attr, nullptr) == GUEST_EINVAL);
}

static void CheckDestroy() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    Require(scePthreadMutexattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    Require(scePthreadMutexattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(pthread_mutexattr_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == GUEST_EINVAL);
    Require(scePthreadMutexattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
}

int main() {
    CheckType();
    CheckKind();
    CheckSceAccessors();
    CheckProcessSharing();
    CheckTimedlockInvalidDeadline();
    CheckDestroy();
    CheckBadPointers();
    PthreadMutexattr attr = nullptr;
    CheckInvalidPriorityAttribute(nullptr);
    CheckInvalidPriorityAttribute(&attr);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    int ceiling = -1;
    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_NONE);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL && ceiling == -1);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_INHERIT) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_INHERIT);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_PROTECT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == sched_get_priority_max_nid_postfix(GuestSched_Rr));
    for (const int outside : {0, 255, 768, 1000}) Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, outside) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 256) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == 256);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == 300);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_PROTECT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == sched_get_priority_max_nid_postfix(GuestSched_Rr));
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_NONE) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_NONE);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, nullptr) == GUEST_EFAULT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    CheckInvalidPriorityAttribute(&attr);
    return 0;
}
