#pragma once

#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/LogMacros.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace AgcDriver {

struct LogLiteral {
    const char* text;
};

namespace AsyncLogDetail {

template<typename T>
inline constexpr bool Deferrable = std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, LogLiteral>;

inline const char* Unwrap(LogLiteral value) {
    return value.text;
}

template<typename T>
T Unwrap(T value) {
    return value;
}

class Formatter {
public:
    virtual ~Formatter() = default;
    virtual void Format(std::string& out) const = 0;
};

template<typename... TArgs>
class Bound final : public Formatter {
public:
    Bound(const char* format, TArgs... args) : format(format), values(args...) {}

    void Format(std::string& out) const override {
        std::apply([&](const auto&... value) { Append(out, value...); }, values);
    }

private:
    void Append(std::string& out, const auto&... value) const {
        char small[512];
        const int size = std::snprintf(small, sizeof(small), format, Unwrap(value)...);
        if (size < 0) return;
        if (static_cast<std::size_t>(size) < sizeof(small)) {
            out.append(small, static_cast<std::size_t>(size));
            return;
        }
        std::string large(static_cast<std::size_t>(size) + 1, '\0');
        std::snprintf(large.data(), large.size(), format, Unwrap(value)...);
        out.append(large.data(), static_cast<std::size_t>(size));
    }

    const char* format;
    std::tuple<TArgs...> values;
};

struct Entry {
    std::FILE* stream = nullptr;
    long long micros = -1;
    std::string text;
    std::unique_ptr<Formatter> deferred;
    std::size_t cost = 0;
};

}

class AsyncLog {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::size_t Capacity = 16u * 1024u * 1024u;

    static AsyncLog& Get() {
        static AsyncLog* const instance = new AsyncLog();
        return *instance;
    }

    int Write(std::FILE* stream, const char* text, std::size_t length) {
        const auto start = Clock::now();
        if (!asynchronous) return WriteNow(stream, text, length);
        AsyncLogDetail::Entry entry;
        entry.stream = stream;
        entry.text.assign(text, length);
        entry.cost = sizeof(entry) + length;
        Enqueue(std::move(entry));
        callerNanoseconds.fetch_add(Elapsed(start), std::memory_order_relaxed);
        return 0;
    }

    template<typename... TArgs>
    void Defer(std::FILE* stream, const char* format, TArgs... args) {
        static_assert((AsyncLogDetail::Deferrable<TArgs> && ...), "deferred log arguments are numbers, enums or LogLiteral");
        const auto start = Clock::now();
        const auto micros = LogTimestampMicros();
        auto bound = std::make_unique<AsyncLogDetail::Bound<TArgs...>>(format, args...);
        if (!asynchronous) {
            std::string text;
            if (micros >= 0) AppendTimestamp(text, micros);
            bound->Format(text);
            WriteNow(stream, text.data(), text.size());
            return;
        }
        AsyncLogDetail::Entry entry;
        entry.stream = stream;
        entry.micros = micros;
        entry.deferred = std::move(bound);
        entry.cost = sizeof(entry) + sizeof(AsyncLogDetail::Bound<TArgs...>);
        Enqueue(std::move(entry));
        callerNanoseconds.fetch_add(Elapsed(start), std::memory_order_relaxed);
    }

