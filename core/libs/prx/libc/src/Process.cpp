#include "prx/libc/include/Api.hpp"
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <condition_variable>
#include <exception>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <stdexcept>
#include "prx/libc/include/Shutdown.hpp"

#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

namespace {

std::mutex shutdownMutex;
std::condition_variable shutdownChanged;
std::vector<void (*)()> shutdownCallbacks;
std::thread::id shutdownThread;
bool shutdownStarted = false;
bool shutdownFinished = false;
std::exception_ptr shutdownFailure;
std::stop_source shutdownSource;
std::atomic<bool> exitRequested{false};
std::atomic<bool> exitStarted{false};

thread_local bool runningExit = false;

void APS5_VABI RunGuestExitCallback(void* callback) {
    reinterpret_cast<GuestExitCallback>(callback)();
}

}

extern "C" std::stop_token LibcShutdownToken_nid_postfix() {
    return shutdownSource.get_token();
}

extern "C" void LibcRequestShutdown_nid_postfix() {
    shutdownSource.request_stop();
}

extern "C" void LibcRequestExit_nid_postfix(int code) {
    if (exitRequested.exchange(true)) return;
    LibcRequestShutdown_nid_postfix();
    std::thread([code] { LibcExitWithoutHandlers_nid_no_patch(code); }).detach();
}

extern "C" [[noreturn]] void LibcAwaitExit_nid_postfix() {
    if (!exitRequested.load()) throw ProcessShutdown{};
    {
        std::lock_guard lock(shutdownMutex);
        if (shutdownThread == std::this_thread::get_id()) throw std::runtime_error("libc: exit thread cannot wait for itself");
    }
    for (;;) exitRequested.wait(true);
}

extern "C" void LibcRegisterShutdown_nid_postfix(void (*callback)()) {
    std::lock_guard lock(shutdownMutex);
    if (callback == nullptr || shutdownStarted) throw std::runtime_error("libc: invalid shutdown registration");
    shutdownCallbacks.push_back(callback);
}

extern "C" void LibcRunShutdown_nid_postfix() {
    std::unique_lock lock(shutdownMutex);
    if (shutdownStarted) {
        if (!shutdownFinished && shutdownThread == std::this_thread::get_id()) throw std::runtime_error("libc: recursive shutdown");
        shutdownChanged.wait(lock, [] { return shutdownFinished; });
        if (shutdownFailure) std::rethrow_exception(shutdownFailure);
        return;
    }
    shutdownStarted = true;
    shutdownThread = std::this_thread::get_id();
    auto callbacks = std::move(shutdownCallbacks);
    lock.unlock();
    LibcRequestShutdown_nid_postfix();
    std::exception_ptr error;
    for (auto it = callbacks.rbegin(); it != callbacks.rend(); ++it) {
        try { (*it)(); }
        catch (...) { if (!error) error = std::current_exception(); }
    }
    lock.lock();
    shutdownFailure = error;
    shutdownFinished = true;
    lock.unlock();
    shutdownChanged.notify_all();
    if (error) std::rethrow_exception(error);
}

namespace {

std::atomic<void (*)()> endFlush{nullptr};
std::atomic<void (*)()> exitDrain{nullptr};
void (*previousAbortHandler)(int) = SIG_DFL;

void FlushOnAbort(int number) {
    LibcRunEndFlush_nid_no_patch();
    if (previousAbortHandler == SIG_IGN) return;
    if (previousAbortHandler != SIG_DFL && previousAbortHandler != SIG_ERR) {
        previousAbortHandler(number);
        return;
    }
#ifndef _WIN32
    std::signal(number, SIG_DFL);
    std::raise(number);
#endif
}

}

extern "C" void LibcSetEndFlush_nid_no_patch(void (*flush)()) {
    if (flush == nullptr) throw std::runtime_error("libc: invalid end flush registration");
    void (*registered)() = nullptr;
    if (!endFlush.compare_exchange_strong(registered, flush) && registered != flush) throw std::runtime_error("libc: end flush already registered");
    static std::once_flag installed;
    std::call_once(installed, [] { previousAbortHandler = std::signal(SIGABRT, FlushOnAbort); });
}

extern "C" void LibcRunEndFlush_nid_no_patch() {
    if (const auto flush = endFlush.load(std::memory_order_acquire)) flush();
}

extern "C" void LibcSetExitDrain_nid_no_patch(void (*drain)()) {
    if (drain == nullptr) throw std::runtime_error("libc: invalid exit drain registration");
    void (*registered)() = nullptr;
    if (!exitDrain.compare_exchange_strong(registered, drain) && registered != drain) throw std::runtime_error("libc: exit drain already registered");
}

namespace {

void StartExit() {
    exitRequested.store(true);
    if (exitStarted.exchange(true)) {
        if (!runningExit) LibcAwaitExit_nid_postfix();
    } else {
        runningExit = true;
    }
}

}

