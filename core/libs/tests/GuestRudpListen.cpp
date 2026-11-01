#include "prx/libSceRudp/include/Api.hpp"
#include "SceTypes.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int NotInitialized = static_cast<int>(0x80770001u);
constexpr int InvalidContext = static_cast<int>(0x80770003u);
constexpr int InvalidArgument = static_cast<int>(0x80770004u);
constexpr int InvalidMuxMode = static_cast<int>(0x80770006u);
constexpr int NotAcceptable = static_cast<int>(0x80770011u);
constexpr int Cancelled = static_cast<int>(0x80770014u);
constexpr int InvalidVport = static_cast<int>(0x80770015u);
constexpr int WouldBlock = static_cast<int>(0x80770016u);
constexpr int VportInUse = static_cast<int>(0x80770017u);
constexpr int AlreadyBound = static_cast<int>(0x8077001Du);
constexpr int InProgress = static_cast<int>(0x80770021u);
constexpr int NoEventHandler = static_cast<int>(0x80770022u);
constexpr int NonBlockOption = 7;

using ListenFn = int (APS5_VABI*)(int, int);
using AcceptFn = int (APS5_VABI*)(int, GuestRudpEventHandler, void*, int*, void*, std::uint32_t*, std::uint16_t*);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceRudp.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceRudp\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Rudp listen: %s\n", message);
    std::abort();
}

void APS5_VABI Handler(int, int, int, void*) {}

int NewContext() {
    int context = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &context) == 0 && context >= 0, "sceRudpCreateContext failed");
    return context;
}

int Bound(int socket, std::uint16_t vport, std::uint8_t muxMode) {
    const int context = NewContext();
    Require(sceRudpBind(context, socket, vport, muxMode) == 0, "a bind expected to succeed failed");
    return context;
}

int BindResult(int socket, std::uint16_t vport, std::uint8_t muxMode) {
    const int context = NewContext();
    const int result = sceRudpBind(context, socket, vport, muxMode);
    Require(sceRudpTerminate(context) == 0, "terminating a probe context failed");
    return result;
}

void Terminate(std::initializer_list<int> contexts) {
    for (const int context : contexts) {
        Require(sceRudpTerminate(context) == 0, "sceRudpTerminate failed");
    }
}

void CheckSocketRecords(ListenFn listen) {
    Require(BindResult(12, 0, 1) == InvalidVport && BindResult(12, 0, 2) == 0, "a failed bind left a record on its socket");

    const int first = Bound(10, 0, 2);
    const int second = Bound(10, 0, 2);
    Require(BindResult(10, 0x2711, 2) == VportInUse && BindResult(10, 0x2712, 2) == VportInUse, "vport 0 did not hand out 0x2711 and then 0x2712");
    Terminate({first});
    const int reused = Bound(10, 0x2711, 2);
    const int third = Bound(10, 0, 2);
    Require(BindResult(10, 0x2713, 2) == VportInUse, "vport 0 did not go on to 0x2713 while the socket kept a context");
    const int manual = Bound(10, 0x2714, 2);
    const int skipping = Bound(10, 0, 2);
    Require(BindResult(10, 0x2715, 2) == VportInUse, "vport 0 did not skip a vport bound by hand");
    Terminate({second, reused, third, manual, skipping});

    const int p2p = Bound(10, 1000, 1);
    const int sameVport = Bound(10, 1000, 1);
    Require(BindResult(10, 0, 2) == InvalidMuxMode, "mux mode 2 was taken on a socket with mux mode 1 contexts");
    Terminate({p2p, sameVport});
    const int restarted = Bound(10, 0, 2);
    Require(BindResult(10, 0x2711, 2) == VportInUse, "vport 0 did not start again at 0x2711 after the socket's last context left");
    Terminate({restarted});

    const int listener = Bound(11, 0, 2);
    Require(listen(listener, 1) == 0, "listen failed");
    Terminate({listener});
    Require(sceRudpTerminate(listener) == InvalidContext, "a terminated listening context was still known");
    Require(BindResult(11, 0x2711, 2) == VportInUse, "a listening context's vport was freed before sceRudpProcessEvents");
    Require(BindResult(11, 1000, 1) == InvalidMuxMode, "a listening context's socket was freed before sceRudpProcessEvents");
    Require(sceRudpProcessEvents(0) == 0, "sceRudpProcessEvents failed");
    Require(BindResult(11, 1000, 1) == 0 && BindResult(11, 0x2711, 2) == 0, "sceRudpProcessEvents did not free a terminated listening context's socket");
}

}

