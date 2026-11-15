#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <string>
#include <tuple>
#include <utility>
#include <windows.h>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static std::string Path(const char* suffix) {
    const auto native = std::filesystem::temp_directory_path() /
        ("anyps5-flock-queued-upgrade-" + std::to_string(::GetCurrentProcessId()) + "-" + suffix + ".bin");
    return native.string();
}

static int Create(const std::string& path) {
    return open_nid_postfix(path.c_str(), 0x602, 0644);
}

static int Open(const std::string& path) {
    return open_nid_postfix(path.c_str(), 2, 0);
}

static void Close(int descriptor) {
    Require(close_nid_postfix(descriptor) == 0);
}

static void Remove(const std::string& path) {
    Require(std::filesystem::remove(path));
}

static constexpr DWORD GateByteLow = 0xFFFFFFFD;
static constexpr DWORD WaiterByteLow = 0xFFFFFFFC;
static constexpr DWORD LockByteHigh = 0x7FFFFFFF;

static HANDLE RawOpen(const std::string& path) {
    const HANDLE handle = ::CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    return handle;
}

static bool RawLock(HANDLE handle, DWORD low, DWORD flags) {
    OVERLAPPED overlapped{};
    overlapped.Offset = low;
    overlapped.OffsetHigh = LockByteHigh;
    return ::LockFileEx(handle, flags, 0, 1, 0, &overlapped) != 0;
}

static void RawUnlock(HANDLE handle, DWORD low) {
    OVERLAPPED overlapped{};
    overlapped.Offset = low;
    overlapped.OffsetHigh = LockByteHigh;
    Require(::UnlockFileEx(handle, 0, 1, 0, &overlapped) != 0);
}

static void WaitUntilQueued(HANDLE probe) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!RawLock(probe, WaiterByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY)) {
            Require(::GetLastError() == ERROR_LOCK_VIOLATION);
            return;
        }
        RawUnlock(probe, WaiterByteLow);
        ::Sleep(1);
    }
    std::abort();
}

static std::pair<int, int> NonblockingUpgrade(int descriptor) {
    const int result = flock_nid_postfix(descriptor, 2 | 4);
    return {result, *__error_nid_postfix()};
}

static void SoleHolderPriority() {
    const std::string path = Path("sole");
    const int holder = Create(path);
    const int waiting = Open(path);
    Require(holder >= 0 && waiting >= 0);
    Require(flock_nid_postfix(holder, 1 | 4) == 0);
    auto waiter = std::async(std::launch::async, [&] { return flock_nid_postfix(waiting, 2); });
    const HANDLE queueProbe = RawOpen(path);
    WaitUntilQueued(queueProbe);
    ::CloseHandle(queueProbe);
    auto upgrade = std::async(std::launch::async, [&] { return flock_nid_postfix(holder, 2); });
    Require(upgrade.wait_for(std::chrono::seconds(2)) == std::future_status::ready && upgrade.get() == 0);
    Require(waiter.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    Require(flock_nid_postfix(holder, 8) == 0);
    Require(waiter.wait_for(std::chrono::seconds(2)) == std::future_status::ready && waiter.get() == 0);
    Require(flock_nid_postfix(waiting, 8) == 0);
    Close(waiting);
    Close(holder);
    Remove(path);
}

static void NonblockingProtocolContention() {
    for (const auto [suffix, byte, flags] : {
             std::tuple{"stalled-gate", GateByteLow, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY},
             std::tuple{"stalled-waiter", WaiterByteLow, LOCKFILE_FAIL_IMMEDIATELY},
         }) {
        const std::string path = Path(suffix);
        const int holder = Create(path);
        Require(holder >= 0);
        Require(flock_nid_postfix(holder, 1 | 4) == 0);
        const HANDLE stalled = RawOpen(path);
        Require(RawLock(stalled, byte, flags));
        auto upgrade = std::async(std::launch::async, [&] { return NonblockingUpgrade(holder); });
        const bool bounded = upgrade.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready;
        RawUnlock(stalled, byte);
        ::CloseHandle(stalled);
        Require(upgrade.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        const auto [result, error] = upgrade.get();
        Require(bounded && result == -1 && error == 35);
        Require(flock_nid_postfix(holder, 8) == 0);
        Close(holder);
        Remove(path);
    }
}

static void NonblockingUpgradeWithPeer() {
    const std::string path = Path("nonblocking-peer");
    const int first = Create(path);
    const int second = Open(path);
    const int waiting = Open(path);
    Require(first >= 0 && second >= 0 && waiting >= 0);
    Require(flock_nid_postfix(first, 1 | 4) == 0);
    Require(flock_nid_postfix(second, 1 | 4) == 0);
    auto waiter = std::async(std::launch::async, [&] { return flock_nid_postfix(waiting, 2); });
    Require(waiter.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    auto upgrade = std::async(std::launch::async, [&] {
        const int result = flock_nid_postfix(first, 2 | 4);
        return std::pair{result, *__error_nid_postfix()};
    });
    Require(upgrade.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    const auto [result, error] = upgrade.get();
    Require(result == -1 && error == 35);
    Require(flock_nid_postfix(second, 8) == 0);
    Require(waiter.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    Require(flock_nid_postfix(first, 8) == 0);
    Require(waiter.wait_for(std::chrono::seconds(2)) == std::future_status::ready && waiter.get() == 0);
    Require(flock_nid_postfix(waiting, 8) == 0);
    Close(waiting);
    Close(second);
    Close(first);
    Remove(path);
}

static void BlockingUpgradersWithWaiter() {
    const std::string path = Path("blocking-peers");
    const int first = Create(path);
    const int second = Open(path);
    const int waiting = Open(path);
    const int probe = Open(path);
    Require(first >= 0 && second >= 0 && waiting >= 0 && probe >= 0);
    Require(flock_nid_postfix(first, 1 | 4) == 0);
    Require(flock_nid_postfix(second, 1 | 4) == 0);
    auto waiter = std::async(std::launch::async, [&] { return flock_nid_postfix(waiting, 2); });
    Require(waiter.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    auto firstUpgrade = std::async(std::launch::async, [&] { return flock_nid_postfix(first, 2); });
    Require(firstUpgrade.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    auto secondUpgrade = std::async(std::launch::async, [&] { return flock_nid_postfix(second, 2); });
    std::future<int>* futures[] = {&firstUpgrade, &secondUpgrade, &waiter};
    const int descriptors[] = {first, second, waiting};
    bool consumed[] = {false, false, false};
    for (int turn = 0; turn != 3; ++turn) {
        int acquired = -1;
        for (int attempt = 0; attempt != 2000 && acquired < 0; ++attempt) {
            for (int index = 0; index != 3; ++index) {
                if (!consumed[index] && futures[index]->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    acquired = index;
                    break;
                }
            }
            if (acquired < 0) ::Sleep(1);
        }
        Require(acquired >= 0);
        Require(futures[acquired]->get() == 0);
        consumed[acquired] = true;
        Require(flock_nid_postfix(probe, 1 | 4) == -1 && *__error_nid_postfix() == 35);
        Require(flock_nid_postfix(probe, 2 | 4) == -1 && *__error_nid_postfix() == 35);
        Require(flock_nid_postfix(descriptors[acquired], 8) == 0);
    }
    Close(probe);
    Close(waiting);
    Close(second);
    Close(first);
    Remove(path);
}

int main() {
    SoleHolderPriority();
    NonblockingProtocolContention();
    NonblockingUpgradeWithPeer();
    BlockingUpgradersWithWaiter();
}
