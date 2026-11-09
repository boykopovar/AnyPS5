#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <signal.h>

namespace {

int fixtureDestroyed = 0;
using Exercise = int (*)();
Exercise exercise = nullptr;

void ProbeAfterStaticDestruction() {
    if (fixtureDestroyed != 1 || exercise == nullptr) {
        std::fputs("memory lifetime: fixture finalizer did not run before the exit probe\n", stderr);
        std::_Exit(1);
    }
    if (exercise() != 0) std::_Exit(1);
    std::fputs("memory lifetime: allocation, backing, mapping and explicit free survived native exit\n", stderr);
}

int RunChild(const char* module) {
    if (std::atexit(ProbeAfterStaticDestruction) != 0) return 2;
    auto* handle = dlopen(module, RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        std::fprintf(stderr, "memory lifetime: load failed: %s\n", dlerror());
        std::_Exit(2);
    }
    const auto prepare = reinterpret_cast<int (*)(int*)>(dlsym(handle, "GuestMemoryLifetimePrepare"));
    exercise = reinterpret_cast<Exercise>(dlsym(handle, "GuestMemoryLifetimeExercise"));
    if (prepare == nullptr || exercise == nullptr || prepare(&fixtureDestroyed) != 0) std::_Exit(2);
    std::exit(0);
}

}

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--child") == 0) return RunChild(argv[2]);
    if (argc != 2) return 2;
    const auto child = fork();
    if (child == -1) return 2;
    if (child == 0) {
        execl(argv[0], argv[0], "--child", argv[1], static_cast<char*>(nullptr));
        std::_Exit(2);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    int status = 0;
    for (;;) {
        const auto result = waitpid(child, &status, WNOHANG);
        if (result == child) break;
        if (result < 0) {
            if (errno == EINTR) continue;
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            return 2;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            std::fputs("memory lifetime: exit probe timed out\n", stderr);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::fprintf(stderr, "memory lifetime: child failed during native exit (status=%d)\n", status);
        return 1;
    }
    return 0;
}
