#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#endif
#ifdef __linux__
#include <sys/prctl.h>
#endif

namespace AgcDriver {

struct CaptureTraceTestAccess {
    [[noreturn]] static void WriterFailure(bool flush);
};

}

namespace {

using namespace std::chrono_literals;

std::atomic<bool> armed{false};
std::atomic<bool> queueFull{false};
std::atomic<bool> sinkFailed{false};
std::atomic<unsigned> attempted{0};
std::atomic<unsigned> finished{0};
std::terminate_handler originalTerminate = nullptr;

[[noreturn]] void SetupFailure(const char* message) {
    std::fprintf(stderr, "capture-trace-failure setup-failed: %s\n", message);
    std::fflush(stderr);
    std::_Exit(87);
}

void Check(bool condition, const char* message) {
    if (!condition) SetupFailure(message);
}

[[noreturn]] void SavedTerminate() {
    std::fprintf(stderr, "capture-trace-failure terminate armed=%d queueFull=%d attempted=%u finished=%u sinkFailed=%d\n",
                 armed.load() ? 1 : 0, queueFull.load() ? 1 : 0, attempted.load(), finished.load(), sinkFailed.load() ? 1 : 0);
    std::fflush(stderr);
    try {
        originalTerminate();
        std::fputs("capture-trace-failure handler-returned\n", stderr);
    } catch (...) {
        std::fputs("capture-trace-failure handler-threw\n", stderr);
    }
    std::fflush(stderr);
    std::abort();
}

class FailingBuffer final : public std::streambuf {
public:
    explicit FailingBuffer(bool failFlush) : failFlush(failFlush) {}

    bool WaitForWriter() {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, 2s, [this] { return entered; });
    }

    void Fail() {
        {
            std::lock_guard lock(mutex);
            released = true;
        }
        changed.notify_all();
    }

protected:
    std::streamsize xsputn(const char*, std::streamsize size) override {
        if (failFlush) return size;
        wait();
        sinkFailed.store(true);
        std::fputs("capture-trace-failure sink-short-write\n", stderr);
        std::fflush(stderr);
        return 0;
    }

    int sync() override {
        if (!failFlush) return 0;
        wait();
        sinkFailed.store(true);
        std::fputs("capture-trace-failure sink-flush-failed\n", stderr);
        std::fflush(stderr);
        return -1;
    }

private:
    void wait() {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [this] { return released; });
    }

    std::mutex mutex;
    std::condition_variable changed;
    const bool failFlush;
    bool entered = false;
    bool released = false;
};

std::string Record(unsigned producer, unsigned item) {
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "failure producer=%02u item=%08u payload=", producer, item);
    return std::string(prefix) + std::string(1900, 'x');
}

}

[[noreturn]] void AgcDriver::CaptureTraceTestAccess::WriterFailure(bool flush) {
    constexpr std::size_t Capacity = 16u * 1024u * 1024u;
    FailingBuffer sink(flush);
    CaptureTrace trace;
    Check(trace.output.is_open(), "the trace output did not open");
    static_cast<std::ostream&>(trace.output).rdbuf(&sink);
    unsigned item = 0;
    for (; item < 64; ++item) trace.append(Record(0, item));
    Check(sink.WaitForWriter(), "the writer did not reach the failing sink");
    const auto line = Record(1, 0);
    for (;;) {
        {
            std::lock_guard lock(trace.mutex);
            if (trace.pending.size() + line.size() + 64 > Capacity) break;
        }
        trace.append(Record(0, item++));
    }
    std::vector<std::thread> producers;
    for (unsigned producer = 1; producer <= 4; ++producer) {
        producers.emplace_back([&, producer] {
            attempted.fetch_add(1);
            try {
                trace.append(Record(producer, 0));
            } catch (...) {
                SetupFailure("a producer threw before the writer failed");
            }
            finished.fetch_add(1);
        });
    }
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (attempted.load() != 4 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    Check(attempted.load() == 4, "the four producers did not start");
    std::this_thread::sleep_for(200ms);
    Check(finished.load() == 0, "a producer passed the full queue before the sink was released");
    {
        std::lock_guard lock(trace.mutex);
        Check(trace.pending.size() + line.size() + 64 > Capacity, "the producers were not held behind a full queue");
        Check(trace.pending.size() <= Capacity, "the queue exceeded its bound");
        queueFull.store(true);
    }
    armed.store(true);
    std::fprintf(stderr, "capture-trace-failure armed mode=%s queueFull=1 attempted=4 finished=0\n", flush ? "flush" : "write");
    std::fflush(stderr);
    sink.Fail();
    for (auto& producer : producers) producer.join();
    SetupFailure("all producers returned after a fatal writer error");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
    const rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
#endif
#ifdef __linux__
    prctl(PR_SET_DUMPABLE, 0);
#endif
    std::thread([] {
        std::this_thread::sleep_for(6s);
        std::fputs("capture-trace-failure watchdog\n", stderr);
        std::fflush(stderr);
        std::_Exit(99);
    }).detach();
    Check(argc == 2, "expected write, flush or control-abort mode");
    const std::string mode = argv[1];
    Check(mode == "write" || mode == "flush" || mode == "control-abort", "unknown failure mode");
    if (mode == "control-abort") {
        std::fputs("capture-trace-failure control-abort\n", stderr);
        std::fflush(stderr);
        std::abort();
    }
    Check(AgcDriver::CaptureTrace::Enabled(), "capture tracing must be enabled");
    const char* path = std::getenv("APS5_CAPTURE_TRACE_PATH");
    Check(path != nullptr && path[0] != '\0', "the test requires an explicit trace path");
    originalTerminate = std::set_terminate(&SavedTerminate);
    Check(originalTerminate != nullptr, "the runtime did not return its original terminate handler");
    AgcDriver::CaptureTraceTestAccess::WriterFailure(mode == "flush");
}
