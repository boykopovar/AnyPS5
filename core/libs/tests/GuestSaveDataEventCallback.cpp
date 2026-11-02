#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <random>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;
constexpr std::int32_t User = 7;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata event callback check failed: %s\n", what.c_str());
    ++failures;
}

struct Call {
    int callback;
    SaveDataEvent event;
    void* userdata;
    std::thread::id thread;
    int terminated;
};

std::mutex callsMutex;
std::condition_variable callsChanged;
std::vector<Call> calls;

void Record(int callback, const SaveDataEvent* event, void* userdata, int terminated = 0) {
    std::lock_guard lock(callsMutex);
    calls.push_back({callback, *event, userdata, std::this_thread::get_id(), terminated});
    callsChanged.notify_all();
}

void APS5_VABI First(const SaveDataEvent* event, void* userdata) { Record(1, event, userdata); }
void APS5_VABI Second(const SaveDataEvent* event, void* userdata) { Record(2, event, userdata); }
void APS5_VABI Terminating(const SaveDataEvent* event, void* userdata) { Record(3, event, userdata, sceSaveDataTerminate()); }
void APS5_VABI Stale(const SaveDataEvent* event, void* userdata) { Record(4, event, userdata); }
void APS5_VABI Fence(const SaveDataEvent* event, void* userdata) { Record(5, event, userdata); }

std::mutex holdMutex;
std::condition_variable holdChanged;
bool holding = false;
bool released = false;

void APS5_VABI Holding(const SaveDataEvent*, void*) {
    std::unique_lock lock(holdMutex);
    holding = true;
    holdChanged.notify_all();
    holdChanged.wait(lock, [] { return released; });
}

std::size_t CallCount() {
    std::lock_guard lock(callsMutex);
    return calls.size();
}

bool WaitForCalls(std::size_t count) {
    std::unique_lock lock(callsMutex);
    return callsChanged.wait_for(lock, std::chrono::seconds(5), [&] { return calls.size() >= count; });
}

Call CallAt(std::size_t index) {
    std::lock_guard lock(callsMutex);
    return calls.at(index);
}

std::size_t CallsOf(int callback, std::size_t from) {
    std::lock_guard lock(callsMutex);
    std::size_t count = 0;
    for (std::size_t index = from; index < calls.size(); ++index) count += calls[index].callback == callback ? 1u : 0u;
    return count;
}

bool WaitForCallback(int callback, std::size_t from) {
    std::unique_lock lock(callsMutex);
    return callsChanged.wait_for(lock, std::chrono::seconds(5), [&] {
        for (std::size_t index = from; index < calls.size(); ++index) {
            if (calls[index].callback == callback) return true;
        }
        return false;
    });
}

class Watchdog {
public:
    explicit Watchdog(const char* what) : what(what), thread([this] {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(10), [this] { return done; })) {
            std::fprintf(stderr, "savedata event callback check failed: %s did not return within 10 s\n", this->what);
            std::_Exit(1);
        }
    }) {}

    ~Watchdog() {
        {
            std::lock_guard lock(mutex);
            done = true;
        }
        changed.notify_all();
        thread.join();
    }

    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;

private:
    const char* what;
    std::mutex mutex;
    std::condition_variable changed;
    bool done = false;
    std::thread thread;
};

#ifdef __linux__
std::size_t TaskCount() {
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator("/proc/self/task"), std::filesystem::directory_iterator{}));
}

bool WaitForTaskCount(std::size_t count) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (TaskCount() != count) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}
#endif

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    Check(static_cast<bool>(file), "writes " + path.string());
}

SceSaveDataDirName DirName(const char* name) {
    SceSaveDataDirName dirName{};
    std::strncpy(dirName.data, name, sizeof(dirName.data) - 1);
    return dirName;
}

SceSaveDataTitleId TitleId(const char* name) {
    SceSaveDataTitleId titleId{};
    std::strncpy(titleId.data, name, sizeof(titleId.data) - 1);
    return titleId;
}

