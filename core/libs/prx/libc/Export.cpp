#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include "prx/libc/include/VarArgsAbi.hpp"

uint32_t Need_sceLibc = 1;

namespace {

constexpr int MtxRecursiveType = 0x4;
constexpr std::uint32_t ClassificationControl = 0x80;
constexpr std::uint32_t ClassificationSpace = 0x04;
constexpr std::uint32_t ClassificationCntrl = 0x40;
constexpr std::uint32_t ClassificationUpper = 0x02;
constexpr std::uint32_t ClassificationLower = 0x10;
constexpr std::uint32_t ClassificationDigit = 0x20;
constexpr std::uint32_t ClassificationXdigit = 0x01;
constexpr std::uint32_t ClassificationPunct = 0x08;

std::uint32_t ClassificationBits(unsigned int character) {
    if (character >= 256) return 0;
    const auto value = static_cast<unsigned char>(character);
    std::uint32_t bits = 0;
    if (value < 32 || value == 127) bits |= ClassificationControl;
    if (value == ' ') bits |= ClassificationSpace;
    if (value >= '\t' && value <= '\r') bits |= ClassificationCntrl;
    if (value >= 'A' && value <= 'Z') bits |= ClassificationUpper;
    if (value >= 'a' && value <= 'z') bits |= ClassificationLower;
    if (value >= '0' && value <= '9') bits |= ClassificationDigit;
    if ((value >= '0' && value <= '9') || (value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f')) bits |= ClassificationXdigit;
    if (value >= 33 && value <= 126 && (bits & 0x232) == 0) bits |= ClassificationPunct;
    return bits;
}

struct ThreadingState {
    std::mutex mutex;
    std::mutex stateLock;
    std::mutex syncMutex;
    std::condition_variable signal;
    std::thread::id owner{};
    int kind = 0;
    int contentions = 0;
};

std::mutex g_threadingStatesLock;
std::unordered_map<const void*, ThreadingState> g_threadingStates;

bool TakeThreadingState(const void* handle, ThreadingState*& state, std::unique_lock<std::mutex>& lock) {
    if (handle == nullptr) return false;
    lock = std::unique_lock<std::mutex>(g_threadingStatesLock);
    const auto found = g_threadingStates.find(handle);
    if (found == g_threadingStates.end()) return false;
    state = &found->second;
    return true;
}

}

extern "C" {

    int APS5_VABI _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(int*, int (APS5_VABI *)(void*, void*, void**), void*);

    int APS5_VABI std_execute_once_nid_postfix(int* flag, int (APS5_VABI *func)(void*, void*, void**), void* arg) {
        return _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(flag, func, arg);
    }

    void APS5_VABI LibcHeapGetTraceInfo_nid_postfix(LibcHeapInfo* info) {
        LibcHeapTraceInfo_nid_no_patch(info);
    }

// Dead import of Cyberpunk 2077 (PPSA04029): no call sites, but the
// Windows loader resolves imports strictly, so it must be present.
void APS5_VABI _ZSt14_Atomic_assertPKcS0__nid_postfix(const char* message, const char* location) {
    std::fprintf(stderr, "[libc] atomic assertion failed: %s%s%s\n",
        message != nullptr ? message : "(null)",
        location != nullptr ? " at " : "", location != nullptr ? location : "");
    std::abort();
}

APS5_EXPORT("rWSuTWY2JN0", libcCyberUnknown18);
int APS5_VABI libcCyberUnknown18(char* buffer, std::size_t size, std::size_t count, const char* format, VaList* args) {
    constexpr std::size_t RsizeMax = static_cast<std::size_t>(-1) >> 1;
    if (buffer == nullptr || format == nullptr || args == nullptr || size == 0 || size > RsizeMax || count > RsizeMax) {
        if (buffer != nullptr && size != 0) buffer[0] = '\0';
        return -1;
    }
    const std::size_t limit = count == static_cast<std::size_t>(-1) ? size : std::min(count + 1, size);
    return static_cast<int>(std::vsnprintf(buffer, limit, format, *reinterpret_cast<std::va_list*>(args)));
}

APS5_EXPORT("tfNbpqL3D0M", libcCyberUnknown19);
int APS5_VABI libcCyberUnknown19(const char* input, const char* format, VaList* args) {
    if (input == nullptr || format == nullptr || args == nullptr) return EOF;
    return std::vsscanf(input, format, *reinterpret_cast<std::va_list*>(args));
}

APS5_EXPORT("Ye20uNnlglA", libcCyberUnknown02);
int APS5_VABI libcCyberUnknown02(int value) {
    return value < 0 ? -value : value;
}

APS5_EXPORT("5Lf51jvohTQ", libcCyberUnknown03);
int APS5_VABI libcCyberUnknown03(void* mutex) {
    if (mutex == nullptr) return 4;
    std::unique_lock<std::mutex> lock(g_threadingStatesLock);
    return g_threadingStates.erase(mutex) != 0 ? 0 : 4;
}

APS5_EXPORT("7yMFgcS8EPA", libcCyberUnknown05);
void APS5_VABI libcCyberUnknown05(void* condition) {
    if (condition == nullptr) return;
    std::unique_lock<std::mutex> lock(g_threadingStatesLock);
    g_threadingStates.erase(condition);
}

APS5_EXPORT("CyXs2l-1kNA", libcCyberUnknown07);
int APS5_VABI libcCyberUnknown07(unsigned int character, unsigned int mask) {
    return (ClassificationBits(character) & mask) != 0;
}

APS5_EXPORT("H+8UBOwfScI", libcCyberUnknown08);
double APS5_VABI libcCyberUnknown08(double value, int exponent) {
    if (exponent == 0) return 1.0;
    const std::uint64_t magnitude = exponent < 0
        ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(exponent)) : static_cast<std::uint64_t>(exponent);
    double result = 1.0;
    double base = value;
    for (std::uint64_t bit = magnitude; bit != 0; bit >>= 1) {
        if ((bit & 1u) != 0) result *= base;
        if (bit > 1) base *= base;
    }
    return exponent < 0 ? 1.0 / result : result;
}

APS5_EXPORT("JhVR7D4Ax6Y", libcCyberUnknown09);
unsigned long APS5_VABI libcCyberUnknown09(const std::uint16_t* first, std::uint16_t** last, int base) {
    if (first == nullptr) {
        if (last != nullptr) *last = nullptr;
        return 0;
    }
    const std::uint16_t* position = first;
    while (*position == 0x09 || *position == 0x0a || *position == 0x0b || *position == 0x0c || *position == 0x0d || *position == 0x20) ++position;
    int negative = 0;
    if (*position == 0x2b) ++position;
    else if (*position == 0x2d) { negative = 1; ++position; }
    std::uint16_t* consumed = const_cast<std::uint16_t*>(position);
    int radix = base;
    if (radix == 0) {
        if (*position == 0x30) {
            ++position;
            if (*position == 0x78 || *position == 0x58) { radix = 16; ++position; }
            else radix = 8;
        } else radix = 10;
    } else if (radix == 16 && *position == 0x30 && position[1] != 0 && (position[1] == 0x78 || position[1] == 0x58)) {
        position += 2;
    }
    unsigned long value = 0;
    bool any = false;
    bool overflow = false;
    for (;; ++position) {
        unsigned long digit;
        if (*position >= 0x30 && *position <= 0x39) digit = *position - 0x30;
        else if (*position >= 0x61 && *position <= 0x66) digit = *position - 0x61 + 10;
        else if (*position >= 0x41 && *position <= 0x46) digit = *position - 0x41 + 10;
        else break;
        if (digit >= static_cast<unsigned long>(radix)) break;
        any = true;
        if (value > (static_cast<unsigned long>(-1) - digit) / static_cast<unsigned long>(radix)) overflow = true;
        else value = value * static_cast<unsigned long>(radix) + digit;
    }
    if (last != nullptr) *last = any ? consumed : const_cast<std::uint16_t*>(first);
    if (overflow) return static_cast<unsigned long>(-1);
    return negative ? static_cast<unsigned long>(0u - value) : value;
}

APS5_EXPORT("SreZybSRWpU", libcCyberUnknown10);
int APS5_VABI libcCyberUnknown10(void* condition) {
    if (condition == nullptr) return 4;
    std::unique_lock<std::mutex> lock(g_threadingStatesLock);
    const auto [iterator, inserted] = g_threadingStates.try_emplace(condition);
    if (!inserted) return 1;
    iterator->second.kind = 0;
    return 0;
}

APS5_EXPORT("VsP3daJgmVA", libcCyberUnknown11);
int APS5_VABI libcCyberUnknown11(void* condition) {
    ThreadingState* state = nullptr;
    std::unique_lock<std::mutex> lock;
    if (!TakeThreadingState(condition, state, lock)) return 4;
    std::unique_lock<std::mutex> sync(state->syncMutex);
    state->signal.notify_all();
    return 0;
}

APS5_EXPORT("YaHc3GS7y7g", libcCyberUnknown12);
int APS5_VABI libcCyberUnknown12(void* mutex, int type, int) {
    if (mutex == nullptr) return 4;
    std::unique_lock<std::mutex> lock(g_threadingStatesLock);
    const auto [iterator, inserted] = g_threadingStates.try_emplace(mutex);
    if (!inserted) return 1;
    iterator->second.kind = type;
    return 0;
}

APS5_EXPORT("gTuXQwP9rrs", libcCyberUnknown13);
int APS5_VABI libcCyberUnknown13(void* mutex) {
    ThreadingState* state = nullptr;
    std::unique_lock<std::mutex> lock;
    if (!TakeThreadingState(mutex, state, lock)) return 4;
    {
        std::unique_lock<std::mutex> stateLock(state->stateLock);
        const auto owner = std::this_thread::get_id();
        if (state->owner == owner) {
            if ((state->kind & MtxRecursiveType) == 0) return 3;
            ++state->contentions;
            return 0;
        }
    }
    state->mutex.lock();
    {
        std::unique_lock<std::mutex> stateLock(state->stateLock);
        state->owner = std::this_thread::get_id();
        state->contentions = 1;
    }
    return 0;
}

APS5_EXPORT("iS4aWbUonl0", libcCyberUnknown14);
int APS5_VABI libcCyberUnknown14(void* mutex) {
    ThreadingState* state = nullptr;
    std::unique_lock<std::mutex> lock;
    if (!TakeThreadingState(mutex, state, lock)) return 4;
    std::unique_lock<std::mutex> stateLock(state->stateLock);
    if (state->owner != std::this_thread::get_id() || state->contentions <= 0) return 4;
    if (--state->contentions == 0) {
        state->owner = std::thread::id{};
        stateLock.unlock();
        state->mutex.unlock();
    }
    return 0;
}

APS5_EXPORT("vEaqE-7IZYc", libcCyberUnknown16);
int APS5_VABI libcCyberUnknown16(void* condition, void* mutex) {
    if (condition == nullptr || mutex == nullptr) return 4;
    ThreadingState* conditionState = nullptr;
    ThreadingState* mutexState = nullptr;
    {
        std::unique_lock<std::mutex> registryLock(g_threadingStatesLock);
        const auto conditionFound = g_threadingStates.find(condition);
        const auto mutexFound = g_threadingStates.find(mutex);
        if (conditionFound == g_threadingStates.end() || mutexFound == g_threadingStates.end()) return 4;
        conditionState = &conditionFound->second;
        mutexState = &mutexFound->second;
    }
    {
        std::unique_lock<std::mutex> sync(conditionState->syncMutex);
        {
            std::unique_lock<std::mutex> stateLock(mutexState->stateLock);
            if (mutexState->owner != std::this_thread::get_id() || mutexState->contentions <= 0) return 4;
            mutexState->owner = std::thread::id{};
            mutexState->contentions = 0;
            stateLock.unlock();
            mutexState->mutex.unlock();
        }
        conditionState->signal.wait(sync);
    }
    mutexState->mutex.lock();
    {
        std::unique_lock<std::mutex> stateLock(mutexState->stateLock);
        mutexState->owner = std::this_thread::get_id();
        mutexState->contentions = 1;
    }
    return 0;
}

}