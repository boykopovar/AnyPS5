#include "prx/libSceLibcInternal/include/MutexObject.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>

namespace {

struct Object {
    PthreadMutex* storage;
};

using Member = void (APS5_VABI*)(void*);

unsigned allocations = 0;
unsigned releases = 0;
std::size_t requestedSize = 0;
void* releasedStorage = nullptr;
Object* observedObject = nullptr;
bool observeRelease = false;
bool rewriteOnRelease = false;
bool throwOnRelease = false;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "mutex object: %s\n", message);
    std::abort();
}

PthreadMutex* Sentinel() {
    return reinterpret_cast<PthreadMutex*>(std::uintptr_t{0x30});
}

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocations;
    requestedSize = bytes;
    if (observedObject != nullptr)
        Require(observedObject->storage == Sentinel(), "allocation precedes the object pointer write");
    return std::malloc(bytes);
}

void APS5_VABI Release(void* pointer) {
    if (throwOnRelease)
        throw std::runtime_error("mutex object release failure");
    ++releases;
    releasedStorage = pointer;
    if (observeRelease) {
        Require(observedObject != nullptr && observedObject->storage == pointer, "release receives the object's storage");
        Require(*static_cast<PthreadMutex*>(pointer) == reinterpret_cast<PthreadMutex>(std::uintptr_t{2}), "the native mutex is destroyed before storage release");
        if (rewriteOnRelease)
            observedObject->storage = Sentinel();
    }
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI Reallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI Align(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI Realign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void Construct(Object& object, Member constructor) {
    object.storage = Sentinel();
    observedObject = &object;
    const unsigned before = allocations;
    constructor(&object);
    Require(allocations == before + 1 && requestedSize == 8, "construction requests eight application-heap bytes once");
    Require(object.storage != nullptr && object.storage != Sentinel() && *object.storage != nullptr, "construction stores an initialized mutex");
    observedObject = nullptr;
}

void Destroy(Object& object, Member destructor, bool rewrite) {
    void* storage = object.storage;
    observedObject = &object;
    observeRelease = true;
    rewriteOnRelease = rewrite;
    const unsigned before = releases;
    destructor(&object);
    Require(releases == before + 1 && releasedStorage == storage, "destruction releases storage exactly once");
    Require(object.storage == (rewrite ? Sentinel() : storage), "destruction leaves the object pointer unchanged after release");
    observedObject = nullptr;
    observeRelease = false;
    rewriteOnRelease = false;
}

int TryOnOtherThread(Object& object) {
    int result = -1;
    std::thread other([&] {
        result = pthread_mutex_trylock_nid_postfix(object.storage);
        if (result == 0)
            pthread_mutex_unlock_nid_postfix(object.storage);
    });
    other.join();
    return result;
}

void RequireInvalid(Member member, void* object) {
    bool threw = false;
    errno = 71;
    try {
        member(object);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && errno == 71, "inaccessible guest pointers throw without changing errno");
}

}

int main(int argc, char** argv) {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release),
        reinterpret_cast<void*>(&AllocateZeroed), reinterpret_cast<void*>(&Reallocate),
        reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    if (argc == 2 && std::strcmp(argv[1], "terminate") == 0) {
        Object object{};
        Construct(object, _ZNSt6_MutexC1Ev_nid_postfix);
        _ZSt13set_terminatePFvvE_nid_postfix([] {
            std::puts("mutex object destructor terminated");
            std::fflush(stdout);
            std::_Exit(0);
        });
        throwOnRelease = true;
        try {
            _ZNSt6_MutexD1Ev_nid_postfix(&object);
        } catch (...) {
            return 1;
        }
        return 1;
    }

    const std::array<Member, 2> constructors{_ZNSt6_MutexC1Ev_nid_postfix, _ZNSt6_MutexC2Ev_nid_postfix};
    const std::array<Member, 2> destructors{_ZNSt6_MutexD1Ev_nid_postfix, _ZNSt6_MutexD2Ev_nid_postfix};
    for (Member constructor : constructors) {
        for (Member destructor : destructors) {
            Object object{};
            Construct(object, constructor);
            _ZNSt6_Mutex5_LockEv_nid_postfix(&object);
            _ZNSt6_Mutex5_LockEv_nid_postfix(&object);
            Require(TryOnOtherThread(object) != 0, "recursive locks exclude another thread");
            std::thread foreignUnlock([&] { _ZNSt6_Mutex7_UnlockEv_nid_postfix(&object); });
            foreignUnlock.join();
            Require(TryOnOtherThread(object) != 0, "a foreign unlock's ignored error does not release the mutex");
            _ZNSt6_Mutex7_UnlockEv_nid_postfix(&object);
            Require(TryOnOtherThread(object) != 0, "one unlock retains the remaining recursion level");
            _ZNSt6_Mutex7_UnlockEv_nid_postfix(&object);
            Require(TryOnOtherThread(object) == 0, "the final unlock releases the mutex");
            _ZNSt6_Mutex7_UnlockEv_nid_postfix(&object);
            Destroy(object, destructor, destructor == destructors[1]);
        }
    }

    Object first{};
    Object second{};
    Construct(first, constructors[0]);
    Construct(second, constructors[1]);
    Require(first.storage != second.storage, "objects own independent storage");
    _ZNSt6_Mutex5_LockEv_nid_postfix(&first);
    Require(TryOnOtherThread(second) == 0, "holding one object does not block another");
    _ZNSt6_Mutex7_UnlockEv_nid_postfix(&first);

    int total = 0;
    std::atomic<int> inside{0};
    std::atomic<bool> overlapped{false};
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([&] {
            for (int i = 0; i < 400; ++i) {
                _ZNSt6_Mutex5_LockEv_nid_postfix(&first);
                if (inside.fetch_add(1) != 0)
                    overlapped.store(true);
                ++total;
                std::this_thread::yield();
                inside.fetch_sub(1);
                _ZNSt6_Mutex7_UnlockEv_nid_postfix(&first);
            }
        });
    }
    for (auto& thread : threads)
        thread.join();
    Require(total == 1600 && !overlapped.load(), "concurrent operations serialize the protected counter");
    Destroy(first, destructors[0], false);
    Destroy(second, destructors[1], false);

    for (Member constructor : constructors) {
        const unsigned before = releases;
        RequireInvalid(constructor, reinterpret_cast<void*>(std::uintptr_t{0x10}));
        Require(releases == before + 1, "an inaccessible object does not leak its newly allocated storage");
    }
    for (Member destructor : destructors)
        RequireInvalid(destructor, nullptr);
    RequireInvalid(_ZNSt6_Mutex5_LockEv_nid_postfix, reinterpret_cast<void*>(std::uintptr_t{0x10}));
    RequireInvalid(_ZNSt6_Mutex7_UnlockEv_nid_postfix, nullptr);
    Object inaccessible{reinterpret_cast<PthreadMutex*>(std::uintptr_t{0x10})};
    RequireInvalid(_ZNSt6_Mutex5_LockEv_nid_postfix, &inaccessible);
    RequireInvalid(_ZNSt6_Mutex7_UnlockEv_nid_postfix, &inaccessible);
    RequireInvalid(destructors[0], &inaccessible);

    std::puts("mutex object checks passed");
    return 0;
}
