#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/TraceFile.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/TraceRevision.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <system_error>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <sys/utsname.h>
#include <unistd.h>
#endif
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#ifndef _WIN32
extern char** environ;
#endif

namespace AgcDriver {

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t GpuRangeCapacity = TraceThread::DefaultCapacity;

struct Session {
    std::mutex mutex;
    std::vector<std::unique_ptr<TraceThread>> threads;
    bool started = false;
    std::optional<TraceWindow> window;
    bool windowOpen = false;
    std::vector<std::vector<TraceThread::Total>> windowBase;
    std::optional<TraceInterval> interval;
    bool intervalOpen = false;
    std::uint64_t intervalFirstPresent = 0;
    double intervalGpuBusy = 0;
    std::vector<std::vector<TraceThread::Total>> intervalBase;
    std::vector<Clock::time_point> intervalPresents;
    Clock::time_point first{};
    std::uint64_t present = 0;
    std::uint64_t firstPresent = 0;
    double gpuBusyAtOpen = 0;
    std::optional<TraceClockSample> gpuClock;
    std::vector<Clock::time_point> presents;
    bool bench = false;
    TraceBenchIntervals benchIntervals;
    std::uint64_t benchDropped = 0;
};

Session& session() {
    static auto& state = *new Session();
    return state;
}

std::unique_ptr<ProfileOutput>& writer() {
    static std::unique_ptr<ProfileOutput> output;
    return output;
}

std::unique_ptr<ProfileOutput>& summaryWriter() {
    static std::unique_ptr<ProfileOutput> output;
    return output;
}

std::unique_ptr<TraceBenchWriter>& benchWriter() {
    static std::unique_ptr<TraceBenchWriter> output;
    return output;
}

std::filesystem::path traceDirectory() {
    const char* directory = std::getenv("APS5_TRACE_DIR");
    return std::filesystem::path(directory != nullptr ? directory : ".");
}

std::uint64_t processId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::string operatingSystem() {
#ifdef _WIN32
    using QueryVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    static const auto query = reinterpret_cast<QueryVersion>(reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (query == nullptr || query(&version) != 0) throw std::runtime_error("RtlGetVersion failed");
    return "Windows " + std::to_string(version.dwMajorVersion) + "." + std::to_string(version.dwMinorVersion) + "." + std::to_string(version.dwBuildNumber);
#else
    utsname name{};
    if (::uname(&name) != 0) throw std::system_error(errno, std::generic_category(), "uname");
    return std::string(name.sysname) + " " + name.release;
#endif
}

std::optional<std::string> processorName() {
    std::array<std::uint32_t, 12> words{};
#if defined(_MSC_VER) && !defined(__clang__)
    int registers[4]{};
    __cpuid(registers, static_cast<int>(0x80000000u));
    if (static_cast<std::uint32_t>(registers[0]) < 0x80000004u) return std::nullopt;
    for (unsigned leaf = 0; leaf < 3; ++leaf) {
        __cpuid(registers, static_cast<int>(0x80000002u + leaf));
        std::memcpy(words.data() + leaf * 4, registers, sizeof registers);
    }
#elif defined(__x86_64__) || defined(__i386__)
    if (__get_cpuid_max(0x80000000u, nullptr) < 0x80000004u) return std::nullopt;
    for (unsigned leaf = 0; leaf < 3; ++leaf) {
        unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
        if (__get_cpuid(0x80000002u + leaf, &eax, &ebx, &ecx, &edx) == 0) throw std::runtime_error("cpuid brand string leaf failed");
        words[leaf * 4] = eax;
        words[leaf * 4 + 1] = ebx;
        words[leaf * 4 + 2] = ecx;
        words[leaf * 4 + 3] = edx;
    }
#else
    return std::nullopt;
#endif
    std::string name(reinterpret_cast<const char*>(words.data()), sizeof words);
    name.resize(std::strlen(name.c_str()));
    const auto first = name.find_first_not_of(' ');
    if (first == std::string::npos) return std::nullopt;
    return name.substr(first, name.find_last_not_of(' ') - first + 1);
}

std::uint64_t memoryMiB() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status) == 0) throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "GlobalMemoryStatusEx");
    return status.ullTotalPhys / (1024 * 1024);
#else
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long size = ::sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || size <= 0) throw std::system_error(errno, std::generic_category(), "sysconf physical memory");
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(size) / (1024 * 1024);
#endif
}

