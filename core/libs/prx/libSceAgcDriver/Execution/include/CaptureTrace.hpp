#pragma once
#include "prx/libc/include/HostThread.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace AgcDriver {

inline const char* CaptureTracePath() {
    const char* configuredPath = std::getenv("APS5_CAPTURE_TRACE_PATH");
    if (configuredPath != nullptr && configuredPath[0] != '\0') return configuredPath;
    return "capture-trace.log";
}

class CaptureTraceFilter {
public:
    explicit CaptureTraceFilter(const char* configured) {
        std::string_view remaining = configured == nullptr ? "" : configured;
        while (!remaining.empty()) {
            const auto separator = remaining.find(',');
            const auto prefix = remaining.substr(0, separator);
            if (!prefix.empty()) prefixes.emplace_back(prefix);
            if (separator == std::string_view::npos) break;
            remaining.remove_prefix(separator + 1);
        }
    }

    bool Active() const {
        return !prefixes.empty();
    }

    bool Matches(std::string_view record) const {
        if (!Active()) return true;
        for (const auto& prefix : prefixes) {
            if (record.starts_with(prefix)) return true;
        }
        return false;
    }

    bool MayMatchFormat(std::string_view format) const {
        if (!Active()) return true;
        const auto literal = format.substr(0, format.find('%'));
        for (const auto& prefix : prefixes) {
            if (literal.starts_with(prefix) || std::string_view(prefix).starts_with(literal)) return true;
        }
        return false;
    }

private:
    std::vector<std::string> prefixes;
};

inline const CaptureTraceFilter& CaptureTracePrefixes() {
    static const CaptureTraceFilter filter(std::getenv("APS5_CAPTURE_TRACE_PREFIXES"));
    return filter;
}

class CaptureTrace {
public:
    static bool Enabled() {
        static const bool enabled = std::getenv("APS5_CAPTURE_TRACE") != nullptr;
        return enabled;
    }

    static void Stop() {
        if (auto* trace = current.load(std::memory_order_acquire)) trace->stop();
    }

    template<typename... TArgs>
    static void Log(const char* format, TArgs... args) {
        if (!Enabled()) return;
        const auto& filter = CaptureTracePrefixes();
        if (!filter.MayMatchFormat(format)) return;
        std::array<char, 2048> line{};
        const auto size = std::snprintf(line.data(), line.size(), format, args...);
        if (size >= 0 && static_cast<std::size_t>(size) < line.size() &&
            !filter.Matches(std::string_view(line.data(), static_cast<std::size_t>(size)))) return;
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
        static auto* const trace = [] {
            static CaptureTrace value;
            current.store(&value, std::memory_order_release);
            return &value;
        }();
        return *trace;
    }

    CaptureTrace() {
        const char* path = CaptureTracePath();
        output.open(path, std::ios::binary);
        if (!output.is_open()) {
            std::fprintf(stderr, "Capture trace open failed for %s\n", path);
            return;
        }
        active = true;
        if (CaptureTracePrefixes().Active()) appendRecord(pending, "capture-trace-filter active=1");
        worker = Libc::StartHostThread([this] { run(); });
    }

    ~CaptureTrace() {
        stop();
    }

    void stop() {
        std::call_once(stopped, [this] {
            {
                std::lock_guard lock(mutex);
                stopping = true;
            }
            changed.notify_one();
            if (worker.joinable()) worker.join();
        });
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

    inline static std::atomic<CaptureTrace*> current{nullptr};
    std::once_flag stopped;
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