    bool Admit(const std::string& line) {
        const auto now = Clock::now();
        std::string summary;
        {
            std::lock_guard lock(coalesceMutex);
            auto found = windows.find(line);
            if (found == windows.end()) {
                if (windows.size() >= CoalesceKeys) return true;
                windows.emplace(line, Window{now, 1, 0});
                return true;
            }
            auto& window = found->second;
            if (now - window.first < CoalesceWindow) {
                if (++window.seen <= PrintedRepeats) return true;
                ++window.suppressed;
                coalescedTotal.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            summary = Summary(found->first, window.suppressed);
            window = Window{now, 1, 0};
        }
        if (!summary.empty()) WriteNow(stderr, summary.data(), summary.size());
        return true;
    }

    void Flush() {
        if (!asynchronous) return;
        std::unique_lock lock(mutex);
        const auto target = queued.load(std::memory_order_relaxed);
        idle.wait(lock, [&] { return finished.load(std::memory_order_relaxed) >= target; });
    }

    bool FlushFor(std::chrono::milliseconds limit) {
        if (!asynchronous) return true;
        const auto target = queued.load(std::memory_order_acquire);
        const auto deadline = Clock::now() + limit;
        while (finished.load(std::memory_order_acquire) < target) {
            if (Clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

private:
    struct Window {
        Clock::time_point first;
        std::uint64_t seen;
        std::uint64_t suppressed;
    };

    static constexpr std::size_t CoalesceKeys = 128;
    static constexpr std::uint64_t PrintedRepeats = 3;
    static constexpr std::chrono::seconds CoalesceWindow{10};
    static constexpr std::chrono::seconds StatsWindow{10};
    static constexpr std::chrono::milliseconds Tick{500};
    static constexpr std::chrono::milliseconds EndFlushLimit{2000};

    AsyncLog() {
        asynchronous = std::getenv("APS5_LOG_SYNC") == nullptr;
        const char* stats = std::getenv("APS5_LOG_STATS");
        statsEnabled = stats == nullptr || std::string(stats) != "0";
        lastStats = Clock::now();
        if (!asynchronous) return;
        LogSink().store(&AsyncLog::Sink, std::memory_order_release);
        std::atexit(&AsyncLog::FlushAtEnd);
        std::thread([this] { Run(); }).detach();
        LibcSetEndFlush_nid_no_patch(&AsyncLog::FlushAtEnd);
    }

    static void FlushAtEnd() {
        auto& log = Get();
        if (log.abandoned.load(std::memory_order_acquire)) return;
        if (!log.FlushFor(EndFlushLimit)) log.abandoned.store(true, std::memory_order_release);
    }

    static int Sink(std::FILE* stream, const char* text, std::size_t length) {
        return Get().Write(stream, text, length);
    }

    static std::uint64_t Elapsed(Clock::time_point start) {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
    }

    static void AppendTimestamp(std::string& out, long long micros) {
        char prefix[64];
        const int size = FormatLogTimestamp(prefix, sizeof(prefix), micros);
        if (size > 0) out.append(prefix, static_cast<std::size_t>(size));
    }

    static std::string Summary(const std::string& line, std::uint64_t suppressed) {
        if (suppressed == 0) return {};
        std::string text = line;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        text += " (repeated " + std::to_string(suppressed) + " more times in the last 10 s)\n";
        return text;
    }

    static int WriteNow(std::FILE* stream, const char* text, std::size_t length) {
        std::fwrite(text, 1, length, stream);
        return std::fflush(stream);
    }

    void Enqueue(AsyncLogDetail::Entry&& entry) {
        bool notify = false;
        {
            std::unique_lock lock(mutex);
            space.wait(lock, [&] { return bytes == 0 || bytes + entry.cost <= Capacity; });
            bytes += entry.cost;
            peak = std::max(peak, bytes);
            pending.push_back(std::move(entry));
            queued.fetch_add(1, std::memory_order_release);
            notify = sleeping;
            sleeping = false;
        }
        if (notify) wake.notify_one();
    }

    void Run() noexcept {
        std::deque<AsyncLogDetail::Entry> batch;
        std::string buffer;
        for (;;) {
            {
                std::unique_lock lock(mutex);
                sleeping = true;
                wake.wait_for(lock, Tick, [&] { return !pending.empty(); });
                sleeping = false;
                batch.swap(pending);
                bytes = 0;
            }
            space.notify_all();
            if (!batch.empty()) {
                const auto start = Clock::now();
                const auto count = batch.size();
                try {
                    Drain(batch, buffer);
                } catch (...) {
                    buffer.clear();
                }
                batch.clear();
                loggerNanoseconds.fetch_add(Elapsed(start), std::memory_order_relaxed);
                {
                    std::lock_guard lock(mutex);
                    finished.fetch_add(count, std::memory_order_release);
                }
                idle.notify_all();
            }
            try {
                Housekeeping();
            } catch (...) {
            }
        }
    }

    void Drain(std::deque<AsyncLogDetail::Entry>& batch, std::string& buffer) {
        std::FILE* current = nullptr;
        buffer.clear();
        std::uint64_t outLines = 0;
        std::uint64_t errLines = 0;
        std::uint64_t written = 0;
        const auto emit = [&] {
            if (current != nullptr && !buffer.empty()) {
                written += buffer.size();
                WriteNow(current, buffer.data(), buffer.size());
            }
            buffer.clear();
        };
        for (auto& entry : batch) {
            if (entry.stream != current) {
                emit();
                current = entry.stream;
            }
            if (entry.deferred) {
                if (entry.micros >= 0) AppendTimestamp(buffer, entry.micros);
                entry.deferred->Format(buffer);
            } else {
                buffer += entry.text;
            }
            (entry.stream == stderr ? errLines : outLines) += 1;
            if (buffer.size() >= 65536) emit();
        }
        emit();
        stdoutLines.fetch_add(outLines, std::memory_order_relaxed);
        stderrLines.fetch_add(errLines, std::memory_order_relaxed);
        loggedBytes.fetch_add(written, std::memory_order_relaxed);
    }

    void Housekeeping() {
        const auto now = Clock::now();
        std::vector<std::string> summaries;
        {
            std::lock_guard lock(coalesceMutex);
            for (auto it = windows.begin(); it != windows.end();) {
                if (now - it->second.first >= CoalesceWindow) {
                    const auto summary = Summary(it->first, it->second.suppressed);
                    if (!summary.empty()) summaries.push_back(summary);
                    it = windows.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (const auto& summary : summaries) WriteNow(stderr, summary.data(), summary.size());
        if (now - lastStats < StatsWindow) return;
        const double seconds = std::chrono::duration<double>(now - lastStats).count();
        lastStats = now;
        const auto outLines = stdoutLines.exchange(0, std::memory_order_relaxed);
        const auto errLines = stderrLines.exchange(0, std::memory_order_relaxed);
        const auto written = loggedBytes.exchange(0, std::memory_order_relaxed);
        const auto caller = callerNanoseconds.exchange(0, std::memory_order_relaxed);
        const auto logger = loggerNanoseconds.exchange(0, std::memory_order_relaxed);
        const auto coalesced = coalescedTotal.exchange(0, std::memory_order_relaxed);
        std::size_t high = 0;
        {
            std::lock_guard lock(mutex);
            high = peak;
            peak = bytes;
        }
        if (!statsEnabled || outLines + errLines + coalesced == 0) return;
        char line[512];
        const int size = std::snprintf(line, sizeof(line), "[log] %.1f s: %llu lines (%llu stdout, %llu stderr), %.1f KiB written, %llu repeats coalesced; callers spent %.2f ms queueing and formatting (%.3f%% of one thread), logger thread %.2f ms, queue peak %.1f KiB\n", seconds, static_cast<unsigned long long>(outLines + errLines), static_cast<unsigned long long>(outLines), static_cast<unsigned long long>(errLines), static_cast<double>(written) / 1024.0, static_cast<unsigned long long>(coalesced), static_cast<double>(caller) / 1e6, static_cast<double>(caller) / 1e9 / seconds * 100.0, static_cast<double>(logger) / 1e6, static_cast<double>(high) / 1024.0);
        if (size > 0) WriteNow(stderr, line, std::min(static_cast<std::size_t>(size), sizeof(line) - 1));
    }

    bool asynchronous = true;
    bool statsEnabled = true;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable space;
    std::condition_variable idle;
    std::deque<AsyncLogDetail::Entry> pending;
    std::size_t bytes = 0;
    std::size_t peak = 0;
    bool sleeping = false;
    std::atomic<std::uint64_t> queued{0};
    std::atomic<std::uint64_t> finished{0};
    std::atomic<bool> abandoned{false};
    std::mutex coalesceMutex;
    std::map<std::string, Window> windows;
    Clock::time_point lastStats;
    std::atomic<std::uint64_t> stdoutLines{0};
    std::atomic<std::uint64_t> stderrLines{0};
    std::atomic<std::uint64_t> loggedBytes{0};
    std::atomic<std::uint64_t> callerNanoseconds{0};
    std::atomic<std::uint64_t> loggerNanoseconds{0};
    std::atomic<std::uint64_t> coalescedTotal{0};
};

inline void InstallAsyncLog_nid_no_patch() {
    static_cast<void>(AsyncLog::Get());
}

inline void FlushAsyncLog_nid_no_patch() {
    AsyncLog::Get().Flush();
}

template<typename... TArgs>
void LogCoalescedStderr_nid_no_patch(const char* format, TArgs... args) {
    char small[1024];
    const int size = std::snprintf(small, sizeof(small), format, args...);
    if (size < 0) return;
    std::string line = static_cast<std::size_t>(size) < sizeof(small) ? std::string(small, static_cast<std::size_t>(size)) : std::string(static_cast<std::size_t>(size), '\0');
    if (static_cast<std::size_t>(size) >= sizeof(small)) std::snprintf(line.data(), line.size() + 1, format, args...);
    if (!AsyncLog::Get().Admit(line)) return;
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

}

#define AGC_LOG_DEFERRED(stream, fmt, ...) \
::AgcDriver::AsyncLog::Get().Defer(stream, "[%s:%d %s] " fmt "\n", ::AgcDriver::LogLiteral{_APS5_FILE_}, __LINE__, ::AgcDriver::LogLiteral{TrimNidPostfix(__func__)}, __VA_ARGS__)
