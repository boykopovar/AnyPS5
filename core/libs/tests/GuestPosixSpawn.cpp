#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#include <windows.h>
#else
extern "C" int kill(int, int);
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef _WIN32
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

    auto* const unmapped = reinterpret_cast<PosixSpawnAttributes**>(0x10);
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_init_nid_postfix(unmapped); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_destroy_nid_postfix(unmapped); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setflags_nid_postfix(unmapped, 1); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getflags_nid_postfix(unmapped, &flags); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getflags_nid_postfix(&attributes, reinterpret_cast<std::int16_t*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getpgroup_nid_postfix(&attributes, reinterpret_cast<std::int32_t*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getschedparam_nid_postfix(&attributes, reinterpret_cast<std::int32_t*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getschedpolicy_nid_postfix(&attributes, reinterpret_cast<std::int32_t*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getsigdefault_nid_postfix(&attributes, reinterpret_cast<GuestSignalSet*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_getsigmask_nid_postfix(&attributes, reinterpret_cast<GuestSignalSet*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setsigdefault_nid_postfix(&attributes, reinterpret_cast<const GuestSignalSet*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setschedparam_nid_postfix(&attributes, reinterpret_cast<const std::int32_t*>(0x10)); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnattr_setsigmask_nid_postfix(&attributes, reinterpret_cast<const GuestSignalSet*>(0x10)); }));
    Check(attributes->schedulingPriority == 450 && std::memcmp(&attributes->signalMask, &mask, sizeof(mask)) == 0
          && std::memcmp(&attributes->signalDefault, &defaults, sizeof(defaults)) == 0);

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
    auto* const unmapped = reinterpret_cast<PosixSpawnFileActions**>(0x10);
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_init_nid_postfix(unmapped); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_destroy_nid_postfix(unmapped); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_addclose_nid_postfix(unmapped, 1); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_adddup2_nid_postfix(unmapped, 1, 2); }));
    Check(ThrowsInvalidArgument([&] { posix_spawn_file_actions_addopen_nid_postfix(unmapped, 1, "/app0/a", 0, 0); }));
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
    Check(posix_spawn_nid_postfix(nullptr, missing.c_str(), &actions, &attributes, arguments, environment) == NoSuchFile);
    Check(posix_spawnp_nid_postfix(nullptr, "missing.elf", &actions, &attributes, arguments, nullptr) == NoSuchFile);
    Check(ThrowsInvalidArgument([&] { posix_spawn_nid_postfix(nullptr, nullptr, nullptr, nullptr, arguments, nullptr); }));
    Check(ThrowsInvalidArgument([&] { posix_spawnp_nid_postfix(nullptr, nullptr, nullptr, nullptr, arguments, nullptr); }));

    Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0 && posix_spawnattr_destroy_nid_postfix(&attributes) == 0);
    std::filesystem::remove_all(directory);
}

static std::filesystem::path OwnExecutablePath() {
#ifdef _WIN32
    std::wstring own(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, own.data(), static_cast<DWORD>(own.size()));
    own.resize(length);
    return std::filesystem::path(own);
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    if (_NSGetExecutablePath(executable.data(), &size) != 0) return {};
    return std::filesystem::canonical(executable.data(), error);
#else
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error);
#endif
}

struct ChildEnvironment {
    std::vector<std::string> storage;
    std::vector<char*> pointers;
};

static ChildEnvironment BuildRoleEnvironment(const char* role) {
    ChildEnvironment result;
#ifdef _WIN32
    const LPWCH block = GetEnvironmentStringsW();
    Check(block != nullptr);
    for (const wchar_t* entry = block; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry, -1, nullptr, 0, nullptr, nullptr);
        Check(size > 0);
        std::string narrow(static_cast<std::size_t>(size), '\0');
        Check(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry, -1, narrow.data(), size, nullptr, nullptr) == size);
        narrow.pop_back();
        if (narrow.rfind("ANYPS5_POSIX_SPAWN_ROLE=", 0) != 0)
            result.storage.push_back(std::move(narrow));
    }
    FreeEnvironmentStringsW(block);