int BackUp(const char* name) {
    const auto dirName = DirName(name);
    const auto titleId = TitleId("PPSA00000");
    SaveDataBackup backup{};
    backup.user_id = User;
    backup.title_id = &titleId;
    backup.dir_name = &dirName;
    return sceSaveDataBackup(&backup);
}

SaveDataEvent Poll(const std::string& what) {
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, what + " is polled");
    return event;
}

bool SameEvent(const SaveDataEvent& left, const SaveDataEvent& right) {
    return left.type == right.type && left.error_code == right.error_code && left.user_id == right.user_id && std::strncmp(left.title_id.data, right.title_id.data, sizeof(left.title_id.data)) == 0 && std::strncmp(left.dir_name.data, right.dir_name.data, sizeof(left.dir_name.data)) == 0;
}

void CheckDelivered(std::size_t index, int callback, void* userdata, std::uint32_t type, const char* dir, const std::string& what) {
    const auto call = CallAt(index);
    Check(call.callback == callback, what + " reaches callback " + std::to_string(callback) + ", got " + std::to_string(call.callback));
    Check(call.userdata == userdata, what + " passes the registered userdata");
    Check(call.event.type == type, what + " has type " + std::to_string(type) + ", got " + std::to_string(call.event.type));
    Check(call.event.error_code == 0, what + " has error 0, got " + std::to_string(call.event.error_code));
    Check(call.event.user_id == User, what + " names the user");
    Check(std::strncmp(call.event.title_id.data, "PPSA00000", sizeof(call.event.title_id.data)) == 0, what + " names the title");
    Check(std::strncmp(call.event.dir_name.data, dir, sizeof(call.event.dir_name.data)) == 0, what + " names the save " + dir);
    Check(call.thread != std::this_thread::get_id(), what + " runs on another thread than the caller");
    const auto polled = Poll(what);
    Check(SameEvent(polled, call.event), what + " is the event the poll returns");
}

void CheckSessionsWithoutRegistration() {
#ifdef __linux__
    const auto before = TaskCount();
#endif
    for (int session = 0; session < 2; ++session) {
        const auto label = "session " + std::to_string(session) + " without a registration";
        Check(sceSaveDataInitialize3(nullptr) == 0, label + ": SaveData initializes");
#ifdef __linux__
        Check(TaskCount() == before, label + ": Initialize3 starts no thread, " + std::to_string(before) + " threads before and " + std::to_string(TaskCount()) + " after");
#endif
        Check(sceSaveDataTerminate() == 0, label + ": SaveData terminates");
    }
}

#ifdef __linux__
void CheckDeliveryThreadStartsAtRegistration() {
    const auto before = TaskCount();
    const auto counted = [&](const std::string& what) { return what + ", " + std::to_string(before) + " threads before and " + std::to_string(TaskCount()) + " now"; };
    int tag = 0;
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes for the delivery thread check");
    Check(TaskCount() == before, counted("Initialize3 starts no delivery thread"));
    Check(sceSaveDataRegisterEventCallback(nullptr, &tag) == SaveDataErrorParameter, "a null callback is PARAMETER in the delivery thread check");
    Check(TaskCount() == before, counted("a refused registration starts no delivery thread"));
    Check(sceSaveDataRegisterEventCallback(First, &tag) == 0, "the first callback registers in the delivery thread check");
    Check(TaskCount() == before + 1, counted("the first registration starts one delivery thread"));
    Check(sceSaveDataRegisterEventCallback(Second, &tag) == 0, "the second callback registers in the delivery thread check");
    Check(sceSaveDataUnregisterEventCallback() == 0, "the second callback unregisters in the delivery thread check");
    Check(sceSaveDataRegisterEventCallback(First, &tag) == 0, "the first callback registers again in the delivery thread check");
    Check(TaskCount() == before + 1, counted("a replacing registration and one after an unregister reuse the delivery thread"));
    {
        Watchdog watchdog("the terminate that joins the delivery thread");
        Check(sceSaveDataTerminate() == 0, "SaveData terminates after the delivery thread check");
    }
    Check(WaitForTaskCount(before), counted("the final terminate joins the delivery thread within 5 s"));
}
#endif

