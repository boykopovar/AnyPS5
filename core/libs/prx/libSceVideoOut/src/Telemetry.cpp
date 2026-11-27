#include "prx/libSceVideoOut/include/Telemetry.hpp"

#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "prx/libSceAgcDriver/TraceRevision.hpp"

namespace VideoOutTelemetry {

namespace {

constexpr char ContractLevel[] = "v1-pad";

constexpr auto DropReportInterval = std::chrono::seconds(10);
constexpr std::size_t DefaultQueueCapacity = 4096;

std::int64_t steadyNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string configuredDirectory() {
#ifdef _WIN32
    const wchar_t* configured = _wgetenv(L"APS5_VIDEOOUT_TELEMETRY_DIR");
    if (configured == nullptr) return {};
    const auto text = std::filesystem::path(configured).u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
#else
    const char* configured = std::getenv("APS5_VIDEOOUT_TELEMETRY_DIR");
    return configured != nullptr ? configured : std::string();
#endif
}

std::int64_t processId() {
#ifdef _WIN32
    return static_cast<std::int64_t>(::GetCurrentProcessId());
#else
    return static_cast<std::int64_t>(::getpid());
#endif
}

std::string head(std::int64_t timeNanos, const char* event) {
    char text[96];
    std::snprintf(text, sizeof(text), "{\"v\":1,\"t_ns\":%lld,\"ev\":\"%s\"", static_cast<long long>(timeNanos), event);
    return text;
}

}

std::optional<FlipTimingWindow> ParseFlipTimingWindow(const char* setting) {
    if (setting == nullptr || *setting == '\0') return std::nullopt;
    const std::string_view text(setting);
    const auto colon = text.find(':');
    if (colon != std::string_view::npos) {
        FlipTimingWindow window{};
        const char* middle = text.data() + colon;
        const char* end = text.data() + text.size();
        const auto first = std::from_chars(text.data(), middle, window.first);
        const auto count = std::from_chars(middle + 1, end, window.count);
        if (first.ec == std::errc{} && first.ptr == middle && count.ec == std::errc{} && count.ptr == end &&
            window.first > 0 && window.count >= 2 && window.count <= 100000 &&
            window.count - 1 <= std::numeric_limits<std::uint64_t>::max() - window.first) {
            return window;
        }
    }
    throw std::runtime_error("APS5_VIDEOOUT_FLIP_TIMING must be FIRST:COUNT with FIRST positive, COUNT 2 to 100000, and the last flip in range");
}

BoundedLineQueue::BoundedLineQueue(std::size_t capacity) : capacity(capacity) {
}

bool BoundedLineQueue::TryPush(std::string line) {
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return pushLocked(std::move(line));
}

bool BoundedLineQueue::Push(std::string line) {
    std::lock_guard lock(mutex);
    return pushLocked(std::move(line));
}

bool BoundedLineQueue::pushLocked(std::string line) {
    if (lines.size() >= capacity) {
        dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    lines.push_back(std::move(line));
    return true;
}

std::deque<std::string> BoundedLineQueue::Drain() {
    std::lock_guard lock(mutex);
    return std::exchange(lines, {});
}

std::uint64_t BoundedLineQueue::Dropped() const {
    return dropped.load(std::memory_order_relaxed);
}

Writer::Writer(const std::string& directory, std::size_t capacity, const std::string& commit,
               std::optional<FlipTimingWindow> flipTiming) : queue(capacity), flipTiming(flipTiming) {
    const std::string path = directory + "/videoout.jsonl";
    output.open(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())), std::ios::binary | std::ios::app);
    if (!output.is_open()) throw std::runtime_error("VideoOut telemetry: cannot open " + path);
    const auto steady = steadyNanos();
    char text[256];
    std::snprintf(text, sizeof(text), ",\"contract\":\"%s\",\"commit\":\"%s\",\"pid\":%lld,\"prx\":\"libSceVideoOut\"", ContractLevel, commit.c_str(), static_cast<long long>(processId()));
    auto contract = head(steady, "contract") + text;
    if (flipTiming) {
        contract += ",\"flip_start\":" + std::to_string(flipTiming->first) + ",\"flip_count\":" + std::to_string(flipTiming->count);
    }
    queue.Push(contract + "}\n");
    thread = std::jthread([this](std::stop_token token) { run(token); });
}

Writer::~Writer() {
    thread.request_stop();
    if (thread.joinable()) thread.join();
}

bool Writer::emit(std::string line, bool mustKeep) {
    return mustKeep ? queue.Push(std::move(line)) : queue.TryPush(std::move(line));
}

void Writer::writeAll(const std::string& text) {
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.flush();
    if (!output) throw std::ios_base::failure("VideoOut telemetry: writing videoout.jsonl failed");
}