#else
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        if (std::strncmp(*entry, "ANYPS5_POSIX_SPAWN_ROLE=", 24) != 0)
            result.storage.push_back(*entry);
    }
#endif
    result.storage.push_back(std::string("ANYPS5_POSIX_SPAWN_ROLE=") + role);
    result.pointers.reserve(result.storage.size() + 1);
    for (auto& entry : result.storage)
        result.pointers.push_back(entry.data());
    result.pointers.push_back(nullptr);
    return result;
}

static void PrepareChildBinary() {
    const auto own = OwnExecutablePath();
    const auto target = own.parent_path() / std::filesystem::path(reinterpret_cast<const char8_t*>(("child.bin" + own.extension().string()).c_str()));
    std::error_code error;
    std::filesystem::copy_file(own, target, std::filesystem::copy_options::overwrite_existing, error);
    Check(!error);
    const auto app0 = std::filesystem::current_path() / "app0";
    Check(std::filesystem::create_directories(app0) || std::filesystem::exists(app0));
    { std::ofstream file(app0 / "child.bin"); file << "child"; Check(file.good()); }
}

static std::int32_t SpawnChildExplicit(const char* role, std::int32_t* processId, bool usePath = false,
    PosixSpawnFileActions** actions = nullptr, int expectedResult = 0) {
    PrepareChildBinary();
    char program[] = "/app0/child.bin";
    char* arguments[] = {program, nullptr};
    auto environment = BuildRoleEnvironment(role);
    if (usePath) {
        Check(posix_spawnp_nid_postfix(processId, program, actions, nullptr, arguments, environment.pointers.data()) == expectedResult);
    } else {
        Check(posix_spawn_nid_postfix(processId, program, actions, nullptr, arguments, environment.pointers.data()) == expectedResult);
    }
    return processId != nullptr ? *processId : 0;
}

static std::int32_t SpawnChild(const char* role) {
    std::int32_t processId = 0;
    Check(SpawnChildExplicit(role, &processId) > 0);
    return processId;
}

static void CleanupChild() {
    std::error_code error;
    std::filesystem::remove(OwnExecutablePath().parent_path() / std::filesystem::path(reinterpret_cast<const char8_t*>(("child.bin" + OwnExecutablePath().extension().string()).c_str())), error);
    std::filesystem::remove_all(std::filesystem::current_path() / "app0", error);
}

static void CheckOrderedFileActions(const std::string& order) {
    std::fprintf(stderr, "Checking spawn file actions: %s\n", order.c_str());
    const auto app0 = std::filesystem::current_path() / "app0";
    Check(std::filesystem::create_directories(app0) || std::filesystem::exists(app0));
    PosixSpawnFileActions* actions = nullptr;
    Check(posix_spawn_file_actions_init_nid_postfix(&actions) == 0);
    const int savedOutput = dup_nid_postfix(1);
    Check(savedOutput >= 0);
    const int parentOutput = open_nid_postfix("/app0/spawn-parent.txt", 0x601, 0600);
    Check(parentOutput >= 0);
    Check(dup2_nid_postfix(parentOutput, 1) == 1);
    Check(close_nid_postfix(parentOutput) == 0);
    const bool closed = order == "close-dup" || order == "close-self-dup";
    const bool reverse = order == "dup-open";
    const bool self = order == "open-self-dup" || order == "close-self-dup";
    const bool chain = order == "dup-replace-dup";
    const bool untouched = order == "untouched-self-dup";
    if (reverse || chain)
        Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 1, 2) == 0);
    if (closed) {
        Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 1) == 0);
    } else if (!untouched) {
        Check(posix_spawn_file_actions_addopen_nid_postfix(&actions, 1, "/app0/spawn-child.txt", 0x601, 0600) == 0);
    }
    if (chain) {
        Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 2, 1) == 0);
        Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 2) == 0);
        Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 1, 2) == 0);
    } else if (!reverse) {
        if (self || untouched)
            Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 1, 1) == 0);
        if (!closed || !self)
            Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 1, 2) == 0);
    }
    std::int32_t processId = 0;
    SpawnChildExplicit("child-streams", &processId, false, &actions, closed ? BadDescriptor : 0);
    Check(dup2_nid_postfix(savedOutput, 1) == 1);
    Check(close_nid_postfix(savedOutput) == 0);
    Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0);
    if (closed) {
        Check(processId == 0);
    } else {
        int status = -1;
        Check(waitpid_nid_postfix(processId, &status, 0) == processId);
        Check(status == 0);
        std::ifstream child(app0 / "spawn-child.txt", std::ios::binary);
        const std::string childOutput((std::istreambuf_iterator<char>(child)), std::istreambuf_iterator<char>());
        Check((childOutput.find("spawn-stdout") != std::string::npos) == (!chain && !untouched));
        Check((childOutput.find("spawn-stderr") != std::string::npos) == (!reverse && !chain && !untouched));
        std::ifstream parent(app0 / "spawn-parent.txt", std::ios::binary);
        const std::string originalOutput((std::istreambuf_iterator<char>(parent)), std::istreambuf_iterator<char>());
        Check((originalOutput.find("spawn-stdout") != std::string::npos) == (chain || untouched));
        Check((originalOutput.find("spawn-stderr") != std::string::npos) == (reverse || chain || untouched));
    }
    CleanupChild();
}