void CheckRegistration() {
    int tag = 0;
    Check(sceSaveDataRegisterEventCallback(First, &tag) == SaveDataErrorNotInitialized, "register before initialize is NOT_INITIALIZED");
    Check(sceSaveDataUnregisterEventCallback() == 0, "unregister before initialize succeeds");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Check(sceSaveDataRegisterEventCallback(nullptr, &tag) == SaveDataErrorParameter, "a null callback is PARAMETER");
}

void CheckBackupDelivery(int* firstTag) {
    Check(sceSaveDataRegisterEventCallback(First, firstTag) == 0, "the first callback registers");
    Check(BackUp("backed") == 0, "the backup succeeds");
    Check(WaitForCalls(1), "the backup's completion reaches the callback within 5 s");
    Check(CallCount() == 1, "the backup is delivered once");
    if (CallCount() >= 1) CheckDelivered(0, 1, firstTag, 2, "backed", "the backup event");
    SaveDataEvent none{};
    Check(sceSaveDataGetEventResult(nullptr, &none) == SaveDataErrorNotFound, "the backup posts one event to poll");
}

void CheckUmountWithBackupDelivery(int* firstTag) {
    std::filesystem::create_directories("app0/sce_sys");
    WriteText("app0/sce_sys/param.json", "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}}}");
    auto dirName = DirName("mounted");
    SaveDataMount3 mount{};
    mount.user_id = User;
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult mounted{};
    Check(sceSaveDataMount3(&mount, &mounted) == 0, "the save mounts");
    const auto before = CallCount();
    Check(sceSaveDataUmountWithBackup(&mounted.mount_point) == 0, "umountWithBackup succeeds");
    Check(WaitForCalls(before + 1), "umountWithBackup's completion reaches the callback within 5 s");
    Check(CallCount() == before + 1, "umountWithBackup is delivered once");
    if (CallCount() >= before + 1) CheckDelivered(before, 1, firstTag, 1, "mounted", "the umountWithBackup event");
}

void CheckUnregister(int* firstTag, int* secondTag) {
    Check(sceSaveDataUnregisterEventCallback() == 0, "the first callback unregisters");
    const auto before = CallCount();
    Check(BackUp("unwatched") == 0, "a backup with no callback succeeds");
    const auto unwatched = Poll("the backup with no callback");
    Check(unwatched.type == 2 && std::strncmp(unwatched.dir_name.data, "unwatched", sizeof(unwatched.dir_name.data)) == 0, "the backup with no callback still posts its event");
    Check(sceSaveDataRegisterEventCallback(Second, secondTag) == 0, "the second callback registers");
    Check(BackUp("watched") == 0, "the second backup succeeds");
    Check(WaitForCalls(before + 1), "the second backup reaches the second callback within 5 s");
    Check(CallCount() == before + 1, "only the second backup is delivered, got " + std::to_string(CallCount() - before) + " calls");
    if (CallCount() >= before + 1) CheckDelivered(before, 2, secondTag, 2, "watched", "the event after the callback was unregistered");
    Check(sceSaveDataRegisterEventCallback(First, firstTag) == 0, "the first callback registers again");
    int replacedTag = 0;
    Check(sceSaveDataRegisterEventCallback(Second, &replacedTag) == 0, "the second callback replaces the first");
    Check(BackUp("replaced") == 0, "the backup after the replacement succeeds");
    Check(WaitForCalls(before + 2), "the backup after the replacement is delivered within 5 s");
    Check(CallCount() == before + 2, "the backup after the replacement is delivered once");
    if (CallCount() >= before + 2) CheckDelivered(before + 1, 2, &replacedTag, 2, "replaced", "the event after the replacement");
}

