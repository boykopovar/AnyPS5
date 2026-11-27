#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <source_location>
#include <stdexcept>
#include <thread>

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

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial, std::size_t size) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

void TestNull() {
    errno = EDOM;
    RequireInvalidArgument([] { _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(nullptr, true); });
    Require(errno == EDOM);
}

void TestUnwritable() {
    errno = EDOM;
    RequireInvalidArgument([] {
        _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(
            reinterpret_cast<WebRtcSequenceCheckerImpl*>(std::uintptr_t{1}), true);
    });
    Require(errno == EDOM);

    const WebRtcSequenceCheckerImpl sentinel = Filled();
    const ReadOnlyPage page(&sentinel, sizeof(sentinel));
    errno = EDOM;
    RequireInvalidArgument([&] {
        _ZN6webrtc32webrtc_sequence_checker_internal19SequenceCheckerImplC2Eb_nid_postfix(
            page.As<WebRtcSequenceCheckerImpl>(), true);
    });
    Require(errno == EDOM);
    Require(std::memcmp(page.As<void>(), &sentinel, sizeof(sentinel)) == 0);
}

}

int main() {
    TestAttached();
    TestDetached();
    TestCurrentQueue();
    TestConstructingThread();
    TestLock();
    TestNull();
    TestUnwritable();
    std::puts("webrtc::webrtc_sequence_checker_internal::SequenceCheckerImpl tests passed");
    return 0;
}
