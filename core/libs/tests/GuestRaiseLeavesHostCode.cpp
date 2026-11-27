#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

static constexpr int SigUsr1 = 30;
static constexpr int Rounds = 10;
static constexpr int MutexTypeRecursive = 2;
static constexpr std::size_t RipOffset = 0xe0;
static constexpr std::size_t ConvertedBytes = 64u << 20u;
static void Require(bool value) { if (!value) std::abort(); }

static std::uintptr_t imageBegin = 0;
static std::uintptr_t imageEnd = 0;
static std::atomic<int> deliveries{0};
static std::atomic<int> outsideImage{0};
static std::atomic<bool> stop{false};
static std::atomic<int> blockedRound{-1};
static std::atomic<int> busyRound{-1};
static std::atomic<int> alertableRound{-1};
static std::atomic<bool> alertableSignaled{false};
static std::atomic<int> insideAlertableWait{0};
static std::atomic<DWORD> alertableThread{0};
static std::atomic<int> boundaryRound{-1};
static std::atomic<bool> atBoundary{false};
static std::atomic<bool> boundaryDone{false};
static std::atomic<int> beforeWait{0};
static HANDLE alertableHandle = nullptr;
static HANDLE release = nullptr;
static HANDLE alertableRelease = nullptr;
static std::atomic<int> stops{0};
static std::atomic<int> resumed{0};
static std::atomic<bool> resume{false};
static std::atomic<int> initializations{0};

static void APS5_VABI Handler(int, void* context) {
    std::uint64_t rip = 0;
    std::memcpy(&rip, static_cast<const unsigned char*>(context) + RipOffset, sizeof(rip));
    if (rip < imageBegin || rip >= imageEnd) outsideImage.fetch_add(1);
    if (GetCurrentThreadId() == alertableThread.load()) {
        if (atBoundary.load()) beforeWait.fetch_add(1);
        else if (!alertableSignaled.load()) insideAlertableWait.fetch_add(1);
    }
    deliveries.fetch_add(1);
}

static void* APS5_VABI Blocked(void*) {
    for (int round = 0; round < Rounds; ++round) {
        blockedRound.store(round);
        Require(WaitForSingleObject(release, INFINITE) == WAIT_OBJECT_0);
        while (blockedRound.load() == round) std::this_thread::yield();
    }
    return nullptr;
}

static void* APS5_VABI Busy(void*) {
    const std::string source(ConvertedBytes, 'a');
    std::vector<wchar_t> converted(ConvertedBytes);
    for (int round = 0; round < Rounds; ++round) {
        busyRound.store(round);
        Require(MultiByteToWideChar(CP_UTF8, 0, source.data(), static_cast<int>(source.size()), converted.data(), static_cast<int>(converted.size())) == static_cast<int>(source.size()));
        while (busyRound.load() == round) std::this_thread::yield();
    }
    return nullptr;
}

static void* APS5_VABI Alertable(void*) {
    Require(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &alertableHandle, 0, FALSE, DUPLICATE_SAME_ACCESS) != 0);
    alertableThread.store(GetCurrentThreadId());
    for (int round = 0; round < Rounds; ++round) {
        atBoundary.store(true);
        boundaryRound.store(round);
        while (!boundaryDone.load()) {
        }
        boundaryDone.store(false);
        atBoundary.store(false);
        alertableRound.store(round);
        DWORD waited = WAIT_IO_COMPLETION;
        while (waited == WAIT_IO_COMPLETION) waited = WaitForSingleObjectEx(alertableRelease, INFINITE, TRUE);
        Require(waited == WAIT_OBJECT_0);
        while (alertableRound.load() == round) std::this_thread::yield();
    }
    return nullptr;
}

static bool InsideNtdll(HANDLE thread) {
    const auto* ntdll = reinterpret_cast<const unsigned char*>(GetModuleHandleW(L"ntdll.dll"));
    const auto* headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(ntdll + reinterpret_cast<const IMAGE_DOS_HEADER*>(ntdll)->e_lfanew);
    const auto begin = reinterpret_cast<std::uintptr_t>(ntdll);
    const auto end = begin + headers->OptionalHeader.SizeOfImage;
    Require(SuspendThread(thread) != static_cast<DWORD>(-1));
    CONTEXT context{};
    context.ContextFlags = CONTEXT_CONTROL;
    const bool read = GetThreadContext(thread, &context) != 0;
    ResumeThread(thread);
    Require(read);
    return context.Rip >= begin && context.Rip < end;
}

