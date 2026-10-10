#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

extern "C" void CallWithCleanup(void (*function)(), int* count);
extern "C" void CallAroundCleanup(void (*before)(), void (*within)(), int* count);
extern "C" void (*_ZSt13set_terminatePFvvE_nid_postfix(void (*)()))();

int cleanups;

struct Counted { ~Counted() { ++cleanups; } };

[[gnu::noinline]] void Throw() { throw 42; }
[[gnu::noinline]] void Nothing() {}
[[gnu::noinline]] void ThrowThroughCleanup() { CallWithCleanup(Throw, &cleanups); }

bool Caught(void (*function)(), int expectedCleanups) {
    cleanups = 0;
    try {
        function();
    } catch (int value) {
        return value == 42 && cleanups == expectedCleanups;
    }
    return false;
}

bool TerminatesWithoutCleanup() {
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        cleanups = 0;
        _ZSt13set_terminatePFvvE_nid_postfix([] { _exit(cleanups == 0 ? 61 : 62); });
        CallWithCleanup(Throw, &cleanups);
        _exit(63);
    }
    int status;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 61;
}

int main() {
    if (!Caught([] { CallWithCleanup(Throw, &cleanups); }, 1)) return 1;
    if (!Caught([] { CallWithCleanup(ThrowThroughCleanup, &cleanups); }, 2)) return 2;
    if (!Caught([] { CallAroundCleanup(Throw, Nothing, &cleanups); }, 0)) return 3;
    if (!Caught([] { CallAroundCleanup(Nothing, Throw, &cleanups); }, 1)) return 4;
    if (!Caught([] { Counted counted; CallWithCleanup(Throw, &cleanups); }, 2)) return 5;
    if (!TerminatesWithoutCleanup()) return 6;
    std::puts("exception C cleanup tests passed");
    return 0;
}
