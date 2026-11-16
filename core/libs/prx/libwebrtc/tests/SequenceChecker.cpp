#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

constexpr int Ebusy = 16;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SequenceCheckerImpl check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

WebRtcTaskQueueBase* Queue() {
    static char storage;
    return reinterpret_cast<WebRtcTaskQueueBase*>(&storage);
}

WebRtcSequenceCheckerImpl Filled() {
    WebRtcSequenceCheckerImpl checker;
    std::memset(&checker, 0xA5, sizeof(checker));
    return checker;
}

void Construct(WebRtcSequenceCheckerImpl& checker, bool attach) {
    _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(&checker, attach);
}

void Destroy(WebRtcSequenceCheckerImpl& checker) {
    Require(pthread_mutex_destroy_nid_postfix(&checker.lock) == 0);
}

int TryLockFromAnotherThread(WebRtcSequenceCheckerImpl& checker) {
    int result = -1;
    std::thread probe([&] {
        result = pthread_mutex_trylock_nid_postfix(&checker.lock);
        if (result == 0)
            pthread_mutex_unlock_nid_postfix(&checker.lock);
    });
    probe.join();
    return result;
}

void TestAttached() {
    WebRtcSequenceCheckerImpl checker = Filled();
    Construct(checker, true);
    Require(checker.attached);
    Require(pthread_equal_nid_postfix(checker.validThread, pthread_self_nid_postfix()) != 0);
    Require(checker.validQueue == nullptr);
    Destroy(checker);
}

void TestDetached() {
    WebRtcSequenceCheckerImpl checker = Filled();
    Construct(checker, false);
    Require(!checker.attached);
    Require(pthread_equal_nid_postfix(checker.validThread, pthread_self_nid_postfix()) != 0);
    Require(checker.validQueue == nullptr);
    Destroy(checker);
}

void TestCurrentQueue() {
    WebRtcCurrentTaskQueueSetter setter {};
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(&setter, Queue());
    WebRtcSequenceCheckerImpl checker = Filled();
    Construct(checker, true);
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(&setter);
    Require(checker.validQueue == Queue());
    Destroy(checker);
}

void TestConstructingThread() {
    WebRtcSequenceCheckerImpl checker = Filled();
    Pthread constructing = nullptr;
    std::thread worker([&] {
        Construct(checker, true);
        constructing = pthread_self_nid_postfix();
    });
    worker.join();
    Require(pthread_equal_nid_postfix(checker.validThread, constructing) != 0);
    Require(pthread_equal_nid_postfix(checker.validThread, pthread_self_nid_postfix()) == 0);
    Destroy(checker);
}

void TestLock() {
    WebRtcSequenceCheckerImpl checker = Filled();
    Construct(checker, true);
    Require(checker.lock != nullptr);
    Require(TryLockFromAnotherThread(checker) == 0);
    Require(pthread_mutex_lock_nid_postfix(&checker.lock) == 0);
    Require(TryLockFromAnotherThread(checker) == Ebusy);
    Require(pthread_mutex_unlock_nid_postfix(&checker.lock) == 0);
    Require(TryLockFromAnotherThread(checker) == 0);
    Destroy(checker);
}

void TestNull() {
    RequireInvalidArgument([] { _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(nullptr, true); });
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        address_ = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(address_ != nullptr);
#else
        void* mapped = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        address_ = mapped;
#endif
    }
    ~GuardedPage() {
        if (!address_)
            return;
#ifdef _WIN32
        VirtualFree(address_, 0, MEM_RELEASE);
#else
        munmap(address_, 4096);
#endif
    }
    template<typename T>
    T* As() const {
        return static_cast<T*>(address_);
    }

private:
    void* address_ = nullptr;
};

void TestGuarded() {
    GuardedPage guard;
    RequireInvalidArgument([&] { _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(guard.As<WebRtcSequenceCheckerImpl>(), true); });
}

}

int main() {
    TestAttached();
    TestDetached();
    TestCurrentQueue();
    TestConstructingThread();
    TestLock();
    TestNull();
    TestGuarded();
    std::puts("webrtc::webrtc_sequence_checker_internal::SequenceCheckerImpl tests passed");
    return 0;
}
