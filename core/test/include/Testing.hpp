#ifndef CORE_TEST_INCLUDE_TESTING_HPP
#define CORE_TEST_INCLUDE_TESTING_HPP

#include <atomic>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string_view>
#include <utility>

inline std::atomic<unsigned>& TestFailureCounter() {
    static std::atomic<unsigned> counter{0};
    return counter;
}

inline void ExitWithTestFailures() {
    if (TestFailureCounter().load(std::memory_order_relaxed) == 0) return;
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(1);
}

inline const bool TestFailureExitHook = std::atexit(ExitWithTestFailures) == 0;

inline void ReportTestFailure(const char* kind, std::string_view message, const std::source_location& location) {
    if (message.empty()) {
        std::fprintf(stderr, "%s:%u: %s\n", location.file_name(), static_cast<unsigned>(location.line()), kind);
    } else {
        std::fprintf(stderr, "%s:%u: %s: %.*s\n", location.file_name(), static_cast<unsigned>(location.line()), kind,
                     static_cast<int>(message.size()), message.data());
    }
    std::fflush(stderr);
}

[[noreturn]] inline void Fail(std::string_view message = {}, const std::source_location& location = std::source_location::current()) {
    ReportTestFailure("requirement failed", message, location);
    std::fflush(stdout);
    std::abort();
}

inline void Require(bool condition, const std::source_location& location = std::source_location::current()) {
    if (!condition) Fail({}, location);
}

inline void Require(bool condition, std::string_view message, const std::source_location& location = std::source_location::current()) {
    if (!condition) Fail(message, location);
}

template <std::invocable TMessage>
inline void Require(bool condition, TMessage&& message, const std::source_location& location = std::source_location::current()) {
    if (condition) return;
    const auto text = std::forward<TMessage>(message)();
    Fail(text, location);
}

inline bool Expect(bool condition, const std::source_location& location = std::source_location::current()) {
    if (condition) return true;
    ReportTestFailure("check failed", {}, location);
    TestFailureCounter().fetch_add(1, std::memory_order_relaxed);
    return false;
}

inline bool Expect(bool condition, std::string_view message, const std::source_location& location = std::source_location::current()) {
    if (condition) return true;
    ReportTestFailure("check failed", message, location);
    TestFailureCounter().fetch_add(1, std::memory_order_relaxed);
    return false;
}

template <std::invocable TMessage>
inline bool Expect(bool condition, TMessage&& message, const std::source_location& location = std::source_location::current()) {
    if (condition) return true;
    const auto text = std::forward<TMessage>(message)();
    return Expect(false, std::string_view(text), location);
}

template <typename TException = std::exception, std::invocable TAction>
inline bool ExpectErr(TAction&& action, std::string_view message, const std::source_location& location = std::source_location::current()) {
    try {
        std::forward<TAction>(action)();
    } catch (const TException&) {
        return true;
    }
    return Expect(false, message.empty() ? std::string_view("the expected exception was not thrown") : message, location);
}

template <typename TException = std::exception, std::invocable TAction>
inline bool ExpectErr(TAction&& action, const std::source_location& location = std::source_location::current()) {
    return ExpectErr<TException>(std::forward<TAction>(action), std::string_view(), location);
}

inline unsigned Failures() {
    return TestFailureCounter().load(std::memory_order_relaxed);
}

[[noreturn]] inline void Skip(std::string_view reason) {
    std::printf("skipped, %.*s\n", static_cast<int>(reason.size()), reason.data());
    std::fflush(stdout);
    std::fflush(stderr);
    std::exit(77);
}

#endif
