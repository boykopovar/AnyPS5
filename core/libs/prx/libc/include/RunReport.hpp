#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_RUNREPORT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_RUNREPORT_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct RunReportGame {
    char titleId[16];
    char contentVersion[32];
    char name[320];
};

extern "C" void LibcRunReportSetGameProvider_nid_no_patch(bool (*provider)(RunReportGame*));
extern "C" void LibcRunReportFinishExit_nid_no_patch(int exitCode) noexcept;
extern "C" void LibcRunReportTerminate_nid_no_patch(const char* what) noexcept;
extern "C" void LibcRunReportRegisterComponent_nid_no_patch(const char* name, const char* commit, int modified) noexcept;
extern "C" void LibcRunReportMissingFunction_nid_no_patch(const char* name, const char* file, int line) noexcept;
extern "C" void LibcRunReportQuitRequested_nid_no_patch() noexcept;
extern "C" bool LibcRunReportEnabled_nid_no_patch() noexcept;
extern "C" void LibcRunReportMilestone_nid_no_patch(int milestone) noexcept;
extern "C" void LibcRunReportFramePresented_nid_no_patch() noexcept;
extern "C" void LibcRunReportShader_nid_no_patch(int what) noexcept;
extern "C" void LibcRunReportShaderCompileTime_nid_no_patch(long long nanoseconds) noexcept;
extern "C" void LibcRunReportUnsupportedInstruction_nid_no_patch(const char* encoding, const char* opcode, long long opcodeValue, const char* file, int line) noexcept;
extern "C" void LibcRunReportImageFormat_nid_no_patch(unsigned format, const char* name, int status, int usage) noexcept;
extern "C" void LibcRunReportDescriptor_nid_no_patch(int type, int status, int usage) noexcept;
extern "C" void LibcRunReportSetDescriptorCounters_nid_no_patch(const std::atomic<unsigned long long>* counters) noexcept;
extern "C" void LibcRunReportSetFormatCounters_nid_no_patch(const std::atomic<unsigned long long>* counters, unsigned count) noexcept;
extern "C" void LibcRunReportSetGpu_nid_no_patch(unsigned vendorId, const char* name, const char* driver, unsigned apiVersion) noexcept;

namespace RunReport {

inline constexpr std::size_t MaxBytes = 65536;
inline constexpr std::size_t MaxListedFunctions = 200;
inline constexpr std::size_t MaxListedFormats = 100;
inline constexpr std::size_t MaxTextCharacters = 300;
inline constexpr std::size_t FrameTimeWindow = 2048;
inline constexpr long long SlowCompileNanoseconds = 100000000LL;

template <typename TValue, std::size_t TCapacity>
class FixedRing {
    static_assert(TCapacity > 0u);

public:
    void Push(const TValue& value) noexcept {
        items[head] = value;
        head = (head + 1u) % TCapacity;
        if (count < TCapacity) ++count;
    }

    std::size_t Size() const noexcept { return count; }

    std::vector<TValue> Values() const {
        std::vector<TValue> values;
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) values.push_back(items[(head + TCapacity - count + index) % TCapacity]);
        return values;
    }

private:
    std::array<TValue, TCapacity> items{};
    std::size_t head = 0;
    std::size_t count = 0;
};

inline double FramePercentile(std::span<const double> sorted, double fraction) noexcept {
    return sorted.empty() ? 0.0 : sorted[std::min(sorted.size() - 1u, static_cast<std::size_t>(fraction * static_cast<double>(sorted.size())))];
}

enum class Kind { None, MissingFunction, UnsupportedShader, UnsupportedFormat, Crash, Assert, Other };
enum class Reason { GuestExit, QuitRequested, RuntimeStop, Crash, UncleanUnknown, Other };
enum class Exit { Clean, Crash, Abort, Killed };

struct Stop {
    Kind kind = Kind::None;
    Reason reason = Reason::GuestExit;
    std::optional<std::string> source;
    std::optional<std::string> message;
};

enum class Stage { Unknown, Loads, Starts, FirstDraw, Presents };
enum class Status { Accepted, Rejected, Unknown };

struct FunctionEntry {
    std::optional<std::string> library;
    std::string name;
    std::int64_t calls = 0;
};

