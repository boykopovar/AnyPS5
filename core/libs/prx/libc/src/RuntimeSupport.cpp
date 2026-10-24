#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <iterator>
#include <cstdlib>
#include <stdexcept>
#include <functional>
#include <regex>
#include <mutex>
#include <vector>
#include <utility>
#include <string>
#include <cinttypes>
#ifdef _WIN32
#include <windows.h>
#endif

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/specifics/gcc/AtomicOps.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/HostEntropy.hpp"

namespace {

std::recursive_mutex g_sysLock;

struct ExitDestructor {
    void (APS5_VABI *func)(void*);
    void* arg;
    void* dsoHandle;
};

std::mutex g_exitMutex;
std::vector<ExitDestructor> g_exitDestructors;
bool g_exitRunnerRegistered = false;

#ifdef _WIN32
struct UcrtStream {
    char* pointer;
    char* base;
    int count;
    long flags;
    long file;
    int charBuffer;
    int bufferSize;
    char* temporaryName;
    CRITICAL_SECTION lock;
};
#endif

}

extern "C" {

FileStream _Stderr_nid_postfix{stderr};
FileStream _Stdout_nid_postfix{stdout};
FileStream _Stdin_nid_postfix{stdin};

std::FILE* GuestStandardOutputFile_nid_no_patch() {
    return GetNativeStream(&_Stdout_nid_postfix);
}

int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle) {
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

int APS5_VABI cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle) {
    return __cxa_atexit_nid_postfix(func, arg, dsoHandle);
}

void APS5_VABI cxa_finalize_nid_postfix(void* dsoHandle) {
    CxaFinalize_nid_no_patch(dsoHandle);
}

void APS5_VABI _Vacopy_nid_postfix(unsigned char* destination, const unsigned char* source) {
    std::uint64_t overflowArea;
    std::memcpy(&overflowArea, source + 16, sizeof(overflowArea));
    std::memcpy(destination + 16, &overflowArea, sizeof(overflowArea));
    unsigned char offsets[16];
    std::memcpy(offsets, source, sizeof(offsets));
    std::memcpy(destination, offsets, sizeof(offsets));
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

std::intmax_t APS5_VABI strtoimax_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, false) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<std::intmax_t>(str, endptr, base, std::strtoimax);
}

std::uintmax_t APS5_VABI strtoumax_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, false) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<std::uintmax_t>(str, endptr, base, std::strtoumax);
}

unsigned int APS5_VABI _ZSt14_Random_devicev_nid_postfix() {
    unsigned int value = 0;
    HostEntropy_nid_no_patch(&value, sizeof(value));
    return value;
}

[[noreturn]] void APS5_VABI _ZSt14_Throw_C_errori_nid_postfix(int code) {
    throw std::runtime_error("_Throw_C_error: C11 thread error " + std::to_string(code));
}

void APS5_VABI _Lockfilelock_nid_postfix(FileStream* stream) {
    if (stream == nullptr) throw std::invalid_argument("_Lockfilelock: null stream");
#ifdef _WIN32
    _lock_file(GetStreamHandle(stream));
#else
    flockfile(GetStreamHandle(stream));
#endif
}

void APS5_VABI _Unlockfilelock_nid_postfix(FileStream* stream) {
    if (stream == nullptr) throw std::invalid_argument("_Unlockfilelock: null stream");
#ifdef _WIN32
    _unlock_file(GetStreamHandle(stream));
#else
    funlockfile(GetStreamHandle(stream));
#endif
}

void APS5_VABI flockfile_nid_postfix(FileStream* stream) {
    _Lockfilelock_nid_postfix(stream);
}

void APS5_VABI funlockfile_nid_postfix(FileStream* stream) {
    _Unlockfilelock_nid_postfix(stream);
}

int APS5_VABI ftrylockfile_nid_postfix(FileStream* stream) {
    if (stream == nullptr) throw std::invalid_argument("ftrylockfile: null stream");
#ifdef _WIN32
    return TryEnterCriticalSection(&reinterpret_cast<UcrtStream*>(GetStreamHandle(stream))->lock) ? 0 : -1;
#else
    return ftrylockfile(GetStreamHandle(stream)) == 0 ? 0 : -1;
#endif
}

std::uint64_t APS5_VABI _Stoul_nid_postfix(const char* str, char** endptr, int base) {
    if (RejectIntegerBase_nid_no_patch(str, endptr, base, false) || StopAtPrefixLetter_nid_no_patch(str, endptr, base)) return 0;
    return ParseInteger<unsigned long long>(str, endptr, base, std::strtoull);
}

void APS5_VABI _Locksyslock_nid_postfix() {
    g_sysLock.lock();
}

void APS5_VABI _Unlocksyslock_nid_postfix() {
    g_sysLock.unlock();
}

__int128 APS5_VABI __divti3_nid_postfix(__int128 dividend, __int128 divisor) {
    return SonyDivti3(dividend, divisor);
}

unsigned __int128 APS5_VABI __udivti3_nid_postfix(unsigned __int128 dividend, unsigned __int128 divisor) {
    return SonyUdivti3(dividend, divisor);
}

unsigned __int128 APS5_VABI __umodti3_nid_postfix(unsigned __int128 dividend, unsigned __int128 divisor) {
    return SonyUmodti3(dividend, divisor);
}

__int128 APS5_VABI __modti3_nid_postfix(__int128 dividend, __int128 divisor) {
    return SonyModti3(dividend, divisor);
}

}