static void RemoveDirectoryTree(const std::filesystem::path& path) {
#ifdef _WIN32
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error);
    const auto& target = error ? path : verbatim;
    const DWORD attributes = GetFileAttributesW(target.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        Check(DeleteFileW(target.c_str()) != 0);
        return;
    }
    WIN32_FIND_DATAW entry{};
    const std::wstring pattern = target.native() + L"\\*";
    const HANDLE find = FindFirstFileW(pattern.c_str(), &entry);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (std::wcscmp(entry.cFileName, L".") == 0 || std::wcscmp(entry.cFileName, L"..") == 0) continue;
            RemoveDirectoryTree(path / entry.cFileName);
        } while (FindNextFileW(find, &entry) != 0);
        FindClose(find);
    }
    Check(RemoveDirectoryW(target.c_str()) != 0);
#else
    std::error_code removeError;
    std::filesystem::remove_all(path, removeError);
    Check(!removeError);
#endif
}

static void CheckOpenActionContract() {
    PrepareChildBinary();
    const auto app0 = std::filesystem::current_path() / "app0";
    RemoveDirectoryTree(app0 / "spawn-open");
    Check(mkdir_nid_postfix("/app0/spawn-open", 0755) == 0);
    Check(mkdir_nid_postfix("/app0/spawn-open/dir", 0755) == 0);
    { std::ofstream file(app0 / "spawn-open" / "regular.txt"); file << "regular"; Check(file.good()); }

    char program[] = "/app0/child.bin";
    char* args[] = {program, nullptr};

    auto spawnOpen = [&](const char* path, int flags, int expected, const char* role = "child-exit-42") {
        auto env = BuildRoleEnvironment(role);
        for (auto spawn : std::array{&posix_spawn_nid_postfix, &posix_spawnp_nid_postfix}) {
            PosixSpawnFileActions* actions = nullptr;
            Check(posix_spawn_file_actions_init_nid_postfix(&actions) == 0);
            Check(posix_spawn_file_actions_addopen_nid_postfix(&actions, 1, path, flags, 0600) == 0);
            if (std::strcmp(role, "child-streams") == 0)
                Check(posix_spawn_file_actions_adddup2_nid_postfix(&actions, 1, 2) == 0);
            constexpr std::int32_t Sentinel = -12345;
            std::int32_t pid = Sentinel;
            *__error_nid_postfix() = 71;
            const int result = spawn(&pid, program, &actions, nullptr, args, env.pointers.data());
            if (result != expected)
                std::fprintf(stderr, "Open action path=%s flags=0x%x: expected %d, observed %d\n", path, flags, expected, result);
            Check(result == expected);
            Check(*__error_nid_postfix() == 71);
            Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0);
            if (expected == 0) {
                Check(pid > 0);
                int status = -1;
                Check(waitpid_nid_postfix(pid, &status, 0) == pid);
                Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == (std::strcmp(role, "child-exit-42") == 0 ? 42 : 0));
            } else {
                Check(pid == Sentinel);
            }
        }
    };

    spawnOpen("/app0/spawn-open/dir", 0x601, 21);
    spawnOpen("/app0/spawn-open/dir", 0x001, 21);
    spawnOpen("/app0/spawn-open/dir", 0, 0);
    spawnOpen("/app0/spawn-open/regular.txt", 0x20000, 20);
    spawnOpen("/app0/spawn-open/regular.txt/child.txt", 0, 20);
    spawnOpen("/app0/spawn-open/regular.txt", 0x3, 22);
    spawnOpen("/app0/spawn-open/dir", 0x20A00, 22);

    std::vector<std::string> deepDirs;
    std::string deepGuest = "/app0/spawn-open";
    for (int i = 0; i < 6; ++i) {
        deepGuest += "/" + std::string(45, static_cast<char>('a' + i));
        Check(mkdir_nid_postfix(deepGuest.c_str(), 0755) == 0);
        deepDirs.push_back(deepGuest);
    }
    const std::string deepFile = deepGuest + "/out.txt";
    spawnOpen(deepFile.c_str(), 0x601, 0, "child-streams");
    {
        const int readFd = open_nid_postfix(deepFile.c_str(), 0, 0);
        Check(readFd >= 0);
        std::string output;
        char chunk[256];
        while (true) {
            const auto n = read_nid_postfix(readFd, chunk, sizeof(chunk));
            if (n <= 0) {
                Check(n == 0);
                break;
            }
            output.append(chunk, static_cast<std::size_t>(n));
        }
        Check(close_nid_postfix(readFd) == 0);
        Check(output.find("spawn-stdout") != std::string::npos && output.find("spawn-stderr") != std::string::npos);
    }

    Check(unlink_nid_postfix(deepFile.c_str()) == 0);
    for (auto it = deepDirs.rbegin(); it != deepDirs.rend(); ++it)
        Check(rmdir_nid_postfix(it->c_str()) == 0);
    Check(unlink_nid_postfix("/app0/spawn-open/regular.txt") == 0);
    Check(rmdir_nid_postfix("/app0/spawn-open/dir") == 0);
    Check(rmdir_nid_postfix("/app0/spawn-open") == 0);
    CleanupChild();
}

