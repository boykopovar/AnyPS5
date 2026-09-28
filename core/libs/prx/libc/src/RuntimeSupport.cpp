#include <algorithm>
#include <cstdio>
#include <iterator>
#include <cstdlib>
#include <stdexcept>
#include <functional>
#include <regex>
#include <mutex>
#include <vector>
#include <utility>
#include <random>
#include <string>
#include <cinttypes>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/specifics/gcc/AtomicOps.hpp"
#include "prx/libc/include/FileStream.hpp"

namespace {

std::recursive_mutex g_sysLock;

struct ExitDestructor {
    void (*func)(void*);
    void* arg;
    void* dsoHandle;
};

std::mutex g_exitMutex;
std::vector<ExitDestructor> g_exitDestructors;
bool g_exitRunnerRegistered = false;

}

extern "C" {

FileStream _Stderr_nid_postfix{stderr};
FileStream _Stdout_nid_postfix{stdout};
FileStream _Stdin_nid_postfix{stdin};

int APS5_VABI __cxa_atexit_nid_postfix(void (*func)(void*), void* arg, void* dsoHandle) {
    std::lock_guard lock(g_exitMutex);
    g_exitDestructors.push_back({func, arg, dsoHandle});
    if (!g_exitRunnerRegistered) {
        g_exitRunnerRegistered = true;
        std::atexit([] { CxaFinalize_nid_no_patch(nullptr); });
    }
    return 0;
}

void CxaFinalize_nid_no_patch(void* dsoHandle) {
    for (;;) {
        ExitDestructor destructor{};
        {
            std::lock_guard lock(g_exitMutex);
            const auto found = std::find_if(g_exitDestructors.rbegin(), g_exitDestructors.rend(), [&](const ExitDestructor& entry) {
                return dsoHandle == nullptr || entry.dsoHandle == dsoHandle;
            });
            if (found == g_exitDestructors.rend()) return;
            destructor = *found;
            g_exitDestructors.erase(std::next(found).base());
        }
        destructor.func(destructor.arg);
    }
}

unsigned int APS5_VABI _Atomic_fetch_add_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicFetchAdd(target, value);
}

unsigned int APS5_VABI _Atomic_fetch_sub_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicFetchSub(target, value);
}

int APS5_VABI _Atomic_compare_exchange_weak_4_nid_postfix(volatile unsigned int* target, unsigned int* expected, unsigned int desired, int successOrder, int failureOrder) {
    (void)successOrder;
    (void)failureOrder;
    return GccAtomicCompareExchangeWeak(target, expected, desired) ? 1 : 0;
}

unsigned int APS5_VABI _Atomic_load_4_nid_postfix(volatile unsigned int* target, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicLoad(target);
}

std::uintmax_t APS5_VABI strtoumax_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoumax(str, endptr, base);
}

unsigned int APS5_VABI _ZSt14_Random_devicev_nid_postfix() {
    static std::random_device device;
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    return device();
}

[[noreturn]] void APS5_VABI _ZSt14_Throw_C_errori_nid_postfix(int code) {
    throw std::runtime_error("_Throw_C_error: C11 thread error " + std::to_string(code));
}

void APS5_VABI _Lockfilelock_nid_postfix(FileStream* stream) {
    if (stream == nullptr) throw std::invalid_argument("_Lockfilelock: null stream");
#ifdef _WIN32
    _lock_file(GetNativeStream(stream));
#else
    flockfile(GetNativeStream(stream));
#endif
}

void APS5_VABI _Unlockfilelock_nid_postfix(FileStream* stream) {
    if (stream == nullptr) throw std::invalid_argument("_Unlockfilelock: null stream");
#ifdef _WIN32
    _unlock_file(GetNativeStream(stream));
#else
    funlockfile(GetNativeStream(stream));
#endif
}

unsigned long APS5_VABI _Stoul_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoul(str, endptr, base);
}

void APS5_VABI _Locksyslock_nid_postfix() {
    g_sysLock.lock();
}

void APS5_VABI _Unlocksyslock_nid_postfix() {
    g_sysLock.unlock();
}

}
