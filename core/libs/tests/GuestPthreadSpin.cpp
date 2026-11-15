#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using PthreadSpinlock = PthreadSpinlockPrivate*;

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GuestPthread_Process_Private = 0;
static constexpr int GuestPthread_Process_Shared = 1;
static constexpr int Increments = 200000;

static void Require(bool value) { if (!value) std::abort(); }

class FaultPages {
public:
    FaultPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, 2 * size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + size, size, PROT_NONE) == 0);
#endif
    }

    ~FaultPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * size);
#endif
    }

    FaultPages(const FaultPages&) = delete;
    FaultPages& operator=(const FaultPages&) = delete;

    void* ReadOnly() const { return memory; }
    void* Unreadable() const { return memory + size; }

    void ProtectReadOnly(void* address, std::size_t bytes) const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(address, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(address, bytes, PROT_READ) == 0);
#endif
    }

    void ProtectReadWrite(void* address, std::size_t bytes) const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(address, bytes, PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(address, bytes, PROT_READ | PROT_WRITE) == 0);
#endif
    }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

struct Shared {
    PthreadSpinlock lock = nullptr;
    long counter = 0;
    int trylock = -1;
    int unlock = -1;
};

static void* APS5_VABI Contender(void* arg) {
    auto& shared = *static_cast<Shared*>(arg);
    shared.trylock = pthread_spin_trylock_nid_postfix(&shared.lock);
    shared.unlock = pthread_spin_unlock_nid_postfix(&shared.lock);
    return nullptr;
}

static void* APS5_VABI Incrementer(void* arg) {
    auto& shared = *static_cast<Shared*>(arg);
    for (int i = 0; i < Increments; ++i) {
        Require(pthread_spin_lock_nid_postfix(&shared.lock) == 0);
        ++shared.counter;
        Require(pthread_spin_unlock_nid_postfix(&shared.lock) == 0);
    }
    return nullptr;
}

static void ArgumentErrors() {
    Require(pthread_spin_init_nid_postfix(nullptr, GuestPthread_Process_Private) == GUEST_EINVAL);
    PthreadSpinlock lock = nullptr;
    Require(pthread_spin_init_nid_postfix(&lock, GuestPthread_Process_Shared) == GUEST_EINVAL);
    Require(pthread_spin_init_nid_postfix(&lock, 2) == GUEST_EINVAL);
    Require(pthread_spin_init_nid_postfix(&lock, -1) == GUEST_EINVAL);
    Require(lock == nullptr);
    Require(pthread_spin_lock_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_trylock_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_unlock_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_lock_nid_postfix(&lock) == GUEST_EINVAL);
    Require(pthread_spin_trylock_nid_postfix(&lock) == GUEST_EINVAL);
    Require(pthread_spin_unlock_nid_postfix(&lock) == GUEST_EINVAL);
    Require(pthread_spin_destroy_nid_postfix(&lock) == GUEST_EINVAL);
}

static void SingleThread(int pshared) {
    PthreadSpinlock lock = nullptr;
    Require(pthread_spin_init_nid_postfix(&lock, pshared) == 0);
    Require(lock != nullptr);
    Require(pthread_spin_unlock_nid_postfix(&lock) == GUEST_EPERM);
    Require(pthread_spin_lock_nid_postfix(&lock) == 0);
    Require(pthread_spin_trylock_nid_postfix(&lock) == GUEST_EBUSY);
    bool threw = false;
    try {
        pthread_spin_lock_nid_postfix(&lock);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == GUEST_EPERM);
    Require(pthread_spin_trylock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_destroy_nid_postfix(&lock) == 0);
    Require(lock == nullptr);
    Require(pthread_spin_destroy_nid_postfix(&lock) == GUEST_EINVAL);
}

static void HeldByAnotherThread() {
    Shared shared;
    Require(pthread_spin_init_nid_postfix(&shared.lock, GuestPthread_Process_Private) == 0);
    Require(pthread_spin_lock_nid_postfix(&shared.lock) == 0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Contender, &shared, nullptr) == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(shared.trylock == GUEST_EBUSY);
    Require(shared.unlock == GUEST_EPERM);
    Require(pthread_spin_unlock_nid_postfix(&shared.lock) == 0);
    Require(pthread_spin_destroy_nid_postfix(&shared.lock) == 0);
}

static void MutualExclusion() {
    Shared shared;
    Require(pthread_spin_init_nid_postfix(&shared.lock, GuestPthread_Process_Private) == 0);
    Pthread first = nullptr;
    Pthread second = nullptr;
    Require(scePthreadCreate(&first, nullptr, Incrementer, &shared, nullptr) == 0);
    Require(scePthreadCreate(&second, nullptr, Incrementer, &shared, nullptr) == 0);
    Require(scePthreadJoin(first, nullptr) == 0);
    Require(scePthreadJoin(second, nullptr) == 0);
    Require(shared.counter == 2L * Increments);
    Require(pthread_spin_destroy_nid_postfix(&shared.lock) == 0);
}

static void FaultErrors() {
    FaultPages pages;
    for (void* pointer : {reinterpret_cast<void*>(std::uintptr_t{0x10}), pages.Unreadable()}) {
        auto* const bad = static_cast<PthreadSpinlock*>(pointer);
        errno = 71;
        Require(pthread_spin_init_nid_postfix(bad, GuestPthread_Process_Private) == GUEST_EFAULT && errno == 71);
        Require(pthread_spin_destroy_nid_postfix(bad) == GUEST_EFAULT && errno == 71);
        Require(pthread_spin_trylock_nid_postfix(bad) == GUEST_EFAULT && errno == 71);
        Require(pthread_spin_lock_nid_postfix(bad) == GUEST_EFAULT && errno == 71);
        Require(pthread_spin_unlock_nid_postfix(bad) == GUEST_EFAULT && errno == 71);
    }
    auto* const readOnly = static_cast<PthreadSpinlock*>(pages.ReadOnly());
    errno = 71;
    Require(pthread_spin_init_nid_postfix(readOnly, GuestPthread_Process_Private) == GUEST_EFAULT && errno == 71);
    Require(pthread_spin_destroy_nid_postfix(readOnly) == GUEST_EINVAL && errno == 71);

    auto* const slot = static_cast<PthreadSpinlock*>(pages.ReadOnly());
    pages.ProtectReadWrite(slot, sizeof(*slot));
    Require(pthread_spin_init_nid_postfix(slot, GuestPthread_Process_Private) == 0 && *slot != nullptr);
    pages.ProtectReadOnly(slot, sizeof(*slot));
    errno = 71;
    Require(pthread_spin_destroy_nid_postfix(slot) == GUEST_EFAULT && errno == 71);
    Require(pthread_spin_trylock_nid_postfix(slot) == 0);
    Require(pthread_spin_unlock_nid_postfix(slot) == 0);
    pages.ProtectReadWrite(slot, sizeof(*slot));
    Require(pthread_spin_destroy_nid_postfix(slot) == 0 && *slot == nullptr);
    pages.ProtectReadOnly(slot, sizeof(*slot));
}

int main() {
    ArgumentErrors();
    FaultErrors();
    SingleThread(GuestPthread_Process_Private);
    HeldByAnotherThread();
    MutualExclusion();
}
