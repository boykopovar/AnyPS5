#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>

static bool failAllocations = false;

void* operator new(std::size_t size) {
    if (failAllocations) throw std::bad_alloc();
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

static void Require(bool value) { if (!value) std::abort(); }

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int success = 0;
constexpr int plain = 0x01;

static void RegisterRejectsNullHandles() {
    void* condition = nullptr;
    void* mutex = nullptr;
    Require(_Cnd_init_nid_postfix(&condition) == success);
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    int ready = 0;
    RequireThrows<std::invalid_argument>([&] { _Cnd_register_at_thread_exit_nid_postfix(nullptr, mutex, &ready); });
    RequireThrows<std::invalid_argument>([&] { _Cnd_register_at_thread_exit_nid_postfix(condition, nullptr, &ready); });
    RequireThrows<std::invalid_argument>([] { _Cnd_unregister_at_thread_exit_nid_postfix(nullptr); });
    _Cnd_register_at_thread_exit_nid_postfix(condition, mutex, nullptr);
    _Cnd_unregister_at_thread_exit_nid_postfix(mutex);
    _Mtx_destroy_nid_postfix(&mutex);
    _Cnd_destroy_nid_postfix(&condition);
}

static void UnregisterSkipsBroadcast() {
    void* condition = nullptr;
    void* mutex = nullptr;
    Require(_Cnd_init_nid_postfix(&condition) == success);
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    int ready = 0;
    _Cnd_register_at_thread_exit_nid_postfix(condition, mutex, &ready);
    _Cnd_unregister_at_thread_exit_nid_postfix(mutex);
    _Cnd_do_broadcast_at_thread_exit_nid_postfix();
    Require(ready == 0);
    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    _Mtx_destroy_nid_postfix(&mutex);
    _Cnd_destroy_nid_postfix(&condition);
}

static void BroadcastWakesWaitingThread() {
    void* condition = nullptr;
    void* mutex = nullptr;
    Require(_Cnd_init_nid_postfix(&condition) == success);
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    int ready = 0;
    std::thread worker([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == success);
        _Cnd_register_at_thread_exit_nid_postfix(condition, mutex, &ready);
        _Cnd_do_broadcast_at_thread_exit_nid_postfix();
    });
    Require(_Cnd_wait_nid_postfix(&condition, &mutex) == success);
    Require(ready == 1);
    worker.join();
    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    _Mtx_destroy_nid_postfix(&mutex);
    _Cnd_destroy_nid_postfix(&condition);
}

static void BroadcastDoesNotAllocate() {
    void* condition = nullptr;
    void* mutex = nullptr;
    Require(_Cnd_init_nid_postfix(&condition) == success);
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    int ready = 0;
    _Cnd_register_at_thread_exit_nid_postfix(condition, mutex, &ready);
    failAllocations = true;
    _Cnd_do_broadcast_at_thread_exit_nid_postfix();
    failAllocations = false;
    Require(ready == 1);
    _Mtx_destroy_nid_postfix(&mutex);
    _Cnd_destroy_nid_postfix(&condition);
}

struct HostPadThread {
    std::thread worker;
    void* result = nullptr;
};

static int APS5_VABI HostPthreadCreate(void** thread, const void*, void* (APS5_VABI* entry)(void*), void* argument) {
    auto* handle = new HostPadThread{};
    handle->worker = std::thread([handle, entry, argument] { handle->result = entry(argument); });
    *thread = handle;
    return 0;
}

static int APS5_VABI HostPthreadJoin(void* thread, void** value) {
    if (!thread) return 22;
    auto* handle = static_cast<HostPadThread*>(thread);
    handle->worker.join();
    if (value) *value = handle->result;
    delete handle;
    return 0;
}

struct ExitPad {
    unsigned (APS5_VABI* const* vtable)(GuestPad*);
    void* condition;
    void* mutex;
    bool started;
    void* notifyCondition;
    void* notifyMutex;
    int* ready;
};

static unsigned APS5_VABI ExitPadGo(GuestPad* base) {
    auto* pad = reinterpret_cast<ExitPad*>(base);
    _ZNSt4_Pad8_ReleaseEv_nid_postfix(base);
    Require(_Mtx_lock_nid_postfix(&pad->notifyMutex) == success);
    _Cnd_register_at_thread_exit_nid_postfix(pad->notifyCondition, pad->notifyMutex, pad->ready);
    return 7;
}

static void PadLaunchBroadcastsAtThreadExit() {
    RegisterPthreadCreate_nid_no_patch(HostPthreadCreate);
    RegisterPthreadJoin_nid_no_patch(HostPthreadJoin);
    unsigned (APS5_VABI* vtable[1])(GuestPad*) = {ExitPadGo};
    void* condition = nullptr;
    void* mutex = nullptr;
    Require(_Cnd_init_nid_postfix(&condition) == success);
    Require(_Mtx_init_nid_postfix(&mutex, plain) == success);
    int ready = 0;
    ExitPad pad{};
    pad.vtable = vtable;
    pad.notifyCondition = condition;
    pad.notifyMutex = mutex;
    pad.ready = &ready;
    Require(_Mtx_lock_nid_postfix(&mutex) == success);
    _ZNSt4_PadC2Ev_nid_postfix(reinterpret_cast<GuestPad*>(&pad));
    void* thread = nullptr;
    _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(reinterpret_cast<GuestPad*>(&pad), &thread);
    Require(_Cnd_wait_nid_postfix(&condition, &mutex) == success);
    Require(ready == 1);
    _ZNSt4_PadD2Ev_nid_postfix(reinterpret_cast<GuestPad*>(&pad));
    int code = 0;
    Require(_Thrd_join_nid_postfix(thread, &code) == success && code == 7);
    Require(_Mtx_unlock_nid_postfix(&mutex) == success);
    _Mtx_destroy_nid_postfix(&mutex);
    _Cnd_destroy_nid_postfix(&condition);
}

int main() {
    RegisterRejectsNullHandles();
    UnregisterSkipsBroadcast();
    BroadcastWakesWaitingThread();
    BroadcastDoesNotAllocate();
    PadLaunchBroadcastsAtThreadExit();
    return 0;
}
