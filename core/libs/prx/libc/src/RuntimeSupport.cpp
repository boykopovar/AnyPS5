#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <functional>
#include <regex>
#include <mutex>
#include <vector>
#include <utility>

namespace {

std::mutex g_sysLock;

}

extern "C" {

FILE* _Stderr_nid_postfix() {
    return stderr;
}

FILE* _Stdout_nid_postfix() {
    return stdout;
}

int __cxa_atexit_nid_postfix(void (*func)(void*), void* arg, void* dsoHandle) {
    (void)dsoHandle;
    static std::vector<std::pair<void (*)(void*), void*>> destructors;
    static bool runnerRegistered = false;
    destructors.emplace_back(func, arg);
    if (!runnerRegistered) {
        runnerRegistered = true;
        std::atexit([] {
            for (auto it = destructors.rbegin(); it != destructors.rend(); ++it) {
                it->first(it->second);
            }
        });
    }
    return 0;
}

unsigned int _Atomic_fetch_add_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}

unsigned int _Atomic_fetch_sub_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST);
}

unsigned long _Stoul_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoul(str, endptr, base);
}

void _Locksyslock_nid_postfix() {
    g_sysLock.lock();
}

void _Unlocksyslock_nid_postfix() {
    g_sysLock.unlock();
}

}
