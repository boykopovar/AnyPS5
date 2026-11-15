#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int SceEinval = static_cast<int>(0x80020016u);
static constexpr int SceEfault = static_cast<int>(0x8002000Eu);
static constexpr int GuestEinval = 22;
static constexpr int GuestEfault = 14;
static constexpr KernelClockid Monotonic = 4;

static void Require(bool value) { if (!value) std::abort(); }

static void CheckClock(const PthreadCondattr* attr, KernelClockid expected) {
    const auto handle = *attr;
    KernelClockid output[] = {0x12345678, -1, 0x23456789};
    *__error_nid_postfix() = 123;
    Require(pthread_condattr_getclock_nid_postfix(attr, &output[1]) == 0);
    Require(output[0] == 0x12345678 && output[1] == expected && output[2] == 0x23456789);
    Require(*attr == handle && *__error_nid_postfix() == 123);
}

static void CheckClockRefused(const PthreadCondattr* attr, KernelClockid* clockId) {
    Require(pthread_condattr_getclock_nid_postfix(attr, clockId) == GuestEfault);
}

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
        auto* attrs = static_cast<PthreadCondattr*>(pointer);
        auto* clockOut = static_cast<KernelClockid*>(pointer);
        auto* sharedOut = static_cast<int*>(pointer);
        const int attrError = null ? GuestEinval : GuestEfault;
        const int sceAttrError = null ? SceEinval : SceEfault;
        KernelClockid clockId = -1;
        int sharing = -1;
        Require(pthread_condattr_destroy_nid_postfix(attrs) == attrError);
        Require(pthread_condattr_setclock_nid_postfix(attrs, Monotonic) == attrError);
        Require(pthread_condattr_getclock_nid_postfix(attrs, &clockId) == attrError && clockId == -1);
        Require(pthread_condattr_setpshared_nid_postfix(attrs, 0) == attrError);
        Require(pthread_condattr_getpshared_nid_postfix(attrs, &sharing) == attrError && sharing == -1);
        Require(scePthreadCondattrDestroy(attrs) == sceAttrError);
        Require(scePthreadCondattrSetclock(attrs, Monotonic) == sceAttrError);
        Require(scePthreadCondattrSetpshared(attrs, 0) == sceAttrError);
        Require(scePthreadCondattrGetpshared(attrs, &sharing) == sceAttrError && sharing == -1);
        Require(scePthreadCondattrInit(attrs) == SceEfault);
        Require(pthread_condattr_init_nid_postfix(attrs) == (null ? GuestEinval : GuestEfault));

        PthreadCondattr attr = nullptr;
        Require(pthread_condattr_init_nid_postfix(&attr) == 0);
        Require(pthread_condattr_getclock_nid_postfix(&attr, clockOut) == GuestEfault);
        Require(pthread_condattr_getpshared_nid_postfix(&attr, sharedOut) == GuestEfault);
        Require(scePthreadCondattrGetpshared(&attr, sharedOut) == SceEfault);
        CheckClock(&attr, 0);
        Require(pthread_condattr_destroy_nid_postfix(&attr) == 0 && attr == nullptr);
    }
}

int main() {
    CheckBadPointers();
    PthreadCondattr unset = nullptr;
    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(nullptr, &clockId) == GuestEinval && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(&unset, &clockId) == GuestEinval && clockId == -1);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, nullptr) == GuestEinval);
    Require(pthread_condattr_getclock_nid_postfix(&unset, nullptr) == GuestEinval);
    int sharing = -1;
    Require(scePthreadCondattrSetclock(nullptr, Monotonic) == SceEinval);
    Require(scePthreadCondattrSetclock(&unset, Monotonic) == SceEinval);
    Require(scePthreadCondattrDestroy(nullptr) == SceEinval);
    Require(scePthreadCondattrDestroy(&unset) == SceEinval);
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == GuestEinval);
    Require(pthread_condattr_destroy_nid_postfix(&unset) == GuestEinval);
    Require(pthread_condattr_setclock_nid_postfix(&unset, Monotonic) == GuestEinval);
    Require(pthread_condattr_setpshared_nid_postfix(nullptr, 0) == GuestEinval);
    Require(pthread_condattr_setpshared_nid_postfix(&unset, 0) == GuestEinval);
    Require(pthread_condattr_getpshared_nid_postfix(nullptr, &sharing) == GuestEinval && sharing == -1);
    Require(pthread_condattr_getpshared_nid_postfix(&unset, &sharing) == GuestEinval && sharing == -1);
    Require(scePthreadCondattrSetpshared(&unset, 0) == SceEinval);
    Require(scePthreadCondattrGetpshared(nullptr, &sharing) == SceEinval && sharing == -1);

    PthreadCondattr attr = nullptr;
    Require(scePthreadCondattrInit(&attr) == SCE_OK);
    CheckClock(&attr, 0);
    CheckClockRefused(&attr, nullptr);
    CheckClock(&attr, 0);
    PthreadCondattr other = nullptr;
    Require(pthread_condattr_init_nid_postfix(&other) == 0);
    for (const KernelClockid clock : {0, 1, 2, Monotonic}) {
        Require(scePthreadCondattrSetclock(&attr, clock) == SCE_OK);
        CheckClock(&attr, clock);
        CheckClock(&other, 0);
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == 0);
        CheckClock(&attr, clock);
    }
    Require(pthread_condattr_setclock_nid_postfix(&other, Monotonic) == 0);
    CheckClock(&other, Monotonic);
    CheckClock(&attr, Monotonic);
    Require(pthread_condattr_destroy_nid_postfix(&other) == 0 && other == nullptr);
    Require(pthread_condattr_destroy_nid_postfix(&other) == GuestEinval);
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == GuestEinval);
    Require(pthread_condattr_getclock_nid_postfix(&other, &clockId) == GuestEinval && clockId == -1);
    Require(scePthreadCondattrSetpshared(&attr, 1) == SceEinval);
    Require(scePthreadCondattrSetpshared(&attr, 0) == SCE_OK);
    Require(scePthreadCondattrGetpshared(&attr, &sharing) == SCE_OK && sharing == 0);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    for (const KernelClockid clock : {-1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}) {
        Require(scePthreadCondattrSetclock(&attr, clock) == SceEinval);
        CheckClock(&attr, Monotonic);
        Require(pthread_condattr_setclock_nid_postfix(&attr, clock) == GuestEinval);
        CheckClock(&attr, Monotonic);
    }
    for (const int invalid : {1, 2, -1, 100, static_cast<int>(0x7fffffff)}) {
        Require(scePthreadCondattrSetpshared(&attr, invalid) == SceEinval);
        Require(pthread_condattr_setpshared_nid_postfix(&attr, invalid) == GuestEinval);
        sharing = -1;
        Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    }
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == 0);
    sharing = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, nullptr) == GuestEfault);
    Require(scePthreadCondattrDestroy(&attr) == SCE_OK && attr == nullptr);
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == GuestEinval && clockId == -1);
    Require(scePthreadCondattrDestroy(&attr) == SceEinval);
    Require(scePthreadCondattrDestroy(nullptr) == SceEinval);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == GuestEinval);
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == GuestEinval);
    sharing = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &sharing) == GuestEinval && sharing == -1);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == GuestEinval);
}
