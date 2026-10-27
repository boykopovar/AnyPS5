#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCETRACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PERFORMANCETRACE_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <condition_variable>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver {
class TraceThread;
}

extern "C" bool AgcDriverTraceRecording_nid_postfix();
extern "C" AgcDriver::TraceThread* AgcDriverTraceThread_nid_postfix();
extern "C" void AgcDriverTraceCount_nid_postfix(const char* scope);

namespace AgcDriver {

class VulkanDevice;

class TraceWindow {
public:
    using Clock = std::chrono::steady_clock;
    enum class Edge { None, Open, Close };

    static std::optional<TraceWindow> Parse(const char* frames, const char* seconds) {
        if (frames == nullptr && seconds == nullptr) return std::nullopt;
        if (frames != nullptr && seconds != nullptr) throw std::invalid_argument("APS5_TRACE_FRAMES and APS5_TRACE_SECONDS cannot be set together");
        const std::string_view name = frames != nullptr ? "APS5_TRACE_FRAMES" : "APS5_TRACE_SECONDS";
        const std::string_view text = frames != nullptr ? frames : seconds;
        const auto colon = text.find(':');
        std::uint64_t start = 0;
        std::uint64_t length = 0;
        if (colon == std::string_view::npos || !number(text.substr(0, colon), start) || !number(text.substr(colon + 1), length) || length == 0)
            throw std::invalid_argument(std::string(name) + "=" + std::string(text) + " is not <start>:<length> with a length above 0");
        return TraceWindow(frames != nullptr, start, length);
    }

    Edge Present(std::uint64_t present, Clock::duration elapsed) {
        if (state == State::Closed) return Edge::None;
        if (state == State::Waiting) {
            if (frames ? present < start : wholeSeconds(elapsed) < start) return Edge::None;
            state = State::Open;
            openedPresent = present;
            openedAt = elapsed;
            return Edge::Open;
        }
        if (frames ? present - openedPresent < length : wholeSeconds(elapsed - openedAt) < length) return Edge::None;
        state = State::Closed;
        return Edge::Close;
    }

    std::string Describe() const {
        return std::string(frames ? "frames " : "seconds ") + std::to_string(start) + ":" + std::to_string(length);
    }

private:
    enum class State { Waiting, Open, Closed };

    TraceWindow(bool frames, std::uint64_t start, std::uint64_t length) : frames(frames), start(start), length(length) {}

    static bool number(std::string_view text, std::uint64_t& value) {
        const auto end = text.data() + text.size();
        const auto [last, error] = std::from_chars(text.data(), end, value);
        return !text.empty() && error == std::errc{} && last == end;
    }

    static std::uint64_t wholeSeconds(Clock::duration elapsed) {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
    }

    bool frames;
    std::uint64_t start;
    std::uint64_t length;
    State state = State::Waiting;
    std::uint64_t openedPresent = 0;
    Clock::duration openedAt{};
};

class TraceInterval {
public:
    using Clock = std::chrono::steady_clock;

    static std::optional<TraceInterval> Parse(const char* seconds) {
        if (seconds == nullptr) return std::nullopt;
        const std::string_view text = seconds;
        std::uint64_t length = 0;
        const auto end = text.data() + text.size();
        const auto [last, error] = std::from_chars(text.data(), end, length);
        if (text.empty() || error != std::errc{} || last != end || length == 0 || length > 31536000)
            throw std::invalid_argument("APS5_TRACE_SUMMARY_EVERY=" + std::string(text) + " is not a whole number of seconds above 0");
        return TraceInterval(length);
    }

    bool Due(Clock::duration elapsed) const {
        return elapsed - begun >= std::chrono::seconds(length);
    }

    void Restart(Clock::duration elapsed) {
        begun = elapsed;
    }

    Clock::duration Begun() const {
        return begun;
    }

    std::string Describe() const {
        return "summary_every " + std::to_string(length);
    }

private:
    explicit TraceInterval(std::uint64_t length) : length(length) {}

