#include "prx/libc/include/HostMainThread.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

namespace {

struct Task {
    void (*run)(void*);
    void* context;
};

struct Queue {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Task> tasks;
    std::atomic<bool> serving{false};
};

Queue& State() {
    static auto& queue = *new Queue();
    return queue;
}

}

bool HostMainThreadServing_nid_no_patch() {
    return State().serving.load(std::memory_order_acquire);
}

void HostMainThreadPost_nid_no_patch(void (*task)(void*), void* context) {
    auto& queue = State();
    {
        std::lock_guard lock(queue.mutex);
        queue.tasks.push_back({task, context});
    }
    queue.ready.notify_one();
}

void HostMainThreadServe_nid_no_patch() {
    auto& queue = State();
    queue.serving.store(true, std::memory_order_release);
    for (;;) {
        Task task;
        {
            std::unique_lock lock(queue.mutex);
            queue.ready.wait(lock, [&] { return !queue.tasks.empty(); });
            task = queue.tasks.front();
            queue.tasks.pop_front();
        }
        task.run(task.context);
    }
}
