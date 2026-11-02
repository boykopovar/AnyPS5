#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

constexpr int Einval = 22;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void WaitFor(const std::atomic<bool>& flag, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!flag && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(flag, message);
}

}

int main() {
    int first = 0;
    int second = 0;
    for (const PthreadKey invalid : {static_cast<PthreadKey>(0), static_cast<PthreadKey>(0xFFFFFFFFu)}) {
        Require(pthread_setspecific_nid_postfix(invalid, &first) == Einval, "setspecific of an out-of-range key returns EINVAL");
        Require(pthread_getspecific_nid_postfix(invalid) == nullptr, "getspecific of an out-of-range key returns NULL");
    }

    PthreadKey key = 0;
    Require(pthread_key_create_nid_postfix(&key, nullptr) == 0 && key != 0, "key create");
    Require(pthread_getspecific_nid_postfix(key) == nullptr, "getspecific of a key never set returns NULL");
    Require(pthread_setspecific_nid_postfix(key, &first) == 0, "setspecific returns 0");
    Require(pthread_getspecific_nid_postfix(key) == &first, "getspecific returns the stored value");
    Require(pthread_setspecific_nid_postfix(key, nullptr) == 0, "setspecific of NULL returns 0");
    Require(pthread_getspecific_nid_postfix(key) == nullptr, "getspecific after storing NULL returns NULL");
    Require(pthread_setspecific_nid_postfix(key, &first) == 0, "setspecific again returns 0");

    std::atomic<bool> stored{false};
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
    std::atomic<void*> seenBefore{&first};
    std::atomic<void*> seenAfter{nullptr};
    std::thread other([&] {
        seenBefore = pthread_getspecific_nid_postfix(key);
        Require(pthread_setspecific_nid_postfix(key, &second) == 0, "helper setspecific returns 0");
        seenAfter = pthread_getspecific_nid_postfix(key);
        stored = true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!release && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        finished = true;
    });
    WaitFor(stored, "helper never stored its value");
    Require(seenBefore.load() == nullptr, "another thread starts with NULL for a key this thread has set");
    Require(seenAfter.load() == &second, "another thread reads back its own value");
    Require(pthread_getspecific_nid_postfix(key) == &first, "this thread still reads its own value");
    release = true;
    WaitFor(finished, "helper never finished");
    other.join();

    Require(pthread_key_delete_nid_postfix(key) == 0, "key delete returns 0");
    Require(pthread_getspecific_nid_postfix(key) == nullptr, "getspecific of a deleted key returns NULL");
    Require(pthread_setspecific_nid_postfix(key, &first) == Einval, "setspecific of a deleted key returns EINVAL");

    PthreadKey reused = 0;
    Require(pthread_key_create_nid_postfix(&reused, nullptr) == 0 && reused == key, "a new key reuses the freed number");
    Require(pthread_getspecific_nid_postfix(reused) == nullptr, "the value stored under the old key is gone");
    Require(pthread_key_delete_nid_postfix(reused) == 0, "second key delete returns 0");
    return 0;
}
