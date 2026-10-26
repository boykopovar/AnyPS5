#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
extern "C" int kill(int, int);
#endif

#if defined(__linux__)
extern char** environ;
#endif

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Posix spawn check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int NoSuchFile = 2;
constexpr int BadDescriptor = 9;

template<typename TCall>
static bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename TCall>
static bool ReportsNotImplemented(TCall call, const std::string& message) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == message;
    }
    return false;
}

static void CheckAttributes() {
    PosixSpawnAttributes* attributes = nullptr;
    Check(posix_spawnattr_init_nid_postfix(&attributes) == 0 && attributes != nullptr);
    Check(attributes->flags == 0 && attributes->processGroup == 0 && attributes->schedulingPriority == 0 && attributes->schedulingPolicy == 0);
    const GuestSignalSet empty{};
    Check(std::memcmp(&attributes->signalDefault, &empty, sizeof(empty)) == 0 && std::memcmp(&attributes->signalMask, &empty, sizeof(empty)) == 0);

    Check(posix_spawnattr_setflags_nid_postfix(&attributes, 0x30) == 0 && attributes->flags == 0x30);
    Check(posix_spawnattr_setflags_nid_postfix(&attributes, -1) == 0 && attributes->flags == -1);
    Check(posix_spawnattr_setflags_nid_postfix(&attributes, 0) == 0 && attributes->flags == 0);

    const GuestSignalSet defaults{{0x00004002u, 0x80000000u, 1u, 0xffffffffu}};
    Check(posix_spawnattr_setsigdefault_nid_postfix(&attributes, &defaults) == 0);
    Check(std::memcmp(&attributes->signalDefault, &defaults, sizeof(defaults)) == 0);
    Check(std::memcmp(&attributes->signalMask, &empty, sizeof(empty)) == 0);

    std::int16_t flags = 1;
    std::int32_t value = 1;
    GuestSignalSet read{{9u, 9u, 9u, 9u}};
    Check(posix_spawnattr_getflags_nid_postfix(&attributes, &flags) == 0 && flags == 0);
    Check(posix_spawnattr_getsigdefault_nid_postfix(&attributes, &read) == 0 && std::memcmp(&read, &defaults, sizeof(read)) == 0);
    Check(posix_spawnattr_getsigmask_nid_postfix(&attributes, &read) == 0 && std::memcmp(&read, &empty, sizeof(read)) == 0);
    Check(posix_spawnattr_setpgroup_nid_postfix(&attributes, 77) == 0 && attributes->processGroup == 77);
    Check(posix_spawnattr_getpgroup_nid_postfix(&attributes, &value) == 0 && value == 77);
    const std::int32_t priority = 450;
    Check(posix_spawnattr_setschedparam_nid_postfix(&attributes, &priority) == 0 && attributes->schedulingPriority == 450);
    Check(posix_spawnattr_getschedparam_nid_postfix(&attributes, &value) == 0 && value == 450);
    Check(posix_spawnattr_setschedpolicy_nid_postfix(&attributes, 2) == 0 && attributes->schedulingPolicy == 2);
    Check(posix_spawnattr_getschedpolicy_nid_postfix(&attributes, &value) == 0 && value == 2);
    const GuestSignalSet mask{{1u, 2u, 3u, 4u}};
    Check(posix_spawnattr_setsigmask_nid_postfix(&attributes, &mask) == 0 && std::memcmp(&attributes->signalMask, &mask, sizeof(mask)) == 0);
    Check(posix_spawnattr_getsigmask_nid_postfix(&attributes, &read) == 0 && std::memcmp(&read, &mask, sizeof(read)) == 0);
    Check(std::memcmp(&attributes->signalDefault, &defaults, sizeof(defaults)) == 0);

    PosixSpawnAttributes* uninitialized = nullptr;
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getflags_nid_postfix(&uninitialized, &flags); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getpgroup_nid_postfix(&attributes, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setpgroup_nid_postfix(&uninitialized, 1); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setschedparam_nid_postfix(&attributes, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setsigmask_nid_postfix(&attributes, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setflags_nid_postfix(&uninitialized, 1); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setsigdefault_nid_postfix(&uninitialized, &defaults); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setsigdefault_nid_postfix(&attributes, nullptr); }));
    Check(ThrowsInvalidArgument([] { posix_spawnattr_init_nid_postfix(nullptr); }));

    Check(posix_spawnattr_destroy_nid_postfix(&attributes) == 0);
    Check(posix_spawnattr_destroy_nid_postfix(&uninitialized) == 0);
}

