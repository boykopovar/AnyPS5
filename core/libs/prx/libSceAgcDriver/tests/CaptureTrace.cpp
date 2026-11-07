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
#include <stdexcept>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver {

struct CaptureTraceTestAccess {
    static void QueueDrop(bool shutdownOnly, bool cumulative = false);
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

void FormatOverflow() {
    const char* configured = RequiredPathVariable();
    const std::string oversized(4096, 'x');
    AgcDriver::CaptureTrace::Log("capture-trace-format payload=%s", oversized.c_str());
    Check(WaitForRecord(configured, "capture-trace-dropped records=1\n"),
          "an oversized record must be counted instead of throwing or writing a partial record");
    AgcDriver::CaptureTrace::Log("%s", "capture-trace-format-recovered");
    Check(WaitForRecord(configured, "capture-trace-format-recovered\n"),
          "capture tracing did not recover after an oversized record");
    Check(ReadFile(configured).find("capture-trace-format payload=") == std::string::npos,
          "the trace contains a truncated oversized record");
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

    void Pause() {
        std::lock_guard lock(mutex);
        entered = false;
        released = false;
    }

    std::string Contents() {
        std::lock_guard lock(mutex);
        return contents;
    }

    bool WaitForRecord(const std::string& record) {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(2), [this, &record] {
            return contents.find(record) != std::string::npos;
        });
    }

protected:
    std::streamsize xsputn(const char* text, std::streamsize size) override {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [this] { return released; });
        contents.append(text, static_cast<std::size_t>(size));
        changed.notify_all();
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
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "queue producer=%02zu item=%08zu payload=", producer, item);
    return std::string(prefix) + std::string(1900, 'x');
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

