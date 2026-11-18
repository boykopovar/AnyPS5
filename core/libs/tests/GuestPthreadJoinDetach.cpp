#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int GuestEfault = 14;

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

class UnreadablePage {
public:
    UnreadablePage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr, "map an unreadable page");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "read the page size");
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED, "map an unreadable page");
#endif
    }

    ~UnreadablePage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size);
#endif
    }

    UnreadablePage(const UnreadablePage&) = delete;
    UnreadablePage& operator=(const UnreadablePage&) = delete;

    void* Address() const { return memory; }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

static void CheckBadPointers() {
    UnreadablePage page;
    void* const bad[] = {reinterpret_cast<void*>(0x10), page.Address()};

    Joinee waiting;
    Require(scePthreadCreate(&waiting.thread, nullptr, Wait, &waiting, "waiting") == SCE_OK, "create a thread to time out on");
    for (void* pointer : bad) {
        Require(pthread_timedjoin_np_nid_postfix(waiting.thread, nullptr, static_cast<const KernelTimespec*>(pointer)) == GuestEfault, "an unreadable deadline gives EFAULT");
    }
    Require(pthread_timedjoin_np_nid_postfix(waiting.thread, nullptr, nullptr) == 22, "a null deadline keeps giving EINVAL");
    waiting.release.store(true);
    Require(scePthreadJoin(waiting.thread, nullptr) == SCE_OK, "an unreadable deadline leaves the thread joinable");

    for (void* pointer : bad) {
        Joinee finished;
        Require(scePthreadCreate(&finished.thread, nullptr, Wait, &finished, "finished") == SCE_OK, "create a thread to join");
        finished.release.store(true);
        Require(scePthreadJoin(finished.thread, static_cast<void**>(pointer)) == SCE_KERNEL_ERROR_EFAULT, "an unwritable result gives EFAULT");
        Require(scePthreadJoin(finished.thread, nullptr) == SCE_KERNEL_ERROR_ESRCH, "the thread was joined all the same");
    }

    Joinee posix;
    Require(scePthreadCreate(&posix.thread, nullptr, Wait, &posix, "posix") == SCE_OK, "create a thread for pthread_join");
    posix.release.store(true);
    Require(pthread_join_nid_postfix(posix.thread, reinterpret_cast<void**>(0x10)) == GuestEfault, "pthread_join gives EFAULT");

    Joinee timed;
    Require(scePthreadCreate(&timed.thread, nullptr, Wait, &timed, "timed") == SCE_OK, "create a thread for a timed join");
    timed.release.store(true);
    KernelTimespec farFuture{};
    farFuture.tv_sec = 0x7fffffff;
    Require(pthread_timedjoin_np_nid_postfix(timed.thread, reinterpret_cast<void**>(0x10), &farFuture) == GuestEfault, "pthread_timedjoin_np gives EFAULT for the result");

    Joinee good;
    Require(scePthreadCreate(&good.thread, nullptr, Wait, &good, "good") == SCE_OK, "create a thread to join");
    good.release.store(true);
    void* result = nullptr;
    Require(scePthreadJoin(good.thread, &result) == SCE_OK && result == &good, "a usable result still receives the return value");
}

int main() {
    CheckBadPointers();
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