static void CheckFileActions() {
    PosixSpawnFileActions* actions = nullptr;
    Check(posix_spawn_file_actions_init_nid_postfix(&actions) == 0 && actions != nullptr && actions->actions.empty());

    char path[] = "/app0/log.txt";
    Check(posix_spawn_file_actions_addopen_nid_postfix(&actions, 3, path, 0x601, 0644) == 0);
    std::strcpy(path, "/changed");
    Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 3, 1) == 0);
    Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 3) == 0);
    Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 0, 0) == 0);

    Check(posix_spawn_file_actions_addopen_nid_postfix(&actions, -1, "/app0/other", 0, 0) == BadDescriptor);
    Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, -1, 1) == BadDescriptor);
    Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 1, -1) == BadDescriptor);
    Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, -2) == BadDescriptor);

    const auto& recorded = actions->actions;
    Check(recorded.size() == 4);
    Check(recorded[0].kind == PosixSpawnFileActionKind::Open && recorded[0].descriptor == 3);
    Check(recorded[0].path == "/app0/log.txt" && recorded[0].openFlags == 0x601 && recorded[0].mode == 0644);
    Check(recorded[1].kind == PosixSpawnFileActionKind::Duplicate && recorded[1].descriptor == 3 && recorded[1].newDescriptor == 1);
    Check(recorded[2].kind == PosixSpawnFileActionKind::Close && recorded[2].descriptor == 3);
    Check(recorded[3].kind == PosixSpawnFileActionKind::Duplicate && recorded[3].descriptor == 0 && recorded[3].newDescriptor == 0);

    PosixSpawnFileActions* uninitialized = nullptr;
    Check(posix_spawn_file_actions_addclose_nid_postfix(&uninitialized, -1) == BadDescriptor);
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_addclose_nid_postfix(&uninitialized, 1); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_adddup2_nid_postfix(&uninitialized, 1, 2); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_addopen_nid_postfix(&uninitialized, 1, "/app0/a", 0, 0); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_addopen_nid_postfix(&actions, 1, nullptr, 0, 0); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_destroy_nid_postfix(&uninitialized); }));
    Check(ThrowsInvalidArgument([] { posix_spawn_file_actions_init_nid_postfix(nullptr); }));
    Check(actions->actions.size() == 4);

    Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0);
}

