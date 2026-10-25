#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int events[2];
char expectedRoot[4096];

void CheckPathsDuringExit() {
    bool passed = false;
    try {
        std::thread worker([&] {
            try {
                const std::filesystem::path root(expectedRoot);
                char cwd[32] = {};
                passed = getcwd_nid_postfix(cwd, sizeof(cwd)) == cwd &&
                    std::strcmp(cwd, "/") == 0 &&
                    ResolvePath_nid_no_patch(".").lexically_relative(root) == "." &&
                    ResolvePath_nid_no_patch("/").lexically_relative(root) == "." &&
                    ResolvePath_nid_no_patch("/lifetime_alias") == root;
                RemovePathAlias_nid_no_patch("/lifetime_alias");
                AddPathAlias_nid_no_patch("/lifetime_alias", expectedRoot);
                passed = passed && ResolvePath_nid_no_patch("/lifetime_alias") == root;
            } catch (const std::exception& error) {
                std::fprintf(stderr, "path services during exit: %s\n", error.what());
                passed = false;
            } catch (...) { passed = false; }
        });
        worker.join();
    } catch (...) { passed = false; }
    const char result = passed ? 'P' : 'F';
    static_cast<void>(!write(events[1], &result, 1));
}

bool PathsSurviveExit() {
    if (pipe(events) != 0) return false;
    const pid_t child = fork();
    if (child < 0) {
        close(events[0]);
        close(events[1]);
        return false;
    }
    if (child == 0) {
        close(events[0]);
        const auto root = std::filesystem::canonical(std::filesystem::current_path()).string();
        if (root.size() >= sizeof(expectedRoot)) std::_Exit(2);
        std::memcpy(expectedRoot, root.c_str(), root.size() + 1);
        if (std::atexit(CheckPathsDuringExit) != 0) std::_Exit(2);
        ResolvePath_nid_no_patch("/");
        AddPathAlias_nid_no_patch("/lifetime_alias", expectedRoot);
        LibcExit_nid_no_patch(0);
    }
    close(events[1]);
    char result = 0;
    const ssize_t count = read(events[0], &result, 1);
    close(events[0]);
    int status = 0;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0 && count == 1 && result == 'P';
}

}

int main() {
    if (!PathsSurviveExit()) {
        std::fputs("guest path services must remain available during process exit\n", stderr);
        return 1;
    }
    return 0;
}
