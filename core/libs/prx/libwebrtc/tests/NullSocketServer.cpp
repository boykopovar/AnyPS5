#include "prx/libwebrtc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
constexpr std::int64_t Forever = std::numeric_limits<std::int64_t>::max();
constexpr std::size_t DestructorSlot = 0;
constexpr std::size_t DeletingDestructorSlot = 1;
constexpr std::size_t CreateSocketSlot = 2;
constexpr std::size_t SetMessageQueueSlot = 3;
constexpr std::size_t WaitSlot = 4;
constexpr std::size_t WakeUpSlot = 5;

using DestructorCall = void(APS5_VABI*)(WebRtcNullSocketServer*);
using CreateSocketCall = void*(APS5_VABI*)(WebRtcNullSocketServer*, int, int);
using SetMessageQueueCall = void(APS5_VABI*)(WebRtcNullSocketServer*, void*);
using WaitCall = bool(APS5_VABI*)(WebRtcNullSocketServer*, std::int64_t, bool);
using WakeUpCall = void(APS5_VABI*)(WebRtcNullSocketServer*);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc NullSocketServer check failed at line %u\n", location.line());
    std::abort();
}

void* lastFreed;
void* APS5_VABI NullSocketServerAllocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI NullSocketServerFree_nid_no_patch(void* pointer) {
    lastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI NullSocketServerCalloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI NullSocketServerReallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI NullSocketServerAlign_nid_no_patch(std::size_t, std::size_t size) { return NullSocketServerAllocate_nid_no_patch(size); }
void* APS5_VABI NullSocketServerRealign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return NullSocketServerReallocate_nid_no_patch(pointer, size); }
int APS5_VABI NullSocketServerPosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = NullSocketServerAlign_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

template<typename T>
T Slot(const WebRtcNullSocketServer& server, std::size_t index) {
    return reinterpret_cast<T>(server.vtable[index]);
}

bool CallWait(WebRtcNullSocketServer& server, std::int64_t microseconds) {
    return Slot<WaitCall>(server, WaitSlot)(&server, microseconds, true);
}

void CallWakeUp(WebRtcNullSocketServer& server) {
    Slot<WakeUpCall>(server, WakeUpSlot)(&server);
}

std::chrono::milliseconds TimedWait(WebRtcNullSocketServer& server, std::int64_t microseconds) {
    const auto start = std::chrono::steady_clock::now();
    Require(CallWait(server, microseconds));
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
}

bool HeldByAnotherThread(WebRtcNullSocketServer& server) {
    int result = -1;
    std::thread probe([&] {
        result = pthread_mutex_trylock_nid_postfix(&server.event.eventMutex);
        if (result == 0)
            pthread_mutex_unlock_nid_postfix(&server.event.eventMutex);
    });
    probe.join();
    Require(result == 0 || result == Ebusy);
    return result == Ebusy;
}

template<typename F>
bool ThrowsInvalidArgument(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestLayout() {
    struct Guarded {
        std::array<unsigned char, 16> before;
        WebRtcNullSocketServer object;
        std::array<unsigned char, 16> after;
    } guarded;
    std::memset(&guarded, 0x5a, sizeof(guarded));
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&guarded.object);
    auto& server = guarded.object;
    Require(server.vtable != nullptr);
    Require(server.vtable[-2] == nullptr);
    Require(server.networkBinder == nullptr);
    Require(server.event.eventMutex != nullptr);
    Require(server.event.eventCond != nullptr);
    Require(!server.event.isManualReset);
    Require(!server.event.eventStatus);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&server);
    for (std::size_t offset = 34; offset < sizeof(server); ++offset) Require(bytes[offset] == 0x5a);
    Require(!HeldByAnotherThread(server));
    Require(Slot<CreateSocketCall>(server, CreateSocketSlot)(&server, 2, 1) == nullptr);
    Require(Slot<CreateSocketCall>(server, CreateSocketSlot)(&server, 10, 2) == nullptr);
    WebRtcNullSocketServer snapshot;
    std::memcpy(&snapshot, &server, sizeof(server));
    Slot<SetMessageQueueCall>(server, SetMessageQueueSlot)(&server, &guarded);
    Slot<SetMessageQueueCall>(server, SetMessageQueueSlot)(&server, nullptr);
    Require(std::memcmp(&snapshot, &server, sizeof(server)) == 0);
    Slot<DestructorCall>(server, DestructorSlot)(&server);
    for (auto value : guarded.before) Require(value == 0x5a);
    for (auto value : guarded.after) Require(value == 0x5a);
}

