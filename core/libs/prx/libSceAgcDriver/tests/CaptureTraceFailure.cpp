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

namespace AgcDriver {

struct CaptureTraceTestAccess {
    static void WriterFailure(bool flush);
};

}

namespace {

using namespace std::chrono_literals;

[[noreturn]] void SetupFailure(const char* message) {
    std::fprintf(stderr, "capture-trace-failure setup-failed: %s\n", message);
    std::fflush(stderr);
    std::_Exit(87);
}

void Check(bool condition, const char* message) {
    if (!condition) SetupFailure(message);
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

    unsigned WriteCalls() const {
        return writeCalls.load();
    }

    unsigned FlushCalls() const {
        return flushCalls.load();
    }

    bool Failed() const {
        return failed.load();
    }

protected:
    std::streamsize xsputn(const char*, std::streamsize size) override {
        writeCalls.fetch_add(1);
        if (failFlush) return size;
        wait();
        failed.store(true);
        std::fputs("capture-trace-failure sink-short-write\n", stderr);
        std::fflush(stderr);
        return 0;
    }

    int sync() override {
        flushCalls.fetch_add(1);
        if (!failFlush) return 0;
        wait();
        failed.store(true);
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
    std::atomic<unsigned> writeCalls{0};
    std::atomic<unsigned> flushCalls{0};
    std::atomic<bool> failed{false};
    bool entered = false;
    bool released = false;
};

std::string Record(unsigned producer, unsigned item) {
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "failure producer=%02u item=%08u payload=", producer, item);
    return std::string(prefix) + std::string(1900, 'x');
}

}

void AgcDriver::CaptureTraceTestAccess::WriterFailure(bool flush) {
    constexpr std::size_t Capacity = 16u * 1024u * 1024u;
    FailingBuffer sink(flush);
    {
        CaptureTrace trace;
        Check(trace.output.is_open(), "the trace output did not open");
        auto& output = static_cast<std::ostream&>(trace.output);
        auto* original = output.rdbuf(&sink);
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
        std::size_t queuedBytes = 0;
        {
            std::lock_guard lock(trace.mutex);
            Check(trace.pending.size() <= Capacity, "the queue exceeded its bound");
            Check(trace.droppedTotal == 0, "filling the queue unexpectedly dropped a record");
            queuedBytes = trace.pending.size();
        }
        std::atomic<unsigned> attempted{0};
        std::mutex stateMutex;
        std::condition_variable stateChanged;
        unsigned finished = 0;
        std::vector<std::thread> producers;
        for (unsigned producer = 1; producer <= 4; ++producer) {
            producers.emplace_back([&, producer] {
                attempted.fetch_add(1);
                try {
                    trace.append(Record(producer, 0));
                } catch (...) {
                    SetupFailure("a producer threw before the writer failed");
                }
                {
                    std::lock_guard lock(stateMutex);
                    ++finished;
                }
                stateChanged.notify_all();
            });
        }
        {
            std::unique_lock lock(stateMutex);
            Check(stateChanged.wait_for(lock, 2s, [&] { return finished == 4; }),
                  "full-queue producers did not return while the sink was paused");
        }
        for (auto& producer : producers) producer.join();
        {
            std::lock_guard lock(trace.mutex);
            Check(trace.pending.size() == queuedBytes, "a full-queue producer added data to the pending queue");
            Check(trace.droppedTotal == 4, "the four rejected records were not counted exactly once");
        }
        Check(attempted.load() == 4, "the four producers did not run");
        Check(!sink.Failed(), "the sink failed before the full-queue producers returned");
        std::fprintf(stderr, "capture-trace-failure armed mode=%s queueFull=1 attempted=4 finished=4\n", flush ? "flush" : "write");
        std::fflush(stderr);
        sink.Fail();
        trace.worker.join();
        Check(sink.Failed(), "the writer did not reach the requested stream failure");
        {
            std::lock_guard lock(trace.mutex);
            Check(!trace.active, "tracing remained active after the stream failure");
            Check(trace.pending.empty(), "failed tracing retained queued records");
        }
        const auto writes = sink.WriteCalls();
        const auto flushes = sink.FlushCalls();
        Check(writes == 1, "the failed sink received another write");
        Check(flushes == (flush ? 1u : 0u), "the failed sink received an unexpected flush");
        for (unsigned producer = 1; producer <= 4; ++producer) trace.append(Record(producer, 1));
        {
            std::lock_guard lock(trace.mutex);
            Check(!trace.active && trace.pending.empty(), "logging after failure reactivated or queued tracing");
            Check(trace.droppedTotal == 4, "logging after failure changed the queue-loss count");
        }
        Check(sink.WriteCalls() == writes && sink.FlushCalls() == flushes,
              "logging after failure attempted more stream I/O");
        output.exceptions(std::ios::goodbit);
        output.clear();
        output.rdbuf(original);
    }
    std::fprintf(stderr, "capture-trace-failure survived mode=%s queueFull=1 attempted=4 finished=4 inactive=1 empty=1 sinkFailed=1\n",
                 flush ? "flush" : "write");
    std::fflush(stderr);
}

int main(int argc, char** argv) {
    std::set_terminate([] {
        std::fputs("capture-trace-failure unexpected-terminate\n", stderr);
        std::fflush(stderr);
        std::_Exit(86);
    });
    std::thread([] {
        std::this_thread::sleep_for(6s);
        std::fputs("capture-trace-failure watchdog\n", stderr);
        std::fflush(stderr);
        std::_Exit(99);
    }).detach();
    Check(argc == 2, "expected write or flush mode");
    const std::string mode = argv[1];
    Check(mode == "write" || mode == "flush", "unknown failure mode");
    Check(AgcDriver::CaptureTrace::Enabled(), "capture tracing must be enabled");
    const char* path = std::getenv("APS5_CAPTURE_TRACE_PATH");
    Check(path != nullptr && path[0] != '\0', "the test requires an explicit trace path");
    AgcDriver::CaptureTraceTestAccess::WriterFailure(mode == "flush");
    return 0;
}
