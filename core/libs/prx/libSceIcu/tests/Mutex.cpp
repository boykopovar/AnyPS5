#include "prx/libSceIcu/libSceIcu.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "umtx check failed: %s\n", what);
    std::abort();
}

struct Storage {
    alignas(8) unsigned char bytes[24]{};
    UMutex* get() { return reinterpret_cast<UMutex*>(bytes); }
};

void Global() {
    umtx_lock_nid_postfix(nullptr);
    umtx_unlock_nid_postfix(nullptr);
    Storage other;
    umtx_lock_nid_postfix(nullptr);
    umtx_lock_nid_postfix(other.get());
    umtx_unlock_nid_postfix(other.get());
    umtx_unlock_nid_postfix(nullptr);
}

void Exclusion() {
    Storage storage;
    long counter = 0;
    auto work = [&] {
        for (int i = 0; i < 100000; ++i) {
            umtx_lock_nid_postfix(storage.get());
            const long value = counter;
            counter = value + 1;
            umtx_unlock_nid_postfix(storage.get());
        }
    };
    std::thread first(work);
    std::thread second(work);
    first.join();
    second.join();
    Require(counter == 200000, "two threads lost an update");
}

void Blocking() {
    Storage storage;
    std::atomic<bool> acquired = false;
    umtx_lock_nid_postfix(storage.get());
    std::thread waiter([&] {
        umtx_lock_nid_postfix(storage.get());
        acquired = true;
        umtx_unlock_nid_postfix(storage.get());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!acquired, "a held mutex was acquired by another thread");
    umtx_unlock_nid_postfix(storage.get());
    waiter.join();
    Require(acquired, "the waiter never acquired the released mutex");
}

void UntouchedStorage() {
    Storage storage;
    umtx_lock_nid_postfix(storage.get());
    umtx_unlock_nid_postfix(storage.get());
    for (unsigned char byte : storage.bytes) Require(byte == 0, "the guest mutex storage was written");
}

void UnlockWithoutLock() {
    static Storage storage;
    try {
        umtx_unlock_nid_postfix(storage.get());
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "unlocking a never locked mutex did not throw");
}

}

int main() {
    Global();
    Exclusion();
    Blocking();
    UntouchedStorage();
    UnlockWithoutLock();
    return 0;
}
