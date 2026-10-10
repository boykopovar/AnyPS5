#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using GuestDestructor = void(APS5_VABI *)(void*);

extern "C" {
int APS5_VABI __cxa_thread_atexit_impl_nid_postfix(GuestDestructor, void*, void*);
int APS5_VABI LibcInternalExtCxaThreadAtexit_nid_postfix(GuestDestructor, void*, void*);
}

namespace {

struct Call {
    int id;
    void* object;
};

std::array<Call, 3> calls{};
std::atomic<std::size_t> callCount{0};
std::atomic<bool> registrationFailed{false};

void Record(int id, void* object) {
    const auto index = callCount.fetch_add(1, std::memory_order_relaxed);
    if (index < calls.size())
        calls[index] = {id, object};
}

void APS5_VABI FirstDestructor(void* object) { Record(1, object); }
void APS5_VABI SecondDestructor(void* object) { Record(2, object); }
void APS5_VABI ThirdDestructor(void* object) { Record(3, object); }

void Fail(const char* message) { std::fprintf(stderr, "%s\n", message); }

}

int main() {
    void* module =
#ifdef _WIN32
        reinterpret_cast<void*>(GetModuleHandleW(nullptr));
#else
        dlopen(nullptr, RTLD_NOW);
#endif
    if (module == nullptr) {
        Fail("could not acquire the test module handle");
        return 1;
    }

    void* directDso = module;
#ifdef _WIN32
    directDso = nullptr;
#endif

    int firstObject = 1;
    int secondObject = 2;
    int thirdObject = 3;
    std::thread worker([&] {
        if (__cxa_thread_atexit_impl_nid_postfix(FirstDestructor, &firstObject, directDso) != 0)
            registrationFailed.store(true);
        if (__cxa_thread_atexit_impl_nid_postfix(SecondDestructor, &secondObject, directDso) != 0)
            registrationFailed.store(true);
        if (LibcInternalExtCxaThreadAtexit_nid_postfix(ThirdDestructor, &thirdObject, module) != 0)
            registrationFailed.store(true);
    });
    worker.join();

#ifndef _WIN32
    dlclose(module);
#endif

    if (registrationFailed.load()) {
        Fail("thread destructor registration failed");
        return 1;
    }
    if (callCount.load() != calls.size()) {
        Fail("thread exit did not run each registered destructor once");
        return 1;
    }
    const std::array<Call, 3> expected = {{{3, &thirdObject}, {2, &secondObject}, {1, &firstObject}}};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (calls[index].id != expected[index].id || calls[index].object != expected[index].object) {
            std::fprintf(stderr, "thread destructor %d received object %p; expected %p\n", calls[index].id,
                         calls[index].object, expected[index].object);
            return 1;
        }
    }
    return 0;
}