    std::uint64_t length;
    Clock::duration begun{};
};

inline thread_local std::uint64_t TraceBatchTag = 0;

class TraceThread {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t DefaultCapacity = 262144;
    static constexpr std::size_t MaximumCapacity = 4194304;
    static constexpr Clock::duration MinimumSlice = std::chrono::microseconds(20);

    static std::size_t ParseCapacity(const char* events) {
        if (events == nullptr) return DefaultCapacity;
        const std::string_view text = events;
        std::size_t capacity = 0;
        const auto end = text.data() + text.size();
        const auto [last, error] = std::from_chars(text.data(), end, capacity);
        if (text.empty() || error != std::errc{} || last != end || capacity == 0 || capacity > MaximumCapacity)
            throw std::invalid_argument("APS5_TRACE_EVENTS=" + std::string(text) + " is not a whole number from 1 through " + std::to_string(MaximumCapacity));
        return capacity;
    }

    struct Slice {
        const char* scope;
        const char* stage;
        Clock::time_point begin;
        Clock::duration length;
        std::uint64_t batch = 0;
    };

    struct Total {
        const char* scope;
        const char* stage;
        Clock::duration length;
        std::uint64_t count;
    };

    struct Snapshot {
        std::string name;
        std::span<const Slice> slices;
        std::vector<Total> totals;
        bool dropped = false;
        std::uint64_t eventsLost = 0;
        std::optional<Clock::time_point> eventsFullAt;
        bool totalsFull = false;
    };

    inline static const std::atomic<bool> AlwaysBuffering{true};

    TraceThread(std::string name, const std::atomic<std::uint64_t>& presents, std::size_t capacity = DefaultCapacity, const std::atomic<bool>& buffering = AlwaysBuffering)
        : name(std::move(name)), presents(presents), buffering(buffering), slices(std::make_unique<Slice[]>(capacity)), capacity(capacity) {}

    TraceThread(const TraceThread&) = delete;
    TraceThread& operator=(const TraceThread&) = delete;

    void Add(const char* scope, const char* stage, Clock::time_point begin, Clock::time_point end) {
        const auto length = end - begin;
        account(scope, stage, length);
        if (!buffering.load(std::memory_order_relaxed)) return;
        const auto present = presents.load(std::memory_order_relaxed);
        if (!sampled || present != sampledPresent) {
            if (sampled) append({nullptr, nullptr, sampleBegin, folded});
            sampled = true;
            sampledPresent = present;
            sampleBegin = begin;
            folded = {};
        }
        if (length < MinimumSlice) folded += length;
        else append({scope, stage, begin, length, TraceBatchTag});
    }

    Snapshot Collect() const {
        Snapshot snapshot{name, {slices.get(), used.load(std::memory_order_acquire)}, {}, false};
        for (const auto& entry : entries) {
            const auto* scope = entry.scope.load(std::memory_order_acquire);
            if (scope == nullptr) continue;
            snapshot.totals.push_back({scope, entry.stage.load(std::memory_order_relaxed), Clock::duration(entry.length.load(std::memory_order_relaxed)), entry.count.load(std::memory_order_relaxed)});
        }
        snapshot.eventsLost = lost.load(std::memory_order_acquire);
        if (snapshot.eventsLost != 0) snapshot.eventsFullAt = Clock::time_point(Clock::duration(firstLost.load(std::memory_order_relaxed)));
        snapshot.totalsFull = totalsFull.load(std::memory_order_relaxed);
        snapshot.dropped = snapshot.eventsLost != 0 || snapshot.totalsFull;
        return snapshot;
    }

private:
    struct Entry {
        std::atomic<const char*> scope{nullptr};
        std::atomic<const char*> stage{nullptr};
        std::atomic<Clock::rep> length{0};
        std::atomic<std::uint64_t> count{0};
    };

    void append(const Slice& slice) {
        const auto index = used.load(std::memory_order_relaxed);
        if (index == capacity) {
            const auto count = lost.load(std::memory_order_relaxed);
            if (count == 0) firstLost.store(slice.begin.time_since_epoch().count(), std::memory_order_relaxed);
            lost.store(count + 1, std::memory_order_release);
            return;
        }
        slices[index] = slice;
        used.store(index + 1, std::memory_order_release);
    }