const std::optional<TraceHost>& host() {
    static const auto value = []() -> std::optional<TraceHost> {
        auto cpu = processorName();
        if (!cpu) return std::nullopt;
        return TraceHost{std::move(*cpu), std::thread::hardware_concurrency(), memoryMiB()};
    }();
    return value;
}

std::optional<TraceTitle> title() {
    if (!HasAppParamJson_nid_no_patch()) return std::nullopt;
    return TraceTitle{GetAppTitleId_nid_postfix().value, GetAppTitle_nid_postfix().value, GetAppContentVersion_nid_no_patch().value};
}

std::vector<std::pair<std::string, std::string>> environment() {
#ifdef _WIN32
    char** entries = _environ;
#else
    char** entries = environ;
#endif
    std::vector<std::pair<std::string, std::string>> result;
    for (; entries != nullptr && *entries != nullptr; ++entries) {
        const std::string_view entry = *entries;
        const auto equals = entry.find('=');
        if (!entry.starts_with("APS5_") || equals == std::string_view::npos) continue;
        std::string value(entry.substr(equals + 1));
        if (value.find_first_of("/\\") != std::string::npos) value = "<path>";
        result.emplace_back(std::string(entry.substr(0, equals)), std::move(value));
    }
    std::sort(result.begin(), result.end());
    return result;
}

TraceSummary identity(std::uint32_t width, std::uint32_t height, const VulkanDevice* device) {
    TraceSummary summary;
    summary.commit = APS5_TRACE_COMMIT;
    if (APS5_TRACE_DIRTY >= 0) summary.dirty = APS5_TRACE_DIRTY != 0;
    summary.environment = environment();
    summary.gpu = device != nullptr ? device->DeviceName() : std::string();
    summary.driver = device != nullptr ? device->DriverInfo() : std::string();
    summary.os = operatingSystem();
    summary.width = width;
    summary.height = height;
    summary.host = host();
    static const auto running = title();
    summary.title = running;
    return summary;
}

std::vector<std::vector<TraceThread::Total>> totalsOf(const std::vector<std::unique_ptr<TraceThread>>& threads) {
    std::vector<std::vector<TraceThread::Total>> result;
    for (const auto& thread : threads) result.push_back(thread->Collect().totals);
    return result;
}

double seconds(Clock::duration elapsed) {
    return std::chrono::duration<double>(elapsed).count();
}

}

void PerformanceTrace::BindThread(const char* name, std::size_t capacity) {
    if (local != nullptr) return;
    if (!Configured()) {
        localName = name;
        return;
    }
    const bool windowed = std::getenv("APS5_TRACE_FRAMES") != nullptr || std::getenv("APS5_TRACE_SECONDS") != nullptr;
    if (windowed && capacity == TraceThread::DefaultCapacity) capacity = TraceThread::ParseCapacity(std::getenv("APS5_TRACE_EVENTS"));
    auto bound = std::make_unique<TraceThread>(name, presentCount, windowed ? capacity : 0, buffering);
    auto& state = session();
    std::lock_guard lock(state.mutex);
    local = bound.get();
    state.threads.push_back(std::move(bound));
}

TraceThread* PerformanceTrace::bindNamed() {
    if (localName == nullptr) return nullptr;
    auto bound = std::make_unique<TraceThread>(localName, presentCount, 0, buffering);
    auto& state = session();
    std::lock_guard lock(state.mutex);
    local = bound.get();
    state.threads.push_back(std::move(bound));
    return local;
}

void PerformanceTrace::SetOverlayTotals(bool on) {
    if (Configured()) return;
    binder.store(on ? &bindNamed : nullptr, std::memory_order_release);
    recording.store(on, std::memory_order_relaxed);
}

bool PerformanceTrace::OverlayTotals() {
    return binder.load(std::memory_order_relaxed) != nullptr;
}

std::optional<std::vector<TraceThreadTotals>> PerformanceTrace::ThreadTotals() {
    auto& state = session();
    std::unique_lock lock(state.mutex, std::try_to_lock);
    if (!lock.owns_lock()) return std::nullopt;
    std::vector<TraceThreadTotals> threads;
    threads.reserve(state.threads.size());
    for (const auto& thread : state.threads) {
        auto snapshot = thread->Collect();
        threads.push_back({std::move(snapshot.name), std::move(snapshot.totals)});
    }
    return threads;
}

