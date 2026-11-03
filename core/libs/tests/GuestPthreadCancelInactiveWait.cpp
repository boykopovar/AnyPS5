#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::_Exit(EXIT_FAILURE);
}

template <class TPredicate>
static void Await(TPredicate predicate, std::source_location location = std::source_location::current()) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(predicate(), location);
}

struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;

    void Park() {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
    }

    bool Entered() {
        std::lock_guard lock(mutex);
        return entered;
    }

    void Release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
};

struct GatedLock {
    std::mutex mutex;
    Gate gate;
    bool first = true;

    void lock() { mutex.lock(); }
    void unlock() {
        mutex.unlock();
        if (first) {
            first = false;
            gate.Park();
        }
    }
};

struct Context {
    GatedLock lock;
    TimedWait::Condition condition;
    PthreadKey key = 0;
    bool timed = false;
    bool destructorWait = false;
    bool naturalReturn = false;
    bool ready = false;
    std::atomic<bool> waitReturned{false};
    std::atomic<bool> returned{false};
    std::atomic<int> cleanup{0};
    std::atomic<int> destructors{0};
};

static void Wait(Context& context) {
    {
        std::unique_lock lock(context.lock);
        const auto deadline = context.timed ? std::optional<std::uint64_t>(TimedWait::DeadlineNanos(60000000)) : std::nullopt;
        while (!context.ready) Require(ThreadCancel::Wait(context.condition, lock, deadline));
    }
    context.waitReturned = true;
    ThreadCancel::Check();
}

static void APS5_VABI DestroySpecific(void* argument) {
    auto& context = *static_cast<Context*>(argument);
    if (context.destructorWait) Wait(context);
    Require(context.waitReturned && context.cleanup == (context.naturalReturn ? 0 : 1));
    ++context.destructors;
}

static void APS5_VABI Cleanup(void* argument) {
    auto& context = *static_cast<Context*>(argument);
    Require(context.cleanup.fetch_add(1) == 0);
    if (!context.destructorWait) Wait(context);
}

static void* APS5_VABI Worker(void* argument) {
    auto& context = *static_cast<Context*>(argument);
    Require(scePthreadSetspecific(context.key, &context) == 0);
    if (context.naturalReturn) {
        context.returned = true;
        return &context;
    }
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, &context, &cleanup);
    scePthreadExit(&context);
    context.returned = true;
    return nullptr;
}

static void Check(bool timed, bool destructorWait, bool naturalReturn, bool posix) {
    Context context;
    context.timed = timed;
    context.destructorWait = destructorWait;
    context.naturalReturn = naturalReturn;
    const int cleanupCount = naturalReturn ? 0 : 1;
    Require(scePthreadKeyCreate(&context.key, reinterpret_cast<pthread_key_destructor_func_t>(DestroySpecific)) == 0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &context, nullptr) == 0);
    Await([&] { return context.lock.gate.Entered(); });
    std::atomic<bool> cancelReturned{false};
    std::thread cancel([&] {
        for (int repeat = 0; repeat < 3; ++repeat)
            Require((posix ? pthread_cancel_nid_postfix(thread) : scePthreadCancel(thread)) == 0);
        cancelReturned = true;
    });
    Await([&] { return cancelReturned.load(); });
    Require(!context.waitReturned && context.returned == naturalReturn && context.destructors == 0);
    Require(context.cleanup == cleanupCount);
    Require(thread->cancelPending.load());
    cancel.join();
    context.lock.gate.Release();
    {
        std::lock_guard lock(context.lock.mutex);
        context.ready = true;
        context.condition.NotifyOne();
    }
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == 0);
    Require(result == &context);
    Require(context.waitReturned && context.returned == naturalReturn && context.cleanup == cleanupCount && context.destructors == 1);
    Require(scePthreadKeyDelete(context.key) == 0);
}

int main(int count, char** arguments) {
    Require(count == 4);
    const std::string_view state(arguments[1]), mode(arguments[2]), api(arguments[3]);
    Require(state == "cleanup" || state == "destructor" || state == "return");
    Require(mode == "untimed" || mode == "timed");
    Require(api == "sce" || api == "posix");
    Check(mode == "timed", state != "cleanup", state == "return", api == "posix");
}