    void account(const char* scope, const char* stage, Clock::duration length) {
        const auto key = reinterpret_cast<std::uintptr_t>(scope) ^ (reinterpret_cast<std::uintptr_t>(stage) >> 3);
        const auto hash = static_cast<std::size_t>((static_cast<std::uint64_t>(key) * 0x9e3779b97f4a7c15ull) >> 55);
        for (std::size_t probe = 0; probe < entries.size(); ++probe) {
            auto& entry = entries[(hash + probe) % entries.size()];
            const auto* current = entry.scope.load(std::memory_order_relaxed);
            if (current == nullptr) {
                entry.stage.store(stage, std::memory_order_relaxed);
                entry.scope.store(scope, std::memory_order_release);
            } else if (current != scope || entry.stage.load(std::memory_order_relaxed) != stage) {
                continue;
            }
            entry.length.store(entry.length.load(std::memory_order_relaxed) + length.count(), std::memory_order_relaxed);
            entry.count.store(entry.count.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
            return;
        }
        totalsFull.store(true, std::memory_order_relaxed);
    }

    const std::string name;
    const std::atomic<std::uint64_t>& presents;
    const std::atomic<bool>& buffering;
    const std::unique_ptr<Slice[]> slices;
    const std::size_t capacity;
    std::atomic<std::size_t> used{0};
    std::atomic<std::uint64_t> lost{0};
    std::atomic<Clock::rep> firstLost{0};
    std::atomic<bool> totalsFull{false};
    std::array<Entry, 512> entries;
    bool sampled = false;
    std::uint64_t sampledPresent = 0;
    Clock::time_point sampleBegin{};
    Clock::duration folded{};
};

struct TraceHost {
    std::string cpu;
    std::uint32_t threads = 0;
    std::uint64_t memoryMiB = 0;
};

struct TraceTitle {
    std::string id;
    std::string name;
    std::string contentVersion;
};

struct TraceSummary {
    std::string commit;
    std::optional<bool> dirty;
    std::vector<std::pair<std::string, std::string>> environment;
    std::string gpu;
    std::string driver;
    std::string os;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string window;
    std::uint64_t firstPresent = 0;
    double gpuBusyMs = 0;
    std::string trace;
    std::optional<std::pair<double, double>> interval;
    std::optional<TraceHost> host;
    std::optional<TraceTitle> title;
};

struct TraceBenchInterval {
    double startS = 0;
    double endS = 0;
    std::uint64_t firstPresent = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    double gpuBusyMs = 0;
    std::vector<std::uint32_t> frameUs;
};

using TraceBenchIntervals = std::vector<std::shared_ptr<const TraceBenchInterval>>;

class TraceBench {
public:
    static constexpr std::size_t MaximumIntervals = 240;

    static void Keep(TraceBenchIntervals& kept, std::shared_ptr<const TraceBenchInterval> interval, std::uint64_t& dropped) {
        kept.push_back(std::move(interval));
        if (kept.size() <= MaximumIntervals) return;
        const auto excess = kept.size() - MaximumIntervals;
        kept.erase(kept.begin(), kept.begin() + static_cast<std::ptrdiff_t>(excess));
        dropped += excess;
    }

    static bool Parse(const char* bench, bool summaryEvery) {
        if (bench == nullptr) return false;
        if (std::string_view(bench) != "1") throw std::invalid_argument("APS5_TRACE_BENCH=" + std::string(bench) + " is not 1");
        if (!summaryEvery) throw std::invalid_argument("APS5_TRACE_BENCH=1 needs APS5_TRACE_SUMMARY_EVERY");
        return true;
    }
};

struct TraceBenchSnapshot {
    TraceSummary summary;
    TraceBenchIntervals intervals;
    std::uint64_t dropped = 0;
};

class TraceBenchWriter {
public:
    explicit TraceBenchWriter(std::function<void(const TraceBenchSnapshot&)> writer) : writer(std::move(writer)) {
        if (!this->writer) throw std::invalid_argument("invalid benchmark writer");
        worker = std::thread([this] { run(); });
    }

