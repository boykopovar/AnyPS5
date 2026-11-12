#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

constexpr int Success = 0;
constexpr int Busy = 3;
constexpr int TryRecursive = 0x102;

unsigned failures = 0;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: sce v2 at_thread_exit: %s\n", what);
    ++failures;
}

struct Pair {
    void* mutex = nullptr;
    void* condition = nullptr;
    int ready = 0;
    Pair() {
        _ZNSt8__sce_v29_Mtx_initEPP13pthread_mutexi_nid_postfix(&mutex, TryRecursive);
        _ZNSt8__sce_v29_Cnd_initEPP12pthread_cond_nid_postfix(&condition);
    }
    ~Pair() {
        _ZNSt8__sce_v230_Cnd_unregister_at_thread_exitEPP13pthread_mutex_nid_postfix(&mutex);
        _ZNSt8__sce_v212_Cnd_destroyEPP12pthread_cond_nid_postfix(&condition);
        _ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(&mutex);
    }
    void Lock() { _ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(&mutex); }
    void Register(bool withReady) {
        _ZNSt8__sce_v228_Cnd_register_at_thread_exitEPP12pthread_condPP13pthread_mutexPi_nid_postfix(&condition, &mutex, withReady ? &ready : nullptr);
    }
    int Probe() {
        int result = -1;
        std::thread other([&] {
            result = _ZNSt8__sce_v212_Mtx_trylockEPP13pthread_mutex_nid_postfix(&mutex);
            if (result == Success) _ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(&mutex);
        });
        other.join();
        return result;
    }
    void Release() {
        while (Probe() == Busy) _ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(&mutex);
    }
};

void Broadcast() {
    _ZNSt8__sce_v232_Cnd_do_broadcast_at_thread_exitEv_nid_postfix();
}

void CheckSingleEntries() {
    {
        Pair pair;
        pair.Register(true);
        Broadcast();
        Require(pair.ready == 1 && pair.Probe() == Success, "a flag entry is set and leaves the mutex free");
    }
    {
        Pair pair;
        pair.Lock();
        pair.Register(true);
        Broadcast();
        Require(pair.ready == 1 && pair.Probe() == Busy, "a flag entry relocks and unlocks once");
        pair.Release();
    }
    {
        Pair pair;
        pair.Lock();
        pair.Register(false);
        Require(pair.Probe() == Busy, "the registering thread keeps the mutex");
        Broadcast();
        Require(pair.ready == 0 && pair.Probe() == Success, "an entry without a flag releases the held mutex");
    }
    {
        Pair pair;
        pair.Lock();
        pair.Lock();
        pair.Register(false);
        Broadcast();
        Require(pair.Probe() == Busy, "an entry without a flag unlocks only once");
        pair.Release();
    }
    {
        Pair pair;
        pair.Lock();
        pair.Register(true);
        pair.Lock();
        pair.Register(false);
        pair.Register(false);
        Broadcast();
        Require(pair.ready == 1 && pair.Probe() == Success, "three entries on one mutex balance its locks");
    }
    {
        Pair pair;
        pair.Register(true);
        pair.Register(true);
        _ZNSt8__sce_v230_Cnd_unregister_at_thread_exitEPP13pthread_mutex_nid_postfix(&pair.mutex);
        Broadcast();
        Require(pair.ready == 0 && pair.Probe() == Success, "unregistering removes every entry of the mutex");
    }
    {
        Pair pair;
        pair.Register(true);
        Broadcast();
        pair.ready = 0;
        Broadcast();
        Require(pair.ready == 0, "an entry is removed once it is broadcast");
    }
}

void CheckThreads() {
    Pair pair;
    std::thread other([&] { pair.Register(true); });
    other.join();
    Broadcast();
    Require(pair.ready == 0, "another thread's entry is not broadcast here");
    std::thread owner([&] {
        pair.Register(true);
        Broadcast();
    });
    owner.join();
    Require(pair.ready == 1, "the registering thread broadcasts its own entry");
    Pair waited;
    waited.Lock();
    std::thread exiting([&] {
        waited.Register(true);
        Broadcast();
    });
    while (waited.ready == 0) _ZNSt8__sce_v29_Cnd_waitEPP12pthread_condPP13pthread_mutex_nid_postfix(&waited.condition, &waited.mutex);
    _ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(&waited.mutex);
    exiting.join();
    Require(waited.ready == 1, "a waiter wakes when the flag is set");
}

void CheckManyEntries() {
    constexpr int Count = 45;
    Pair* pairs = new Pair[Count];
    for (int i = 0; i < Count; ++i) pairs[i].Register(true);
    _ZNSt8__sce_v230_Cnd_unregister_at_thread_exitEPP13pthread_mutex_nid_postfix(&pairs[3].mutex);
    _ZNSt8__sce_v230_Cnd_unregister_at_thread_exitEPP13pthread_mutex_nid_postfix(&pairs[25].mutex);
    pairs[3].Register(true);
    Broadcast();
    bool all = true;
    for (int i = 0; i < Count; ++i) all = all && pairs[i].ready == (i == 25 ? 0 : 1);
    Require(all, "entries beyond one block of 20 are kept and broadcast");
    delete[] pairs;
}

void CheckGlobalMutex() {
    _ZNSt8__sce_v226_Lock_at_thread_exit_mutexEv_nid_postfix();
    _ZNSt8__sce_v226_Lock_at_thread_exit_mutexEv_nid_postfix();
    Pair pair;
    pair.Register(true);
    std::atomic<bool> registered {false};
    Pair other;
    std::thread blocked([&] {
        other.Register(true);
        registered = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!registered, "registering waits for the at-thread-exit mutex");
    _ZNSt8__sce_v228_Unlock_at_thread_exit_mutexEv_nid_postfix();
    _ZNSt8__sce_v228_Unlock_at_thread_exit_mutexEv_nid_postfix();
    blocked.join();
    Require(registered, "the at-thread-exit mutex is recursive and released");
    Broadcast();
    Require(pair.ready == 1 && other.ready == 0, "entries registered under the held mutex are kept");
}

}

int main() {
    Require(scePthreadSelf() != nullptr, "libkernel threads are loaded");
    CheckSingleEntries();
    CheckThreads();
    CheckManyEntries();
    CheckGlobalMutex();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
