#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

static_assert(sizeof(GuestOnce) == 16 && offsetof(GuestOnce, mutex) == 8);
using Initializer = void (APS5_VABI *)();

static void Require(bool value) { if (!value) std::abort(); }
static GuestOnce control{}, nested{}, retry{};
static std::atomic<int> calls{0}, arrived{0};
static int published = 0, nestedValue = 0, attempts = 0;
static void APS5_VABI Nested() { nestedValue = 17; }
static void APS5_VABI Initialize() {
    ++calls;
    while (arrived.load() != 16) std::this_thread::yield();
    Require(pthread_once_nid_postfix(&nested, Nested) == 0);
    published = 42;
}
static void APS5_VABI Retry() {
    if (++attempts == 1) throw 7;
}
static GuestOnce exited{}, cancelled{};
static std::int32_t sceExited = 0, sceCancelled = 0;
static int retried = 0;
static void APS5_VABI ExitInsideInitializer() { scePthreadExit(nullptr); }
static void APS5_VABI CancelInsideInitializer() {
    Require(scePthreadCancel(scePthreadSelf()) == 0);
    scePthreadTestcancel();

}
static void APS5_VABI CountRetry() { ++retried; }
static void APS5_VABI AbortCleanup(void*) { std::abort(); }
static void APS5_VABI PushAndThrow() {
    std::uintptr_t info[8];
    __pthread_cleanup_push_imp_nid_postfix(AbortCleanup, nullptr, info);

    throw 7;
}
static GuestOnce pushed{};
static std::int32_t scePushed = 0;
struct OnceCall { GuestOnce* control; std::int32_t* sceControl; Initializer initialize; };
static void* APS5_VABI CallOnce(void* argument) {
    const auto& call = *static_cast<const OnceCall*>(argument);
    if (call.control) pthread_once_nid_postfix(call.control, call.initialize);
    else scePthreadOnce(call.sceControl, call.initialize);
    return nullptr;
}
static void LeaveInsideInitializer(OnceCall call) {
    Pthread thread{};
    Require(scePthreadCreate(&thread, nullptr, CallOnce, &call, nullptr) == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
}
int main() {
    std::array<std::thread, 16> workers;
    for (auto& worker : workers) worker = std::thread([] {
        ++arrived;
        Require(pthread_once_nid_postfix(&control, Initialize) == 0);
        Require(published == 42 && nestedValue == 17);
    });
    for (auto& worker : workers) worker.join();
    Require(calls == 1 && control.state == 1 && control.mutex == nullptr);
    Require(pthread_once_nid_postfix(&control, Initialize) == 0 && calls == 1);
    try { pthread_once_nid_postfix(&retry, Retry); std::abort(); }
    catch (int value) { Require(value == 7 && retry.state == 0); }
    Require(pthread_once_nid_postfix(&retry, Retry) == 0 && attempts == 2);
    Require(pthread_once_nid_postfix(nullptr, Initialize) == 22);
    Require(pthread_once_nid_postfix(&control, nullptr) == 22);
    GuestOnce invalid{9, nullptr};
    Require(pthread_once_nid_postfix(&invalid, Initialize) == 22);
    LeaveInsideInitializer({&exited, nullptr, ExitInsideInitializer});
    Require(exited.state == 0);
    Require(pthread_once_nid_postfix(&exited, CountRetry) == 0 && exited.state == 1 && retried == 1);
    LeaveInsideInitializer({&cancelled, nullptr, CancelInsideInitializer});
    Require(cancelled.state == 0);
    Require(pthread_once_nid_postfix(&cancelled, CountRetry) == 0 && cancelled.state == 1 && retried == 2);
    LeaveInsideInitializer({nullptr, &sceExited, ExitInsideInitializer});
    Require(sceExited == 0);
    Require(scePthreadOnce(&sceExited, CountRetry) == 0 && sceExited == 1 && retried == 3);
    LeaveInsideInitializer({nullptr, &sceCancelled, CancelInsideInitializer});
    Require(sceCancelled == 0);
    Require(scePthreadOnce(&sceCancelled, CountRetry) == 0 && sceCancelled == 1 && retried == 4);
    try { pthread_once_nid_postfix(&pushed, PushAndThrow); std::abort(); }
    catch (int value) { Require(value == 7 && pushed.state == 0); }
    Require(pthread_once_nid_postfix(&pushed, CountRetry) == 0 && pushed.state == 1 && retried == 5);
    try { scePthreadOnce(&scePushed, PushAndThrow); std::abort(); }
    catch (int value) { Require(value == 7 && scePushed == 0); }
    Require(scePthreadOnce(&scePushed, CountRetry) == 0 && scePushed == 1 && retried == 6);
}
