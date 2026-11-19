#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc TaskQueueBase check failed at line %u\n", location.line());
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

WebRtcTaskQueueBase* Queue(int index) {
    static char storage[3];
    return reinterpret_cast<WebRtcTaskQueueBase*>(&storage[index]);
}

WebRtcTaskQueueBase* Current() {
    return _ZN6webrtc13TaskQueueBase7CurrentEv_nid_postfix();
}

void Set(WebRtcCurrentTaskQueueSetter& setter, WebRtcTaskQueueBase* queue) {
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(&setter, queue);
}

void Restore(WebRtcCurrentTaskQueueSetter& setter) {
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(&setter);
}

void TestNoQueue() {
    Require(Current() == nullptr);
}

void TestSetAndRestore() {
    WebRtcCurrentTaskQueueSetter setter {Queue(2)};
    Set(setter, Queue(0));
    Require(setter.previous == nullptr);
    Require(Current() == Queue(0));
    Restore(setter);
    Require(Current() == nullptr);
}

void TestNested() {
    WebRtcCurrentTaskQueueSetter outer {};
    WebRtcCurrentTaskQueueSetter inner {};
    WebRtcCurrentTaskQueueSetter cleared {};
    Set(outer, Queue(0));
    Set(inner, Queue(1));
    Require(inner.previous == Queue(0));
    Require(Current() == Queue(1));
    Set(cleared, nullptr);
    Require(cleared.previous == Queue(1));
    Require(Current() == nullptr);
    Restore(cleared);
    Require(Current() == Queue(1));
    Restore(inner);
    Require(Current() == Queue(0));
    Restore(outer);
    Require(Current() == nullptr);
}

void TestRestoreUsesTheSetter() {
    WebRtcCurrentTaskQueueSetter setter {};
    Set(setter, Queue(0));
    setter.previous = Queue(2);
    Restore(setter);
    Require(Current() == Queue(2));
    WebRtcCurrentTaskQueueSetter reset {nullptr};
    Restore(reset);
    Require(Current() == nullptr);
}

void TestPerThread() {
    WebRtcCurrentTaskQueueSetter setter {};
    Set(setter, Queue(0));
    WebRtcTaskQueueBase* seenBefore = Queue(2);
    WebRtcTaskQueueBase* seenWhileSet = nullptr;
    WebRtcTaskQueueBase* seenAfter = Queue(2);
    std::thread worker([&] {
        seenBefore = Current();
        WebRtcCurrentTaskQueueSetter workerSetter {};
        Set(workerSetter, Queue(1));
        seenWhileSet = Current();
        Restore(workerSetter);
        seenAfter = Current();
    });
    worker.join();
    Require(seenBefore == nullptr);
    Require(seenWhileSet == Queue(1));
    Require(seenAfter == nullptr);
    Require(Current() == Queue(0));
    Restore(setter);
    Require(Current() == nullptr);
}

void TestNullSetter() {
    RequireInvalidArgument([] { _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(nullptr, Queue(0)); });
    Require(Current() == nullptr);
    RequireInvalidArgument([] { _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(nullptr); });
    Require(Current() == nullptr);
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

void TestGuardedSetter() {
    GuardedPage guard;
    RequireInvalidArgument([&] { _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(guard.As<WebRtcCurrentTaskQueueSetter>(), Queue(0)); });
    Require(Current() == nullptr);
    RequireInvalidArgument([&] { _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(guard.As<WebRtcCurrentTaskQueueSetter>()); });
    Require(Current() == nullptr);
}

}

int main() {
    TestNoQueue();
    TestSetAndRestore();
    TestNested();
    TestRestoreUsesTheSetter();
    TestPerThread();
    TestNullSetter();
    TestGuardedSetter();
    return 0;
}
