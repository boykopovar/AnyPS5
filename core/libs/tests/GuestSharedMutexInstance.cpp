#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "shared mutex instance check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

namespace {

int g_first;
int g_second;

void Identity() {
    void** first = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
    void** again = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
    void** other = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_second);
    Require(first != nullptr && again != nullptr && other != nullptr);
    Require(first == again);
    Require(first != other);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_second);
}

void ReferenceCount() {
    void** first = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
    void** again = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
    Require(first == again);
    Require(_Smtx_lock_exclusive_nid_postfix(first) == 0);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
    std::atomic<int> shared{-1};
    std::thread other([&] { shared.store(_Smtx_try_lock_shared_nid_postfix(first)); });
    other.join();
    Require(shared.load() != 0);
    Require(_Smtx_unlock_exclusive_nid_postfix(first) == 0);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
}

void Exclusion() {
    void** smtx = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
    Require(smtx != nullptr);
    Require(_Smtx_lock_exclusive_nid_postfix(smtx) == 0);
    std::atomic<int> shared{-1};
    std::thread reader([&] { shared.store(_Smtx_try_lock_shared_nid_postfix(smtx)); });
    reader.join();
    Require(shared.load() != 0);
    Require(_Smtx_unlock_exclusive_nid_postfix(smtx) == 0);
    Require(_Smtx_try_lock_shared_nid_postfix(smtx) == 0);
    Require(_Smtx_unlock_shared_nid_postfix(smtx) == 0);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
}

void SharedReaders() {
    void** smtx = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
    Require(smtx != nullptr);
    Require(_Smtx_lock_shared_nid_postfix(smtx) == 0);
    std::atomic<int> reader{-1};
    std::thread first([&] {
        const int taken = _Smtx_try_lock_shared_nid_postfix(smtx);
        reader.store(taken);
        if (taken == 0) _Smtx_unlock_shared_nid_postfix(smtx);
    });
    first.join();
    Require(reader.load() == 0);
    std::atomic<int> writer{-1};
    std::thread second([&] {
        const int taken = _Smtx_try_lock_exclusive_nid_postfix(smtx);
        writer.store(taken);
        if (taken == 0) _Smtx_unlock_exclusive_nid_postfix(smtx);
    });
    second.join();
    Require(writer.load() != 0);
    Require(_Smtx_unlock_shared_nid_postfix(smtx) == 0);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
}

void UnknownRelease() {
    __std_release_shared_mutex_for_instance_nid_postfix(&g_second);
    void** smtx = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_second);
    Require(smtx != nullptr);
    Require(_Smtx_lock_exclusive_nid_postfix(smtx) == 0);
    Require(_Smtx_unlock_exclusive_nid_postfix(smtx) == 0);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_second);
    __std_release_shared_mutex_for_instance_nid_postfix(&g_second);
}

void ReacquireAfterRelease() {
    for (int round = 0; round < 32; ++round) {
        void** smtx = __std_acquire_shared_mutex_for_instance_nid_postfix(&g_first);
        Require(smtx != nullptr);
        Require(_Smtx_lock_shared_nid_postfix(smtx) == 0);
        Require(_Smtx_unlock_shared_nid_postfix(smtx) == 0);
        __std_release_shared_mutex_for_instance_nid_postfix(&g_first);
    }
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    KernelTimespec resolution{};
    Require(sceKernelClockGetres(0, &resolution) == 0);
    if (std::strcmp(argv[1], "identity") == 0) Identity();
    else if (std::strcmp(argv[1], "refcount") == 0) ReferenceCount();
    else if (std::strcmp(argv[1], "exclusion") == 0) Exclusion();
    else if (std::strcmp(argv[1], "shared") == 0) SharedReaders();
    else if (std::strcmp(argv[1], "unknown") == 0) UnknownRelease();
    else if (std::strcmp(argv[1], "reacquire") == 0) ReacquireAfterRelease();
    else Require(false);
    std::puts("shared mutex for instance identity, lifetime and exclusion passed");
}