void Writer::writeDrops(bool force) {
    const auto total = queue.Dropped();
    if (total == 0 || (total == reportedDrops && !force)) return;
    reportedDrops = total;
    char text[64];
    std::snprintf(text, sizeof(text), ",\"dropped\":%llu}\n", static_cast<unsigned long long>(total));
    writeAll(head(steadyNanos(), "drops") + text);
}

void Writer::run(std::stop_token token) {
    try {
        auto nextDropReport = std::chrono::steady_clock::now() + DropReportInterval;
        std::mutex sleepMutex;
        std::condition_variable_any sleepWake;
        while (true) {
            {
                std::unique_lock lock(sleepMutex);
                sleepWake.wait_for(lock, token, std::chrono::milliseconds(200), [] { return false; });
            }
            std::string batch;
            for (auto& line : queue.Drain()) batch += line;
            if (!batch.empty()) writeAll(batch);
            if (std::chrono::steady_clock::now() >= nextDropReport) {
                nextDropReport += DropReportInterval;
                writeDrops(false);
            }
            if (token.stop_requested()) break;
        }
        std::string rest;
        for (auto& line : queue.Drain()) rest += line;
        if (!rest.empty()) writeAll(rest);
        writeDrops(true);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[videoout] telemetry writer stopped: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "[videoout] telemetry writer stopped\n");
    }
}

bool Writer::EmitPad(std::uint64_t frame, const PadInputState& state) {
    char text[192];
    std::snprintf(text, sizeof(text), ",\"n\":%llu,\"buttons\":%u,\"sticks\":[%u,%u,%u,%u],\"triggers\":[%u,%u]}\n",
        static_cast<unsigned long long>(frame), state.buttons, state.sticks[0], state.sticks[1], state.sticks[2], state.sticks[3],
        state.analogButtonsL2, state.analogButtonsR2);
    return emit(head(steadyNanos(), "pad") + text, true);
}

void Writer::EmitPresent(std::uint64_t frame, double fps) {
    char text[96];
    std::snprintf(text, sizeof(text), ",\"n\":%llu,\"fps\":%.2f}\n", static_cast<unsigned long long>(frame), fps);
    emit(head(steadyNanos(), "present") + text, false);
}

void Writer::EmitCompletedFlip(std::uint64_t generation, std::uint64_t sequence, std::uint64_t processTimeUs) {
    if (!flipTiming) return;
    std::uint8_t state = flipGenerationState.load(std::memory_order_acquire);
    if (state == 0 && flipGenerationState.compare_exchange_strong(state, 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
        flipGeneration = generation;
        flipGenerationState.store(2, std::memory_order_release);
        state = 2;
    }
    while (state == 1) {
        std::this_thread::yield();
        state = flipGenerationState.load(std::memory_order_acquire);
    }
    if (flipGeneration != generation || sequence < flipTiming->first || sequence - flipTiming->first >= flipTiming->count) return;
    char text[128];
    std::snprintf(text, sizeof(text), ",\"n\":%llu,\"generation\":%llu,\"process_us\":%llu}\n",
        static_cast<unsigned long long>(sequence), static_cast<unsigned long long>(generation),
        static_cast<unsigned long long>(processTimeUs));
    emit(head(steadyNanos(), "flip_complete") + text, false);
}

std::uint64_t Writer::Dropped() const {
    return queue.Dropped();
}

bool PadChangeFilter::Changed(const PadInputState& state) const {
    return state.buttons != last.buttons || state.sticks != last.sticks ||
        state.analogButtonsL2 != last.analogButtonsL2 || state.analogButtonsR2 != last.analogButtonsR2;
}

void PadChangeFilter::Commit(const PadInputState& state) {
    last.buttons = state.buttons;
    last.sticks = state.sticks;
    last.analogButtonsL2 = state.analogButtonsL2;
    last.analogButtonsR2 = state.analogButtonsR2;
}

bool EmitPadIfChanged(Writer& writer, PadChangeFilter& filter, std::uint64_t frame, const PadInputState& state) {
    if (!filter.Changed(state)) return false;
    if (!writer.EmitPad(frame, state)) return false;
    filter.Commit(state);
    return true;
}

std::unique_ptr<Writer> OpenFromEnvironment(const char* directory, const std::string& commit, const char* flipTiming) {
    if (directory == nullptr || directory[0] == '\0') return nullptr;
    return std::make_unique<Writer>(directory, DefaultQueueCapacity, commit, ParseFlipTimingWindow(flipTiming));
}

Writer* Instance() {
    static const std::unique_ptr<Writer> writer = OpenFromEnvironment(configuredDirectory().c_str(), APS5_TRACE_COMMIT,
        std::getenv("APS5_VIDEOOUT_FLIP_TIMING"));
    return writer.get();
}

}
