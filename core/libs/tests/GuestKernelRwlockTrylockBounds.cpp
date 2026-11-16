#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int SceKernelErrorEbusy = 0x80020010;
constexpr int SceKernelErrorEinval = 0x80020016;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
bool ThrowsRuntimeError(F&& fn) {
    try {
        fn();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void TestNullAndDestroyedRejection() {
    Require(ThrowsRuntimeError([] { pthread_rwlock_tryrdlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { pthread_rwlock_trywrlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { scePthreadRwlockTryrdlock(nullptr); }));
    Require(ThrowsRuntimeError([] { scePthreadRwlockTrywrlock(nullptr); }));

    PthreadRwlock destroyed = reinterpret_cast<PthreadRwlock>(std::uintptr_t{1});
    Require(pthread_rwlock_tryrdlock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&destroyed) == PosixEinval);
    Require(scePthreadRwlockTryrdlock(&destroyed) == SceKernelErrorEinval);
    Require(scePthreadRwlockTrywrlock(&destroyed) == SceKernelErrorEinval);

    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0 && rw != nullptr);
    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEinval);
    Require(scePthreadRwlockTryrdlock(&rw) == SceKernelErrorEinval);
    Require(scePthreadRwlockTrywrlock(&rw) == SceKernelErrorEinval);
}

void TestStaticAutoInitialization() {
    PthreadRwlock rw1 = nullptr;
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw1) == 0);
    Require(rw1 != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&rw1) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rw1) == 0);

    PthreadRwlock rw2 = nullptr;
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw2) == 0);
    Require(rw2 != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&rw2) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rw2) == 0);

    PthreadRwlock rw3 = nullptr;
    Require(scePthreadRwlockTryrdlock(&rw3) == 0);
    Require(rw3 != nullptr);
    Require(scePthreadRwlockUnlock(&rw3) == 0);
    Require(scePthreadRwlockDestroy(&rw3) == 0);

    PthreadRwlock rw4 = nullptr;
    Require(scePthreadRwlockTrywrlock(&rw4) == 0);
    Require(rw4 != nullptr);
    Require(scePthreadRwlockUnlock(&rw4) == 0);
    Require(scePthreadRwlockDestroy(&rw4) == 0);
}

void TestSingleThreadExclusivity() {
    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == PosixEbusy);
    Require(scePthreadRwlockTrywrlock(&rw) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTryrdlock(&rw) == SceKernelErrorEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == 0);
    Require(scePthreadRwlockTryrdlock(&rw) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(scePthreadRwlockTrywrlock(&rw) == SceKernelErrorEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(scePthreadRwlockUnlock(&rw) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

void TestCrossThreadContention() {
    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);

    std::atomic<bool> writerAcquired{false};
    std::atomic<bool> allowWriterRelease{false};
    std::atomic<bool> verifierDone{false};

    std::thread writerThread([&] {
        Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == 0);
        writerAcquired.store(true);
        while (!allowWriterRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    while (!writerAcquired.load()) {
        std::this_thread::yield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(scePthreadRwlockTryrdlock(&rw) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTrywrlock(&rw) == SceKernelErrorEbusy);

    allowWriterRelease.store(true);
    writerThread.join();

    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    std::atomic<bool> reader1Acquired{false};
    std::atomic<bool> reader2Acquired{false};
    std::atomic<bool> allowReaderRelease{false};

    std::thread reader1Thread([&] {
        Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == 0);
        reader1Acquired.store(true);
        while (!allowReaderRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    std::thread reader2Thread([&] {
        Require(scePthreadRwlockTryrdlock(&rw) == 0);
        reader2Acquired.store(true);
        while (!allowReaderRelease.load()) {
            std::this_thread::yield();
        }
        Require(scePthreadRwlockUnlock(&rw) == 0);
    });

    while (!reader1Acquired.load() || !reader2Acquired.load()) {
        std::this_thread::yield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(scePthreadRwlockTrywrlock(&rw) == SceKernelErrorEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    allowReaderRelease.store(true);
    reader1Thread.join();
    reader2Thread.join();

    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

void TestWaitingWriterTryrdlockInteraction() {
    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);

    std::atomic<bool> holdingReaderAcquired{false};
    std::atomic<bool> waitingWriterStarted{false};
    std::atomic<bool> allowUnwind{false};

    std::thread holdingReader([&] {
        Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == 0);
        holdingReaderAcquired.store(true);

        while (!waitingWriterStarted.load()) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == 0);
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

        while (!allowUnwind.load()) {
            std::this_thread::yield();
        }
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    while (!holdingReaderAcquired.load()) {
        std::this_thread::yield();
    }

    std::thread blockedWriter([&] {
        waitingWriterStarted.store(true);
        Require(pthread_rwlock_wrlock_nid_postfix(&rw) == 0);
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    while (!waitingWriterStarted.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rw) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == PosixEbusy);
    Require(scePthreadRwlockTryrdlock(&rw) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTrywrlock(&rw) == SceKernelErrorEbusy);

    allowUnwind.store(true);
    holdingReader.join();
    blockedWriter.join();

    Require(pthread_rwlock_trywrlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

} // namespace

int main() {
    TestNullAndDestroyedRejection();
    TestStaticAutoInitialization();
    TestSingleThreadExclusivity();
    TestCrossThreadContention();
    TestWaitingWriterTryrdlockInteraction();
    return 0;
}
