extern "C" {
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/WindowsThreadLocal.hpp"
#include "../include/Cancel.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <alloca.h>
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

#ifdef __linux__
#include <sys/syscall.h>
#else
static const auto initialThreadId = std::this_thread::get_id();
#endif

void APS5_VABI scePthreadExit(void* retval) {
    if (!CurrentGuestState().current)
        throw std::runtime_error("scePthreadExit: current thread is not registered");
    auto* self = CurrentGuestState().current;
    CurrentGuestState().exiting = true;
    while (CurrentGuestState().cleanupHandlers) __pthread_cleanup_pop_imp_nid_postfix(1);
    finishThread(self, retval);
    CurrentGuestState().current = nullptr;
    asm volatile("ldmxcsr %0" : : "m"(self->hostMxcsr));
#ifdef _WIN32
    ReleaseThread(self);
    _endthreadex(0);
#else
    if (threadExitJump)
        _longjmp(*threadExitJump, 1);
    pthread_exit(retval);
#endif
    throw std::runtime_error("Native thread exit returned");
}

Pthread APS5_VABI scePthreadSelf() {
#ifdef _WIN32
    if (!CurrentGuestState().current) {
        auto adopted = std::make_unique<PthreadPrivate>();
        std::uint32_t currentMxcsr = 0x1f80;
        asm volatile("stmxcsr %0" : "=m"(currentMxcsr));
        adopted->hostMxcsr = currentMxcsr;
        HANDLE handle = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
            throw std::system_error(GetLastError(), std::system_category(), "Adopting guest thread");
        adopted->nativeHandle = handle;
        adopted->hostAdopted = true;
        adopted->initialThread.store(IsInitialPthread_nid_no_patch(), std::memory_order_release);
        adopted->threadId = std::this_thread::get_id();
        adopted->_detached = true;
        adopted->references.store(1, std::memory_order_relaxed);
        TimedWait::BindThreadWaitState(&adopted->waitCount);
        SetStackFromHost(adopted.get());
        CurrentGuestState().current = adopted.release();
    }
#else
    if (!CurrentGuestState().current) {
        adoptedThread = std::make_unique<PthreadPrivate>();
        std::uint32_t currentMxcsr = 0x1f80;
        asm volatile("stmxcsr %0" : "=m"(currentMxcsr));
        adoptedThread->hostMxcsr = currentMxcsr;
        adoptedThread->_detached = true;
        adoptedThread->_adopted = true;
        adoptedThread->initialThread.store(IsInitialPthread_nid_no_patch(), std::memory_order_release);
        BindHostCpuClock(adoptedThread.get());
        adoptedThread->threadId = std::this_thread::get_id();
        adoptedThread->hostThread = pthread_self();
        SetStackFromHost(adoptedThread.get());
        CurrentGuestState().current = adoptedThread.get();
    }
#endif
    return CurrentGuestState().current;
}

void APS5_VABI scePthreadYield() {
#ifdef _WIN32
    thread_local unsigned int spins = 0;
    thread_local std::uint64_t lastTsc = 0;
    const auto now = __rdtsc();
    if (now - lastTsc > 10000) {
        spins = 0;
    }
    lastTsc = now;
    if (SwitchToThread()) {
        spins = 0;
        return;
    }
    if (++spins < 512) {
        _mm_pause();
        return;
    }
    spins = 0;
    Sleep(1);
    lastTsc = __rdtsc();
#else
    std::this_thread::yield();
#endif
}

int APS5_VABI scePthreadCancel(Pthread thread) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    GuestThreadReference reference;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    if (!reference.Retain(thread)) return SCE_KERNEL_ERROR_ESRCH;
    registered.unlock();
    thread->cancelPending.store(true);
    for (;;) {
        {
            std::lock_guard lock(thread->cancelLock);
            if (!thread->cancelWait || thread->cancelWaitExiting) return SCE_OK;
            thread->cancelWait->NotifyAll();
        }
        scePthreadYield();

    }
}

void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    auto* handler = static_cast<CleanupHandler*>(info);
    handler->routine = routine;
    handler->argument = argument;
    handler->onHeap = 0;
    handler->previous = CurrentGuestState().cleanupHandlers;
    CurrentGuestState().cleanupHandlers = handler;
}

void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute) {
    auto* handler = CurrentGuestState().cleanupHandlers;
    if (!handler) return;
    CurrentGuestState().cleanupHandlers = handler->previous;
    if (execute) handler->routine(handler->argument);
    if (handler->onHeap) delete handler;
}

void APS5_VABI pthread_cleanup_push_nid_postfix(void (APS5_VABI* routine)(void*), void* argument) {
    auto* handler = new CleanupHandler{CurrentGuestState().cleanupHandlers, routine, argument, 1};
    CurrentGuestState().cleanupHandlers = handler;
}

void APS5_VABI pthread_cleanup_pop_nid_postfix(int execute) {
    __pthread_cleanup_pop_imp_nid_postfix(execute);

}

int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2) {
    return thread1 == thread2 ? 1 : 0;
}

KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void) {
    return DEFAULT_THREAD_AFFINITY;
}

int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
    if (!thread || !mask) return SCE_KERNEL_ERROR_EINVAL;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    const KernelCpumask affinity = thread->affinity.load(std::memory_order_relaxed);
    if (!GuestCopyOut_nid_no_patch(mask, &affinity, sizeof(affinity))) return SCE_KERNEL_ERROR_EFAULT;
    return SCE_OK;
}

