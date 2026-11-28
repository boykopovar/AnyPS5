#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> calls{0};

static void APS5_VABI Prepare() { ++calls; }
static void APS5_VABI Parent() { ++calls; }
static void APS5_VABI Child() { ++calls; }

static bool Same(const PthreadAtforkHandlers& entry, PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child) {
    return entry.prepare == prepare && entry.parent == parent && entry.child == child;
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
    void* Writable() const { return memory; }

    void MakeReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(memory, page, PROT_READ) == 0);
#endif
    }

    void MakeReadWrite() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, page, PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory, page, PROT_READ | PROT_WRITE) == 0);
#endif
    }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void CheckPthreadSemUnreadableEfault() {
    GuardPage guard;
    void* const badPointers[] = {reinterpret_cast<void*>(std::uintptr_t{0x10}), guard.Unreadable()};
    for (void* bad : badPointers) {
        auto* const badSem = static_cast<PthreadSem*>(bad);
        errno = 42;
        Require(scePthreadSemPost(badSem) == SCE_KERNEL_ERROR_EFAULT);
        Require(errno == 42);
        errno = 42;
        Require(scePthreadSemTrywait(badSem) == SCE_KERNEL_ERROR_EFAULT);
        Require(errno == 42);
        errno = 42;
        Require(scePthreadSemWait(badSem) == SCE_KERNEL_ERROR_EFAULT);
        Require(errno == 42);
        errno = 42;
        Require(scePthreadSemTimedwait(badSem, 1000) == SCE_KERNEL_ERROR_EFAULT);
        Require(errno == 42);
        errno = 42;
        Require(scePthreadSemInit(badSem, 0, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(errno == 42);
        int value = -1;
        errno = 42;
        Require(scePthreadSemGetvalue(badSem, &value) == SCE_KERNEL_ERROR_EFAULT);
        Require(value == -1);
        Require(errno == 42);
    }

    guard.MakeReadOnly();
    errno = 42;
    Require(scePthreadSemInit(static_cast<PthreadSem*>(guard.Writable()), 0, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 42);
    guard.MakeReadWrite();

    PthreadSem sem = nullptr;
    errno = 42;
    Require(scePthreadSemInit(&sem, 0, 1, nullptr) == 0);
    Require(errno == 42);

    int value = -1;
    errno = 42;
    Require(scePthreadSemGetvalue(&sem, &value) == 0 && value == 1);
    Require(errno == 42);

    errno = 42;
    Require(scePthreadSemGetvalue(&sem, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 42);

    for (void* bad : badPointers) {
        errno = 42;
        Require(scePthreadSemGetvalue(&sem, static_cast<int*>(bad)) == SCE_KERNEL_ERROR_EFAULT);
        Require(errno == 42);
    }

    Require(scePthreadSemPost(&sem) == 0);
    Require(scePthreadSemGetvalue(&sem, &value) == 0 && value == 2);
    Require(scePthreadSemTrywait(&sem) == 0);
    Require(scePthreadSemWait(&sem) == 0);
    Require(scePthreadSemTimedwait(&sem, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadSemDestroy(&sem) == 0);
}

int main() {
    Require(PthreadAtforkHandlers_nid_no_patch().empty());
    Require(pthread_atfork_nid_postfix(Prepare, Parent, Child) == 0);
    Require(pthread_atfork_nid_postfix(nullptr, Parent, nullptr) == 0);
    Require(pthread_atfork_nid_postfix(nullptr, nullptr, nullptr) == 0);
    Require(pthread_atfork_nid_postfix(Prepare, Parent, Child) == 0);
    auto handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == 4);
    Require(Same(handlers[0], Prepare, Parent, Child));
    Require(Same(handlers[1], nullptr, Parent, nullptr));
    Require(Same(handlers[2], nullptr, nullptr, nullptr));
    Require(Same(handlers[3], Prepare, Parent, Child));

    constexpr std::size_t threadCount = 4;
    constexpr std::size_t perThread = 64;
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < threadCount; ++index) {
        threads.emplace_back([] {
            for (std::size_t count = 0; count < perThread; ++count) Require(pthread_atfork_nid_postfix(nullptr, nullptr, Child) == 0);
        });
    }
    for (auto& thread : threads) thread.join();
    handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == 4 + threadCount * perThread);
    Require(Same(handlers[3], Prepare, Parent, Child));
    for (std::size_t index = 4; index < handlers.size(); ++index) Require(Same(handlers[index], nullptr, nullptr, Child));
    Require(calls == 0);

    CheckPthreadSemUnreadableEfault();
    return 0;
}
