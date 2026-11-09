#include "prx/libc/include/general/LogMacros.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>

namespace {

std::atomic<LogSinkFn> logSink{nullptr};

}

extern "C" std::atomic<LogSinkFn>* LibcLogSink_nid_no_patch() {
    return &logSink;
}

extern "C" long long LibcLogTimestampMicros_nid_no_patch() {
    static const bool timestamps = std::getenv("APS5_LOG_TIMESTAMPS") != nullptr;
    if (!timestamps) return -1;
    using Clock = std::chrono::steady_clock;
    static const auto started = Clock::now();
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count());
}
