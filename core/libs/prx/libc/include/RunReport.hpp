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
#include <thread>
#include <vector>

#define RUN_REPORT_HAS_FINISH_HOOK 1

struct RunReportGame {
    char titleId[16];
    char contentVersion[32];
    char name[320];
};

extern "C" void LibcRunReportSetGameProvider_nid_no_patch(bool (*provider)(RunReportGame*));
extern "C" void LibcRunReportFinishExit_nid_no_patch(int exitCode) noexcept;
extern "C" const char* LibcRunReportRelaunchEnvironment_nid_no_patch() noexcept;
extern "C" void LibcRunReportResume_nid_no_patch() noexcept;
extern "C" bool LibcRunReportHandoff_nid_no_patch(int exitCode) noexcept;
extern "C" void LibcRunReportBeginWorker_nid_no_patch() noexcept;
extern "C" void LibcRunReportRegisterComponent_nid_no_patch(const char* name, const char* commit, int modified) noexcept;
extern "C" void LibcRunReportMissingFunction_nid_no_patch(const char* name, const char* file, int line) noexcept;
extern "C" void LibcRunReportQuitRequested_nid_no_patch() noexcept;
extern "C" bool LibcRunReportEnabled_nid_no_patch() noexcept;
extern "C" void LibcRunReportMilestone_nid_no_patch(int milestone) noexcept;
extern "C" void LibcRunReportFramePresented_nid_no_patch() noexcept;
extern "C" void LibcRunReportShader_nid_no_patch(int what) noexcept;
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
    bool worker = true;
    BuildInfo build;
    std::function<GameInfo()> game;
    std::function<void()> beforeCheckpointRemoval;
    std::function<void()> duringCheckpointWrite;
    std::function<void()> afterResumeRead;
    std::chrono::milliseconds checkpointInterval{5000};
    std::chrono::milliseconds lockWait{2000};
    std::chrono::milliseconds tableWait{200};
    bool deferredWorker = false;
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
std::vector<std::filesystem::path> RecoverAbandonedRuns(const std::vector<std::filesystem::path>& directories, std::string& warnings, const std::vector<std::filesystem::path>& nestedRoots = {});
std::optional<Report> ParseCheckpoint(std::string_view bytes, std::string_view id, const HostIdentifiers& host);

class FileLock {
public:
    static std::unique_ptr<FileLock> TryAcquire(const std::filesystem::path& path, bool& busy);
    ~FileLock();
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
    bool IsCurrent() const;
    void RemoveAndRelease();
    void* NativeHandle() const;
private:
    FileLock() = default;
    struct Handle;
    std::unique_ptr<Handle> handle;
    std::filesystem::path path;
};

struct PublishRelease {
    void operator()(FileLock* lock) const {
        if (lock == nullptr) return;
        lock->RemoveAndRelease();
        delete lock;
    }
};
using PublishLock = std::unique_ptr<FileLock, PublishRelease>;

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
    const std::filesystem::path& CheckpointPath() const noexcept { return checkpointPath; }
    Report Snapshot(Exit exit, Stop stop, std::optional<std::int64_t> exitCode, bool recovered) const;
    void NoteQuitRequested() noexcept;
    void NoteMilestone(int milestone);
    void NoteFramePresented();
    void NoteMissingFunction(std::string_view library, std::string_view name, std::string_view source);
    void NoteShader(int what);
    void NoteUnsupportedInstruction(std::optional<std::string> encoding, std::optional<std::string> opcode, std::optional<std::int64_t> opcodeValue, std::string_view source);
    void NoteImageFormat(std::int64_t format, std::optional<std::string> name, Status status, int usage);
    void NoteDescriptor(int type, Status status, int usage);
    void NoteGpu(GpuInfo gpu);
    void SetDescriptorCounters(const std::atomic<unsigned long long>* counters);
    void SetFormatCounters(const std::atomic<unsigned long long>* counters, unsigned count);
    void NoteGame();
    bool WriteCheckpoint();
    bool WriteCheckpoint(std::chrono::milliseconds wait);
    bool Resume();
    bool Finalized() const noexcept { return finalized; }
    void BeginWorker();
    std::unique_lock<std::timed_mutex> LockTablesForTest();
    struct Tables;

private:
    PublishLock AcquirePublishLock(std::chrono::milliseconds wait) const;
    void Work();
    void RefreshGame(bool force = false);
    BuildInfo Provenance() const;
    void CheckpointSoon();
    Observations Observe() const;
    void NoteLost(int section);

    Settings settings;
    std::filesystem::path directory;
    std::filesystem::path reportPath;
    std::filesystem::path checkpointPath;
    std::filesystem::path lockPath;
    std::unique_ptr<FileLock> lock;
    std::chrono::steady_clock::time_point started;
    std::mutex mutex;
    std::timed_mutex writeMutex;
    std::condition_variable changed;
    std::thread worker;
    bool stopWorker = false;
    bool workerBusy = false;
    std::atomic<int> state{0};
    bool quitRequested = false;
    bool started_ = false;
    bool workerRunning = false;
    bool workerWanted = false;
    bool reportPublished = false;
    std::string publishedBytes;
    std::atomic<bool> finalized{false};
    std::atomic<int> checkpointsRequested{0};
    std::atomic<bool> checkpointWanted{false};
    struct GameState;
    std::shared_ptr<GameState> game;
    mutable std::mutex machineMutex;
    SystemInfo machineInfo;
    std::shared_ptr<Tables> tables;
    std::atomic<std::int64_t> milestoneNanos[4];
    std::atomic<std::int64_t> frames{0};
    std::atomic<std::int64_t> firstFrameNanos{-1};
    std::atomic<std::int64_t> lastFrameNanos{-1};
    std::atomic<std::int64_t> compiled{0}, failedShaders{0}, cacheHitCount{0};
    std::atomic<int> lost[4];
    std::atomic<const std::atomic<unsigned long long>*> descriptorCounters{nullptr};
    std::atomic<const std::atomic<unsigned long long>*> formatCounters{nullptr};
    std::atomic<unsigned> formatCounterCount{0};
    HostIdentifiers host;
};

#ifdef _WIN32
void ProcessDetachCleanup();
#endif

}

#endif
