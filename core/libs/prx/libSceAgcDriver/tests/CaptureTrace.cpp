#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver {

struct CaptureTraceTestAccess {
    static void QueueBackpressure();
};

}

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void RemoveEnvironmentVariable(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

bool FileExists(const char* path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

std::string ReadFile(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    Check(stream.is_open(), "the trace file was not created at the expected path");
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool WaitForRecord(const char* path, const std::string& record) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (FileExists(path) && ReadFile(path).find(record) != std::string::npos) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

const char* RequiredPathVariable() {
    const char* configured = std::getenv("APS5_CAPTURE_TRACE_PATH");
    Check(configured != nullptr && configured[0] != '\0',
          "this case requires a non-empty APS5_CAPTURE_TRACE_PATH");
    return configured;
}

void PathDefault() {
    RemoveEnvironmentVariable("APS5_CAPTURE_TRACE_PATH");
    Check(std::string(AgcDriver::CaptureTracePath()) == "capture-trace.log",
          "an absent APS5_CAPTURE_TRACE_PATH must select capture-trace.log");
}

void PathEmpty() {
    const char* configured = std::getenv("APS5_CAPTURE_TRACE_PATH");
    Check(configured != nullptr && configured[0] == '\0',
          "this case requires an empty APS5_CAPTURE_TRACE_PATH");
    Check(std::string(AgcDriver::CaptureTracePath()) == "capture-trace.log",
          "an empty APS5_CAPTURE_TRACE_PATH must select capture-trace.log");
}

void PathExplicit() {
    const char* configured = RequiredPathVariable();
    Check(std::string(AgcDriver::CaptureTracePath()) == std::string(configured),
          "a non-empty APS5_CAPTURE_TRACE_PATH must be selected verbatim");
}

void ExplicitWrites() {
    const char* configured = RequiredPathVariable();
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    AgcDriver::CaptureTrace::Log("capture-trace-explicit label=%d", 7);
    Check(WaitForRecord(configured, "capture-trace-explicit label=7"),
          "the configured trace path did not receive the capture record");
    std::remove(configured);
}

void DefaultWrites() {
    RemoveEnvironmentVariable("APS5_CAPTURE_TRACE_PATH");
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    Check(std::string(AgcDriver::CaptureTracePath()) == "capture-trace.log",
          "an absent APS5_CAPTURE_TRACE_PATH must keep the default path");
    AgcDriver::CaptureTrace::Log("capture-trace-default label=%d", 5);
    Check(WaitForRecord("capture-trace.log", "capture-trace-default label=5"),
          "the default trace path did not receive the capture record");
    std::remove("capture-trace.log");
}

void OpenFailure() {
    const char* configured = RequiredPathVariable();
    Check(!FileExists(configured), "the configured trace path must not already exist");
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    bool threw = false;
    try {
        AgcDriver::CaptureTrace::Log("capture-trace-failure label=%d", 9);
    } catch (const std::exception& error) {
        threw = true;
        std::cerr << "unexpected exception: " << error.what() << '\n';
    }
    Check(!threw, "an unopenable trace path must not throw from Log");
    Check(!FileExists(configured), "an unopenable trace path must not create the requested file");
    Check(!FileExists("capture-trace.log"),
          "an unopenable trace path must not fall back to capture-trace.log");
}

void BestEffortOverflow() {
    const char* configured = RequiredPathVariable();
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    const std::string oversized(4096, 'x');
    bool threw = false;
    try {
        AgcDriver::CaptureTrace::LogBestEffort("capture-trace-best-effort payload=%s", oversized.c_str());
    } catch (...) {
        threw = true;
    }
    Check(!threw, "best-effort capture logging must not propagate formatting failures");
    std::remove(configured);
}

class PausedTraceBuffer final : public std::streambuf {
public:
    bool WaitForWrite() {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(2), [this] { return entered; });
    }

    void Release() {
        {
            std::lock_guard lock(mutex);
            released = true;
        }
        changed.notify_all();
    }

    std::string Contents() {
        std::lock_guard lock(mutex);
        return contents;
    }

protected:
    std::streamsize xsputn(const char* text, std::streamsize size) override {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [this] { return released; });
        contents.append(text, static_cast<std::size_t>(size));
        return size;
    }

    int sync() override {
        return 0;
    }

private:
    std::mutex mutex;
    std::condition_variable changed;
    std::string contents;
    bool entered = false;
    bool released = false;
};

std::string QueueRecord(std::size_t producer, std::size_t item) {
    return "queue producer=" + std::to_string(producer) + " item=" + std::to_string(item) + " payload=" + std::string(1900, 'x');
}

void Disabled() {
    RemoveEnvironmentVariable("APS5_CAPTURE_TRACE");
    Check(!AgcDriver::CaptureTrace::Enabled(),
          "capture tracing must stay disabled when APS5_CAPTURE_TRACE is absent");
    AgcDriver::CaptureTrace::Log("capture-trace-disabled label=%d", 3);
    Check(!FileExists("capture-trace.log"),
          "disabled tracing must not initialize or write capture-trace.log");
}

}

