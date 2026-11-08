#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace {
std::mutex s_mutex;
std::condition_variable s_cv;
std::queue<std::pair<WKRunLoopFunction, void*>> s_taskQueue;
std::atomic<bool> s_stopRequested{false};
std::atomic<bool> s_isRunning{false};
std::atomic<bool> s_isInitialized{false};
std::thread::id s_mainThreadId;
}

extern "C" {

void APS5_VABI WKRunLoopInitializeMain() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_mainThreadId = std::this_thread::get_id();
    s_isInitialized.store(true, std::memory_order_release);
    s_stopRequested.store(false, std::memory_order_release);
    std::queue<std::pair<WKRunLoopFunction, void*>> emptyQueue;
    std::swap(s_taskQueue, emptyQueue);
}

void APS5_VABI WKRunLoopRunMain() {
    s_isRunning.store(true, std::memory_order_release);
    while (!s_stopRequested.load(std::memory_order_acquire)) {
        std::pair<WKRunLoopFunction, void*> task{nullptr, nullptr};
        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_cv.wait(lock, [] {
                return s_stopRequested.load(std::memory_order_acquire) || !s_taskQueue.empty();
            });
            if (s_stopRequested.load(std::memory_order_acquire)) {
                break;
            }
            if (!s_taskQueue.empty()) {
                task = s_taskQueue.front();
                s_taskQueue.pop();
            }
        }
        if (task.first) {
            task.first(task.second);
        }
    }
    s_isRunning.store(false, std::memory_order_release);
}

void APS5_VABI WKRunLoopStopMain() {
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_stopRequested.store(true, std::memory_order_release);
    }
    s_cv.notify_all();
}

void APS5_VABI WKRunLoopCallOnMainThread(WKRunLoopFunction function, void* context) {
    if (!function) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_taskQueue.push({function, context});
    }
    s_cv.notify_one();
}

}
