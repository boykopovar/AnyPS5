#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

struct InitEnvParams {
    int argc;
    std::uint32_t pad;
    const char* argv[3];
};

extern "C" {
int APS5_VABI cxa_atexit_nid_postfix(void (APS5_VABI *)(void*), void*, void*);
void APS5_VABI cxa_finalize_nid_postfix(void*);
int APS5_VABI LibcInternalExtCxaThreadAtexit_nid_postfix(void (APS5_VABI *)(void*), void*, void*);
void APS5_VABI init_env_nid_postfix(const InitEnvParams*);
}

namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("lifecycle check failed");
}

int order = 0;
int firstAt = -1;
int secondAt = -1;
int otherAt = -1;
void APS5_VABI First(void*) { firstAt = order++; }
void APS5_VABI Second(void*) { secondAt = order++; }
void APS5_VABI Other(void*) { otherAt = order++; }

std::atomic<int> threadCleanups{0};
std::atomic<void*> threadObjectSeen{nullptr};
int threadObjectSentinel = 0;
void APS5_VABI ThreadDone(void* object) {
    threadObjectSeen.store(object);
    ++threadCleanups;
}

}

int main() {
    void* first = reinterpret_cast<void*>(1);
    void* second = reinterpret_cast<void*>(2);
    Require(cxa_atexit_nid_postfix(First, nullptr, first) == 0);
    Require(cxa_atexit_nid_postfix(Second, nullptr, first) == 0);
    Require(cxa_atexit_nid_postfix(Other, nullptr, second) == 0);
    cxa_finalize_nid_postfix(first);
    Require(secondAt == 0 && firstAt == 1 && otherAt == -1);
    cxa_finalize_nid_postfix(reinterpret_cast<void*>(3));
    Require(otherAt == -1);
    cxa_finalize_nid_postfix(second);
    Require(otherAt == 2);

    std::thread worker([] {
        void* image =
#ifdef _WIN32
            reinterpret_cast<void*>(GetModuleHandleW(nullptr));
#else
            dlopen(nullptr, RTLD_NOW);
#endif
        Require(image != nullptr);
        Require(LibcInternalExtCxaThreadAtexit_nid_postfix(ThreadDone, &threadObjectSentinel, image) == 0);
#ifndef _WIN32
        dlclose(image);
#endif
    });
    worker.join();
    Require(threadCleanups == 1);
    Require(threadObjectSeen.load() == &threadObjectSentinel);

    try {
        init_env_nid_postfix(nullptr);
    } catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), "not implemented") == nullptr);
        return 0;
    }
    return 0;
}
