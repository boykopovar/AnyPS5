#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal unexpected: %s\n", message);
    std::abort();
}

bool customHandler1Called = false;
void CustomHandler1() {
    customHandler1Called = true;
}

bool customHandler2Called = false;
void CustomHandler2() {
    customHandler2Called = true;
}

struct TestUnexpectedException {};

void ThrowingHandler() {
    throw TestUnexpectedException{};
}

void RuntimeErrorHandler() {
    throw std::runtime_error("unexpected exception message");
}

void CheckUnexpectedHandlerLifecycle() {
    LibcUnexpectedHandler initial = _ZSt14get_unexpectedv_nid_postfix();
    Require(initial != nullptr, "initial unexpected handler is not null");

    LibcUnexpectedHandler prev1 = _ZSt14set_unexpectedPFvvE_nid_postfix(&CustomHandler1);
    Require(prev1 == initial, "setting custom handler returns initial handler");
    Require(_ZSt14get_unexpectedv_nid_postfix() == &CustomHandler1, "get_unexpected returns CustomHandler1");

    LibcUnexpectedHandler prev2 = _ZSt14set_unexpectedPFvvE_nid_postfix(&CustomHandler2);
    Require(prev2 == &CustomHandler1, "setting second handler returns CustomHandler1");
    Require(_ZSt14get_unexpectedv_nid_postfix() == &CustomHandler2, "get_unexpected returns CustomHandler2");

    LibcUnexpectedHandler prevNull = _ZSt14set_unexpectedPFvvE_nid_postfix(nullptr);
    Require(prevNull == &CustomHandler2, "setting null handler returns CustomHandler2");
    LibcUnexpectedHandler restored = _ZSt14get_unexpectedv_nid_postfix();
    Require(restored != nullptr && restored == initial, "setting null handler restores initial default handler");
}

void CheckUnexpectedInvocation() {
    _ZSt14set_unexpectedPFvvE_nid_postfix(&ThrowingHandler);
    Require(_ZSt14get_unexpectedv_nid_postfix() == &ThrowingHandler, "throwing handler installed");

    bool caughtCustom = false;
    try {
        _ZSt10unexpectedv_nid_postfix();
    } catch (const TestUnexpectedException&) {
        caughtCustom = true;
    }
    Require(caughtCustom, "throwing handler exception propagates through unexpected");

    _ZSt14set_unexpectedPFvvE_nid_postfix(&RuntimeErrorHandler);
    bool caughtRuntime = false;
    try {
        _ZSt10unexpectedv_nid_postfix();
    } catch (const std::runtime_error& error) {
        caughtRuntime = std::strcmp(error.what(), "unexpected exception message") == 0;
    }
    Require(caughtRuntime, "runtime error propagates with expected message");

    _ZSt14set_unexpectedPFvvE_nid_postfix(nullptr);
}

#ifndef _WIN32
void CheckUnexpectedTerminatesOnReturn() {
    const pid_t child = fork();
    Require(child >= 0, "fork failed");
    if (child == 0) {
        _ZSt13set_terminatePFvvE_nid_postfix([] { std::_Exit(77); });
        _ZSt14set_unexpectedPFvvE_nid_postfix([] {});
        _ZSt10unexpectedv_nid_postfix();
        std::_Exit(1);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child, "waitpid failed");
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 77, "returning unexpected handler falls through to terminate");
}
#endif

}

int main() {
    CheckUnexpectedHandlerLifecycle();
    CheckUnexpectedInvocation();
#ifndef _WIN32
    CheckUnexpectedTerminatesOnReturn();
#endif
    std::puts("libc internal unexpected tests passed");
    return 0;
}
