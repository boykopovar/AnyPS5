#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace {

constexpr int FunctionUnlock = 0;
constexpr int FunctionLock = 1;
constexpr int FunctionTryLock = 2;
constexpr int FunctionTest = 3;
constexpr int GuestGetLock = 11;
constexpr int GuestSetLock = 12;
constexpr int GuestSetLockWait = 13;
constexpr std::int16_t GuestUnlock = 2;
constexpr std::int16_t GuestWriteLock = 3;
constexpr std::int16_t GuestFromCurrent = 1;
constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;

struct GuestLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};
static_assert(sizeof(GuestLock) == 32 && offsetof(GuestLock, type) == 20);

}

int APS5_VABI lockf_nid_postfix(int descriptor, int function, std::int64_t size) {
    GuestLock lock{};
    lock.start = 0;
    lock.length = size;
    lock.whence = GuestFromCurrent;
    int command = 0;
    switch (function) {
        case FunctionUnlock:
            command = GuestSetLock;
            lock.type = GuestUnlock;
            break;
        case FunctionLock:
            command = GuestSetLockWait;
            lock.type = GuestWriteLock;
            break;
        case FunctionTryLock:
            command = GuestSetLock;
            lock.type = GuestWriteLock;
            break;
        case FunctionTest:
            lock.type = GuestWriteLock;
            if (fcntl_nid_postfix(descriptor, GuestGetLock, &lock) == -1) return -1;
            if (lock.type == GuestUnlock || (lock.system == 0 && lock.pid == getpid_nid_postfix())) return 0;
            *__error_nid_postfix() = GuestEagain;
            return -1;
        default:
            *__error_nid_postfix() = GuestEinval;
            return -1;
    }
    return fcntl_nid_postfix(descriptor, command, &lock);
}
