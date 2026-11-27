#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <exception>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal _Rng_abort: %s\n", message);
    std::abort();
}

#ifndef _WIN32
void RequireAbort(const char* message, const char* expected) {
    int pipeEnds[2];
    Require(pipe(pipeEnds) == 0, "pipe");
    const pid_t child = fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        close(pipeEnds[0]);
        dup2(pipeEnds[1], 2);
        libSceLibcInternalInventory_237_nid_no_patch(message);
    }
    close(pipeEnds[1]);
    char output[256] = {};
    std::size_t used = 0;
    for (ssize_t got; (got = read(pipeEnds[0], output + used, sizeof(output) - 1 - used)) > 0;) used += static_cast<std::size_t>(got);
    close(pipeEnds[0]);
    int status = 0;
    Require(waitpid(child, &status, 0) == child, "waitpid");
    Require(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "aborts");
    Require(std::strncmp(output, expected, std::strlen(expected)) == 0, "writes the message and a newline to stderr");
}
#endif

}

int main() {
    bool threw = false;
    try {
        libSceLibcInternalInventory_237_nid_no_patch(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a null message throws");
#ifndef _WIN32
    RequireAbort("invalid random engine argument", "invalid random engine argument\n");
    RequireAbort("", "\n");
#endif
    std::puts("_Rng_abort tests passed");
    return 0;
}