    TraceBenchWriter(const TraceBenchWriter&) = delete;
    TraceBenchWriter& operator=(const TraceBenchWriter&) = delete;

    ~TraceBenchWriter() {
        Stop();
    }

    void Stop() {
        std::lock_guard stopLock(stopMutex);
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_one();
        if (worker.joinable()) worker.join();
    }

    void Write(TraceBenchSnapshot snapshot) {
        {
            std::lock_guard lock(mutex);
            if (failure) std::rethrow_exception(failure);
            if (stopping) throw std::runtime_error("benchmark writer is stopping");
            if (pending) ++superseded;
            pending = std::move(snapshot);
        }
        changed.notify_one();
    }

    void Flush() {
        std::unique_lock lock(mutex);
        idle.wait(lock, [&] { return failure || (!pending && !writing); });
        if (failure) std::rethrow_exception(failure);
    }

    std::uint64_t Superseded() {
        std::lock_guard lock(mutex);
        return superseded;
    }

private:
    void run() noexcept {
        try {
            for (;;) {
                TraceBenchSnapshot snapshot;
                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [&] { return stopping || pending.has_value(); });
                    if (!pending) return;
                    snapshot = std::move(*pending);
                    pending.reset();
                    writing = true;
                }
                writer(snapshot);
                {
                    std::lock_guard lock(mutex);
                    writing = false;
                }
                idle.notify_all();
            }
        } catch (...) {
            {
                std::lock_guard lock(mutex);
                failure = std::current_exception();
                writing = false;
            }
            idle.notify_all();
        }
    }

    std::function<void(const TraceBenchSnapshot&)> writer;
    std::mutex stopMutex;
    std::mutex mutex;
    std::condition_variable changed;
    std::condition_variable idle;
    std::optional<TraceBenchSnapshot> pending;
    std::uint64_t superseded = 0;
    bool writing = false;
    bool stopping = false;
    std::exception_ptr failure;
    std::thread worker;
};

struct TraceGpuRange {
    const char* name;
    double beginNs;
    double endNs;
    std::uint64_t batch = 0;
};

struct TraceClockSample {
    double gpuNs = 0;
    std::chrono::steady_clock::time_point cpu{};
};

class TraceFormat {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::array<std::string_view, 5> WorkerBusyScopes{"Driver.Packet", "Driver.Suspend", "Driver.EndSubmission", "Driver.Rewind", "Driver.Completion"};
    static constexpr std::string_view WorkerWaitScope = "Driver.WorkerWait";
    static constexpr std::string_view WorkerPrefix = "queue worker ";
    static constexpr std::size_t TopPhases = 8;