void CheckTerminateFromCallback() {
    const auto before = CallCount();
    int tag = 0;
    {
        Watchdog watchdog("a terminate from inside the callback");
        Check(sceSaveDataRegisterEventCallback(Terminating, &tag) == 0, "the terminating callback registers");
        Check(BackUp("terminating") == 0, "the backup for the terminating callback succeeds");
        Check(WaitForCalls(before + 1), "the terminating callback runs within 5 s");
    }
    if (CallCount() >= before + 1) Check(CallAt(before).terminated == SaveDataErrorBusy, "a terminate from inside the callback is BUSY, got " + std::to_string(CallAt(before).terminated));
    Poll("the terminating callback's backup");
    Check(sceSaveDataUnregisterEventCallback() == 0, "the terminating callback unregisters");
}

void CheckIdleTerminateAndRestart(int* firstTag) {
    {
        Watchdog watchdog("an idle terminate");
        Check(sceSaveDataTerminate() == 0, "SaveData terminates with the delivery thread idle");
    }
    Check(sceSaveDataRegisterEventCallback(First, firstTag) == SaveDataErrorNotInitialized, "register after terminate is NOT_INITIALIZED");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes again");
    const auto before = CallCount();
    Check(BackUp("restarted") == 0, "a backup after the restart succeeds");
    Poll("the backup after the restart");
    Check(sceSaveDataRegisterEventCallback(First, firstTag) == 0, "the first callback registers after the restart");
    Check(BackUp("redelivered") == 0, "a backup with a callback after the restart succeeds");
    Check(WaitForCalls(before + 1), "the backup after the restart is delivered within 5 s");
    Check(CallCount() == before + 1, "terminate dropped the callback: the restart delivers only after registering again");
    if (CallCount() >= before + 1) CheckDelivered(before, 1, firstTag, 2, "redelivered", "the event after the restart");
    {
        Watchdog watchdog("the final terminate");
        Check(sceSaveDataTerminate() == 0, "SaveData terminates again");
    }
}

struct Race {
    std::atomic<bool> start{false};
    int staleTag = 0;
    int registered = 0;
    int terminated = 0;
};

struct RaceOutcomes {
    int notInitialized = 0;
    int registered = 0;
};

void* APS5_VABI RaceRegister(void* argument) {
    auto& race = *static_cast<Race*>(argument);
    while (!race.start.load(std::memory_order_acquire)) std::this_thread::yield();
    race.registered = sceSaveDataRegisterEventCallback(Stale, &race.staleTag);
    return nullptr;
}

void* APS5_VABI RaceTerminate(void* argument) {
    auto& race = *static_cast<Race*>(argument);
    while (!race.start.load(std::memory_order_acquire)) std::this_thread::yield();
    race.terminated = sceSaveDataTerminate();
    return nullptr;
}

Pthread Spawn(PthreadEntry entry, Race& race, const char* name) {
    Pthread thread = nullptr;
    if (scePthreadCreate(&thread, nullptr, entry, &race, name) != 0) {
        std::fprintf(stderr, "savedata event callback check failed: scePthreadCreate failed for %s\n", name);
        std::_Exit(1);
    }
    return thread;
}

bool WaitForFinalTerminate() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (sceSaveDataDeleteTransactionResource(0) != SaveDataErrorNotInitialized) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::yield();
    }
    return sceSaveDataTerminate() == SaveDataErrorNotInitialized;
}