void PerformanceTrace::OpenInterval(Clock::time_point now, std::uint64_t present) {
    auto& state = session();
    state.interval->Restart(now - state.first);
    state.intervalOpen = true;
    state.intervalFirstPresent = present;
    state.intervalGpuBusy = VulkanDevice::PresentCounts().gpuBusyMs;
    state.intervalPresents.assign(1, now);
    if (summaryWriter()) return;
    summaryWriter() = std::make_unique<ProfileOutput>([directory = traceDirectory()](std::string_view text) {
        try {
            std::filesystem::create_directories(directory);
            TraceFile::AppendLine(directory / "aps5-summary.jsonl", text);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[trace] appending to aps5-summary.jsonl failed: %s\n", error.what());
            std::fflush(stderr);
            throw;
        }
    }, std::numeric_limits<std::size_t>::max());
    ProfilePrint_nid_no_patch("[trace] summarizing %s from present %llu\n", state.interval->Describe().c_str(), static_cast<unsigned long long>(present));
}

void PerformanceTrace::FlushInterval(Clock::time_point now, std::uint32_t width, std::uint32_t height, const VulkanDevice* device) {
    auto& state = session();
    state.intervalBase.resize(state.threads.size());
    std::vector<TraceThread::Snapshot> snapshots;
    for (std::size_t index = 0; index < state.threads.size(); ++index) {
        auto snapshot = state.threads[index]->Collect();
        auto totals = std::move(snapshot.totals);
        snapshot.slices = {};
        snapshot.totals = TraceFormat::Since(totals, state.intervalBase[index]);
        state.intervalBase[index] = std::move(totals);
        snapshots.push_back(std::move(snapshot));
    }
    auto summary = identity(width, height, device);
    summary.window = state.interval->Describe();
    summary.firstPresent = state.intervalFirstPresent;
    summary.gpuBusyMs = VulkanDevice::PresentCounts().gpuBusyMs - state.intervalGpuBusy;
    summary.interval = std::make_pair(seconds(state.intervalPresents.front() - state.first), seconds(now - state.first));
    summaryWriter()->Write(TraceFormat::Summary(summary, snapshots, state.intervalPresents));
    if (!state.bench) return;
    TraceBench::Keep(state.benchIntervals, std::make_shared<const TraceBenchInterval>(TraceBenchInterval{summary.interval->first, summary.interval->second, summary.firstPresent, width, height, summary.gpuBusyMs, TraceFormat::FrameMicroseconds(state.intervalPresents)}), state.benchDropped);
    if (!benchWriter()) {
        const auto name = "aps5-bench-" + summary.commit + "-" + std::to_string(processId()) + ".json.gz";
        benchWriter() = std::make_unique<TraceBenchWriter>([directory = traceDirectory(), name](const TraceBenchSnapshot& snapshot) {
            try {
                std::filesystem::create_directories(directory);
                TraceFile::ReplaceGzip(directory / name, TraceFormat::Bench(snapshot.summary, snapshot.intervals, snapshot.dropped));
            } catch (const std::exception& error) {
                std::fprintf(stderr, "[trace] writing %s failed: %s\n", name.c_str(), error.what());
                std::fflush(stderr);
                throw;
            }
        });
        ProfilePrint_nid_no_patch("[trace] benchmark frames go to %s, rewritten every interval\n", name.c_str());
    }
    benchWriter()->Write(TraceBenchSnapshot{std::move(summary), state.benchIntervals, state.benchDropped});
}

