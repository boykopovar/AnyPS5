#include "prx/libSceVideoOut/include/Telemetry.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
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

Writer::Writer(const std::string& directory, std::size_t capacity, const std::string& commit) : queue(capacity) {
    const std::string path = directory + "/videoout.jsonl";
    output.open(path, std::ios::binary | std::ios::app);
    if (!output.is_open()) throw std::runtime_error("VideoOut telemetry: cannot open " + path);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    const auto steady = steadyNanos();
    char text[256];
    std::snprintf(text, sizeof(text), ",\"contract\":\"%s\",\"commit\":\"%s\",\"pid\":%lld,\"prx\":\"libSceVideoOut\"}\n", ContractLevel, commit.c_str(), static_cast<long long>(processId()));
    queue.Push(head(steady, "contract") + text);
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

std::unique_ptr<Writer> OpenFromEnvironment(const char* directory, const std::string& commit) {
    if (directory == nullptr || directory[0] == '\0') return nullptr;
    return std::make_unique<Writer>(directory, DefaultQueueCapacity, commit);
}

Writer* Instance() {
    static const std::unique_ptr<Writer> writer = OpenFromEnvironment(std::getenv("APS5_VIDEOOUT_TELEMETRY_DIR"), APS5_TRACE_COMMIT);
    return writer.get();
}

}