    static void AppendString(std::string& output, std::string_view text) {
        output += '"';
        std::size_t index = 0;
        while (index < text.size()) {
            const char character = text[index];
            const auto byte = static_cast<unsigned char>(character);
            if (character == '"' || character == '\\') {
                output += '\\';
                output += character;
            } else if (byte >= 0x80) {
                if (const auto length = utf8SequenceLength(text, index); length != 0) {
                    output.append(text.substr(index, length));
                    index += length;
                    continue;
                }
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(byte));
                output += escaped;
            } else if (byte < 0x20 || byte == 0x7f) {
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(byte));
                output += escaped;
            } else {
                output += character;
            }
            ++index;
        }
        output += '"';
    }

    static std::string Name(const char* scope, const char* stage) {
        return stage == nullptr ? std::string(scope) : std::string(scope) + "." + stage;
    }

    static std::string Events(std::uint64_t process, Clock::time_point origin, std::span<const TraceThread::Snapshot> threads, std::size_t presenter, std::span<const Clock::time_point> presents, std::uint64_t firstPresent) {
        std::string output = "{\"traceEvents\":[\n";
        bool first = true;
        const auto event = [&](std::string_view name, const char* phase, Clock::time_point at, std::size_t tid) {
            if (!first) output += ",\n";
            first = false;
            output += "{\"name\":";
            AppendString(output, name);
            char text[96];
            std::snprintf(text, sizeof text, ",\"ph\":\"%s\",\"ts\":%.3f,\"pid\":%llu,\"tid\":%llu", phase, microseconds(at - origin), static_cast<unsigned long long>(process), static_cast<unsigned long long>(tid));
            output += text;
        };
        event("process_name", "M", origin, 0);
        output += ",\"args\":{\"name\":\"AnyPS5\"}}";
        for (std::size_t index = 0; index < threads.size(); ++index) {
            const auto& thread = threads[index];
            const auto tid = index + 1;
            event("thread_name", "M", origin, tid);
            output += ",\"args\":{\"name\":";
            AppendString(output, thread.name);
            output += "}}";
            std::vector<TraceThread::Slice> slices(thread.slices.begin(), thread.slices.end());
            std::stable_sort(slices.begin(), slices.end(), [](const auto& a, const auto& b) { return a.begin != b.begin ? a.begin < b.begin : a.length > b.length; });
            const auto folded = thread.name + " folded";
            for (const auto& slice : slices) {
                char text[112];
                if (slice.scope == nullptr) {
                    event(folded, "C", slice.begin, tid);
                    std::snprintf(text, sizeof text, ",\"args\":{\"ms\":%.3f}}", milliseconds(slice.length));
                } else {
                    event(Name(slice.scope, slice.stage), "X", slice.begin, tid);
                    if (slice.batch != 0) std::snprintf(text, sizeof text, ",\"dur\":%.3f,\"args\":{\"batch\":%llu}}", microseconds(slice.length), static_cast<unsigned long long>(slice.batch));
                    else std::snprintf(text, sizeof text, ",\"dur\":%.3f}", microseconds(slice.length));
                }
                output += text;
            }
            if (thread.eventsFullAt) {
                event("trace buffer full", "i", *thread.eventsFullAt, tid);
                output += ",\"s\":\"t\",\"args\":{\"events_lost\":" + std::to_string(thread.eventsLost) + "}}";
            }
        }
        for (std::size_t index = 0; index + 1 < presents.size(); ++index) {
            event("present", "i", presents[index], presenter + 1);
            output += ",\"s\":\"g\",\"args\":{\"present\":" + std::to_string(firstPresent + index) + "}}";
        }
        output += "\n]}\n";
        return output;
    }

    static std::vector<TraceThread::Slice> GpuSlices(std::span<const TraceGpuRange> ranges, const TraceClockSample& first, const TraceClockSample& last, Clock::time_point origin) {
        const double gpuSpan = last.gpuNs - first.gpuNs;
        const double scale = gpuSpan > 0 ? std::chrono::duration<double, std::nano>(last.cpu - first.cpu).count() / gpuSpan : 1.0;
        const auto map = [&](double gpuNs) { return first.cpu + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::nano>((gpuNs - first.gpuNs) * scale)); };
        std::vector<TraceThread::Slice> slices;
        for (const auto& range : ranges) {
            const auto begin = map(range.beginNs);
            const auto end = map(range.endNs);
            if (end <= begin || begin < origin) continue;
            slices.push_back({range.name, nullptr, begin, end - begin, range.batch});
        }
        return slices;
    }

    static std::vector<TraceThread::Total> Since(std::span<const TraceThread::Total> now, std::span<const TraceThread::Total> before) {
        std::vector<TraceThread::Total> result;
        for (const auto& total : now) {
            auto delta = total;
            for (const auto& old : before) {
                if (old.scope != total.scope || old.stage != total.stage) continue;
                delta.length -= old.length;
                delta.count -= old.count;
                break;
            }
            if (delta.count != 0) result.push_back(delta);
        }
        return result;
    }

    static std::string Summary(const TraceSummary& summary, std::span<const TraceThread::Snapshot> threads, std::span<const Clock::time_point> presents) {
        const auto count = presents.empty() ? std::size_t{0} : presents.size() - 1;
        const auto span = presents.empty() ? Clock::duration{} : presents.back() - presents.front();
        std::vector<double> frames;
        for (std::size_t index = 1; index < presents.size(); ++index) frames.push_back(milliseconds(presents[index] - presents[index - 1]));
        std::sort(frames.begin(), frames.end());
        const auto percentile = [&](double fraction) { return frames.empty() ? 0.0 : frames[std::min(frames.size() - 1, static_cast<std::size_t>(fraction * static_cast<double>(frames.size())))]; };
        const double perPresent = count != 0 ? 1.0 / static_cast<double>(count) : 0.0;

        std::string output = "{\"commit\":";
        AppendString(output, summary.commit);
        output += ",\"dirty\":";
        output += summary.dirty.has_value() ? (*summary.dirty ? "true" : "false") : "null";
        output += ",\"env\":{";
        for (std::size_t index = 0; index < summary.environment.size(); ++index) {
            if (index != 0) output += ',';
            AppendString(output, summary.environment[index].first);
            output += ':';
            AppendString(output, summary.environment[index].second);
        }
        output += "},\"gpu\":";
        AppendString(output, summary.gpu);
        output += ",\"driver\":";
        AppendString(output, summary.driver);
        output += ",\"os\":";
        AppendString(output, summary.os);
        AppendMachine(output, summary);
        output += ",\"resolution\":";
        AppendString(output, std::to_string(summary.width) + "x" + std::to_string(summary.height));
        output += ",\"window\":{\"spec\":";
        AppendString(output, summary.window);
        output += ",\"first_present\":" + std::to_string(summary.firstPresent);
        output += ",\"ms\":" + number(milliseconds(span)) + "}";
        if (summary.interval) output += ",\"interval_start_s\":" + number(summary.interval->first) + ",\"interval_end_s\":" + number(summary.interval->second);
        output += ",\"presents\":" + std::to_string(count);
        output += ",\"frame_ms\":{\"p50\":" + number(percentile(0.5)) + ",\"p90\":" + number(percentile(0.9)) + ",\"p99\":" + number(percentile(0.99)) + ",\"max\":" + number(frames.empty() ? 0.0 : frames.back()) + "}";
        output += ",\"gpu_busy_ms_per_present\":" + number(summary.gpuBusyMs * perPresent);

        output += ",\"worker_busy_pct\":{";
        bool firstWorker = true;
        std::map<std::string, Clock::duration> phases;
        bool dropped = false;
        struct DroppedThread {
            std::string name;
            std::uint64_t eventsLost = 0;
            std::optional<Clock::time_point> eventsFullAt;
            bool totalsFull = false;
        };
        std::vector<DroppedThread> droppedList;
        for (const auto& thread : threads) {
            dropped = dropped || thread.dropped;
            if (thread.eventsLost != 0 || thread.totalsFull) {
                auto entry = std::find_if(droppedList.begin(), droppedList.end(), [&](const DroppedThread& candidate) { return candidate.name == thread.name; });
                if (entry == droppedList.end()) entry = droppedList.insert(droppedList.end(), DroppedThread{thread.name});
                entry->eventsLost += thread.eventsLost;
                if (thread.eventsLost != 0 && thread.eventsFullAt && (!entry->eventsFullAt || *thread.eventsFullAt < *entry->eventsFullAt)) entry->eventsFullAt = thread.eventsFullAt;
                entry->totalsFull = entry->totalsFull || thread.totalsFull;
            }
            Clock::duration busy{};
            for (const auto& total : thread.totals) {
                const std::string_view scope = total.scope;
                if (total.stage == nullptr && std::find(WorkerBusyScopes.begin(), WorkerBusyScopes.end(), scope) != WorkerBusyScopes.end()) busy += total.length;
                if (scope != WorkerWaitScope) phases[Name(total.scope, total.stage)] += total.length;
            }
            if (!thread.name.starts_with(WorkerPrefix)) continue;
            if (!firstWorker) output += ',';
            firstWorker = false;
            AppendString(output, thread.name);
            output += ':' + number(span != Clock::duration{} ? 100.0 * static_cast<double>(busy.count()) / static_cast<double>(span.count()) : 0.0);
        }
        output += "},\"top_phases_ms_per_frame\":{";
        std::vector<std::pair<std::string, Clock::duration>> top(phases.begin(), phases.end());
        std::stable_sort(top.begin(), top.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (std::size_t index = 0; index < top.size() && index < TopPhases; ++index) {
            if (index != 0) output += ',';
            AppendString(output, top[index].first);
            output += ':' + number(milliseconds(top[index].second) * perPresent);
        }
        output += "},\"trace_dropped\":";
        output += dropped ? "true" : "false";
        if (!droppedList.empty()) {
            std::string droppedThreads;
            for (const auto& entry : droppedList) {
                if (!droppedThreads.empty()) droppedThreads += ',';
                AppendString(droppedThreads, entry.name);
                droppedThreads += ":{";
                if (entry.eventsLost != 0) {
                    if (entry.eventsFullAt && !presents.empty() && *entry.eventsFullAt >= presents.front() && *entry.eventsFullAt <= presents.back()) droppedThreads += "\"events_full_after_s\":" + number(std::chrono::duration<double>(*entry.eventsFullAt - presents.front()).count()) + ",";
                    droppedThreads += "\"events_lost\":" + std::to_string(entry.eventsLost) + ",";
                }
                droppedThreads += std::string("\"totals_full\":") + (entry.totalsFull ? "true" : "false") + "}";
            }
            output += ",\"trace_dropped_threads\":{" + droppedThreads + "}";
        }
        output += ",\"trace\":";
        AppendString(output, summary.trace);
        output += "}\n";
        return output;
    }

    static std::vector<std::uint32_t> FrameMicroseconds(std::span<const Clock::time_point> presents) {
        std::vector<std::uint32_t> frames;
        for (std::size_t index = 1; index < presents.size(); ++index) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(presents[index] - presents[index - 1]).count();
            frames.push_back(static_cast<std::uint32_t>(std::clamp<std::int64_t>(elapsed, 0, std::numeric_limits<std::uint32_t>::max())));
        }
        return frames;
    }

    static std::string Bench(const TraceSummary& summary, std::span<const std::shared_ptr<const TraceBenchInterval>> intervals, std::uint64_t intervalsDropped) {
        std::string output = "{\"format\":\"aps5-bench/1\",\"commit\":";
        AppendString(output, summary.commit);
        output += ",\"dirty\":";
        output += summary.dirty.has_value() ? (*summary.dirty ? "true" : "false") : "null";
        output += ",\"env\":{";
        for (std::size_t index = 0; index < summary.environment.size(); ++index) {
            if (index != 0) output += ',';
            AppendString(output, summary.environment[index].first);
            output += ':';
            AppendString(output, summary.environment[index].second);
        }
        output += "},\"gpu\":";
        AppendString(output, summary.gpu);
        output += ",\"driver\":";
        AppendString(output, summary.driver);
        output += ",\"os\":";
        AppendString(output, summary.os);
        AppendMachine(output, summary);
        output += ",\"intervals_dropped\":" + std::to_string(intervalsDropped) + ",\"intervals\":[";
        for (std::size_t index = 0; index < intervals.size(); ++index) {
            const auto& interval = *intervals[index];
            if (index != 0) output += ',';
            output += "\n{\"start_s\":" + number(interval.startS) + ",\"end_s\":" + number(interval.endS);
            output += ",\"first_present\":" + std::to_string(interval.firstPresent);
            output += ",\"resolution\":";
            AppendString(output, std::to_string(interval.width) + "x" + std::to_string(interval.height));
            output += ",\"gpu_busy_ms\":" + number(interval.gpuBusyMs) + ",\"frame_us\":[";
            for (std::size_t frame = 0; frame < interval.frameUs.size(); ++frame) {
                if (frame != 0) output += ',';
                output += std::to_string(interval.frameUs[frame]);
            }
            output += "]}";
        }
        output += "\n]}\n";
        return output;
    }

