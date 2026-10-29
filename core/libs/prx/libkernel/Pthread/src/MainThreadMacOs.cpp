#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace {

struct GuestStart {
    void (*entry)(void*, void*);
    void* block;
};

std::atomic<bool> mainLoopRunning {false};

void* RunGuest(void* argument) {
    const auto start = *static_cast<const GuestStart*>(argument);
    start.entry(start.block, nullptr);
    std::fprintf(stderr, "guest entry returned\n");
    std::abort();
}

void KeepAlive(CFRunLoopTimerRef, void*) {}

}

extern "C" {

[[noreturn]] void Aps5StartGuest_nid_no_patch(void (*entry)(void*, void*), void* block) {
    static GuestStart start;
    start = {entry, block};
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) std::abort();
    const auto stackSize = std::max<std::size_t>(pthread_get_stacksize_np(pthread_self()), 8u << 20);
    if (pthread_attr_setstacksize(&attributes, stackSize) != 0) std::abort();
    mainLoopRunning.store(true, std::memory_order_release);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, RunGuest, &start) != 0) {
        std::fprintf(stderr, "cannot start the guest thread\n");
        std::abort();
    }
    pthread_attr_destroy(&attributes);
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(nullptr, 1.0e10, 1.0e10, 0, 0, KeepAlive, nullptr);
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
    for (;;) CFRunLoopRun();
}

void Aps5RunOnMainThread_nid_no_patch(void (*function)(void*), void* context) {
    if (pthread_main_np() != 0 || !mainLoopRunning.load(std::memory_order_acquire)) {
        function(context);
        return;
    }
    dispatch_sync_f(dispatch_get_main_queue(), context, function);
}

}

#endif
