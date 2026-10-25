#include "prx/libc/include/FileLock.hpp"

#ifdef _WIN32

#include <cerrno>
#include <io.h>
#include <map>
#include <memory>
#include <mutex>
#include <windows.h>

#include "prx/libc/include/WindowsDescriptor.hpp"

namespace {

constexpr int LockShared = 1;
constexpr int LockExclusive = 2;
constexpr int LockNonblocking = 4;
constexpr int LockUnlock = 8;
constexpr DWORD LockByteLow = 0xFFFFFFFE;
constexpr DWORD LockByteHigh = 0x7FFFFFFF;

struct LockState {
    std::mutex mutex;
    int mode = 0;
    HANDLE locker = nullptr;

    LockState() = default;
    LockState(const LockState&) = delete;
    LockState& operator=(const LockState&) = delete;
    ~LockState() {
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
    if (state->mode != 0) {
        OVERLAPPED overlapped{};
        overlapped.Offset = LockByteLow;
        overlapped.OffsetHigh = LockByteHigh;
        if (!::UnlockFileEx(state->locker != nullptr ? state->locker : handle, 0, 1, 0, &overlapped)) return -1;
        state->mode = 0;
    }
    if (mode == LockUnlock) return 0;
    if (state->locker == nullptr && (WindowsHandleAccess_nid_no_patch(handle, fd, "flock") & (FILE_READ_DATA | FILE_WRITE_DATA)) == 0) {
        const auto locker = ::ReOpenFile(handle, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
        if (locker == INVALID_HANDLE_VALUE) return -1;
        state->locker = locker;
    }
    DWORD flags = mode == LockExclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    if (operation & LockNonblocking) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    OVERLAPPED overlapped{};
    overlapped.Offset = LockByteLow;
    overlapped.OffsetHigh = LockByteHigh;
    if (!::LockFileEx(state->locker != nullptr ? state->locker : handle, flags, 0, 1, 0, &overlapped)) return -1;
    state->mode = mode;
    return 0;
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
