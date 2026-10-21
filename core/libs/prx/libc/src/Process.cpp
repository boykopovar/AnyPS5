#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <algorithm>
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

extern "C" int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle);

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

#ifdef _WIN32
namespace {
struct ChildProcess {
    std::int32_t processId;
    std::int64_t processHandle;
};

std::mutex childProcessMutex;
std::vector<ChildProcess> childProcesses;

int ChildProcessWaitStatus(DWORD code) {
    if (code < 0x80000000u) return static_cast<int>(code & 0xffu) << 8;
    switch (code) {
    case 0xC0000005u:
    case 0xC00000FDu: return 11;
    case 0xC000001Du:
    case 0xC0000096u: return 4;
    case 0x80000003u: return 5;
    case 0xC0000094u:
    case 0xC0000095u: return 8;
    case 0xC0000409u:
    case 0xC0000374u: return 6;
    default: {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string text(8, '0');
        for (std::size_t index = 0; index < 8; ++index) text[7 - index] = digits[(code >> (index * 4)) & 0xfu];
        NotImplemented_nid_no_patch(("waitpid: a child exited with the status 0x" + text + " is not representable").c_str());
        return 15;
    }
        return 15;
    }
}
}

extern "C" void RegisterChildProcess_nid_no_patch(std::int32_t processId, std::int64_t processHandle) {
    std::lock_guard lock(childProcessMutex);
    childProcesses.push_back({processId, processHandle});
}

extern "C" std::int64_t WaitChildProcess_nid_no_patch(std::int32_t processId, unsigned options, int* status) {
    const bool hang = (options & 1u) != 0;
    const auto matches = [&](const ChildProcess& child) { return processId <= 0 || child.processId == processId; };
    {
        std::lock_guard lock(childProcessMutex);
        if (std::none_of(childProcesses.begin(), childProcesses.end(), matches)) return -2;
    }
    int encoded = 0;
    while (true) {
        std::int32_t reaped = 0;
        {
            std::lock_guard lock(childProcessMutex);
            for (std::size_t index = 0; index < childProcesses.size(); ++index) {
                auto& child = childProcesses[index];
                if (!matches(child)) continue;
                const HANDLE handle = reinterpret_cast<HANDLE>(child.processHandle);
                if (WaitForSingleObject(handle, 0) != WAIT_OBJECT_0) continue;
                DWORD code = 0;
                if (!GetExitCodeProcess(handle, &code)) {
                    *__error_nid_postfix() = 14;
                    return -1;
                }
                encoded = ChildProcessWaitStatus(code);
                CloseHandle(handle);
                reaped = child.processId;
                childProcesses.erase(childProcesses.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }
        if (reaped != 0) {
            if (status != nullptr) *status = encoded;
            return reaped;
        }
        if (hang) return 0;
        constexpr std::size_t groupSize = MAXIMUM_WAIT_OBJECTS;
        std::lock_guard lock(childProcessMutex);
        std::vector<HANDLE> handles;
        for (std::size_t start = 0; start < childProcesses.size(); start += groupSize) {
            handles.clear();
            for (std::size_t index = start; index < childProcesses.size() && index < start + groupSize; ++index)
                handles.push_back(reinterpret_cast<HANDLE>(childProcesses[index].processHandle));
            if (WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, 250) != WAIT_TIMEOUT) break;
        }
    }
}
#endif

}
