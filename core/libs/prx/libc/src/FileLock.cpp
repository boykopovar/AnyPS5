#include "prx/libc/include/FileLock.hpp"

#ifdef _WIN32

#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <io.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <windows.h>

#include "prx/libc/include/WindowsDescriptor.hpp"

namespace {

constexpr int LockShared = 1;
constexpr int LockExclusive = 2;
constexpr int LockNonblocking = 4;
constexpr int LockUnlock = 8;
constexpr DWORD LockByteLow = 0xFFFFFFFE;
constexpr DWORD GateByteLow = 0xFFFFFFFD;
constexpr DWORD WaiterByteLow = 0xFFFFFFFC;
constexpr DWORD ReaderByteLow = 0xFFFFFFFB;
constexpr DWORD LockByteHigh = 0x7FFFFFFF;

using DirectoryKey = std::pair<DWORD, std::uint64_t>;

struct DirectoryHolders {
    int shared = 0;
    bool exclusive = false;
};

std::mutex g_directoryMutex;
std::condition_variable g_directoryChanged;
std::map<DirectoryKey, DirectoryHolders> g_directoryLocks;

bool SetDirectoryLock(const DirectoryKey& key, int& held, int mode, bool nonblocking) {
    std::unique_lock lock(g_directoryMutex);
    const auto release = [&] {
        const auto found = g_directoryLocks.find(key);
        if (found != g_directoryLocks.end()) {
            if (held == LockExclusive) found->second.exclusive = false;
            else if (held == LockShared) --found->second.shared;
            if (!found->second.exclusive && found->second.shared == 0) g_directoryLocks.erase(found);
        }
        held = 0;
    };
    const auto busy = [&] {
        const auto found = g_directoryLocks.find(key);
        if (found == g_directoryLocks.end()) return false;
        const int others = found->second.shared - (held == LockShared ? 1 : 0);
        return (found->second.exclusive && held != LockExclusive) || (mode == LockExclusive && others != 0);
    };
    if (mode != LockUnlock && busy()) {
        if (nonblocking) {
            ::SetLastError(ERROR_LOCK_VIOLATION);
            return false;
        }
        if (held != 0) {
            release();
            g_directoryChanged.notify_all();
        }
        g_directoryChanged.wait(lock, [&] { return !busy(); });
    }
    release();
    if (mode != LockUnlock) {
        auto& holders = g_directoryLocks[key];
        if (mode == LockExclusive) holders.exclusive = true;
        else ++holders.shared;
        held = mode;
    }
    g_directoryChanged.notify_all();
    return true;
}

std::optional<DirectoryKey> DirectoryOf(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION information{};
    if (!::GetFileInformationByHandle(handle, &information) || (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return std::nullopt;
    return DirectoryKey{information.dwVolumeSerialNumber, (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) | information.nFileIndexLow};
}

bool LockByte(HANDLE handle, DWORD low, DWORD flags) {
    OVERLAPPED overlapped{};
    overlapped.Offset = low;
    overlapped.OffsetHigh = LockByteHigh;
    return ::LockFileEx(handle, flags, 0, 1, 0, &overlapped) != 0;
}

bool UnlockByte(HANDLE handle, DWORD low) {
    OVERLAPPED overlapped{};
    overlapped.Offset = low;
    overlapped.OffsetHigh = LockByteHigh;
    return ::UnlockFileEx(handle, 0, 1, 0, &overlapped) != 0;
}

bool Failure(DWORD error) {
    ::SetLastError(error);
    return false;
}

bool AcquireShared(HANDLE handle, int& held, bool nonblocking) {
    const DWORD flags = nonblocking ? LOCKFILE_FAIL_IMMEDIATELY : 0;
    if (!LockByte(handle, ReaderByteLow, flags)) return false;
    if (held == LockExclusive) {
        if (!LockByte(handle, LockByteLow, LOCKFILE_FAIL_IMMEDIATELY) || !UnlockByte(handle, LockByteLow)) {
            const DWORD error = ::GetLastError();
            UnlockByte(handle, ReaderByteLow);
            throw std::runtime_error("flock: downgrade failed, error=" + std::to_string(error));
        }
    } else if (!LockByte(handle, LockByteLow, flags)) {
        const DWORD error = ::GetLastError();
        if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader rollback failed, error=" + std::to_string(::GetLastError()));
        return Failure(error);
    }
    held = LockShared;
    return true;
}

bool AcquireExclusive(HANDLE handle, int& held, bool nonblocking) {
    const DWORD gateFlags = LOCKFILE_EXCLUSIVE_LOCK | (nonblocking ? LOCKFILE_FAIL_IMMEDIATELY : 0);
    if (!LockByte(handle, GateByteLow, gateFlags)) return false;
    bool wait = false;
    DWORD error = ERROR_SUCCESS;
    try {
        error = [&]() -> DWORD {
        if (held == LockShared) {
            if (!UnlockByte(handle, ReaderByteLow)) return ::GetLastError();
            if (LockByte(handle, ReaderByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY)) {
                if (nonblocking) {
                    if (!LockByte(handle, WaiterByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY)) {
                        const DWORD waiter = ::GetLastError();
                        if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader gate unlock failed, error=" + std::to_string(::GetLastError()));
                        if (!LockByte(handle, ReaderByteLow, LOCKFILE_FAIL_IMMEDIATELY)) throw std::runtime_error("flock: reader restore failed, error=" + std::to_string(::GetLastError()));
                        return waiter;
                    }
                }
                if (!UnlockByte(handle, LockByteLow)) {
                    const DWORD unlock = ::GetLastError();
                    if (nonblocking) UnlockByte(handle, WaiterByteLow);
                    if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader gate unlock failed, error=" + std::to_string(::GetLastError()));
                    if (!LockByte(handle, ReaderByteLow, LOCKFILE_FAIL_IMMEDIATELY)) throw std::runtime_error("flock: reader restore failed, error=" + std::to_string(::GetLastError()));
                    return unlock;
                }
                held = 0;
                if (!nonblocking && !LockByte(handle, WaiterByteLow, LOCKFILE_EXCLUSIVE_LOCK)) {
                    const DWORD waiter = ::GetLastError();
                    if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader gate unlock failed, error=" + std::to_string(::GetLastError()));
                    return waiter;
                }
                if (LockByte(handle, LockByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY)) {
                    held = LockExclusive;
                    if (!UnlockByte(handle, WaiterByteLow)) {
                        const DWORD waiter = ::GetLastError();
                        UnlockByte(handle, ReaderByteLow);
                        throw std::runtime_error("flock: waiter unlock failed, error=" + std::to_string(waiter));
                    }
                    if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader gate unlock failed, error=" + std::to_string(::GetLastError()));
                    return ERROR_SUCCESS;
                }
                const DWORD conflict = ::GetLastError();
                if (nonblocking) {
                    if (!LockByte(handle, LockByteLow, LOCKFILE_FAIL_IMMEDIATELY)) {
                        const DWORD restore = ::GetLastError();
                        UnlockByte(handle, WaiterByteLow);
                        UnlockByte(handle, ReaderByteLow);
                        throw std::runtime_error("flock: lost the shared lock of a failed upgrade, error=" + std::to_string(restore));
                    }
                    held = LockShared;
                }
                if (!UnlockByte(handle, WaiterByteLow)) {
                    const DWORD waiter = ::GetLastError();
                    UnlockByte(handle, ReaderByteLow);
                    throw std::runtime_error("flock: waiter unlock failed, error=" + std::to_string(waiter));
                }
                if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader gate unlock failed, error=" + std::to_string(::GetLastError()));
                if (nonblocking) {
                    if (!LockByte(handle, ReaderByteLow, LOCKFILE_FAIL_IMMEDIATELY)) {
                        throw std::runtime_error("flock: reader restore failed, error=" + std::to_string(::GetLastError()));
                    }
                    return conflict;
                }
                if (conflict != ERROR_LOCK_VIOLATION) return conflict;
            } else {
                const DWORD readers = ::GetLastError();
                if (!LockByte(handle, ReaderByteLow, LOCKFILE_FAIL_IMMEDIATELY)) {
                    throw std::runtime_error("flock: reader restore failed, error=" + std::to_string(::GetLastError()));
                }
                if (readers != ERROR_LOCK_VIOLATION || nonblocking) return readers;
                if (!UnlockByte(handle, LockByteLow)) return ::GetLastError();
                held = 0;
                if (!UnlockByte(handle, ReaderByteLow)) throw std::runtime_error("flock: reader unlock failed, error=" + std::to_string(::GetLastError()));
            }
        } else if (LockByte(handle, LockByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY)) {
            held = LockExclusive;
            return ERROR_SUCCESS;
        } else if (::GetLastError() != ERROR_LOCK_VIOLATION || nonblocking) {
            return ::GetLastError();
        }
        if (!LockByte(handle, WaiterByteLow, LOCKFILE_FAIL_IMMEDIATELY)) return ::GetLastError();
        wait = true;
        return ERROR_SUCCESS;
        }();
    } catch (...) {
        UnlockByte(handle, GateByteLow);
        throw;
    }
    if (!UnlockByte(handle, GateByteLow)) {
        const DWORD gate = ::GetLastError();
        if (wait) UnlockByte(handle, WaiterByteLow);
        throw std::runtime_error("flock: gate unlock failed, error=" + std::to_string(gate));
    }
    if (error != ERROR_SUCCESS) return Failure(error);
    if (!wait) return true;
    for (;;) {
        const bool acquired = LockByte(handle, LockByteLow, LOCKFILE_EXCLUSIVE_LOCK);
        const DWORD waited = ::GetLastError();
        if (!acquired) {
            if (!UnlockByte(handle, WaiterByteLow)) throw std::runtime_error("flock: waiter unlock failed, error=" + std::to_string(::GetLastError()));
            return Failure(waited);
        }
        if (LockByte(handle, GateByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY)) {
            held = LockExclusive;
            if (!UnlockByte(handle, WaiterByteLow)) {
                const DWORD waiter = ::GetLastError();
                UnlockByte(handle, GateByteLow);
                throw std::runtime_error("flock: waiter unlock failed, error=" + std::to_string(waiter));
            }
            if (!UnlockByte(handle, GateByteLow)) throw std::runtime_error("flock: gate unlock failed, error=" + std::to_string(::GetLastError()));
            return true;
        }
        const DWORD gate = ::GetLastError();
        if (!UnlockByte(handle, LockByteLow)) throw std::runtime_error("flock: yielded lock unlock failed, error=" + std::to_string(::GetLastError()));
        if (!UnlockByte(handle, WaiterByteLow)) throw std::runtime_error("flock: waiter unlock failed, error=" + std::to_string(::GetLastError()));
        if (gate != ERROR_LOCK_VIOLATION) return Failure(gate);
        if (!LockByte(handle, GateByteLow, LOCKFILE_EXCLUSIVE_LOCK)) return false;
        if (!LockByte(handle, WaiterByteLow, LOCKFILE_FAIL_IMMEDIATELY)) {
            const DWORD requeue = ::GetLastError();
            if (!UnlockByte(handle, GateByteLow)) throw std::runtime_error("flock: gate unlock failed, error=" + std::to_string(::GetLastError()));
            return Failure(requeue);
        }
        if (!UnlockByte(handle, GateByteLow)) {
            const DWORD gateUnlock = ::GetLastError();
            UnlockByte(handle, WaiterByteLow);
            throw std::runtime_error("flock: gate unlock failed, error=" + std::to_string(gateUnlock));
        }
    }
}

struct LockState {
    std::mutex mutex;
    int mode = 0;
    HANDLE locker = nullptr;
    bool classified = false;
    std::optional<DirectoryKey> directory;

    LockState() = default;
    LockState(const LockState&) = delete;
    LockState& operator=(const LockState&) = delete;
    ~LockState() {
        if (directory && mode != 0) SetDirectoryLock(*directory, mode, LockUnlock, true);
        if (locker != nullptr) ::CloseHandle(locker);
    }
};

std::mutex g_mutex;
std::map<int, std::shared_ptr<LockState>> g_locks;

std::shared_ptr<LockState> Materialize(int fd) {
    const auto found = g_locks.find(fd);
    if (found != g_locks.end()) return found->second;
    return g_locks.emplace(fd, std::make_shared<LockState>()).first->second;
}

}

extern "C" {

int GuestFileLock_nid_no_patch(int fd, int operation) {
    const int mode = operation & (LockShared | LockExclusive | LockUnlock);
    if (mode != LockShared && mode != LockExclusive && mode != LockUnlock) {
        errno = EINVAL;
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        ::SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }
    const auto state = [&] {
        std::lock_guard lock(g_mutex);
        return Materialize(fd);
    }();
    std::lock_guard lock(state->mutex);
    if (state->mode == mode) return 0;
    if (!state->classified) {
        state->directory = DirectoryOf(handle);
        state->classified = true;
    }
    if (state->directory) return SetDirectoryLock(*state->directory, state->mode, mode, (operation & LockNonblocking) != 0) ? 0 : -1;
    if (mode == LockUnlock) {
        const auto target = state->locker != nullptr ? state->locker : handle;
        if (state->mode == LockShared) {
            if (!UnlockByte(target, LockByteLow)) return -1;
            if (!UnlockByte(target, ReaderByteLow)) throw std::runtime_error("flock: reader unlock failed, error=" + std::to_string(::GetLastError()));
        } else if (state->mode == LockExclusive && !UnlockByte(target, LockByteLow)) {
            return -1;
        }
        state->mode = 0;
        return 0;
    }
    if (state->mode == 0 && state->locker == nullptr && (WindowsHandleAccess_nid_no_patch(handle, fd, "flock") & (FILE_READ_DATA | FILE_WRITE_DATA)) == 0) {
        const auto locker = ::ReOpenFile(handle, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
        if (locker == INVALID_HANDLE_VALUE) return -1;
        state->locker = locker;
    }
    const auto target = state->locker != nullptr ? state->locker : handle;
    const bool nonblocking = (operation & LockNonblocking) != 0;
    const bool acquired = mode == LockShared ? AcquireShared(target, state->mode, nonblocking) : AcquireExclusive(target, state->mode, nonblocking);
    return acquired ? 0 : -1;
}

void GuestFileLockShare_nid_no_patch(int from, int to) {
    std::lock_guard lock(g_mutex);
    auto state = Materialize(from);
    g_locks[to] = std::move(state);
}

void GuestFileLockForget_nid_no_patch(int fd) {
    std::lock_guard lock(g_mutex);
    g_locks.erase(fd);
}

}

#endif
