#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>

namespace {

struct GuestSharedMutex {
    PthreadMutex mutex;
    PthreadCond gate1;
    PthreadCond gate2;
    unsigned state;
    unsigned padding;
};
static_assert(sizeof(GuestSharedMutex) == 32);

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Predicate>
void WaitUntil(Predicate predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        Require(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::yield();
    }
}

unsigned ReadState(GuestSharedMutex* self) {
    _ZNSt3__15mutex4lockEv_nid_postfix(&self->mutex);
    const unsigned state = self->state;
    _ZNSt3__15mutex6unlockEv_nid_postfix(&self->mutex);
    return state;
}

bool GateReady(GuestSharedMutex* self, PthreadCond GuestSharedMutex::* gate) {
    _ZNSt3__15mutex4lockEv_nid_postfix(&self->mutex);
    const bool ready = (self->*gate) != nullptr;
    _ZNSt3__15mutex6unlockEv_nid_postfix(&self->mutex);
    return ready;
}

}

int main() {
    GuestSharedMutex shared{};
    shared.state = 123;
    _ZNSt3__119__shared_mutex_baseC1Ev_nid_postfix(&shared);
    Require(shared.mutex == nullptr && shared.gate1 == nullptr && shared.gate2 == nullptr && shared.state == 0,
            "constructor did not initialize the guest object");
    Require(pthread_mutex_init_nid_postfix(&shared.mutex, nullptr) == 0, "pthread mutex init failed");

    _ZNSt3__119__shared_mutex_base11lock_sharedEv_nid_postfix(&shared);
    std::atomic<bool> secondReaderHolding{false};
    std::atomic<bool> releaseSecondReader{false};
    std::thread secondReader([&] {
        _ZNSt3__119__shared_mutex_base11lock_sharedEv_nid_postfix(&shared);
        secondReaderHolding = true;
        while (!releaseSecondReader) std::this_thread::yield();
        _ZNSt3__119__shared_mutex_base13unlock_sharedEv_nid_postfix(&shared);
    });
    WaitUntil([&] { return secondReaderHolding.load(); }, "second reader did not enter concurrently");
    Require(ReadState(&shared) == 2, "concurrent readers were not counted");
    _ZNSt3__119__shared_mutex_base13unlock_sharedEv_nid_postfix(&shared);

    std::atomic<bool> writerAcquired{false};
    std::atomic<bool> releaseWriter{false};
    std::thread writer([&] {
        _ZNSt3__119__shared_mutex_base4lockEv_nid_postfix(&shared);
        writerAcquired = true;
        while (!releaseWriter) std::this_thread::yield();
        _ZNSt3__119__shared_mutex_base6unlockEv_nid_postfix(&shared);
    });
    WaitUntil([&] { return ReadState(&shared) == 0x80000001U; }, "writer did not wait for the reader");
    WaitUntil([&] { return GateReady(&shared, &GuestSharedMutex::gate2); }, "writer did not wait on the reader gate");
    Require(!writerAcquired, "writer entered while a reader held the mutex");

    std::atomic<bool> lateReaderStarted{false};
    std::atomic<bool> lateReaderAcquired{false};
    std::thread lateReader([&] {
        lateReaderStarted = true;
        _ZNSt3__119__shared_mutex_base11lock_sharedEv_nid_postfix(&shared);
        lateReaderAcquired = true;
        _ZNSt3__119__shared_mutex_base13unlock_sharedEv_nid_postfix(&shared);
    });
    WaitUntil([&] { return lateReaderStarted.load(); }, "late reader did not start");
    WaitUntil([&] { return GateReady(&shared, &GuestSharedMutex::gate1); }, "reader did not wait behind the writer");
    Require(!lateReaderAcquired, "reader bypassed a waiting writer");

    releaseSecondReader = true;
    secondReader.join();
    WaitUntil([&] { return writerAcquired.load(); }, "writer did not acquire after readers exited");
    Require(!lateReaderAcquired, "reader bypassed an active writer");
    releaseWriter = true;
    writer.join();
    lateReader.join();
    Require(lateReaderAcquired && ReadState(&shared) == 0, "reader did not resume after writer exit");
    Require(shared.gate1 != nullptr && shared.gate2 != nullptr, "condition variables were not initialized lazily");

    _ZNSt3__118condition_variableD1Ev_nid_postfix(&shared.gate1);
    _ZNSt3__118condition_variableD1Ev_nid_postfix(&shared.gate2);
    _ZNSt3__15mutexD1Ev_nid_postfix(&shared.mutex);

    bool rejected = false;
    try {
        _ZNSt3__119__shared_mutex_baseC1Ev_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{1}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "invalid guest address was accepted");
    return 0;
}
