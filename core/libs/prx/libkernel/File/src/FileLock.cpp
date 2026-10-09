#include "prx/libkernel/File/include/FileLock.hpp"

#ifdef _WIN32

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <io.h>
#include <map>
#include <mutex>
#include <windows.h>

extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);

namespace {

constexpr int LockShared = 1;
constexpr int LockExclusive = 2;
constexpr int LockNonblocking = 4;
constexpr int LockUnlock = 8;

std::mutex g_mutex;
std::map<int, int> g_locks;

void IgnoreInvalidFlockParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}

int LockFailure() {
    errno = ::GetLastError() == ERROR_INVALID_HANDLE ? EBADF : 0;
    return -1;
}

int HeldLock(int fd) {
    std::lock_guard lock(g_mutex);
    const auto found = g_locks.find(fd);
    return found == g_locks.end() ? 0 : found->second;
}

void SetHeldLock(int fd, int mode) {
    std::lock_guard lock(g_mutex);
    if (mode == 0) {
        g_locks.erase(fd);
    } else {
        g_locks[fd] = mode;
    }
}

}

namespace File {

int Flock(int fd, int operation) {
    const int mode = operation & (LockShared | LockExclusive | LockUnlock);
    if (mode != LockShared && mode != LockExclusive && mode != LockUnlock) {
        errno = EINVAL;
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidFlockParameter);
    const auto nativeHandle = ::_get_osfhandle(fd);
    _set_thread_local_invalid_parameter_handler(previous);
    if (nativeHandle == -1 || nativeHandle == -2) {
        errno = EBADF;
        ::SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }
    const auto handle = reinterpret_cast<HANDLE>(nativeHandle);
    errno = 0;
    const int held = HeldLock(fd);
    if (held == mode) return 0;
    if (held != 0) {
        OVERLAPPED overlapped{};
        if (!::UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped)) return LockFailure();
        SetHeldLock(fd, 0);
    }
    if (mode == LockUnlock) return 0;
    DWORD flags = mode == LockExclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    if (operation & LockNonblocking) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    OVERLAPPED overlapped{};
    if (!::LockFileEx(handle, flags, 0, MAXDWORD, MAXDWORD, &overlapped)) return LockFailure();
    SetHeldLock(fd, mode);
    return 0;
}

void ForgetFileLock(int fd) {
    SetHeldLock(fd, 0);
}

}

#endif
