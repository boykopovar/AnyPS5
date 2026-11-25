#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>

static void Require(bool condition, const char* message = "File lock check failed") {
    if (!condition) throw std::runtime_error(message);
}

static void ExpectInvalidArgument(const std::function<void()>& func) {
    try {
        func();
        Require(false, "Expected std::invalid_argument was not thrown");
    } catch (const std::invalid_argument&) {
    }
}

static std::filesystem::path UniqueTempFile(const char* tag) {
    static std::atomic<unsigned> serial{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::path(
        "guest_libc_file_lock_" + std::string(tag) + "_" + std::to_string(stamp) + "_" + std::to_string(serial++) + ".tmp");
}

int main() {
    ExpectInvalidArgument([] { _Lockfilelock_nid_postfix(nullptr); });
    ExpectInvalidArgument([] { _Unlockfilelock_nid_postfix(nullptr); });

    const auto pathA = UniqueTempFile("a");
    const auto pathB = UniqueTempFile("b");
    const std::string pathAStr = pathA.string();
    const std::string pathBStr = pathB.string();

    auto* streamA = fopen_nid_postfix(pathAStr.c_str(), "w+b");
    Require(streamA != nullptr, "Failed to open streamA");
    auto* streamB = fopen_nid_postfix(pathBStr.c_str(), "w+b");
    Require(streamB != nullptr, "Failed to open streamB");

    _Lockfilelock_nid_postfix(streamA);
    _Lockfilelock_nid_postfix(streamA);
    Require(fputs_nid_postfix("recursive-owner", streamA) >= 0, "fputs while recursively locked failed");
    Require(fflush_nid_postfix(streamA) == 0, "fflush while recursively locked failed");
    _Unlockfilelock_nid_postfix(streamA);
    _Unlockfilelock_nid_postfix(streamA);

    _Lockfilelock_nid_postfix(streamA);
    _Lockfilelock_nid_postfix(streamA);
    _Unlockfilelock_nid_postfix(streamA);

    int tryLockResult = 0;
    std::thread probe([&] {
        tryLockResult = ftrylockfile_nid_postfix(streamA);
        if (tryLockResult == 0) {
            _Unlockfilelock_nid_postfix(streamA);
        }
    });
    probe.join();
    Require(tryLockResult == -1, "Worker acquired lock while owner still held one recursive lock");

    std::atomic<bool> workerAcquired{false};
    std::thread worker([&] {
        _Lockfilelock_nid_postfix(streamA);
        workerAcquired.store(true, std::memory_order_release);
        _Unlockfilelock_nid_postfix(streamA);
    });

    std::atomic<bool> streamBResult{false};
    std::thread streamBThread([&] {
        _Lockfilelock_nid_postfix(streamB);
        Require(fputs_nid_postfix("stream-b", streamB) >= 0, "fputs on streamB failed");
        Require(fflush_nid_postfix(streamB) == 0, "fflush on streamB failed");
        _Unlockfilelock_nid_postfix(streamB);
        streamBResult.store(true, std::memory_order_release);
    });
    streamBThread.join();
    Require(streamBResult.load(std::memory_order_acquire), "streamB lock/unlock failed while streamA was held");
    Require(!workerAcquired.load(std::memory_order_acquire), "Worker acquired streamA before final unlock");

    _Unlockfilelock_nid_postfix(streamA);

    worker.join();
    Require(workerAcquired.load(std::memory_order_acquire), "Worker never acquired lock after final unlock");

    Require(fclose_nid_postfix(streamA) == 0, "fclose streamA failed");
    Require(fclose_nid_postfix(streamB) == 0, "fclose streamB failed");
    std::error_code ec;
    std::filesystem::remove(pathA, ec);
    std::filesystem::remove(pathB, ec);

    return 0;
}