int main() {
    const auto listen = Resolve<ListenFn>("sceRudpListen");
    const auto accept = Resolve<AcceptFn>("sceRudpAccept");
    alignas(16) static std::array<unsigned char, 0x10000> pool{};
    int accepted = -1;

    Require(sceRudpSetEventHandler(Handler, nullptr) == NotInitialized, "a handler was set before sceRudpInit");
    Require(sceRudpBind(0, 5, 0, 2) == NotInitialized && listen(0, 1) == NotInitialized, "bind or listen ran before sceRudpInit");
    Require(accept(0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == NotInitialized, "accept ran before sceRudpInit");

    Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0, "sceRudpInit failed");
    Require(sceRudpSetEventHandler(nullptr, nullptr) == NoEventHandler, "a null handler was taken");

    const int server = NewContext();
    Require(sceRudpBind(-1, 5, 0, 2) == InvalidArgument && sceRudpBind(server, -1, 0, 2) == InvalidArgument, "a negative id or socket was taken");
    Require(sceRudpBind(12345, 5, 0, 0) == InvalidMuxMode && sceRudpBind(server, 5, 0, 3) == InvalidMuxMode, "a mux mode other than 1 or 2 was taken");
    Require(sceRudpBind(12345, 5, 0, 2) == InvalidContext, "an unknown context was bound");
    Require(sceRudpBind(server, 5, 0, 2) == 0, "binding with mux mode 2 and vport 0 failed");
    Require(sceRudpBind(server, 5, 0x3000, 2) == AlreadyBound, "a second bind was taken");
    Require(sceRudpBind(server, 5, 0x2711, 2) == AlreadyBound && sceRudpBind(server, 5, 0x3000, 1) == AlreadyBound && sceRudpBind(server, 6, 0, 1) == AlreadyBound, "a bound context reached the socket checks");
    Require(sceRudpBind(server, 5, 0, 3) == InvalidMuxMode, "a bound context took mux mode 3");

    const int second = NewContext();
    Require(sceRudpBind(second, 5, 0x2711, 2) == VportInUse, "the first automatic vport was not 0x2711");
    Require(sceRudpBind(second, 5, 0x3000, 1) == InvalidMuxMode, "another mux mode on the same socket was taken");
    Require(sceRudpBind(second, 5, 0, 2) == 0, "a second automatic vport failed");
    const int third = NewContext();
    Require(sceRudpBind(third, 5, 0, 1) == InvalidMuxMode, "mux mode 1 with vport 0 on a mux mode 2 socket did not fail the mux mode check");
    Require(sceRudpBind(third, 5, 0x2712, 2) == VportInUse, "the second automatic vport was not 0x2712");
    Require(sceRudpBind(third, 6, 0, 1) == InvalidVport, "vport 0 with mux mode 1 was taken");
    Require(sceRudpBind(third, 7, 0, 2) == 0, "an automatic vport on another socket failed");
    const int fourth = NewContext();
    Require(sceRudpBind(fourth, 7, 0x2711, 2) == VportInUse, "another socket did not start again at 0x2711");
    Require(sceRudpBind(fourth, 8, 0x100, 1) == 0, "binding with mux mode 1 failed");

    Require(listen(-1, 1) == InvalidArgument && listen(server, 0) == InvalidArgument, "a negative id or backlog was taken");
    Require(listen(server, 1) == NoEventHandler && listen(12345, 1) == NoEventHandler, "listen ran without an event handler");
    Require(sceRudpSetEventHandler(Handler, nullptr) == 0, "setting the event handler failed");
    Require(listen(12345, 1) == InvalidContext, "listen took an unknown context");
    Require(listen(fourth, 1) == NotAcceptable, "listen took a mux mode 1 context");
    const int unbound = NewContext();
    Require(listen(unbound, 1) == NotAcceptable, "listen took an unbound context");

    Require(accept(-1, nullptr, nullptr, &accepted, nullptr, nullptr, nullptr) == InvalidArgument, "accept took a negative id");
    Require(accept(server, Handler, nullptr, nullptr, nullptr, nullptr, nullptr) == InvalidArgument, "accept took a null new context id");
    Require(accept(12345, Handler, nullptr, &accepted, nullptr, nullptr, nullptr) == InvalidContext, "accept took an unknown context");
    Require(accept(server, Handler, nullptr, &accepted, nullptr, nullptr, nullptr) == NotAcceptable, "accept took a context that is not listening");

    Require(listen(server, 0x1000) == 0, "listen failed on a bound mux mode 2 context");
    Require(listen(server, 1) == InProgress, "a second listen was taken");
    Require(sceRudpBind(server, 5, 0x3000, 2) == NotAcceptable, "a listening context did not answer NOT_ACCEPTABLE to a bind");
    Require(accept(server, Handler, nullptr, &accepted, nullptr, nullptr, nullptr) == WouldBlock && accepted == -1, "a non-blocking accept with nothing pending did not return WOULDBLOCK");

    const std::uint32_t blocking = 0;
    Require(sceRudpSetOption(server, NonBlockOption, &blocking, sizeof(blocking)) == 0, "clearing the non-blocking option failed");
    int blockedResult = 0;
    std::thread waiter([&] { blockedResult = accept(server, Handler, nullptr, &accepted, nullptr, nullptr, nullptr); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(sceRudpTerminate(server) == 0, "terminating the listening context failed");
    waiter.join();
    Require(blockedResult == Cancelled && accepted == -1, "a blocking accept was not cancelled by sceRudpTerminate");
    CheckSocketRecords(listen);

    Require(sceRudpEnd() == 0, "sceRudpEnd failed");
    Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0, "sceRudpInit after sceRudpEnd failed");
    const int fresh = NewContext();
    Require(sceRudpBind(fresh, 5, 0, 2) == 0, "binding after sceRudpEnd failed");
    Require(listen(fresh, 1) == NoEventHandler, "the event handler survived sceRudpEnd");
    Require(sceRudpEnd() == 0, "the last sceRudpEnd failed");

    std::puts("Rudp listen tests passed");
    return 0;
}
