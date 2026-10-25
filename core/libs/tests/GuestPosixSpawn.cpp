#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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
        "posix_spawn: process creation not implemented"));
    Check(posix_spawn_nid_postfix(&processId, missing.c_str(), &actions, &attributes, arguments, environment) == NoSuchFile);
    Check(posix_spawn_nid_postfix(&processId, "", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(posix_spawn_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);

    Check(setenv_nid_postfix("PATH", bothDirectories.c_str(), 1) == 0);
    Check(ReportsNotImplemented([&] { posix_spawnp_nid_postfix(&processId, "tool.elf", &actions, &attributes, arguments, nullptr); },
        "posix_spawnp: process creation not implemented"));
    Check(posix_spawnp_nid_postfix(&processId, "missing.elf", &actions, &attributes, arguments, nullptr) == NoSuchFile);
    Check(posix_spawnp_nid_postfix(&processId, "", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(setenv_nid_postfix("PATH", emptyOnly.c_str(), 1) == 0);
    Check(posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(ReportsNotImplemented([&] { posix_spawnp_nid_postfix(&processId, existing.c_str(), nullptr, nullptr, arguments, nullptr); },
        "posix_spawnp: process creation not implemented"));
    Check(posix_spawnp_nid_postfix(&processId, missing.c_str(), nullptr, nullptr, arguments, nullptr) == NoSuchFile);

    Check(setenv_nid_postfix("PATH", (emptyOnly + ":").c_str(), 1) == 0);
    Check(posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr) == NoSuchFile);
    Check(chdir_nid_postfix(guest.c_str()) == 0);
    Check(ReportsNotImplemented([&] { posix_spawnp_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr); },
        "posix_spawnp: process creation not implemented"));
    Check(ReportsNotImplemented([&] { posix_spawn_nid_postfix(&processId, "tool.elf", nullptr, nullptr, arguments, nullptr); },
        "posix_spawn: process creation not implemented"));
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

int main() {
    CheckAttributes();
    CheckFileActions();
    CheckSpawn();
}
