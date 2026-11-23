#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc Thread::Quit check failed at line %u\n", location.line());
    std::abort();
}

template<class TFunction>
void RequireThrows(TFunction&& function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

struct ThreadImage {
    std::array<std::uint8_t, 0x64> prefix;
    std::uint32_t stop;
    void* socketServer;
    std::array<std::uint8_t, 16> suffix;
};

static_assert(offsetof(ThreadImage, stop) == 0x64);
static_assert(offsetof(ThreadImage, socketServer) == 0x68);
static_assert(std::is_same_v<decltype(&_ZN3rtc6Thread4QuitEv_nid_postfix), void (APS5_VABI*)(void*)>);

class ISocketFactory {
public:
    virtual ~ISocketFactory() = default;
    virtual void* APS5_VABI CreateSocket(int, int) = 0;
};

class ISocketServer : public ISocketFactory {
public:
    virtual void APS5_VABI SetMessageQueue(void*) = 0;
    virtual bool APS5_VABI Wait(std::int64_t, bool) = 0;
    virtual void APS5_VABI WakeUp() = 0;
};

class ObservingServer final : public ISocketServer {
public:
    explicit ObservingServer(ThreadImage& thread) : thread(thread) {}
    void* APS5_VABI CreateSocket(int, int) override { Require(false); return nullptr; }
    void APS5_VABI SetMessageQueue(void*) override { Require(false); }
    bool APS5_VABI Wait(std::int64_t, bool) override { Require(false); return false; }
    void APS5_VABI WakeUp() override {
        Require(std::atomic_ref(thread.stop).load(std::memory_order_acquire) == 1);
        Require(_ZN3rtc6Thread10IsQuittingEv_nid_postfix(&thread));
        ++calls;
        if (fail) throw std::runtime_error("socket wake failure");
    }
    ThreadImage& thread;
    unsigned calls = 0;
    bool fail = false;
};

void TestVirtualDispatchAndOrdering() {
    ThreadImage thread{};
    thread.prefix.fill(0xa5);
    thread.suffix.fill(0x5a);
    ObservingServer server(thread);
    thread.socketServer = &server;
    for (const auto previous : {0u, 1u, 7u, std::numeric_limits<std::uint32_t>::max()}) {
        thread.stop = previous;
        const auto calls = server.calls;
        _ZN3rtc6Thread4QuitEv_nid_postfix(&thread);
        Require(thread.stop == 1 && server.calls == calls + 1);
        Require(thread.socketServer == &server);
        for (const auto byte : thread.prefix) Require(byte == 0xa5);
        for (const auto byte : thread.suffix) Require(byte == 0x5a);
    }
    server.fail = true;
    thread.stop = 0;
    RequireThrows([&] { _ZN3rtc6Thread4QuitEv_nid_postfix(&thread); });
    Require(thread.stop == 1 && server.calls == 5);
    server.fail = false;
    _ZN3rtc6Thread7RestartEv_nid_postfix(&thread);
    Require(!_ZN3rtc6Thread10IsQuittingEv_nid_postfix(&thread));
    _ZN3rtc6Thread4QuitEv_nid_postfix(&thread);
    Require(server.calls == 6);
}

void TestRealNullSocketServer() {
    WebRtcNullSocketServer server{};
    _ZN3rtc16NullSocketServerC1Ev_nid_postfix(&server);
    ThreadImage thread{};
    thread.socketServer = &server;
    using Wait = bool (APS5_VABI*)(WebRtcNullSocketServer*, std::int64_t, bool);
    const auto wait = reinterpret_cast<Wait>(server.vtable[4]);
    _ZN3rtc6Thread4QuitEv_nid_postfix(&thread);
    Require(server.event.eventStatus);
    Require(wait(&server, 0, false));
    Require(!server.event.eventStatus);
    _ZN3rtc6Thread7RestartEv_nid_postfix(&thread);
    std::atomic<bool> ready = false;
    std::atomic<bool> returned = false;
    std::thread worker([&] {
        ready.store(true, std::memory_order_release);
        Require(wait(&server, 2000000, false));
        returned.store(std::atomic_ref(thread.stop).load(std::memory_order_acquire) == 1,
                       std::memory_order_release);
    });
    while (!ready.load(std::memory_order_acquire)) std::this_thread::yield();
    _ZN3rtc6Thread4QuitEv_nid_postfix(&thread);
    worker.join();
    Require(returned.load(std::memory_order_acquire));
    Require(!server.event.eventStatus);
    using Destroy = void (APS5_VABI*)(WebRtcNullSocketServer*);
    reinterpret_cast<Destroy>(server.vtable[0])(&server);
}

void TestInvalidInputs() {
    RequireThrows([] { _ZN3rtc6Thread4QuitEv_nid_postfix(nullptr); });
    ThreadImage thread{};
    RequireThrows([&] { _ZN3rtc6Thread4QuitEv_nid_postfix(&thread); });
    Require(thread.stop == 1);
    void* vtable = nullptr;
    thread.socketServer = &vtable;
    RequireThrows([&] { _ZN3rtc6Thread4QuitEv_nid_postfix(&thread); });
    std::array<void*, 6> slots{};
    vtable = slots.data();
    RequireThrows([&] { _ZN3rtc6Thread4QuitEv_nid_postfix(&thread); });
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free),
        reinterpret_cast<void*>(&Calloc), reinterpret_cast<void*>(&Reallocate),
        reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    try {
        TestVirtualDispatchAndOrdering();
        TestRealNullSocketServer();
        TestInvalidInputs();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Thread::Quit exception: %s\n", error.what());
        return 1;
    }
    std::puts("libwebrtc Thread::Quit tests passed");
}