void PerformanceTrace::Present(std::uint32_t width, std::uint32_t height, const VulkanDevice* device) {
    const auto now = Clock::now();
    BindThread("presenter");
    auto& state = session();
    std::lock_guard lock(state.mutex);
    if (!state.started) {
        state.window = TraceWindow::Parse(std::getenv("APS5_TRACE_FRAMES"), std::getenv("APS5_TRACE_SECONDS"));
        state.interval = TraceInterval::Parse(std::getenv("APS5_TRACE_SUMMARY_EVERY"));
        state.bench = TraceBench::Parse(std::getenv("APS5_TRACE_BENCH"), state.interval.has_value());
        state.first = now;
        state.started = true;
    }
    const auto present = state.present++;
    const auto edge = state.window ? state.window->Present(present, now - state.first) : TraceWindow::Edge::None;
    if (state.interval) {
        if (!state.intervalOpen) {
            OpenInterval(now, present);
        } else {
            state.intervalPresents.push_back(now);
            if (state.interval->Due(now - state.first)) {
                FlushInterval(now, width, height, device);
                OpenInterval(now, present);
            }
        }
    }
    if (edge == TraceWindow::Edge::Open) {
        state.windowOpen = true;
        state.windowBase = totalsOf(state.threads);
        state.firstPresent = present;
        state.gpuBusyAtOpen = VulkanDevice::PresentCounts().gpuBusyMs;
        TraceClockSample clock;
        if (device != nullptr && device->CalibrateClock(clock.gpuNs, clock.cpu)) state.gpuClock = clock;
        {
            std::lock_guard gpu(gpuMutex);
            gpuRanges.clear();
            gpuLost = 0;
            gpuRanges.reserve(GpuRangeCapacity);
        }
        state.presents.push_back(now);
        presentCount.fetch_add(1, std::memory_order_relaxed);
        buffering.store(true, std::memory_order_relaxed);
        recording.store(true, std::memory_order_relaxed);
        ProfilePrint_nid_no_patch("[trace] recording %s from present %llu%s\n", state.window->Describe().c_str(), static_cast<unsigned long long>(present), state.gpuClock ? "" : "; no GPU track: the device has no calibrated timestamps");
        return;
    }
    if (!state.windowOpen) {
        if (!state.interval) return;
        presentCount.fetch_add(1, std::memory_order_relaxed);
        recording.store(true, std::memory_order_relaxed);
        return;
    }
    state.presents.push_back(now);
    presentCount.fetch_add(1, std::memory_order_relaxed);
    if (edge != TraceWindow::Edge::Close) return;
    state.windowOpen = false;
    buffering.store(false, std::memory_order_relaxed);
    recording.store(state.interval.has_value(), std::memory_order_relaxed);

    std::vector<TraceThread::Snapshot> snapshots;
    std::size_t presenter = 0;
    state.windowBase.resize(state.threads.size());
    for (std::size_t index = 0; index < state.threads.size(); ++index) {
        if (state.threads[index].get() == local) presenter = index;
        snapshots.push_back(state.threads[index]->Collect());
        snapshots.back().totals = TraceFormat::Since(snapshots.back().totals, state.windowBase[index]);
    }
    std::vector<TraceThread::Slice> gpuSlices;
    TraceClockSample clock;
    if (state.gpuClock && device != nullptr && device->CalibrateClock(clock.gpuNs, clock.cpu)) {
        std::lock_guard gpu(gpuMutex);
        gpuSlices = TraceFormat::GpuSlices(gpuRanges, *state.gpuClock, clock, state.presents.front());
        snapshots.push_back({"gpu", gpuSlices, {}, gpuLost != 0, gpuLost});
    }
    const auto process = processId();
    auto summary = identity(width, height, device);
    summary.window = state.window->Describe();
    summary.firstPresent = state.firstPresent;
    summary.gpuBusyMs = VulkanDevice::PresentCounts().gpuBusyMs - state.gpuBusyAtOpen;
    summary.trace = "aps5-trace-" + summary.commit + "-" + std::to_string(process) + ".json.gz";
    auto events = TraceFormat::Events(process, state.presents.front(), snapshots, presenter, state.presents, state.firstPresent);
    auto& output = writer();
    output = std::make_unique<ProfileOutput>([directory = traceDirectory(), name = summary.trace, line = TraceFormat::Summary(summary, snapshots, state.presents)](std::string_view text) {
        try {
            std::filesystem::create_directories(directory);
            TraceFile::WriteGzip(directory / name, text);
            TraceFile::AppendLine(directory / "aps5-summary.jsonl", line);
            std::fprintf(stderr, "[trace] wrote %s and its line in aps5-summary.jsonl\n", name.c_str());
            std::fflush(stderr);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[trace] writing %s failed: %s\n", name.c_str(), error.what());
            std::fflush(stderr);
            throw;
        }
    }, std::numeric_limits<std::size_t>::max());
    ProfilePrint_nid_no_patch("[trace] closed at present %llu, writing %s (%zu bytes of JSON)\n", static_cast<unsigned long long>(present), summary.trace.c_str(), events.size());
    output->Write(std::move(events));
}

void PerformanceTrace::StopOutput() {
    if (writer()) writer()->Stop();
    if (summaryWriter()) summaryWriter()->Stop();
    if (benchWriter()) benchWriter()->Stop();
}

}
