#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static std::array<std::atomic<void*>, 2> trackedObjects{};
static std::atomic<unsigned> releasedWorkers{0};

static void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }

static void APS5_VABI HeapRelease(void* pointer) {
    if (pointer == nullptr) return;
    for (unsigned i = 0; i < trackedObjects.size(); ++i) {
        if (trackedObjects[i].load(std::memory_order_acquire) != pointer) continue;
        void* state = pointer;
        std::memcpy(&state, pointer, sizeof(state));
        Require(state == nullptr);
        releasedWorkers.fetch_or(1u << i, std::memory_order_release);
    }
    std::free(pointer);
}

static void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
static void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
static void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
static void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) {
    return std::realloc(pointer, bytes);
}
static int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer ? 0 : 12;
}

static void* NewThreadStruct() {
    void* object = _Znwm_nid_postfix(sizeof(void*));
    Require(object != nullptr);
    _ZNSt3__115__thread_structC1Ev_nid_postfix(object);
    void* state = nullptr;
    std::memcpy(&state, object, sizeof(state));
    Require(state != nullptr);
    return object;
}

int main() {
    const std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(&HeapAllocate), reinterpret_cast<void*>(&HeapRelease),
        reinterpret_cast<void*>(&HeapCalloc), reinterpret_cast<void*>(&HeapReallocate),
        reinterpret_cast<void*>(&HeapAlign), reinterpret_cast<void*>(&HeapRealign),
        reinterpret_cast<void*>(&HeapPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    PthreadKey* const key = _ZNSt3__119__thread_local_dataEv_nid_postfix();
    Require(key != nullptr && *key > 0);
    Require(_ZNSt3__119__thread_local_dataEv_nid_postfix() == key);
    Require(pthread_getspecific_nid_postfix(*key) == nullptr);

    void* mainObject = NewThreadStruct();
    Require(pthread_setspecific_nid_postfix(*key, mainObject) == 0);
    Require(pthread_getspecific_nid_postfix(*key) == mainObject);

    std::array<void*, 2> workerObjects{};
    std::atomic<int> ready{0};
    std::array<std::thread, 2> workers;
    for (std::size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::thread([&, i] {
            Require(pthread_getspecific_nid_postfix(*key) == nullptr);
            void* object = NewThreadStruct();
            trackedObjects[i].store(object, std::memory_order_release);
            Require(pthread_setspecific_nid_postfix(*key, object) == 0);
            workerObjects[i] = object;
            ready.fetch_add(1, std::memory_order_release);
            while (ready.load(std::memory_order_acquire) != 2) std::this_thread::yield();
            Require(pthread_getspecific_nid_postfix(*key) == object);
        });
    }
    for (auto& worker : workers) worker.join();
    Require(releasedWorkers.load(std::memory_order_acquire) == 3);
    Require(workerObjects[0] != nullptr && workerObjects[1] != nullptr);
    Require(workerObjects[0] != workerObjects[1]);
    Require(pthread_getspecific_nid_postfix(*key) == mainObject);

    Require(pthread_setspecific_nid_postfix(*key, nullptr) == 0);
    _ZNSt3__115__thread_structD1Ev_nid_postfix(mainObject);
    bool doubleDestructionRejected = false;
    try {
        _ZNSt3__115__thread_structD1Ev_nid_postfix(mainObject);
    } catch (const std::invalid_argument&) {
        doubleDestructionRejected = true;
    }
    Require(doubleDestructionRejected);
    _ZdlPv_nid_postfix(mainObject);

    bool nullRejected = false;
    try {
        _ZNSt3__115__thread_structC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullRejected = true;
    }
    Require(nullRejected);
    return 0;
}