void AgcDriver::CaptureTraceTestAccess::QueueBackpressure() {
    constexpr std::size_t Capacity = 16u * 1024u * 1024u;
    constexpr std::size_t Producers = 4;
    constexpr std::size_t ItemsPerProducer = 64;
    const char* configured = RequiredPathVariable();
    PausedTraceBuffer sink;
    std::exception_ptr failure;
    std::size_t initialItems = 0;
    std::atomic<bool> bounded = true;
    {
        CaptureTrace trace;
        Check(trace.output.is_open(), "the queue test trace path must be writable");
        auto& output = static_cast<std::ostream&>(trace.output);
        auto* original = output.rdbuf(&sink);
        std::vector<std::thread> producers;
        std::vector<std::exception_ptr> producerErrors(Producers);
        std::mutex stateMutex;
        std::condition_variable stateChanged;
        std::size_t attempted = 0;
        std::size_t finished = 0;
        auto append = [&](std::size_t producer, std::size_t item) {
            trace.append(QueueRecord(producer, item));
            std::lock_guard lock(trace.mutex);
            if (trace.pending.size() > Capacity) bounded.store(false);
        };
        try {
            for (; initialItems < 64; ++initialItems) append(0, initialItems);
            Check(sink.WaitForWrite(), "the trace worker did not reach the paused sink");
            for (;;) {
                const auto line = QueueRecord(0, initialItems);
                {
                    std::lock_guard lock(trace.mutex);
                    if (trace.pending.size() + line.size() + 64 > Capacity) break;
                }
                append(0, initialItems++);
            }
            for (std::size_t producer = 0; producer < Producers; ++producer) {
                producers.emplace_back([&, producer] {
                    {
                        std::lock_guard lock(stateMutex);
                        ++attempted;
                    }
                    stateChanged.notify_all();
                    try {
                        for (std::size_t item = 0; item < ItemsPerProducer; ++item) append(producer + 1, item);
                    } catch (...) {
                        producerErrors[producer] = std::current_exception();
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
                Check(stateChanged.wait_for(lock, std::chrono::seconds(2), [&] { return attempted == Producers; }),
                      "the queue producers did not start");
                Check(!stateChanged.wait_for(lock, std::chrono::milliseconds(200), [&] { return finished != 0; }),
                      "a full capture queue must wait for its sink instead of failing or growing");
            }
            sink.Release();
            {
                std::unique_lock lock(stateMutex);
                Check(stateChanged.wait_for(lock, std::chrono::seconds(5), [&] { return finished == Producers; }),
                      "the queue producers did not resume after the sink recovered");
            }
        } catch (...) {
            failure = std::current_exception();
        }
        sink.Release();
        if (failure) {
            std::lock_guard lock(trace.mutex);
            trace.pending.clear();
        }
        trace.spaceAvailable.notify_all();
        for (auto& producer : producers) producer.join();
        {
            std::lock_guard lock(trace.mutex);
            trace.stopping = true;
        }
        trace.changed.notify_one();
        trace.worker.join();
        output.rdbuf(original);
        if (!failure) {
            for (const auto& error : producerErrors) {
                if (error) {
                    failure = error;
                    break;
                }
            }
        }
    }
    if (failure) std::rethrow_exception(failure);
    Check(bounded.load(), "the pending capture queue exceeded 16 MiB");
    std::istringstream records(sink.Contents());
    std::vector<std::size_t> nextItem(Producers + 1, 0);
    std::string record;
    std::size_t sequence = 0;
    while (std::getline(records, record)) {
        std::istringstream fields(record);
        unsigned long long foundSequence = 0;
        std::string elapsed;
        std::string label;
        std::string producerField;
        std::string itemField;
        fields >> foundSequence >> elapsed >> label >> producerField >> itemField;
        Check(foundSequence == ++sequence, "the recovered trace lost or reordered sequence numbers");
        Check(label == "queue" && producerField.starts_with("producer=") && itemField.starts_with("item="),
              "the recovered trace contains an incomplete record");
        const auto producer = std::stoull(producerField.substr(9));
        const auto item = std::stoull(itemField.substr(5));
        Check(producer <= Producers, "the recovered trace contains an unexpected producer");
        Check(item == nextItem[producer]++, "the recovered trace lost or reordered a producer's records");
        const auto prefix = std::to_string(foundSequence) + " " + elapsed + " ";
        Check(record == prefix + QueueRecord(producer, item), "the recovered trace changed a record's payload");
    }
    Check(sequence == initialItems + Producers * ItemsPerProducer, "the recovered trace did not drain every queued record");
    Check(nextItem[0] == initialItems, "the recovered trace lost the initial queued records");
    for (std::size_t producer = 1; producer <= Producers; ++producer) {
        Check(nextItem[producer] == ItemsPerProducer, "the recovered trace lost a producer's queued records");
    }
    std::remove(configured);
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected exactly one test mode argument");
        const std::string mode = argv[1];
        if (mode == "path-default") PathDefault();
        else if (mode == "path-empty") PathEmpty();
        else if (mode == "path-explicit") PathExplicit();
        else if (mode == "explicit-writes") ExplicitWrites();
        else if (mode == "default-writes") DefaultWrites();
        else if (mode == "open-failure") OpenFailure();
        else if (mode == "best-effort-overflow") BestEffortOverflow();
        else if (mode == "queue-backpressure") AgcDriver::CaptureTraceTestAccess::QueueBackpressure();
        else if (mode == "disabled") Disabled();
        else throw std::runtime_error("unknown capture trace test mode: " + mode);
        std::cout << "capture trace tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
