#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#ifdef __APPLE__
#include <cstdint>
#include <mach-o/dyld.h>
#include <vector>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

#ifdef __APPLE__
std::string Executable() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    if (_NSGetExecutablePath(executable.data(), &size) != 0) return {};
    return executable.data();
}
#else
std::string Executable() {
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error).string();
}
#endif

void RunChild(const std::string& executable, const char* mode, const char* envValue) {
    const pid_t pid = fork();
    Require(pid >= 0, "fork failed for mimalloc restart child");
    if (pid == 0) {
        unsetenv("APS5_MIMALLOC_RESTARTED");
        if (envValue == nullptr) unsetenv("APS5_MIMALLOC");
        else setenv("APS5_MIMALLOC", envValue, 1);
        char* arguments[] = {const_cast<char*>(executable.c_str()), const_cast<char*>(mode), nullptr};
        execv(executable.c_str(), arguments);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        Require(errno == EINTR, "waitpid failed for mimalloc restart child");
    }
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "mimalloc restart child failed");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "usage: mimalloc_restart_tests off|on|once|log");
    const std::string_view mode = argv[1];
    Require(std::getenv("APS5_MIMALLOC_RESTARTED") == nullptr, "the restart marker stayed in the environment");
    using InHeap = bool (*)(const void*);
    const auto inHeap = reinterpret_cast<InHeap>(dlsym(RTLD_DEFAULT, "mi_is_in_heap_region"));
    if (mode != "on" && mode != "log" && mode != "child-on") {
        Require(inHeap == nullptr, "mimalloc was loaded without APS5_MIMALLOC or after a restart");
        if (mode == "off") {
            const auto executable = Executable();
            Require(!executable.empty(), "could not resolve test executable path");
            const char* falsyValues[] = {"", "0", "false", "FALSE", "off", "no"};
            for (const char* value : falsyValues) RunChild(executable, "child-off", value);
            const char* truthyValues[] = {"true", "on"};
            for (const char* value : truthyValues) RunChild(executable, "child-on", value);
        }
        return 0;
    }
    Require(inHeap != nullptr, "APS5_MIMALLOC did not load mimalloc");
    void* block = std::malloc(64);
    const auto object = std::make_unique<int>(1);
    const auto array = std::make_unique<int[]>(64);
    Require(inHeap(block), "malloc did not allocate through mimalloc");
    Require(inHeap(object.get()) && inHeap(array.get()), "operator new did not allocate through mimalloc");
    std::free(block);
    if (mode == "log") std::printf("mimalloc_restart_tests: the restarted process writes to its standard output\n");
    return 0;
}