private:
    static std::size_t utf8SequenceLength(std::string_view text, std::size_t index) {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        unsigned char low = 0x80;
        unsigned char high = 0xbf;
        if (lead >= 0xc2 && lead <= 0xdf) {
            length = 2;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            length = 3;
            if (lead == 0xe0) low = 0xa0;
            if (lead == 0xed) high = 0x9f;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            length = 4;
            if (lead == 0xf0) low = 0x90;
            if (lead == 0xf4) high = 0x8f;
        } else {
            return 0;
        }
        if (text.size() - index < length) return 0;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto next = static_cast<unsigned char>(text[index + offset]);
            if (next < low || next > high) return 0;
            low = 0x80;
            high = 0xbf;
        }
        return length;
    }

    static void AppendMachine(std::string& output, const TraceSummary& summary) {
        if (summary.host) {
            output += ",\"cpu\":";
            AppendString(output, summary.host->cpu);
            output += ",\"cpu_threads\":" + std::to_string(summary.host->threads);
            output += ",\"memory_mib\":" + std::to_string(summary.host->memoryMiB);
        }
        if (summary.title) {
            output += ",\"title_id\":";
            AppendString(output, summary.title->id);
            output += ",\"title\":";
            AppendString(output, summary.title->name);
            output += ",\"content_version\":";
            AppendString(output, summary.title->contentVersion);
        }
    }

    static double microseconds(Clock::duration elapsed) {
        return std::chrono::duration<double, std::micro>(elapsed).count();
    }

    static double milliseconds(Clock::duration elapsed) {
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    static std::string number(double value) {
        char text[32];
        std::snprintf(text, sizeof text, "%.3f", value);
        return text;
    }
};