struct UnsupportedEntry {
    std::optional<std::string> encoding;
    std::optional<std::string> opcode;
    std::optional<std::int64_t> opcodeValue;
    std::optional<std::vector<std::string>> modifiers;
    std::int64_t count = 0;
};

struct FormatEntry {
    std::int64_t format = 0;
    std::optional<std::string> name;
    Status status = Status::Unknown;
    int usage = 5;
    std::int64_t count = 0;
};

struct DescriptorEntry {
    int type = 0;
    Status status = Status::Unknown;
    int usage = 3;
    std::int64_t count = 0;
};

struct Coverage {
    bool milestones = false, missing = false, shaders = false, unsupported = false, imageFormats = false, descriptors = false;
};

struct Truncated {
    std::int64_t functions = 0, unsupported = 0, imageFormats = 0, descriptors = 0;
};

struct FrameTimes {
    double p50 = 0.0, p90 = 0.0, p99 = 0.0;
};

struct Observations {
    std::optional<double> loaded, started, firstDraw, firstPresent;
    std::int64_t framesPresented = 0;
    std::optional<double> averageFps, fpsWindowSeconds;
    std::optional<FrameTimes> frameMs;
    std::int64_t shadersCompiled = 0, shadersFailed = 0;
    std::optional<std::int64_t> cacheHits;
    std::optional<double> shaderCompileMs;
    std::optional<std::int64_t> slowCompiles;
    std::vector<FunctionEntry> functions;
    std::vector<UnsupportedEntry> unsupported;
    std::vector<FormatEntry> imageFormats;
    std::vector<DescriptorEntry> descriptors;
    Coverage coverage;
    Truncated truncated;
    bool lostObservations = false;
};

struct BuildInfo {
    std::optional<std::string> commit;
    std::optional<std::string> release;
    std::optional<bool> modified;
};

struct GpuInfo {
    std::optional<std::string> vendor;
    std::optional<std::string> name;
    std::optional<std::string> driver;
    std::optional<std::string> vulkan;
};

struct SystemInfo {
    std::optional<std::string> osVersion;
    std::optional<std::string> cpu;
    std::optional<std::int64_t> ramGiB;
    std::optional<GpuInfo> gpu;
};

struct GameInfo {
    std::optional<std::string> titleId;
    std::optional<std::string> contentVersion;
    std::optional<std::string> name;
};

struct Report {
    std::string generated;
    BuildInfo build;
    GameInfo game;
    std::optional<std::string> os;
    SystemInfo machine;
    std::string runId;
    double seconds = 0.0;
    Exit exit = Exit::Clean;
    std::optional<std::int64_t> exitCode;
    bool recovered = false;
    bool incomplete = false;
    Stop stop;
    Observations observed;
};

struct HostIdentifiers {
    std::vector<std::string> words;
    std::vector<std::string> users;
    std::vector<std::string> hosts;
    std::vector<std::string> osIdentity;
};

std::optional<std::string> OsReleaseValue(std::string_view raw);
bool PublicDistroId(std::string_view id);
std::vector<std::string> OsIdentityFromOsRelease(std::istream& stream);
HostIdentifiers MachineNames(const HostIdentifiers& host, bool forOsVersion);
std::string FormatOsVersion(std::string_view name, std::string_view versionId, std::string_view kernelRelease);

enum class Choice { Default, On, Off };

struct Toggle {
    bool enabled = true;
    bool explicitlyOn = false;
    std::vector<std::string> warnings;
};

using EnvironmentVariables = std::vector<std::pair<std::string, std::string>>;

struct Recipe {
    std::string generated;
    std::string runId;
    BuildInfo build;
    std::vector<std::string> arguments;
    EnvironmentVariables environment;
    std::vector<std::string> configFiles;
};

struct Settings {
    std::filesystem::path executableDirectory;
    std::filesystem::path userStateDirectory;
    std::string runId;
    BuildInfo build;
    std::function<GameInfo()> game;
    std::chrono::milliseconds tableWait{200};
    std::vector<std::string> requiredComponents;
    std::optional<SystemInfo> machine;
    Coverage coverage;
    std::vector<std::string> arguments;
    EnvironmentVariables environment;
    std::vector<std::string> configFiles;
};

bool RecipeVariableAllowed(std::string_view name);
EnvironmentVariables RecipeEnvironment(const EnvironmentVariables& all);
EnvironmentVariables ProcessEnvironment();
std::vector<std::string> ConfigFileNames(const EnvironmentVariables& environment);