void TestZeroWait() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    Require(TimedWait(server, 0) < std::chrono::seconds(2));
    Require(!server.event.eventStatus);
    Require(TimedWait(server, -1000) < std::chrono::seconds(2));
    Require(!HeldByAnotherThread(server));
    Slot<DestructorCall>(server, DestructorSlot)(&server);
}

void TestWakeUpBeforeWait() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    CallWakeUp(server);
    Require(server.event.eventStatus);
    Require(!HeldByAnotherThread(server));
    Require(TimedWait(server, Forever) < std::chrono::seconds(2));
    Require(!server.event.eventStatus);
    Require(TimedWait(server, 200000) >= std::chrono::milliseconds(150));
    Require(!server.event.eventStatus);
    CallWakeUp(server);
    Require(TimedWait(server, 0) < std::chrono::seconds(2));
    Require(!server.event.eventStatus);
    Slot<DestructorCall>(server, DestructorSlot)(&server);
}

void TestTimeout() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    const auto elapsed = TimedWait(server, 300000);
    Require(elapsed >= std::chrono::milliseconds(250));
    Require(elapsed < std::chrono::seconds(10));
    Require(!server.event.eventStatus);
    Require(!HeldByAnotherThread(server));
    Slot<DestructorCall>(server, DestructorSlot)(&server);
}

void TestWakeUpFromAnotherThread() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CallWakeUp(server);
    });
    const auto elapsed = TimedWait(server, Forever);
    waker.join();
    Require(elapsed >= std::chrono::milliseconds(50));
    Require(elapsed < std::chrono::seconds(10));
    Require(!server.event.eventStatus);
    Require(!HeldByAnotherThread(server));
    Slot<DestructorCall>(server, DestructorSlot)(&server);
}

void TestDeletingDestructor() {
    auto* server = static_cast<WebRtcNullSocketServer*>(_Znwm_nid_postfix(sizeof(WebRtcNullSocketServer)));
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(server);
    lastFreed = nullptr;
    Slot<DestructorCall>(*server, DeletingDestructorSlot)(server);
    Require(lastFreed == server);
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
    WebRtcNullSocketServer* Server() const {
        return static_cast<WebRtcNullSocketServer*>(address_);
    }

private:
    void* address_ = nullptr;
};

void TestInvalidObjects() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    Require(ThrowsInvalidArgument([] { _ZN3rtc16NullSocketServerC1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<DestructorCall>(server, DestructorSlot)(nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<DestructorCall>(server, DeletingDestructorSlot)(nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<WaitCall>(server, WaitSlot)(nullptr, 0, true); }));
    Require(ThrowsInvalidArgument([&] { Slot<WakeUpCall>(server, WakeUpSlot)(nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<CreateSocketCall>(server, CreateSocketSlot)(nullptr, 2, 1); }));
    Require(ThrowsInvalidArgument([&] { Slot<SetMessageQueueCall>(server, SetMessageQueueSlot)(nullptr, nullptr); }));
    GuardedPage guard;
    Require(ThrowsInvalidArgument([&] { _ZN3rtc16NullSocketServerC1Ev_nid_postfix(guard.Server()); }));
    Require(ThrowsInvalidArgument([&] { Slot<DestructorCall>(server, DestructorSlot)(guard.Server()); }));
    Slot<DestructorCall>(server, DestructorSlot)(&server);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&NullSocketServerAllocate_nid_no_patch), reinterpret_cast<void*>(&NullSocketServerFree_nid_no_patch),
        reinterpret_cast<void*>(&NullSocketServerCalloc_nid_no_patch), reinterpret_cast<void*>(&NullSocketServerReallocate_nid_no_patch),
        reinterpret_cast<void*>(&NullSocketServerAlign_nid_no_patch), reinterpret_cast<void*>(&NullSocketServerRealign_nid_no_patch),
        reinterpret_cast<void*>(&NullSocketServerPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        TestLayout();
        TestZeroWait();
        TestWakeUpBeforeWait();
        TestTimeout();
        TestWakeUpFromAnotherThread();
        TestDeletingDestructor();
        TestInvalidObjects();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NullSocketServer exception: %s\n", error.what());
        return 1;
    }
    std::puts("rtc::NullSocketServer tests passed");
    return 0;
}
