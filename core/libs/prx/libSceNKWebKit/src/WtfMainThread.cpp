#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <atomic>
#include <mutex>
#include <thread>

namespace {

std::atomic<bool> s_mainThreadInitialized{false};
std::thread::id s_mainThreadId;
std::mutex s_mainThreadMutex;

}

extern "C" {

void APS5_VABI _ZN3WTF20initializeMainThreadEv() {
    std::lock_guard<std::mutex> lock(s_mainThreadMutex);
    s_mainThreadId = std::this_thread::get_id();
    s_mainThreadInitialized.store(true, std::memory_order_release);
}

bool APS5_VABI _ZN3WTF12isMainThreadEv() {
    if (!s_mainThreadInitialized.load(std::memory_order_acquire)) {
        return true;
    }
    std::lock_guard<std::mutex> lock(s_mainThreadMutex);
    return std::this_thread::get_id() == s_mainThreadId;
}

bool APS5_VABI _ZN3WTF13isMainRunLoopEv() {
    return _ZN3WTF12isMainThreadEv();
}

bool APS5_VABI _ZN3WTF22isMainThreadOrGCThreadEv() {
    return _ZN3WTF12isMainThreadEv();
}

}
