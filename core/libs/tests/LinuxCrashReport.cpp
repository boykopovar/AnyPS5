#include "prx/libc/include/Api.hpp"
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

[[gnu::noinline]] void WriteNearNull() {
    auto* volatile target = reinterpret_cast<volatile int*>(std::uintptr_t{0x18});
    *target = 1;
}

[[gnu::noinline]] void Trap() {
    __builtin_trap();
}

void SendSegv() {
    kill(getpid(), SIGSEGV);
}

struct Outcome {
    std::string output;
    int signal = 0;
};

Outcome RunChild(void (*crash)()) {
    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) std::exit(2);
    const pid_t child = fork();
    if (child < 0) std::exit(2);
    if (child == 0) {
        dup2(pipeEnds[1], STDERR_FILENO);
        close(pipeEnds[0]);
        close(pipeEnds[1]);
        crash();
        _exit(0);
    }
    close(pipeEnds[1]);
    Outcome outcome;
    char buffer[4096];
    for (ssize_t got; (got = read(pipeEnds[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(got));
    close(pipeEnds[0]);
    int status = 0;
    waitpid(child, &status, 0);
    outcome.signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    return outcome;
}

bool Has(const Outcome& outcome, const char* text) {
    return outcome.output.find(text) != std::string::npos;
}

}

int main(int, char** argv) {
    if (__error_nid_postfix() == nullptr) return 1;
    const char* slash = std::strrchr(argv[0], '/');
    const std::string program = std::string(slash == nullptr ? argv[0] : slash + 1) + "+0x";

    const Outcome fault = RunChild(WriteNearNull);
    Expect(fault.signal == SIGSEGV, "a write near address 0 did not end the process with SIGSEGV");
    Expect(Has(fault, "[libc] fatal SIGSEGV (code 1) at address 0x0000000000000018 in thread "), "the fault report does not name SIGSEGV, SEGV_MAPERR and the address");
    Expect(Has(fault, "  rip 0x") && Has(fault, program.c_str()), "the fault report does not place rip in the test executable");
    Expect(Has(fault, "  rax 0x") && Has(fault, " r15 0x"), "the fault report does not list the registers");
    Expect(Has(fault, "  frame chain from rbp:") && Has(fault, "  stack words that point at code, from rsp:"), "the fault report does not walk the stack");

    const Outcome trap = RunChild(Trap);
    Expect(trap.signal == SIGILL, "a trap instruction did not end the process with SIGILL");
    Expect(Has(trap, "[libc] fatal SIGILL"), "the trap is not reported");

    const Outcome sent = RunChild(SendSegv);
    Expect(sent.signal == SIGSEGV, "a SIGSEGV sent with kill did not end the process with SIGSEGV");
    Expect(Has(sent, "[libc] fatal SIGSEGV (code 0)"), "a SIGSEGV sent with kill is not reported");

    if (failures != 0) {
        std::fprintf(stderr, "%d crash report checks failed\n", failures);
        return 1;
    }
    std::printf("linux crash report tests passed\n");
    return 0;
}
