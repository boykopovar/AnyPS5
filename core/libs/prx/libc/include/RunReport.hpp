#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_RUNREPORT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_RUNREPORT_HPP

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
#include <string>
#include <string_view>
#include <vector>

struct RunReportGame {
    char titleId[16];
    char contentVersion[32];
    char name[320];
};

extern "C" void LibcRunReportSetGameProvider_nid_no_patch(bool (*provider)(RunReportGame*));
extern "C" void LibcRunReportFinishExit_nid_no_patch(int exitCode) noexcept;
extern "C" void LibcRunReportRegisterComponent_nid_no_patch(const char* name, const char* commit, int modified) noexcept;
extern "C" void LibcRunReportMissingFunction_nid_no_patch(const char* name, const char* file, int line) noexcept;

namespace RunReport {

inline constexpr std::size_t MaxBytes = 65536;
inline constexpr std::size_t MaxListedFunctions = 200;
inline constexpr std::size_t MaxListedFormats = 100;
inline constexpr std::size_t MaxTextCharacters = 300;

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

struct Observations {
    std::optional<double> loaded, started, firstDraw, firstPresent;
    std::int64_t framesPresented = 0;
    std::optional<double> averageFps, fpsWindowSeconds;
    std::int64_t shadersCompiled = 0, shadersFailed = 0;
    std::optional<std::int64_t> cacheHits;
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
};

void RegisterComponent(std::string_view name, std::string_view commit, int modified);
std::optional<std::string> SourceOf(std::string_view file, int line);
SystemInfo ProbeHost();
std::string GenerateRunId();
std::string FormatUtc(std::chrono::system_clock::time_point time);
std::string Serialize(const Report& report);
std::string Serialize(const Report& report, const HostIdentifiers& host);
std::optional<std::string> SafeText(std::string_view text, const HostIdentifiers& host);
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
    void NoteMissingFunction(std::string_view library, std::string_view name, std::string_view source);
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
    bool started_ = false;
    struct GameState;
    std::shared_ptr<GameState> game;
    mutable std::mutex machineMutex;
    SystemInfo machineInfo;
    std::shared_ptr<Tables> tables;
    std::atomic<int> lost[4];
    HostIdentifiers host;
};

}

#endif
