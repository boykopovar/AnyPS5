#include <Testing/Test.hpp>

#include <pthread.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern "C" void CallWithCleanup(void (*function)(), int* count);
extern "C" void CallAroundCleanup(void (*before)(), void (*within)(), int* count);
extern "C" void (*_ZSt13set_terminatePFvvE_nid_postfix(void (*)()))();

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int thrownValue = 42;
constexpr int terminatedWithoutCleanup = 61;
constexpr int terminatedAfterCleanup = 62;
constexpr int notTerminated = 63;

int cleanups;

struct Counted {
    ~Counted() { ++cleanups; }
};

[[gnu::noinline]] void Throw() { throw thrownValue; }
[[gnu::noinline]] void Nothing() {}
[[gnu::noinline]] void ThrowThroughCleanup() { CallWithCleanup(Throw, &cleanups); }

void* ThrowWithoutHandler(void*) {
    CallWithCleanup(Throw, &cleanups);
    _exit(notTerminated);
}

void RequireCaughtWithCleanups(void (*function)(), int expectedCleanups) {
    cleanups = 0;
    bool caught = false;
    int value = 0;
    try {
        function();
    } catch (int thrown) {
        caught = true;
        value = thrown;
    }
    Require(caught, "the exception reaches the host handler");
    RequireEqual(value, thrownValue, "caught value");
    RequireEqual(cleanups, expectedCleanups, "cleanups run");
}

const Case singleCleanup{"CCleanup_ThrowInsideCleanupScope_RunsCleanupOnce", [] {
    RequireCaughtWithCleanups([] { CallWithCleanup(Throw, &cleanups); }, 1);
}};

const Case nestedCleanups{"CCleanup_ThrowThroughNestedCleanupScopes_RunsBothCleanups", [] {
    RequireCaughtWithCleanups([] { CallWithCleanup(ThrowThroughCleanup, &cleanups); }, 2);
}};

const Case beforeScope{"CCleanup_ThrowBeforeCleanupVariable_SkipsCleanup", [] {
    RequireCaughtWithCleanups([] { CallAroundCleanup(Throw, Nothing, &cleanups); }, 0);
}};

const Case afterScope{"CCleanup_ThrowAfterCleanupVariable_RunsCleanup", [] {
    RequireCaughtWithCleanups([] { CallAroundCleanup(Nothing, Throw, &cleanups); }, 1);
}};

const Case mixedFrames{"CCleanup_HostDestructorAboveCleanupFrame_RunsBoth", [] {
    RequireCaughtWithCleanups([] {
        Counted counted;
        CallWithCleanup(Throw, &cleanups);
    }, 2);
}};

const Case uncaught{"CCleanup_UncaughtException_TerminatesWithoutRunningCleanup", [] {
    const pid_t child = fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        cleanups = 0;
        _ZSt13set_terminatePFvvE_nid_postfix([] { _exit(cleanups == 0 ? terminatedWithoutCleanup : terminatedAfterCleanup); });
        pthread_t thread;
        if (pthread_create(&thread, nullptr, ThrowWithoutHandler, nullptr) != 0) _exit(notTerminated);
        pthread_join(thread, nullptr);
        _exit(notTerminated);
    }
    int status = 0;
    RequireEqual(waitpid(child, &status, 0), child, "wait for the child");
    Require(WIFEXITED(status), "child exits through the terminate handler");
    RequireEqual(WEXITSTATUS(status), terminatedWithoutCleanup,
        "terminate handler status (62 means cleanup ran, 63 means no termination)");
}};

} // namespace