static void WaitDeliveries(int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (deliveries.load() < expected) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

static void APS5_VABI Stop(int, void*) {
    stops.fetch_add(1);
    while (!resume.load()) std::this_thread::yield();
    resumed.fetch_add(1);
}

static void WaitCount(const std::atomic<int>& count, int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (count.load() < expected) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

struct Relock {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    int levels = 1;
    bool timed = false;
    bool signaled = false;
    std::atomic<bool> waiting{false};
    std::atomic<int> result{-1};
};

static void* APS5_VABI Relocker(void* opaque) {
    auto& relock = *static_cast<Relock*>(opaque);
    for (int level = 0; level < relock.levels; ++level) Require(scePthreadMutexLock(&relock.mutex) == 0);
    relock.waiting.store(true);
    int waited = 0;
    while (waited == 0 && !relock.signaled)
        waited = relock.timed ? scePthreadCondTimedwait(&relock.cond, &relock.mutex, 10000000) : scePthreadCondWait(&relock.cond, &relock.mutex);
    relock.result.store(waited);
    for (int level = 0; level < relock.levels; ++level) Require(scePthreadMutexUnlock(&relock.mutex) == 0);
    Require(scePthreadMutexUnlock(&relock.mutex) != 0);
    return nullptr;
}

static void RaiseInRelock(int levels, bool timed) {
    for (int round = 0; round < Rounds; ++round) {
        Relock relock;
        relock.levels = levels;
        relock.timed = timed;
        PthreadMutexattr attr = nullptr;
        Require(scePthreadMutexattrInit(&attr) == 0);
        if (levels > 1) Require(scePthreadMutexattrSettype(&attr, MutexTypeRecursive) == 0);
        Require(scePthreadMutexInit(&relock.mutex, &attr, nullptr) == 0);
        Require(scePthreadMutexattrDestroy(&attr) == 0);
        Require(scePthreadCondInit(&relock.cond, nullptr, nullptr) == 0);
        Pthread waiter = nullptr;
        Require(scePthreadCreate(&waiter, nullptr, Relocker, &relock, "relocking after a wake") == 0);
        while (!relock.waiting.load()) std::this_thread::yield();
        Require(scePthreadMutexLock(&relock.mutex) == 0);
        relock.signaled = true;
        Require(scePthreadCondSignal(&relock.cond) == 0);
        for (int raise = 0; raise < 2; ++raise) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            resume.store(false);
            const int before = stops.load();
            Require(sceKernelRaiseException(waiter, SigUsr1) == 0);
            WaitCount(stops, before + 1);
            Require(relock.result.load() == -1);
            resume.store(true);
            WaitCount(resumed, before + 1);
        }
        Require(scePthreadMutexUnlock(&relock.mutex) == 0);
        Require(scePthreadJoin(waiter, nullptr) == 0);
        Require(relock.result.load() == 0);
        Require(scePthreadCondDestroy(&relock.cond) == 0);
        Require(scePthreadMutexDestroy(&relock.mutex) == 0);
    }
}

static void APS5_VABI Initialize() {
    initializations.fetch_add(1);
    while (!resume.load()) std::this_thread::yield();
}

struct OnceCall {
    GuestOnce* control = nullptr;
    std::atomic<bool> calling{false};
    std::atomic<int> result{-1};
};

static void* APS5_VABI CallOnce(void* opaque) {
    auto& call = *static_cast<OnceCall*>(opaque);
    call.calling.store(true);
    call.result.store(pthread_once_nid_postfix(call.control, Initialize));
    return nullptr;
}

static void RaiseInOnce() {
    for (int round = 0; round < Rounds; ++round) {
        GuestOnce control{0, nullptr};
        OnceCall first{&control};
        OnceCall second{&control};
        resume.store(false);
        const int before = stops.load();
        Pthread initializer = nullptr;
        Require(scePthreadCreate(&initializer, nullptr, CallOnce, &first, "running a once initializer") == 0);
        while (initializations.load() != round + 1) std::this_thread::yield();
        Require(sceKernelRaiseException(initializer, SigUsr1) == 0);
        WaitCount(stops, before + 1);
        Pthread waiter = nullptr;
        Require(scePthreadCreate(&waiter, nullptr, CallOnce, &second, "waiting on a once initializer") == 0);
        while (!second.calling.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(sceKernelRaiseException(waiter, SigUsr1) == 0);
        WaitCount(stops, before + 2);
        Require(first.result.load() == -1 && second.result.load() == -1);
        resume.store(true);
        Require(scePthreadJoin(initializer, nullptr) == 0);
        Require(scePthreadJoin(waiter, nullptr) == 0);
        Require(first.result.load() == 0 && second.result.load() == 0);
        Require(control.state == 1);
        Require(initializations.load() == round + 1);
    }
}

int main() {
    const auto* image = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const auto* headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
    imageBegin = reinterpret_cast<std::uintptr_t>(image);
    imageEnd = imageBegin + headers->OptionalHeader.SizeOfImage;
    release = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    alertableRelease = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Require(release != nullptr && alertableRelease != nullptr);
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);

    Pthread blocked = nullptr;
    Require(scePthreadCreate(&blocked, nullptr, Blocked, nullptr, "blocked in a host wait") == 0);
    for (int round = 0; round < Rounds; ++round) {
        while (blockedRound.load() != round) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto raisedAt = std::chrono::steady_clock::now();
        Require(sceKernelRaiseException(blocked, SigUsr1) == 0);
        Require(std::chrono::steady_clock::now() - raisedAt < std::chrono::milliseconds(500));
        Require(SetEvent(release) != 0);
        WaitDeliveries(round + 1);
        blockedRound.store(round + Rounds);
    }
    Require(scePthreadJoin(blocked, nullptr) == 0);

    Pthread busy = nullptr;
    Require(scePthreadCreate(&busy, nullptr, Busy, nullptr, "busy in host code") == 0);
    for (int round = 0; round < Rounds; ++round) {
        while (busyRound.load() != round) std::this_thread::yield();
        Require(sceKernelRaiseException(busy, SigUsr1) == 0);
        WaitDeliveries(Rounds + round + 1);
        busyRound.store(round + Rounds);
    }
    Require(scePthreadJoin(busy, nullptr) == 0);

    Pthread alertable = nullptr;
    Require(scePthreadCreate(&alertable, nullptr, Alertable, nullptr, "in an alertable host wait") == 0);
    for (int round = 0; round < Rounds; ++round) {
        while (boundaryRound.load() != round) std::this_thread::yield();
        alertableSignaled.store(false);
        Require(sceKernelRaiseException(alertable, SigUsr1) == 0);
        WaitDeliveries(2 * Rounds + 2 * round + 1);
        boundaryDone.store(true);
        while (alertableRound.load() != round) std::this_thread::yield();
        const auto entryDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!InsideNtdll(alertableHandle)) {
            Require(std::chrono::steady_clock::now() < entryDeadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto raisedAt = std::chrono::steady_clock::now();
        Require(sceKernelRaiseException(alertable, SigUsr1) == 0);
        Require(std::chrono::steady_clock::now() - raisedAt < std::chrono::milliseconds(500));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        alertableSignaled.store(true);
        Require(SetEvent(alertableRelease) != 0);
        WaitDeliveries(2 * Rounds + 2 * round + 2);
        alertableRound.store(round + Rounds);
    }
    Require(scePthreadJoin(alertable, nullptr) == 0);

    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
    CloseHandle(release);
    CloseHandle(alertableRelease);
    CloseHandle(alertableHandle);
    std::printf("deliveries %d, outside the image %d, before the alertable wait %d, inside an alertable host wait %d\n", deliveries.load(), outsideImage.load(), beforeWait.load(), insideAlertableWait.load());
    Require(outsideImage.load() == 0);
    Require(beforeWait.load() == Rounds);
    Require(insideAlertableWait.load() == 0);

    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Stop)) == 0);
    RaiseInRelock(1, false);
    RaiseInRelock(3, false);
    RaiseInRelock(1, true);
    RaiseInRelock(3, true);
    RaiseInOnce();
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
}
