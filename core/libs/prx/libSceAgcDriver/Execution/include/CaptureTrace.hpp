#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace AgcDriver {

inline const char* CaptureTracePath() {
    const char* configuredPath = std::getenv("APS5_CAPTURE_TRACE_PATH");
    if (configuredPath != nullptr && configuredPath[0] != '\0') return configuredPath;
    return "capture-trace.log";
}

class CaptureTrace {
public:
    static bool Enabled() {
        static const bool enabled = std::getenv("APS5_CAPTURE_TRACE") != nullptr;
        return enabled;
    }

    template<typename... TArgs>
    static void Log(const char* format, TArgs... args) {
        if (!Enabled()) return;
        std::array<char, 2048> line{};
        const auto size = std::snprintf(line.data(), line.size(), format, args...);
        static CaptureTrace& trace = instance();
        if (size < 0 || static_cast<std::size_t>(size) >= line.size()) {
            trace.drop();
            return;
        }
        trace.append(std::string(line.data(), static_cast<std::size_t>(size)));
    }

    template<typename... TArgs>
    static void LogBestEffort(const char* format, TArgs... args) noexcept {
        try {
            Log(format, args...);
        } catch (...) {
        }
    }

private:
    friend struct CaptureTraceTestAccess;

    static CaptureTrace& instance() {
        static CaptureTrace trace;
        return trace;
    }

    CaptureTrace() {
        const char* path = CaptureTracePath();
        output.open(path, std::ios::binary);
        if (!output.is_open()) {
            std::fprintf(stderr, "Capture trace open failed for %s\n", path);
            return;
        }
        active = true;
        worker = std::thread([this] { run(); });
    }

    ~CaptureTrace() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_one();
        if (worker.joinable()) worker.join();
    }

    void append(std::string line) {
        std::lock_guard lock(mutex);
        if (!active || stopping) return;
        if (droppedPending != 0 || pending.size() + line.size() + 64 > 16 * 1024 * 1024) {
            ++droppedPending;
            ++droppedTotal;
            return;
        }
        appendRecord(pending, line);
        if (line.starts_with("blit ")) flushRequested = true;
        if (pending.size() >= 65536 || flushRequested) changed.notify_one();
    }

    void drop() {
        std::lock_guard lock(mutex);
        if (!active || stopping) return;
        ++droppedPending;
        ++droppedTotal;
    }

    void appendRecord(std::string& destination, const std::string& line) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        destination += std::to_string(++sequence) + " " + std::to_string(elapsed) + "us " + line + "\n";
    }

    void fail(const char* reason) {
        unsigned long long dropped;
        {
            std::lock_guard lock(mutex);
            active = false;
            pending.clear();
            dropped = droppedTotal;
        }
        std::fprintf(stderr, "Capture trace failed: %s (dropped records: %llu)\n", reason, dropped);
    }

    void run() {
        for (;;) {
            std::string batch;
            bool stop;
            {
                std::unique_lock lock(mutex);
                changed.wait_for(lock, std::chrono::milliseconds(100), [this] { return stopping || flushRequested || pending.size() >= 65536; });
                batch.swap(pending);
                if (droppedPending != 0) {
                    appendRecord(batch, "capture-trace-dropped records=" + std::to_string(droppedPending));
                    droppedPending = 0;
                }
                flushRequested = false;
                stop = stopping;
                if (stop) {
                    appendRecord(batch, "capture-trace-summary dropped=" + std::to_string(droppedTotal));
                    active = false;
                }
            }
            if (batch.empty()) continue;
            output.write(batch.data(), static_cast<std::streamsize>(batch.size()));
            if (!output) {
                fail("write failed");
                return;
            }
            output.flush();
            if (!output) {
                fail("flush failed");
                return;
            }
            if (stop) return;
        }
    }

    std::ofstream output;
    std::mutex mutex;
    std::condition_variable changed;
    std::string pending;
    std::thread worker;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    unsigned long long sequence = 0;
    unsigned long long droppedPending = 0;
    unsigned long long droppedTotal = 0;
    bool active = false;
    bool stopping = false;
    bool flushRequested = false;
};

}
