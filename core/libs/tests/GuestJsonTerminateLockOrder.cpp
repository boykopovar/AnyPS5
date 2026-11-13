#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <thread>

using namespace SceJson2;

extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}

static void Require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}

static std::atomic<std::size_t> heapAllocations{0};

extern "C" void* APS5_VABI _Znwm_nid_postfix(std::size_t bytes) {
    void* memory = std::malloc(bytes == 0 ? 1 : bytes);
    if (!memory) throw std::bad_alloc();
    heapAllocations.fetch_add(1, std::memory_order_relaxed);
    return memory;
}
extern "C" void APS5_VABI _ZdlPv_nid_postfix(void* memory) {
    if (!memory) return;
    Require(heapAllocations.fetch_sub(1, std::memory_order_relaxed) != 0, "Legacy heap releases must balance live allocations");
    std::free(memory);
}

struct Schedule {
    std::mutex mutex;
    std::condition_variable changed;
    std::thread::id registrar;
    std::thread::id terminator;
    bool registrarAllocating = false;
    bool terminatorLocking = false;
    bool finished = false;
};
static Schedule schedule;

extern "C" int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attribute) {
    *attribute = reinterpret_cast<PthreadMutexattr>(new int(0));
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attribute, int type) {
    Require(type == 2, "Json2 recovery mutex must be recursive");
    *reinterpret_cast<int*>(*attribute) = type;
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attribute) {
    delete reinterpret_cast<int*>(*attribute);
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr*, const char*) {
    *mutex = reinterpret_cast<PthreadMutex>(new std::recursive_mutex);
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    delete reinterpret_cast<std::recursive_mutex*>(*mutex);
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    {
        std::lock_guard lock(schedule.mutex);
        if (std::this_thread::get_id() == schedule.terminator) {
            schedule.terminatorLocking = true;
            schedule.changed.notify_all();
        }
    }
    reinterpret_cast<std::recursive_mutex*>(*mutex)->lock();
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    reinterpret_cast<std::recursive_mutex*>(*mutex)->unlock();
    return 0;
}

static void* APS5_VABI Allocate(void*, std::size_t bytes, void*) {
    std::unique_lock lock(schedule.mutex);
    if (std::this_thread::get_id() == schedule.registrar && !schedule.registrarAllocating) {
        schedule.registrarAllocating = true;
        schedule.changed.notify_all();
        schedule.changed.wait(lock, [] { return schedule.terminatorLocking; });
    }
    return std::malloc(bytes);
}
static void APS5_VABI Free(void*, void* memory, void*) { std::free(memory); }
static void APS5_VABI Notify(void*, int, std::size_t, void*) { Require(false, "Lock order regression must not fail allocation"); }
static void* const table[]{nullptr, nullptr, reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Notify)};
struct Allocator { void* const* vtable = table; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static Value replacement{};

static const Value& APS5_VABI Mismatch(ValueType, const Value*, const Value*, void*) { return replacement; }

int main() {
    Allocator allocator;
    Parameter parameter{&allocator, nullptr, 8192, 2, {}};
    std::uint8_t initializer = 0;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
            "Initialize legacy lock order regression");
    Value source{};
    _ZN3sce4Json5ValueC1Ev(&source);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&source, "true", 4) == 0, "Create an initialized legacy recovery root");
    _ZN3sce4Json5ValueC1El(&replacement, 42);

    std::thread watchdog([] {
        std::unique_lock lock(schedule.mutex);
        if (!schedule.changed.wait_for(lock, std::chrono::seconds(5), [] { return schedule.finished; })) {
            std::fprintf(stderr, "terminate waits for the recovery mutex under the state mutex while a handler registration waits for the state mutex under the recovery mutex\n");
            std::abort();
        }
    });

    int registration = -1;
    std::thread registrar([&] {
        {
            std::lock_guard lock(schedule.mutex);
            schedule.registrar = std::this_thread::get_id();
        }
        registration = _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, Mismatch, nullptr);
    });
    {
        std::unique_lock lock(schedule.mutex);
        schedule.changed.wait(lock, [] { return schedule.registrarAllocating; });
        schedule.terminator = std::this_thread::get_id();
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate while a legacy handler registration holds the recovery mutex");
    registrar.join();
    Require(registration == 0, "Handler registration must complete across a concurrent termination");
    {
        std::lock_guard lock(schedule.mutex);
        schedule.finished = true;
        schedule.changed.notify_all();
    }
    watchdog.join();

    _ZN3sce4Json5ValueD1Ev(&source);
    _ZN3sce4Json5ValueD1Ev(&replacement);
    Require(heapAllocations.load(std::memory_order_relaxed) == 0, "Termination must release the handler map registered during it");
}