static void CheckClosedStandardStreams() {
    PrepareChildBinary();
    const auto app0 = std::filesystem::current_path() / "app0";
    { std::ofstream input(app0 / "closed-stdin.txt", std::ios::binary); input << "secret"; Check(input.good()); }
    const int savedInput = dup_nid_postfix(0);
    const int savedOutput = dup_nid_postfix(1);
    const int savedError = dup_nid_postfix(2);
    Check(savedInput >= 0 && savedOutput >= 0 && savedError >= 0);
    const int redirectedInput = open_nid_postfix("/app0/closed-stdin.txt", 0, 0);
    const int redirectedOutput = open_nid_postfix("/app0/closed-parent.txt", 0x601, 0600);
    Check(redirectedInput >= 0 && redirectedOutput >= 0);
    Check(dup2_nid_postfix(redirectedInput, 0) == 0);
    Check(dup2_nid_postfix(redirectedOutput, 1) == 1);
    Check(dup2_nid_postfix(redirectedOutput, 2) == 2);
    Check(close_nid_postfix(redirectedInput) == 0);
    Check(close_nid_postfix(redirectedOutput) == 0);

    PosixSpawnFileActions* actions = nullptr;
    Check(posix_spawn_file_actions_init_nid_postfix(&actions) == 0);
    Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 0) == 0);
    Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 1) == 0);
    Check(posix_spawn_file_actions_addclose_nid_postfix(&actions, 2) == 0);
    char program[] = "/app0/child.bin";
    char* args[] = {program, nullptr};
    auto env = BuildRoleEnvironment("child-closed-std");
    std::int32_t pid = 0;
    const int spawned = posix_spawn_nid_postfix(&pid, program, &actions, nullptr, args, env.pointers.data());
    Check(dup2_nid_postfix(savedInput, 0) == 0);
    Check(dup2_nid_postfix(savedOutput, 1) == 1);
    Check(dup2_nid_postfix(savedError, 2) == 2);
    Check(close_nid_postfix(savedInput) == 0);
    Check(close_nid_postfix(savedOutput) == 0);
    Check(close_nid_postfix(savedError) == 0);
    Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0);
    Check(spawned == 0);
    int status = -1;
    Check(waitpid_nid_postfix(pid, &status, 0) == pid);
    Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 0);
    {
        std::ifstream parent(app0 / "closed-parent.txt", std::ios::binary);
        const std::string leaked((std::istreambuf_iterator<char>(parent)), std::istreambuf_iterator<char>());
        Check(leaked.empty());
    }
    CleanupChild();
}

