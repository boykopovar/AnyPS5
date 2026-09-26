#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
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
    std::_Exit(code);
}

[[noreturn]] void LibcExit_nid_no_patch(int code) {
    LibcRunShutdown_nid_postfix();
    std::exit(code);
}

void APS5_VABI exit_nid_postfix(int code) {
    LibcExit_nid_no_patch(code);
}

[[noreturn]] void abort_nid_postfix(
    uint64_t arg0, uint64_t arg1, uint64_t arg2,
    uint64_t arg3, uint64_t arg4, uint64_t arg5
) {
    (void)arg0; (void)arg1; (void)arg2;
    (void)arg3; (void)arg4; (void)arg5;
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

int APS5_VABI atexit_nid_postfix(atexit_func_t func) {
    if (func == nullptr)
        return 0;
    return std::atexit(func);
}

}