int APS5_VABI scePthreadGetname(Pthread thread, char* name) {
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    char copy[THREAD_NAME_CAPACITY];
    std::size_t length = 0;
    {
        std::lock_guard lock(thread->nameLock);
        length = std::min<std::size_t>(thread->name.size(), THREAD_NAME_CAPACITY - 1);
        std::memcpy(copy, thread->name.data(), length);
    }
    copy[length] = '\0';
    if (!GuestCopyOut_nid_no_patch(name, copy, length + 1)) return SCE_KERNEL_ERROR_EFAULT;
    return SCE_OK;
}

int APS5_VABI scePthreadGetprio(Pthread thread, int* prio) {
    if (!prio) return SCE_KERNEL_ERROR_EINVAL;
    const auto live = LockLiveGuestThread(thread);
    if (!live) return SCE_KERNEL_ERROR_ESRCH;
    const int priority = thread->priority.load(std::memory_order_relaxed);
    if (!GuestCopyOut_nid_no_patch(prio, &priority, sizeof(priority))) return SCE_KERNEL_ERROR_EFAULT;
    return SCE_OK;
}

int APS5_VABI scePthreadGetthreadid(void) {
    return static_cast<int>(scePthreadSelf()->tid);
}

int APS5_VABI scePthreadRename(Pthread thread, const char* name) {
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    if (!LockRegisteredGuestThread(thread)) return SCE_KERNEL_ERROR_ESRCH;
    std::string copy;
    if (const int error = CopyGuestString_nid_no_patch(name, GUEST_THREAD_NAME_CAPACITY, copy))
        return error == 14 ? SCE_KERNEL_ERROR_EFAULT : error == 63 ? SCE_KERNEL_ERROR_ENAMETOOLONG : SCE_KERNEL_ERROR_ENOMEM;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    std::lock_guard lock(thread->nameLock);
    thread->name = std::move(copy);
#ifdef _WIN32
    SetThreadDescription(static_cast<HANDLE>(thread->nativeHandle), ThreadDescription(thread->name).c_str());
    ApplyJobAffinity(*thread, thread->nativeHandle);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
    if (!thread || mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    thread->affinity.store(mask, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
    if (state != GUEST_CANCEL_ENABLE && state != GUEST_CANCEL_DISABLE) return SCE_KERNEL_ERROR_EINVAL;
    if (old_state) {
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(old_state, &CurrentGuestState().cancelState, sizeof(int));
        errno = savedErrno;
        if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    }
    CurrentGuestState().cancelState = state;
    if (CurrentGuestState().cancelType == GUEST_CANCEL_ASYNCHRONOUS) ThreadCancel::Check();
    return SCE_OK;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
    if (type != GUEST_CANCEL_DEFERRED && type != GUEST_CANCEL_ASYNCHRONOUS) return SCE_KERNEL_ERROR_EINVAL;
    if (old_type) {
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(old_type, &CurrentGuestState().cancelType, sizeof(int));
        errno = savedErrno;
        if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    }
    CurrentGuestState().cancelType = type;
    if (CurrentGuestState().cancelType == GUEST_CANCEL_ASYNCHRONOUS) ThreadCancel::Check();
    return SCE_OK;
}

void APS5_VABI scePthreadTestcancel() {
    ThreadCancel::Check();
}

int APS5_VABI scePthreadSetprio(Pthread thread, int prio) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    const auto live = LockLiveGuestThread(thread);
    if (!live) return SCE_KERNEL_ERROR_ESRCH;
    const auto level = static_cast<std::uint32_t>(prio) & 0xffffu;
    if (!((level >= 256 && level <= 767) || (level >= 896 && level <= 959))) {
        *__error_nid_postfix() = GUEST_EINVAL;
        return SCE_KERNEL_ERROR_EINVAL;
    }
    thread->priority.store(prio, std::memory_order_relaxed);
    return SCE_OK;
}

static void APS5_VABI ResetOnce(void* once) {
    constexpr int32_t Never = 0;
    std::atomic_ref<int32_t> state(*static_cast<int32_t*>(once));
    state.store(Never, std::memory_order_release);
    state.notify_all();
}

int APS5_VABI scePthreadOnce(int32_t* once, void (APS5_VABI* init)(void)) {
    if (!once || !init) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int32_t Never = 0;
    constexpr int32_t Done = 1;
    constexpr int32_t Running = 2;
    std::atomic_ref<int32_t> state(*once);
    for (int32_t expected = Never; !state.compare_exchange_strong(expected, Running, std::memory_order_acq_rel); expected = Never) {
        if (expected != Running) return SCE_OK;
        state.wait(Running, std::memory_order_acquire);
    }
    CleanupHandler reset{};
    void* const mark = ThreadCancel::CleanupMark();
    __pthread_cleanup_push_imp_nid_postfix(ResetOnce, once, &reset);

    try {
        init();
    } catch (...) {
        ThreadCancel::CleanupRestore(mark);
        ResetOnce(once);
        throw;
    }
    ThreadCancel::CleanupRestore(mark);
    state.store(Done, std::memory_order_release);
    state.notify_all();
    return SCE_OK;
}

}

extern "C" {

void APS5_VABI __pthread_cxa_finalize_nid_postfix(void* argument) {
    CxaFinalize_nid_no_patch(argument);
}

}