static void CheckSpawnNullProcessId(bool usePath) {
    Check(SpawnChildExplicit("child-exit-42", nullptr, usePath) == 0);
    int status = -1;
    const int reaped = waitpid_nid_postfix(-1, &status, 0);
    Check(reaped > 0);
    Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 42);
    CleanupChild();
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
        if (std::strcmp(role, "child-streams") == 0) {
            constexpr char output[] = "spawn-stdout";
            constexpr char error[] = "spawn-stderr";
            Check(write_nid_postfix(1, output, sizeof(output) - 1) == sizeof(output) - 1);
            Check(write_nid_postfix(2, error, sizeof(error) - 1) == sizeof(error) - 1);
            std::_Exit(0);
        }
        if (std::strcmp(role, "child-exit-42") == 0) std::_Exit(42);
        if (std::strcmp(role, "child-exit-neg") == 0) std::exit(-1);
#ifndef _WIN32
        if (std::strcmp(role, "child-stop") == 0) {
            std::raise(SIGSTOP);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::_Exit(44);
        }
        if (std::strcmp(role, "child-kill-bus") == 0) std::raise(SIGBUS);
        if (std::strcmp(role, "child-kill-sys") == 0) std::raise(SIGSYS);
        if (std::strcmp(role, "child-kill-usr1") == 0) std::raise(SIGUSR1);
#endif
        if (std::strcmp(role, "child-sleep") == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            std::_Exit(43);
        }
        if (std::strcmp(role, "child-laze") == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            std::_Exit(47);
        }
        if (std::strcmp(role, "child-closed-std") == 0) {
            char byte = 0;
            const bool isolatedIn = read_nid_postfix(0, &byte, 1) == 0;
            const bool isolatedOut = write_nid_postfix(1, "leak", 4) == 4;
            const bool isolatedErr = write_nid_postfix(2, "leak", 4) == 4;
            std::_Exit(isolatedIn && isolatedOut && isolatedErr ? 0 : 12);
        }
        std::_Exit(99);
    }
    CheckAttributes();
    CheckFileActions();
    CheckSpawn();
    CheckOpenActionContract();
    CheckClosedStandardStreams();
    for (const char* order : {"open-dup", "close-dup", "dup-open", "open-self-dup", "close-self-dup", "dup-replace-dup", "untouched-self-dup"})
        CheckOrderedFileActions(order);
    CheckSpawnNullProcessId(false);
    CheckSpawnNullProcessId(true);
    CheckSpawnedChild("child-exit-42", 42);
    CheckSpawnedChild("child-sleep", 43);
    CheckSpawnedChild("child-exit-neg", 255);
    CheckConcurrentWaiters();
#ifndef _WIN32
    CheckStoppedChild();
    CheckKilledChild("child-kill-bus", 10);
    CheckKilledChild("child-kill-sys", 12);
    CheckKilledChild("child-kill-usr1", 30);
#else
    CheckTerminatedChild("child-laze", 0xC0000005u, 11);
    CheckTerminatedChild("child-laze", 0xC0000096u, 4);
    CheckTerminatedChild("child-laze", 0x80000003u, 5);
    CheckTerminatedChild("child-laze", 0xC0000094u, 8);
    CheckTerminatedChild("child-laze", 0xC0000409u, 6);
    CheckTerminatedChild("child-laze", 0xFFFFFFFFu, 0);
#endif
}