static void CheckSpawn() {
    const auto name = "anyps5-spawn-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = std::filesystem::current_path() / name;
    Check(std::filesystem::create_directories(directory / "empty"));
    { std::ofstream file(directory / "tool.elf"); file << "tool"; }
    const std::string guest = "/" + name;
    const std::string existing = guest + "/tool.elf";
    const std::string missing = guest + "/missing.elf";
    const std::string bothDirectories = guest + "/empty:" + guest;
    const std::string emptyOnly = guest + "/empty";

    PosixSpawnAttributes* attributes = nullptr;
    PosixSpawnFileActions* actions = nullptr;
    Check(posix_spawnattr_init_nid_postfix(&attributes) == 0 && posix_spawn_file_actions_init_nid_postfix(&actions) == 0);
    Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 3) == 0);
    char program[] = "tool.elf";
    char* arguments[] = {program, nullptr};
    char* environment[] = {nullptr};
    std::int32_t processId = 77;

    Check(ReportsNotImplemented([&] { posix_spawn_nid_postfix(&processId, existing.c_str(), &actions, &attributes, arguments, environment); },
        "posix_spawn: executables outside /app0 (" + existing + ") not implemented"));
    Check(posix_spawn_nid_postfix(&processId, missing.c_str(), &actions, &attributes, arguments, environment) == NoSuchFile);
    Check(posix_spawn_nid_postfix(&processId, "", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(posix_spawn_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);

    Check(setenv_nid_postfix("PATH", bothDirectories.c_str(), 1) == 0);
    Check(ReportsNotImplemented([&] { posix_spawnp_nid_postfix(&processId, "tool.elf", &actions, &attributes, arguments, nullptr); },
        "posix_spawnp: executables outside /app0 (" + existing + ") not implemented"));
    Check(posix_spawnp_nid_postfix(&processId, "missing.elf", &actions, &attributes, arguments, nullptr) == NoSuchFile);
    Check(posix_spawnp_nid_postfix(&processId, "", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(setenv_nid_postfix("PATH", emptyOnly.c_str(), 1) == 0);
    Check(posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(ReportsNotImplemented([&] { posix_spawnp_nid_postfix(&processId, existing.c_str(), nullptr, nullptr, arguments, nullptr); },
        "posix_spawnp: executables outside /app0 (" + existing + ") not implemented"));
    Check(posix_spawnp_nid_postfix(&processId, missing.c_str(), nullptr, nullptr, arguments, nullptr) == NoSuchFile);

    Check(setenv_nid_postfix("PATH", (emptyOnly + ":").c_str(), 1) == 0);
    Check(posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(chdir_nid_postfix(guest.c_str()) == 0);
    Check(ReportsNotImplemented([&] { posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr); },
        "posix_spawnp: executables outside /app0 (./tool.elf) not implemented"));
    Check(ReportsNotImplemented([&] { posix_spawn_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr); },
        "posix_spawn: executables outside /app0 (tool.elf) not implemented"));
    Check(chdir_nid_postfix("/") == 0);

    Check(unsetenv_nid_postfix("PATH") == 0);
    Check(posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(processId == 77);

    PosixSpawnAttributes* uninitializedAttributes = nullptr;
    PosixSpawnFileActions* uninitializedActions = nullptr;
    Check(ThrowsInvalidArgument([&] { posix_spawn_nid_postfix(&processId, missing.c_str(), &uninitializedActions, nullptr, arguments, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnp_nid_postfix(&processId, "missing.elf", nullptr, &uninitializedAttributes, arguments, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_nid_postfix(&processId, nullptr, nullptr, nullptr, arguments, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnp_nid_postfix(&processId, nullptr, nullptr, nullptr, arguments, nullptr); }));

    Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0 && posix_spawnattr_destroy_nid_postfix(&attributes) == 0);
    std::filesystem::remove_all(directory);
}

static std::filesystem::path OwnExecutablePath() {
#ifdef _WIN32
    std::wstring own(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, own.data(), static_cast<DWORD>(own.size()));
    own.resize(length);
    return std::filesystem::path(own);
#else
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error);
#endif
}

static std::int32_t SpawnChild(const char* role) {
    const auto own = OwnExecutablePath();
    const auto target = own.parent_path() / std::filesystem::path(reinterpret_cast<const char8_t*>(("child.bin" + own.extension().string()).c_str()));
    std::error_code error;
    std::filesystem::copy_file(own, target, std::filesystem::copy_options::overwrite_existing, error);
    Check(!error);
    const auto app0 = std::filesystem::current_path() / "app0";
    Check(std::filesystem::create_directories(app0) || std::filesystem::exists(app0));
    { std::ofstream file(app0 / "child.bin"); file << "child"; }

    char program[] = "/app0/child.bin";
    char* arguments[] = {program, nullptr};
    std::vector<std::string> entries;
#ifdef _WIN32
    char** const parentEnvironment = _environ;
#else
    char** const parentEnvironment = environ;
#endif
    for (char** entry = parentEnvironment; entry != nullptr && *entry != nullptr; ++entry)
        entries.push_back(*entry);
    entries.push_back(std::string("ANYPS5_POSIX_SPAWN_ROLE=") + role);
    std::vector<char*> environment;
    for (auto& entry : entries)
        environment.push_back(entry.data());
    environment.push_back(nullptr);
    std::int32_t processId = 0;
    Check(posix_spawn_nid_postfix(&processId, program, nullptr, nullptr, arguments, environment.data()) == 0);
    Check(processId > 0);
    return processId;
}

static void CleanupChild() {
    std::error_code error;
    std::filesystem::remove(OwnExecutablePath().parent_path() / std::filesystem::path(reinterpret_cast<const char8_t*>(("child.bin" + OwnExecutablePath().extension().string()).c_str())), error);
    std::filesystem::remove_all(std::filesystem::current_path() / "app0", error);
}

static void CheckSpawnedChild(const char* role, int expectedCode) {
    const std::int32_t processId = SpawnChild(role);
    int status = -1;
    if (std::strcmp(role, "child-sleep") == 0)
        Check(waitpid_nid_postfix(processId, &status, 1) == 0);
    Check(waitpid_nid_postfix(processId, &status, 0) == processId);
    Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == expectedCode);
    status = -1;
    Check(waitpid_nid_postfix(processId, &status, 0) == -1 && *__error_nid_postfix() == 10);
    CleanupChild();
}

static void CheckConcurrentWaiters() {
    const std::int32_t processId = SpawnChild("child-sleep");
    int statusA = -1;
    int statusB = -1;
    int errorA = 0;
    int errorB = 0;
    int resultA = 0;
    int resultB = 0;
    const auto waiter = [processId](int* status, int* error) {
        const int result = waitpid_nid_postfix(processId, status, 0);
        *error = *__error_nid_postfix();
        return result;
    };
    std::thread threadA([&] { resultA = waiter(&statusA, &errorA); });
    std::thread threadB([&] { resultB = waiter(&statusB, &errorB); });
    threadA.join();
    threadB.join();
    const bool reapedA = resultA == processId;
    const bool reapedB = resultB == processId;
    Check(reapedA != reapedB);
    const int reapedStatus = reapedA ? statusA : statusB;
    Check((reapedStatus & 0x7f) == 0 && ((reapedStatus >> 8) & 0xff) == 43);
    Check((reapedA ? errorB : errorA) == 10);
    CleanupChild();
}

#ifndef _WIN32
static void CheckStoppedChild() {
    const std::int32_t processId = SpawnChild("child-stop");
    int status = -1;
    Check(waitpid_nid_postfix(processId, &status, 2) == processId);
    Check((status & 0x7f) == 0x7f && (status >> 8) == 17);
    Check(kill(processId, SIGCONT) == 0);
    Check(waitpid_nid_postfix(processId, &status, 4) == processId && status == 0x13);
    Check(waitpid_nid_postfix(processId, &status, 0) == processId);
    Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 44);
    Check(waitpid_nid_postfix(processId, &status, 0) == -1 && *__error_nid_postfix() == 10);
    CleanupChild();
}

static void CheckKilledChild(const char* role, int guestSignal) {
    const std::int32_t processId = SpawnChild(role);
    int status = -1;
    Check(waitpid_nid_postfix(processId, &status, 0) == processId);
    Check((status & 0x7f) == guestSignal);
    Check(waitpid_nid_postfix(processId, &status, 0) == -1 && *__error_nid_postfix() == 10);
    CleanupChild();
}
#else
static void CheckTerminatedChild(const char* role, unsigned code, int guestSignal) {
    const std::int32_t processId = SpawnChild(role);
    const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(processId));
    Check(process != nullptr);
    Check(TerminateProcess(process, code) != 0);
    CloseHandle(process);
    int status = -1;
    Check(waitpid_nid_postfix(processId, &status, 0) == processId);
    Check(code == 0xFFFFFFFFu ? ((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 255)
                              : ((status & 0x7f) == guestSignal && (status & 0x80) == 0));
    Check(waitpid_nid_postfix(processId, &status, 0) == -1 && *__error_nid_postfix() == 10);
    CleanupChild();
}
#endif

int main() {
    if (const char* role = std::getenv("ANYPS5_POSIX_SPAWN_ROLE")) {
        if (std::strcmp(role, "child-exit-42") == 0) std::_Exit(42);
        if (std::strcmp(role, "child-exit-neg") == 0) std::exit(-1);
#ifndef _WIN32
        if (std::strcmp(role, "child-stop") == 0) {
            std::raise(SIGSTOP);
            std::_Exit(44);
        }
        if (std::strcmp(role, "child-kill-bus") == 0) std::raise(SIGBUS);
        if (std::strcmp(role, "child-kill-sys") == 0) std::raise(SIGSYS);
#endif
        if (std::strcmp(role, "child-sleep") == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            std::_Exit(43);
        }
        if (std::strcmp(role, "child-laze") == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            std::_Exit(47);
        }
        std::_Exit(99);
    }
    CheckAttributes();
    CheckFileActions();
    CheckSpawn();
    CheckSpawnedChild("child-exit-42", 42);
    CheckSpawnedChild("child-sleep", 43);
    CheckSpawnedChild("child-exit-neg", 255);
    CheckConcurrentWaiters();
#ifndef _WIN32
    CheckStoppedChild();
    CheckKilledChild("child-kill-bus", 10);
    CheckKilledChild("child-kill-sys", 12);
#else
    CheckTerminatedChild("child-laze", 0xC0000005u, 11);
    CheckTerminatedChild("child-laze", 0xC0000096u, 4);
    CheckTerminatedChild("child-laze", 0x80000003u, 5);
    CheckTerminatedChild("child-laze", 0xC0000094u, 8);
    CheckTerminatedChild("child-laze", 0xC0000409u, 6);
    CheckTerminatedChild("child-laze", 0xFFFFFFFFu, 0);
#endif
}
