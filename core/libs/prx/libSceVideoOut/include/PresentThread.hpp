#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_PRESENTTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_PRESENTTHREAD_HPP

#include <atomic>
#include <functional>
#include <future>
#include <stop_token>
#include <thread>
#include <utility>
#include "prx/libc/include/HostMainThread.hpp"

class PresentThread {
public:
    static bool OnHostMainThread() {
#ifdef __APPLE__
        return HostMainThreadServing_nid_no_patch();
#else
        return false;
#endif
    }

    template<typename Function>
    void Start(Function function) {
        if (!OnHostMainThread()) {
            thread = std::jthread(std::move(function));
            return;
        }
        onMain = true;
        finished = done.get_future();
        auto* task = new std::function<void()>([this, function = std::move(function)]() mutable {
            runner.store(std::this_thread::get_id());
            function(stop.get_token());
            done.set_value();
        });
        HostMainThreadPost_nid_no_patch([](void* context) {
            auto* run = static_cast<std::function<void()>*>(context);
            (*run)();
            delete run;
        }, task);
    }

    void request_stop() {
        if (onMain) stop.request_stop();
        else thread.request_stop();
    }

    bool joinable() const { return onMain ? finished.valid() : thread.joinable(); }

    void join() {
        if (!onMain) {
            thread.join();
            return;
        }
        finished.wait();
        finished = {};
    }

    std::thread::id get_id() const { return onMain ? runner.load() : thread.get_id(); }

private:
    std::jthread thread;
    std::stop_source stop;
    std::promise<void> done;
    std::future<void> finished;
    std::atomic<std::thread::id> runner;
    bool onMain = false;
};

#endif
