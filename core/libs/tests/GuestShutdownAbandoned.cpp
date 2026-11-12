#include "prx/libc/include/ShutdownJoin.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef SHUTDOWN_JOIN_TEST_STUB
#include <unistd.h>
static std::atomic<bool> abandonedFlag{false};
extern "C" bool LibcHostShutdownAbandoned_nid_no_patch() { return abandonedFlag.load(std::memory_order_acquire); }
extern "C" void LibcNoteHostShutdownAbandoned_nid_no_patch() { abandonedFlag.store(true, std::memory_order_release); }
extern "C" int LibcExitCode_nid_no_patch() { return 42; }
extern "C" [[noreturn]] void LibcTerminate_nid_no_patch(int code) { _exit(code); }
#else
#include "prx/libc/include/Shutdown.hpp"
#endif

static void Require(bool value) { if (!value) std::abort(); }

static int LockCase() {
    std::mutex mutex;
    mutex.lock();
    LibcNoteHostShutdownAbandoned_nid_no_patch();
    LockOrEndIfHostAbandoned(mutex, LibcExitCode_nid_no_patch());
    return 2;
}

static int LateJoinCase() {
    std::atomic<bool> returned{false};
    std::thread worker([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        LibcNoteHostShutdownAbandoned_nid_no_patch();
        std::mutex held;
        held.lock();
        held.lock();
    });
    JoinWhenReturnedOrEndIfAbandoned(worker, returned, 42);
    return 2;
}

static int JoinCase() {
    std::atomic<bool> finished{false};
    std::thread worker([&finished] { finished.store(true, std::memory_order_release); });
    while (!finished.load(std::memory_order_acquire)) std::this_thread::yield();
    JoinOrEndIfHostAbandoned(worker, LibcExitCode_nid_no_patch());
    Require(!worker.joinable());
    Require(!LibcHostShutdownAbandoned_nid_no_patch());
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "lock") == 0) return LockCase();
    if (argc == 2 && std::strcmp(argv[1], "join") == 0) return JoinCase();
    Require(JoinCase() == 0);
    const auto started = std::chrono::steady_clock::now();
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        const int status = LateJoinCase();
        _exit(status);
    }
    int status = 0;
    for (;;) {
        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        Require(waited == 0);
        if (std::chrono::steady_clock::now() - started > std::chrono::seconds(3)) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            std::abort();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 42);
    return 0;
}
