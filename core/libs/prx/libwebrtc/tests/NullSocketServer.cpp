#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition)
        return;
    std::fprintf(stderr, "libwebrtc NullSocketServer check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    allocations.emplace(pointer, size);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

struct GuardedPage {
    void* address = nullptr;
    std::size_t size = 4096;

    GuardedPage() {
#ifdef _WIN32
        address = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
#else
        void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        address = mapped == MAP_FAILED ? nullptr : mapped;
#endif
        Require(address != nullptr);
    }

    ~GuardedPage() {
        if (!address)
            return;
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, size);
#endif
    }
};

using DestroyFn = void(APS5_VABI*)(WebRtcNullSocketServer*);
using DeleteFn = void(APS5_VABI*)(WebRtcNullSocketServer*);
using CreateSocketFn = void*(APS5_VABI*)(WebRtcNullSocketServer*, int, int);
using SetMessageQueueFn = void(APS5_VABI*)(WebRtcNullSocketServer*, void*);
using WaitFn = bool(APS5_VABI*)(WebRtcNullSocketServer*, std::int64_t, bool);
using WakeUpFn = void(APS5_VABI*)(WebRtcNullSocketServer*);

DestroyFn SlotDestroy(const WebRtcNullSocketServer& server) {
    return reinterpret_cast<DestroyFn>(server.vtable[0]);
}

DeleteFn SlotDelete(const WebRtcNullSocketServer& server) {
    return reinterpret_cast<DeleteFn>(server.vtable[1]);
}

CreateSocketFn SlotCreateSocket(const WebRtcNullSocketServer& server) {
    return reinterpret_cast<CreateSocketFn>(server.vtable[2]);
}

SetMessageQueueFn SlotSetMessageQueue(const WebRtcNullSocketServer& server) {
    return reinterpret_cast<SetMessageQueueFn>(server.vtable[3]);
}

WaitFn SlotWait(const WebRtcNullSocketServer& server) {
    return reinterpret_cast<WaitFn>(server.vtable[4]);
}

WakeUpFn SlotWakeUp(const WebRtcNullSocketServer& server) {
    return reinterpret_cast<WakeUpFn>(server.vtable[5]);
}

void TestLifecycleAndSlots() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    Require(server.vtable != nullptr);
    Require(server.networkBinder == nullptr);
    Require(server.eventMutex != nullptr);
    Require(server.eventCond != nullptr);
    Require(!server.isManualReset);
    Require(!server.eventStatus);

    Require(SlotCreateSocket(server)(&server, 2, 1) == nullptr);
    SlotSetMessageQueue(server)(&server, nullptr);
    Require(SlotWait(server)(&server, 0, true));

    SlotDestroy(server)(&server);
    Require(server.eventMutex == nullptr);
    Require(server.eventCond == nullptr);
}

void TestDeletingDestructor() {
    auto* server = static_cast<WebRtcNullSocketServer*>(_Znwm_nid_postfix(sizeof(WebRtcNullSocketServer)));
    Require(allocations.size() == 1);
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(server);
    SlotDelete(*server)(server);
    Require(allocations.empty());
}

void TestWakeUpBeforeWaitAndThreadedWakeUp() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);

    SlotWakeUp(server)(&server);
    Require(server.eventStatus);
    Require(SlotWait(server)(&server, 0, false));
    Require(!server.eventStatus);

    const auto start = std::chrono::steady_clock::now();
    Require(SlotWait(server)(&server, 5000, true));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(1));

    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        SlotWakeUp(server)(&server);
    });
    Require(SlotWait(server)(&server, std::numeric_limits<std::int64_t>::max(), true));
    waker.join();
    Require(!server.eventStatus);

    SlotDestroy(server)(&server);
}

void TestInvalidArguments() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    auto destroy = SlotDestroy(server);
    auto del = SlotDelete(server);
    auto createSocket = SlotCreateSocket(server);
    auto setMessageQueue = SlotSetMessageQueue(server);
    auto wait = SlotWait(server);
    auto wakeUp = SlotWakeUp(server);
    destroy(&server);

    RequireInvalidArgument([&] { _ZN3rtc16NullSocketServerC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { destroy(nullptr); });
    RequireInvalidArgument([&] { del(nullptr); });
    RequireInvalidArgument([&] { createSocket(nullptr, 2, 1); });
    RequireInvalidArgument([&] { setMessageQueue(nullptr, nullptr); });
    RequireInvalidArgument([&] { wait(nullptr, 0, false); });
    RequireInvalidArgument([&] { wakeUp(nullptr); });

    GuardedPage readOnly;
    auto* guardedServer = static_cast<WebRtcNullSocketServer*>(readOnly.address);
    RequireInvalidArgument([&] { _ZN3rtc16NullSocketServerC1Ev_nid_postfix(guardedServer); });
    RequireInvalidArgument([&] { destroy(guardedServer); });
    RequireInvalidArgument([&] { del(guardedServer); });
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestLifecycleAndSlots();
    TestDeletingDestructor();
    TestWakeUpBeforeWaitAndThreadedWakeUp();
    TestInvalidArguments();
    Require(allocations.empty());
    std::puts("libwebrtc NullSocketServer tests passed");
}