struct TraceThreadTotals {
    std::string name;
    std::vector<TraceThread::Total> totals;
};

class PerformanceTrace {
public:
    static constexpr const char* RenderPassScope = "Graphics.RenderPass";

    static bool Configured() {
        static const bool configured = std::getenv("APS5_TRACE_FRAMES") != nullptr || std::getenv("APS5_TRACE_SECONDS") != nullptr || std::getenv("APS5_TRACE_SUMMARY_EVERY") != nullptr || std::getenv("APS5_TRACE_BENCH") != nullptr;
        return configured;
    }

    static void BindThread(const char* name, std::size_t capacity = TraceThread::DefaultCapacity);
    static void Present(std::uint32_t width, std::uint32_t height, const VulkanDevice* device);

    static void GpuRange(const char* name, double beginNs, double endNs, std::uint64_t batch = 0) {
        if (!buffering.load(std::memory_order_relaxed)) return;
        std::lock_guard lock(gpuMutex);
        if (gpuRanges.size() == gpuRanges.capacity()) {
            ++gpuLost;
            return;
        }
        gpuRanges.push_back({name, beginNs, endNs, batch});
    }

    static bool Recording() {
        return AgcDriverTraceRecording_nid_postfix();
    }

    static TraceThread* Thread() {
        return AgcDriverTraceThread_nid_postfix();
    }

    static void Count(const char* scope) {
        if (Recording()) AgcDriverTraceCount_nid_postfix(scope);
    }

    static void SetOverlayTotals(bool on);
    static bool OverlayTotals();
    static std::optional<std::vector<TraceThreadTotals>> ThreadTotals();
    static void StopOutput();

private:
    friend bool ::AgcDriverTraceRecording_nid_postfix();
    friend TraceThread* ::AgcDriverTraceThread_nid_postfix();
    friend void ::AgcDriverTraceCount_nid_postfix(const char* scope);

    static TraceThread* bindNamed();

    static void OpenInterval(std::chrono::steady_clock::time_point now, std::uint64_t present);
    static void FlushInterval(std::chrono::steady_clock::time_point now, std::uint32_t width, std::uint32_t height, const VulkanDevice* device);

    static std::atomic<bool> recording;
    inline static std::atomic<bool> buffering{false};
    inline static std::atomic<std::uint64_t> presentCount{0};
    static thread_local TraceThread* local;
    static thread_local const char* localName;
    static std::atomic<TraceThread* (*)()> binder;
    inline static std::mutex gpuMutex;
    inline static std::vector<TraceGpuRange> gpuRanges;
    inline static std::uint64_t gpuLost = 0;
};

}

#endif