bool RaceRegisterWithFinalTerminate(int iteration, bool hold, RaceOutcomes& outcomes) {
    const auto failuresBefore = failures;
    const auto label = "iteration " + std::to_string(iteration) + (hold ? " with the delivery thread held" : " without a held delivery thread");
    Check(sceSaveDataInitialize3(nullptr) == 0, label + ": SaveData initializes");
    int holdTag = 0;
    if (hold) {
        {
            std::lock_guard lock(holdMutex);
            holding = false;
            released = false;
        }
        Check(sceSaveDataRegisterEventCallback(Holding, &holdTag) == 0, label + ": the holding callback registers");
        Check(BackUp("held") == 0, label + ": the backup for the holding callback succeeds");
        std::unique_lock lock(holdMutex);
        Check(holdChanged.wait_for(lock, std::chrono::seconds(5), [] { return holding; }), label + ": the holding callback runs within 5 s");
    }
    Race race;
    const auto registering = Spawn(RaceRegister, race, "SaveDataRaceRegister");
    const auto terminating = Spawn(RaceTerminate, race, "SaveDataRaceTerminate");
    race.start.store(true, std::memory_order_release);
    if (hold) {
        Check(WaitForFinalTerminate(), label + ": the racing terminate reaches zero initializations within 5 s");
        {
            std::lock_guard lock(holdMutex);
            released = true;
        }
        holdChanged.notify_all();
    }
    Check(scePthreadJoin(registering, nullptr) == 0, label + ": the registering thread joins");
    Check(scePthreadJoin(terminating, nullptr) == 0, label + ": the terminating thread joins");
    Check(race.terminated == 0, label + ": the racing final terminate succeeds, got " + std::to_string(race.terminated));
    Check(race.registered == 0 || race.registered == SaveDataErrorNotInitialized, label + ": the racing registration is OK or NOT_INITIALIZED, got " + std::to_string(race.registered));
    if (race.registered == SaveDataErrorNotInitialized) ++outcomes.notInitialized;
    if (race.registered == 0) ++outcomes.registered;
    const auto before = CallCount();
    Check(sceSaveDataInitialize3(nullptr) == 0, label + ": SaveData initializes again");
    Check(BackUp("raced") == 0, label + ": the backup after the race succeeds");
    int fenceTag = 0;
    Check(sceSaveDataRegisterEventCallback(Fence, &fenceTag) == 0, label + ": the fence callback registers");
    Check(BackUp("fenced") == 0, label + ": the fenced backup succeeds");
    Check(WaitForCallback(5, before), label + ": the fenced backup reaches the fence callback within 5 s");
    Check(CallsOf(4, before) == 0, label + ": the callback whose registration returned " + std::to_string(race.registered) + " beside the final terminate is called after Initialize3, " + std::to_string(CallsOf(4, before)) + " time(s)");
    Check(sceSaveDataUnregisterEventCallback() == 0, label + ": the fence callback unregisters");
    SaveDataEvent drained{};
    while (sceSaveDataGetEventResult(nullptr, &drained) == 0) {
    }
    Check(sceSaveDataTerminate() == 0, label + ": SaveData terminates after the race");
    return failures == failuresBefore;
}

void CheckRegisterRacingFinalTerminate() {
    constexpr int Iterations = 200;
    RaceOutcomes held;
    RaceOutcomes plain;
    int ran = 0;
    while (ran < Iterations) {
        Watchdog watchdog("a registration racing the final terminate");
        const bool hold = ran % 2 == 0;
        const bool passed = RaceRegisterWithFinalTerminate(ran, hold, hold ? held : plain);
        ++ran;
        if (!passed) break;
    }
    std::printf("register against the final terminate: %d of %d iterations; held delivery thread: %d NOT_INITIALIZED, %d OK; plain race: %d NOT_INITIALIZED, %d OK\n", ran, Iterations, held.notInitialized, held.registered, plain.notInitialized, plain.registered);
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-event-callback-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    int firstTag = 0;
    int secondTag = 0;
    CheckSessionsWithoutRegistration();
#ifdef __linux__
    CheckDeliveryThreadStartsAtRegistration();
#endif
    CheckRegistration();
    CheckBackupDelivery(&firstTag);
    CheckUmountWithBackupDelivery(&firstTag);
    CheckUnregister(&firstTag, &secondTag);
    CheckTerminateFromCallback();
    CheckIdleTerminateAndRestart(&firstTag);
    CheckRegisterRacingFinalTerminate();
    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata event callback tests passed\n");
    return 0;
}
