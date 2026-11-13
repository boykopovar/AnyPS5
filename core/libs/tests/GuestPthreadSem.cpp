#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

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
    void* const bad[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 3, nullptr) == SCE_OK);
    int value = -1;
    Require(scePthreadSemGetvalue(&sem, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    for (void* pointer : bad) {
        Require(scePthreadSemInit(static_cast<PthreadSem*>(pointer), 0, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadSemGetvalue(static_cast<PthreadSem*>(pointer), &value) == SCE_KERNEL_ERROR_EFAULT && value == -1);
        Require(scePthreadSemGetvalue(&sem, static_cast<int*>(pointer)) == SCE_KERNEL_ERROR_EFAULT);
    }
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 3);
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}

struct CancellableWaitContext {
    PthreadSem* sem = nullptr;
    std::atomic<bool> waiting{false};
};

static void* APS5_VABI CancellableWaiter(void* arg) {
    auto& context = *static_cast<CancellableWaitContext*>(arg);
    context.waiting.store(true, std::memory_order_release);
    (void)scePthreadSemWait(context.sem);
    return nullptr;
}

int main() {
    CheckBadPointers();
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
    std::atomic<bool> busyWaiting{false};
    std::atomic<int> waitResult{-1};
    std::thread waiterThread([&] {
        busyWaiting.store(true, std::memory_order_release);
        waitResult = scePthreadSemWait(&busySem);
    });
    while (!busyWaiting.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(scePthreadSemDestroy(&busySem) == SCE_OK);
    waiterThread.join();
    Require(waitResult == SCE_KERNEL_ERROR_EINVAL);

    PthreadSem timedSem = nullptr;
    Require(scePthreadSemInit(&timedSem, 0, 0, nullptr) == SCE_OK);
    std::atomic<bool> timedWaiting{false};
    std::atomic<int> timedResult{-1};
    std::thread timedThread([&] {
        timedWaiting.store(true, std::memory_order_release);
        timedResult = scePthreadSemTimedwait(&timedSem, 2000000);
    });
    while (!timedWaiting.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(scePthreadSemDestroy(&timedSem) == SCE_OK);
    timedThread.join();
    Require(timedResult == SCE_KERNEL_ERROR_ETIMEDOUT);

    PthreadSem cancelSem = nullptr;
    Require(scePthreadSemInit(&cancelSem, 0, 0, nullptr) == SCE_OK);
    CancellableWaitContext cancelContext{&cancelSem};
    Pthread cancelThread = nullptr;
    Require(scePthreadCreate(&cancelThread, nullptr, CancellableWaiter, &cancelContext, nullptr) == SCE_OK);
    while (!cancelContext.waiting.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(scePthreadCancel(cancelThread) == SCE_OK);
    Require(scePthreadJoin(cancelThread, nullptr) == SCE_OK);
    Require(scePthreadSemDestroy(&cancelSem) == SCE_OK);

    for (int iter = 0; iter < 50; ++iter) {
        PthreadSem raceSem = nullptr;
        Require(scePthreadSemInit(&raceSem, 0, 0, nullptr) == SCE_OK);
        std::atomic<bool> raceWaiting{false};
        std::thread racer([&] {
            raceWaiting.store(true, std::memory_order_release);
            Require(scePthreadSemWait(&raceSem) == SCE_OK);
            Require(scePthreadSemDestroy(&raceSem) == SCE_OK);
        });
        while (!raceWaiting.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        Require(scePthreadSemPost(&raceSem) == SCE_OK);
        racer.join();
    }

    for (int iter = 0; iter < 50; ++iter) {
        PthreadSem queuedSem = nullptr;
        Require(scePthreadSemInit(&queuedSem, 0, 0, nullptr) == SCE_OK);
        std::atomic<bool> postStarted{false};
        std::atomic<int> postResult{-1};
        std::thread concurrentPost([&] {
            postStarted.store(true, std::memory_order_release);
            try {
                postResult = scePthreadSemPost(&queuedSem);
            } catch (const std::exception&) {
                postResult = -2;
            }
        });
        while (!postStarted.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        Require(scePthreadSemDestroy(&queuedSem) == SCE_OK);
        concurrentPost.join();
        Require(postResult == SCE_OK || postResult == SCE_KERNEL_ERROR_EINVAL || postResult == -2);
    }
}
