#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int GuestEagain = 35;
static constexpr int GuestEfault = 14;
static constexpr int GuestEnametoolong = 63;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "pthread setname check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestMutex() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, "first") == 0 && mutex->_name == "first");
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, "second") == GuestEagain && mutex->_name == "first");
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, "held") == GuestEagain && mutex->_name == "first");
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    RequireThrows([] { pthread_mutex_setname_np_nid_postfix(nullptr, "mutex"); });
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    RequireThrows([&] { pthread_mutex_setname_np_nid_postfix(&mutex, "destroyed"); });
}

static void TestRwlock() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, "first") == 0 && rwlock->_name == "first");
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, "read") == GuestEagain && rwlock->_name == "first");
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    RequireThrows([] { pthread_rwlock_setname_np_nid_postfix(nullptr, "rwlock"); });
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    RequireThrows([&] { pthread_rwlock_setname_np_nid_postfix(&rwlock, "destroyed"); });
    PthreadRwlock unset = nullptr;
    RequireThrows([&] { pthread_rwlock_setname_np_nid_postfix(&unset, "static"); });
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

    char* Name(const char* text) const {
        char* name = memory + page - std::strlen(text) - 1;
        std::memcpy(name, text, std::strlen(text) + 1);
        return name;
    }

    char* Unterminated(std::size_t length) const {
        char* name = memory + page - length;
        std::memset(name, 'a', length);
        return name;
    }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void TestMutexBadPointers() {
    GuardPage guard;
    void* const badMutex[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : badMutex) Require(pthread_mutex_setname_np_nid_postfix(static_cast<PthreadMutex*>(pointer), "name") == GuestEfault);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    const char* const badName[] = {reinterpret_cast<const char*>(0x10), static_cast<const char*>(guard.Unreadable()), guard.Unterminated(8)};
    for (const char* name : badName) Require(pthread_mutex_setname_np_nid_postfix(&mutex, name) == GuestEfault && !mutex->_named);
    const std::string longName(1024, 'a');
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, longName.c_str()) == GuestEnametoolong && !mutex->_named);
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, nullptr) == 0 && !mutex->_named);
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, guard.Name("edge")) == 0 && mutex->_name == "edge");
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

static void TestRwlockBadPointers() {
    GuardPage guard;
    void* const badRwlock[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : badRwlock) Require(pthread_rwlock_setname_np_nid_postfix(static_cast<PthreadRwlock*>(pointer), "name") == GuestEfault);
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    const char* const badName[] = {reinterpret_cast<const char*>(0x10), static_cast<const char*>(guard.Unreadable()), guard.Unterminated(8)};
    for (const char* name : badName) Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, name) == GuestEfault && !rwlock->_named);
    const std::string longName(1024, 'a');
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, longName.c_str()) == GuestEnametoolong && !rwlock->_named);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, guard.Name("edge")) == 0 && rwlock->_name == "edge");
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

static void TestBarrierBadPointers() {
    GuardPage guard;
    void* const badBarrier[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : badBarrier) Require(pthread_barrier_setname_np_nid_postfix(static_cast<PthreadBarrierPrivate**>(pointer), "name") == GuestEfault);
    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    const char* const badName[] = {reinterpret_cast<const char*>(0x10), static_cast<const char*>(guard.Unreadable()), guard.Unterminated(8)};
    for (const char* name : badName) Require(pthread_barrier_setname_np_nid_postfix(&barrier, name) == GuestEfault && !barrier->named);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, nullptr) == 0 && !barrier->named);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, guard.Name("edge")) == 0 && barrier->name == "edge");
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
}

static void TestBarrier() {
    PthreadBarrierPrivate* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "first") == 0 && barrier->name == "first");
    Require(pthread_barrier_wait_nid_postfix(&barrier) == -1);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "second") == GuestEagain && barrier->name == "first");
    RequireThrows([] { pthread_barrier_setname_np_nid_postfix(nullptr, "barrier"); });
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    RequireThrows([&] { pthread_barrier_setname_np_nid_postfix(&barrier, "destroyed"); });
    PthreadBarrierPrivate* unset = nullptr;
    RequireThrows([&] { pthread_barrier_setname_np_nid_postfix(&unset, "unset"); });
}

int main() {
    TestMutex();
    TestRwlock();
    TestBarrier();
    TestBarrierBadPointers();
    TestMutexBadPointers();
    TestRwlockBadPointers();
    std::puts("pthread setname tests passed");
}
