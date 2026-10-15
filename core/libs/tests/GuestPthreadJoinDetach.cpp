#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

extern "C" int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
extern "C" int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
extern "C" int APS5_VABI scePthreadDetach(Pthread thread);

static constexpr int SCE_OK = 0;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

struct Joinee {
    Pthread thread = nullptr;
    std::atomic<bool> release{false};
};

struct Joiner {
    Pthread target = nullptr;
    void* result = nullptr;
    int status = -1;
};

static void* APS5_VABI Wait(void* opaque) {
    auto* joinee = static_cast<Joinee*>(opaque);
    while (!joinee->release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return opaque;
}

static void* APS5_VABI Join(void* opaque) {
    auto* joiner = static_cast<Joiner*>(opaque);
    joiner->status = scePthreadJoin(joiner->target, &joiner->result);
    return nullptr;
}

static bool Joining(Pthread thread) {
    std::lock_guard lock(thread->_join_mtx);
    return thread->_joining;
}

int main() {
    Joinee joinee;
    Require(scePthreadCreate(&joinee.thread, nullptr, Wait, &joinee, "joinee") == SCE_OK, "create the joinee");
    Joiner joiner{joinee.thread};
    Pthread joinerThread = nullptr;
    Require(scePthreadCreate(&joinerThread, nullptr, Join, &joiner, "joiner") == SCE_OK, "create the joiner");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!Joining(joinee.thread)) {
        Require(std::chrono::steady_clock::now() < deadline, "the joiner waits in scePthreadJoin");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(scePthreadDetach(joinee.thread) == SCE_KERNEL_ERROR_EINVAL, "detaching a thread with a joiner returns EINVAL");
    Require(scePthreadJoin(joinee.thread, nullptr) == SCE_KERNEL_ERROR_EOPNOTSUPP, "a second joiner gets EOPNOTSUPP");
    joinee.release.store(true);
    Require(scePthreadJoin(joinerThread, nullptr) == SCE_OK, "join the joiner");
    Require(joiner.status == SCE_OK && joiner.result == &joinee, "the first join completes with the return value");

    Joinee detached;
    Require(scePthreadCreate(&detached.thread, nullptr, Wait, &detached, "detached") == SCE_OK, "create a thread to detach");
    Require(scePthreadDetach(detached.thread) == SCE_OK, "detach a thread without a joiner");
    Require(scePthreadJoin(detached.thread, nullptr) == SCE_KERNEL_ERROR_EINVAL, "joining a detached thread returns EINVAL");
    detached.release.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 0;
}