extern "C" {

[[noreturn]] void APS5_VABI _Exit_nid_postfix(int code) {
    static const bool trace = std::getenv("APS5_TRACE_EXIT") != nullptr;
    if (trace) {
        std::fprintf(stderr, "[libc] _Exit(%d) called from %p\n", code, __builtin_return_address(0));
        std::fflush(stderr);
    }
    LibcTerminate_nid_no_patch(code);
}

[[noreturn]] void LibcTerminate_nid_no_patch(int code) {
    if (const auto drain = exitDrain.exchange(nullptr)) {
        try {
            auto completion = std::make_shared<std::promise<void>>();
            auto finished = completion->get_future();
            std::thread([completion, drain] {
                try {
                    drain();
                    completion->set_value();
                } catch (...) {
                    completion->set_exception(std::current_exception());
                }
            }).detach();
            if (finished.wait_for(std::chrono::seconds(8)) == std::future_status::ready) finished.get();
            else std::fputs("[libc] host exit drain exceeded 8 seconds\n", stderr);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[libc] host exit drain failed: %s\n", error.what());
        } catch (...) {
            std::fputs("[libc] host exit drain failed\n", stderr);
        }
    }
    LibcRunEndFlush_nid_no_patch();
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
#endif
    std::_Exit(code);
}

void LibcStartExit_nid_postfix() {
    StartExit();
}

[[noreturn]] void LibcExit_nid_no_patch(int code) {
    StartExit();
    CxaFinalize_nid_no_patch(nullptr);
    LibcRunShutdown_nid_postfix();
    std::exit(code);
}

[[noreturn]] void LibcExitWithoutHandlers_nid_no_patch(int code) {
    StartExit();
    LibcRunShutdown_nid_postfix();
    std::fflush(nullptr);
    LibcTerminate_nid_no_patch(code);
}

void APS5_VABI exit_nid_postfix(int code) {
    static const bool trace = std::getenv("APS5_TRACE_EXIT") != nullptr;
    if (trace) {
        std::fprintf(stderr, "[libc] exit(%d) called from %p\n", code, __builtin_return_address(0));
        std::fflush(stderr);
    }
    LibcExit_nid_no_patch(code);
}

[[noreturn]] void APS5_VABI catchReturnFromMain_nid_postfix(int status) {
    LibcExit_nid_no_patch(status);
}

[[noreturn]] void abort_nid_postfix(
    uint64_t arg0, uint64_t arg1, uint64_t arg2,
    uint64_t arg3, uint64_t arg4, uint64_t arg5
) {
    (void)arg0; (void)arg1; (void)arg2;
    (void)arg3; (void)arg4; (void)arg5;
    LibcRunEndFlush_nid_no_patch();
    std::fprintf(stderr, "[libc] abort() called from %p\n", __builtin_return_address(0));
    std::fflush(nullptr);
    std::abort();
}

int* APS5_VABI libc_error_nid_postfix() {
    return &errno;
}

int* APS5_VABI __error_nid_postfix() {
    return &errno;
}

[[noreturn]] void __stack_chk_fail_nid_postfix() {
    std::abort();
}

int APS5_VABI atexit_nid_postfix(GuestExitCallback func) {
    if (func == nullptr)
        return 0;
    return __cxa_atexit_nid_postfix(RunGuestExitCallback, reinterpret_cast<void*>(func), nullptr);
}

namespace {
std::mutex quickExitMutex;
std::vector<GuestExitCallback> quickExitCallbacks;
}

int APS5_VABI at_quick_exit_nid_postfix(GuestExitCallback func) {
    if (func == nullptr)
        return 0;
    std::lock_guard lock(quickExitMutex);
    quickExitCallbacks.push_back(func);
    return 0;
}

[[noreturn]] void APS5_VABI quick_exit_nid_postfix(int status) {
    std::vector<GuestExitCallback> callbacks;
    {
        std::lock_guard lock(quickExitMutex);
        callbacks = std::move(quickExitCallbacks);
    }
    for (auto it = callbacks.rbegin(); it != callbacks.rend(); ++it) (*it)();
    _Exit_nid_postfix(status);

}

int APS5_VABI daemon_nid_postfix(int noChdir, int noClose) {
    (void)noChdir;
    (void)noClose;
    NotImplemented_nid_no_patch("daemon: forking into the background");
    return -1;
}

}

namespace {
constexpr int SaRestart = 0x2;
std::atomic<SigactionFn_nid_no_patch> registeredSigaction{nullptr};
std::atomic<std::uint32_t*> registeredInterruptSet{nullptr};

struct InterruptSigaction {
    std::uintptr_t handler;
    int flags;
    std::uint32_t mask[4];
};
static_assert(sizeof(InterruptSigaction) == 32 && offsetof(InterruptSigaction, flags) == 8 && offsetof(InterruptSigaction, mask) == 12);
}

extern "C" {

void RegisterSigaction_nid_no_patch(SigactionFn_nid_no_patch fn, std::uint32_t* interruptSet) {
    registeredInterruptSet.store(interruptSet, std::memory_order_release);
    registeredSigaction.store(fn, std::memory_order_release);
}

int APS5_VABI siginterrupt_nid_postfix(int guest, int flag) {
    const auto sigaction = registeredSigaction.load(std::memory_order_acquire);
    auto* const interruptSet = registeredInterruptSet.load(std::memory_order_acquire);
    if (sigaction == nullptr || interruptSet == nullptr) throw std::runtime_error("siginterrupt: libkernel has not registered sigaction");
    InterruptSigaction action{};
    if (sigaction(guest, nullptr, &action) != 0) return -1;
    const auto index = static_cast<std::uint32_t>(guest - 1);
    if (flag) {
        interruptSet[index >> 5] |= 1u << (index & 31);
        action.flags &= ~SaRestart;
    } else {
        interruptSet[index >> 5] &= ~(1u << (index & 31));
        action.flags |= SaRestart;
    }
    return sigaction(guest, &action, nullptr);
}

}