void RegisterComponent(std::string_view name, std::string_view commit, int modified);
std::optional<std::string> SourceOf(std::string_view file, int line);
SystemInfo ProbeHost();
std::string GenerateRunId();
std::string FormatUtc(std::chrono::system_clock::time_point time);
std::string Serialize(const Report& report);
std::string Serialize(const Report& report, const HostIdentifiers& host);
std::string SerializeRecipe(const Recipe& recipe, const HostIdentifiers& host);
std::optional<std::string> SafeText(std::string_view text, const HostIdentifiers& host);
std::string RedactedValue(std::string_view text, const HostIdentifiers& host);
HostIdentifiers CurrentHostIdentifiers();
Toggle ResolveToggle(std::optional<std::string_view> flag, std::optional<std::string_view> environment);
std::optional<std::string> FlagFromArguments(const std::vector<std::string>& arguments);
std::vector<std::string> CommandLineArguments();
std::filesystem::path ExecutableDirectory();
std::filesystem::path UserStateDirectory();
bool WriteAtomically(const std::filesystem::path& target, std::string_view bytes, std::string_view uniqueName, std::string& error);
bool ReadBounded(const std::filesystem::path& path, std::size_t limit, std::string& bytes);
bool FolderIsReachable(const std::filesystem::path& folder);

class Collector {
public:
    explicit Collector(Settings settings);
    ~Collector();
    Collector(const Collector&) = delete;
    Collector& operator=(const Collector&) = delete;
    bool Start();
    void Finish(Exit exit, Stop stop, std::optional<std::int64_t> exitCode);
    bool Finished() const noexcept;
    const std::filesystem::path& ReportPath() const noexcept { return reportPath; }
    Report Snapshot(Exit exit, Stop stop, std::optional<std::int64_t> exitCode) const;
    void NoteQuitRequested() noexcept;
    void NoteMilestone(int milestone);
    void NoteFramePresented();
    std::vector<double> FrameTimesMilliseconds() const;
    void NoteMissingFunction(std::string_view library, std::string_view name, std::string_view source);
    void NoteShader(int what);
    void NoteCompileTime(long long nanoseconds);
    double CompileMilliseconds() const;
    void NoteUnsupportedInstruction(std::optional<std::string> encoding, std::optional<std::string> opcode, std::optional<std::int64_t> opcodeValue, std::string_view source);
    void NoteImageFormat(std::int64_t format, std::optional<std::string> name, Status status, int usage);
    void NoteDescriptor(int type, Status status, int usage);
    void NoteGpu(GpuInfo gpu);
    void SetDescriptorCounters(const std::atomic<unsigned long long>* counters);
    void SetFormatCounters(const std::atomic<unsigned long long>* counters, unsigned count);
    void NoteGame();
    std::unique_lock<std::timed_mutex> LockTablesForTest();
    struct Tables;

private:
    void RefreshGame(bool force = false);
    BuildInfo Provenance() const;
    Observations Observe() const;
    void NoteLost(int section);

    Settings settings;
    std::vector<std::filesystem::path> candidates;
    std::filesystem::path executableFolder;
    std::filesystem::path reportPath;
    std::chrono::steady_clock::time_point started;
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic<int> state{0};
    bool quitRequested = false;
    bool started_ = false;
    struct GameState;
    std::shared_ptr<GameState> game;
    mutable std::mutex machineMutex;
    SystemInfo machineInfo;
    std::shared_ptr<Tables> tables;
    std::atomic<std::int64_t> milestoneNanos[4];
    std::atomic<std::int64_t> frames{0};
    mutable std::mutex frameMutex;
    FixedRing<double, FrameTimeWindow> frameTimes;
    std::atomic<std::int64_t> firstFrameNanos{-1};
    std::atomic<std::int64_t> lastFrameNanos{-1};
    std::atomic<std::int64_t> compiled{0}, failedShaders{0}, cacheHitCount{0}, compileNanos{0}, slowCompileCount{0};
    std::atomic<int> lost[4];
    std::atomic<const std::atomic<unsigned long long>*> descriptorCounters{nullptr};
    std::atomic<const std::atomic<unsigned long long>*> formatCounters{nullptr};
    std::atomic<unsigned> formatCounterCount{0};
    HostIdentifiers host;
};

}

#endif
