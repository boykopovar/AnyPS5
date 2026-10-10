#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <condition_variable>
#include <exception>
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
using GuestExitCallback = void (APS5_VABI*)();
std::mutex exitCallbackMutex;
std::vector<GuestExitCallback> exitCallbacks;

void runGuestExitCallback() {
    GuestExitCallback callback;
    {
        std::lock_guard lock(exitCallbackMutex);
        callback = exitCallbacks.back();
        exitCallbacks.pop_back();
    }
    callback();
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
    std::thread([code] { LibcExit_nid_no_patch(code); }).detach();
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
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
#endif
    std::_Exit(code);
}

[[noreturn]] void LibcExit_nid_no_patch(int code) {
    exitRequested.store(true);
    if (exitStarted.exchange(true)) LibcAwaitExit_nid_postfix();
    LibcRunShutdown_nid_postfix();
    CxaFinalize_nid_no_patch(nullptr);
    std::exit(code);
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
    std::lock_guard lock(exitCallbackMutex);
    exitCallbacks.push_back(func);
    const int result = std::atexit(runGuestExitCallback);
    if (result != 0) exitCallbacks.pop_back();
    return result;
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