void AgcDriver::CaptureTraceTestAccess::QueueDrop(bool shutdownOnly, bool cumulative) {
    constexpr std::size_t Capacity = 16u * 1024u * 1024u;
    constexpr std::size_t Producers = 4;
    const char* configured = RequiredPathVariable();
    PausedTraceBuffer sink;
    std::exception_ptr failure;
    std::size_t initialItems = 0;
    std::atomic<bool> bounded = true;
    const std::size_t rounds = cumulative ? 2 : 1;
    std::size_t lostTotal = 0;
    std::vector<std::string> expectedMarkers;
    std::vector<std::size_t> queuedByRound;
    for (std::size_t round = 0; round < rounds; ++round) {
        const auto lost = Producers * (round == 0 ? 64u : 17u) + (shutdownOnly ? 0u : 1u);
        lostTotal += lost;
        expectedMarkers.push_back("capture-trace-dropped records=" + std::to_string(lost));
    }
    const auto summary = "capture-trace-summary dropped=" + std::to_string(lostTotal);
    {
        CaptureTrace trace;
        Check(trace.output.is_open(), "the queue test trace path must be writable");
        auto& output = static_cast<std::ostream&>(trace.output);
        output.rdbuf(&sink);
        auto append = [&](std::size_t producer, std::size_t item) {
            trace.append(QueueRecord(producer, item));
            std::lock_guard lock(trace.mutex);
            if (trace.pending.size() > Capacity) bounded.store(false);
        };
        for (std::size_t round = 0; round < rounds && !failure; ++round) {
            if (round != 0) sink.Pause();
            const std::size_t itemsPerProducer = round == 0 ? 64 : 17;
            std::vector<std::thread> producers;
            std::vector<std::exception_ptr> producerErrors(Producers);
            std::mutex stateMutex;
            std::condition_variable stateChanged;
            std::size_t attempted = 0;
            std::size_t finished = 0;
            try {
                for (std::size_t item = 0; item < 64; ++item) append(0, initialItems++);
                Check(sink.WaitForWrite(), "the trace worker did not reach the paused sink");
                for (;;) {
                    const auto line = QueueRecord(0, initialItems);
                    {
                        std::lock_guard lock(trace.mutex);
                        if (trace.pending.size() + line.size() + 64 > Capacity) break;
                    }
                    append(0, initialItems++);
                }
                queuedByRound.push_back(initialItems);
                for (std::size_t producer = 0; producer < Producers; ++producer) {
                    producers.emplace_back([&, producer] {
                        {
                            std::lock_guard lock(stateMutex);
                            ++attempted;
                        }
                        stateChanged.notify_all();
                        try {
                            for (std::size_t item = 0; item < itemsPerProducer; ++item) append(producer + 1, item);
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
                    Check(stateChanged.wait_for(lock, std::chrono::seconds(2), [&] { return finished == Producers; }),
                          "a full capture queue must return from every caller while its writer is paused");
                }
                if (!shutdownOnly) trace.append("queue-short-during-loss");
            } catch (...) {
                failure = std::current_exception();
            }
            sink.Release();
            for (auto& producer : producers) producer.join();
            if (!failure) {
                for (const auto& error : producerErrors) {
                    if (error) {
                        failure = error;
                        break;
                    }
                }
            }
            if (!failure && !shutdownOnly) {
                try {
                    Check(sink.WaitForRecord(expectedMarkers[round] + "\n"),
                          "the recovered writer did not report the exact dropped-record count");
                    append(Producers + 1, round);
                    Check(sink.WaitForRecord(QueueRecord(Producers + 1, round) + "\n"),
                          "the recovered writer did not accept a new record");
                } catch (...) {
                    failure = std::current_exception();
                }
            }
        }
    }
    if (failure) std::rethrow_exception(failure);
    Check(bounded.load(), "the pending capture queue exceeded 16 MiB");
    std::istringstream records(sink.Contents());
    std::string record;
    std::size_t sequence = 0;
    std::size_t queued = 0;
    std::size_t recovered = 0;
    std::size_t markers = 0;
    std::size_t summaries = 0;
    while (std::getline(records, record)) {
        std::istringstream fields(record);
        unsigned long long foundSequence = 0;
        std::string elapsed;
        std::string label;
        std::string producerField;
        std::string itemField;
        fields >> foundSequence >> elapsed >> label >> producerField >> itemField;
        Check(foundSequence == ++sequence, "the recovered trace lost or reordered sequence numbers");
        Check(elapsed.ends_with("us"), "the recovered trace lost its elapsed-time field");
        Check(summaries == 0, "the trace contains a record after its shutdown summary");
        const auto prefix = std::to_string(foundSequence) + " " + elapsed + " ";
        const auto body = record.substr(prefix.size());
        if (body.starts_with("capture-trace-dropped records=")) {
            Check(markers < rounds && body == expectedMarkers[markers],
                  "the trace did not report each dropped-record batch exactly once");
            Check(queued == queuedByRound[markers] && recovered == markers,
                  "the dropped marker did not follow the queued records and precede recovery");
            ++markers;
            continue;
        }
        if (body == summary) {
            Check(markers == rounds, "the shutdown total appeared before the loss markers");
            ++summaries;
            continue;
        }
        Check(label == "queue" && producerField.starts_with("producer=") && itemField.starts_with("item="),
              "the recovered trace contains an incomplete record");
        const auto producer = std::stoull(producerField.substr(9));
        const auto item = std::stoull(itemField.substr(5));
        if (producer == 0) {
            Check(markers == recovered && item == queued++, "the trace lost or reordered an accepted queued record");
        } else {
            Check(!shutdownOnly && producer == Producers + 1 && item == recovered && markers == recovered + 1,
                  "the trace retained a dropped record or lost recovery ordering");
            ++recovered;
        }
        Check(record == prefix + QueueRecord(producer, item), "the recovered trace changed a record's payload");
    }
    Check(queued == initialItems, "the recovered trace lost the initial accepted records");
    Check(recovered == (shutdownOnly ? 0u : rounds), "the trace lost or duplicated its recovery records");
    Check(markers == rounds && summaries == 1, "the trace did not report each loss marker and one shutdown total");
    Check(sequence == initialItems + recovered + rounds + 1, "the trace contains an unexpected number of records");
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
        else if (mode == "format-overflow") FormatOverflow();
        else if (mode == "queue-drop") AgcDriver::CaptureTraceTestAccess::QueueDrop(false);
        else if (mode == "queue-drop-shutdown") AgcDriver::CaptureTraceTestAccess::QueueDrop(true);
        else if (mode == "queue-drop-cumulative") AgcDriver::CaptureTraceTestAccess::QueueDrop(false, true);
        else if (mode == "disabled") Disabled();
        else throw std::runtime_error("unknown capture trace test mode: " + mode);
        std::cout << "capture trace tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
