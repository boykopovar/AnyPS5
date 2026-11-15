#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {

constexpr int Internal = static_cast<int>(0x809F000B);
constexpr int NotFound = static_cast<int>(0x809F0008);
constexpr int Interrupted = 73;
const std::filesystem::path Backup = "_sd_backup/kept";
const std::filesystem::path Old = "_sd_backup_old/kept";
const std::filesystem::path Staged = "_sd_backup_tmp/kept";
const char* stopStep = nullptr;
int stopCall = 1;
int calls = 0;
bool failStep = false;
int rollbackCall = 0;
int rollbackCalls = 0;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    throw std::runtime_error("backup recovery requirement failed at line " + std::to_string(location.line()));
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    Require(stream.is_open());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void Write(const std::filesystem::path& path, const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    Require(static_cast<bool>(stream));
}

using Snapshot = std::map<std::string, std::string>;

Snapshot Tree(const std::filesystem::path& root) {
    Snapshot result;
    if (!std::filesystem::exists(root)) return result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const std::string name = entry.path().lexically_relative(root).generic_string();
        result.emplace(name, entry.is_directory() ? "D" : "F" + Read(entry.path()));
    }
    return result;
}

SceSaveDataDirName Name() {
    SceSaveDataDirName name{};
    std::strcpy(name.data, "kept");
    return name;
}

bool Step(const char* step, const char*) {
    if (rollbackCall != 0 && std::strcmp(step, "backup.rollback") == 0 && ++rollbackCalls == rollbackCall) return false;
    if (stopStep == nullptr || std::strcmp(stopStep, step) != 0 || ++calls != stopCall) return true;
    if (failStep) return false;
    std::_Exit(Interrupted);
}

void SetStep(const char* step, bool fail = false, int call = 1) {
    stopStep = step;
    stopCall = call;
    calls = 0;
    failStep = fail;
    SaveDataSetFileStepHook_nid_no_patch(Step);
}

void BackUp(const char* version) {
    auto name = Name();
    SaveDataMount3 request{};
    request.user_id = 7;
    request.dir_name = &name;
    request.mount_mode = 2 | 32;
    request.blocks = 96;
    SaveDataMountResult result{};
    Require(sceSaveDataMount3(&request, &result) == 0);
    Write("_sd/kept/nested/data.bin", std::string(version) + std::string("\0\xff\x01", 3));
    Write("_sd/kept/empty", "");
    if (std::strcmp(version, "new") == 0) Write("_sd/kept/added", "added");
    Require(sceSaveDataSetParam(&result.mount_point, 1, version, std::strlen(version) + 1) == 0);
    Require(sceSaveDataUmountWithBackup(&result.mount_point) == 0);
    SaveDataEvent event{};
    Require(sceSaveDataGetEventResult(nullptr, &event) == 0);
    Require(event.type == 1 && event.user_id == 7 && std::strcmp(event.dir_name.data, "kept") == 0);
    const bool failed = failStep && std::strcmp(stopStep, "backup.cleanup") != 0;
    Require(event.error_code == (failed ? Internal : 0));
    if (failStep) Require(calls != 0);
}

void CheckBackup(const char* expected) {
    auto name = Name();
    SaveDataParam param;
    std::memset(&param, 0xA5, sizeof(param));
    const auto untouched = param;
    SaveDataCheckBackupData request{};
    request.dir_name = &name;
    request.param = &param;
    const int result = sceSaveDataCheckBackupData(&request);
    if (std::strcmp(expected, "missing") == 0 || std::strcmp(expected, "invalid") == 0) {
        Require(result == (std::strcmp(expected, "missing") == 0 ? NotFound : Internal));
        Require(std::memcmp(&param, &untouched, sizeof(param)) == 0);
    } else {
        Require(result == 0);
        Require(std::strcmp(param.title, expected) == 0);
        Require(Read(Backup / "save/nested/data.bin") == std::string(expected) + std::string("\0\xff\x01", 3));
    }
}

int Child(int argc, char** argv) {
    AddPathAlias_nid_no_patch("/app0", std::filesystem::current_path().string().c_str());
    Require(sceSaveDataInitialize3(nullptr) == 0);
    const std::string operation = argv[2];
    if (argc > 7) rollbackCall = std::stoi(argv[7]);
    if (argc > 4) SetStep(argv[4], argc > 5 && std::strcmp(argv[5], "fail") == 0, argc > 6 ? std::stoi(argv[6]) : 1);
    if (operation == "backup") {
        BackUp(argv[3]);
    } else if (operation == "check") {
        CheckBackup(argv[3]);
    } else if (operation == "restore") {
        auto name = Name();
        SaveDataRestoreBackupData request{};
        request.dir_name = &name;
        Require(sceSaveDataRestoreBackupData(&request) == 0);
        Require(Read("_sd/kept/nested/data.bin") == std::string(argv[3]) + std::string("\0\xff\x01", 3));
    } else if (operation == "delete") {
        auto name = Name();
        SaveDataDelete request{};
        request.dir_name = &name;
        Require(sceSaveDataDelete(&request) == (std::strcmp(argv[3], "invalid") == 0 ? Internal : 0));
    } else {
        throw std::runtime_error("unknown child operation");
    }
    Require(sceSaveDataTerminate() == 0);
    return 0;
}

int Run(const std::string& executable, std::initializer_list<const char*> arguments) {
    std::vector<const char*> values{"savedata-backup-recovery", "--child"};
    values.insert(values.end(), arguments.begin(), arguments.end());
    values.push_back(nullptr);
#ifdef _WIN32
    const auto result = _spawnv(_P_WAIT, executable.c_str(), values.data());
    Require(result != -1);
    return static_cast<int>(result);
#else
    std::vector<char*> writable;
    for (const auto value : values) writable.push_back(const_cast<char*>(value));
    pid_t child = 0;
    Require(posix_spawn(&child, executable.c_str(), nullptr, nullptr, writable.data(), environ) == 0);
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status));
    return WEXITSTATUS(status);
#endif
}

void Clean() {
    for (const char* root : {"_sd", "_sd_backup", "_sd_backup_old", "_sd_backup_tmp", "_sd_restore_tmp", "_sd_restore_old"}) {
        std::filesystem::remove_all(root);
    }
}

Snapshot Start(const std::string& executable) {
    Clean();
    Require(Run(executable, {"backup", "old"}) == 0);
    return Tree(Backup);
}

void NoCopies() {
    Require(!std::filesystem::exists(Old) && !std::filesystem::exists(Staged));
}

void NoLiveSave() {
    for (const char* path : {"_sd/kept", "_sd/kept.param", "_sd/kept.blocks"}) Require(!std::filesystem::exists(path));
}

void InterruptedReplacement(const std::string& executable) {
    for (const char* step : {"backup.copy", "backup.aside", "backup.place"}) {
        const auto original = Start(executable);
        Require(Run(executable, {"backup", "new", step}) == Interrupted);
        Require(Run(executable, {"check", "old"}) == 0);
        Require(Tree(Backup) == original);
        NoCopies();
        Require(Run(executable, {"check", "old"}) == 0);
    }
}

void InterruptedRecovery(const std::string& executable) {
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    Require(!std::filesystem::exists(Backup) && Tree(Old) == original);
    for (int attempt = 0; attempt < 2; ++attempt) {
        Require(Run(executable, {"check", "old", "backup.recover"}) == Interrupted);
        Require(!std::filesystem::exists(Backup) && Tree(Old) == original);
    }
    Require(Run(executable, {"check", "old", "backup.recover.cleanup"}) == Interrupted);
    Require(Tree(Backup) == original);
    Require(Run(executable, {"check", "old"}) == 0);
    Require(Tree(Backup) == original);
    NoCopies();
}

void RecoveryFailure(const std::string& executable) {
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    const auto staged = Tree(Staged);
    Require(Run(executable, {"check", "invalid", "backup.recover", "fail"}) == 0);
    Require(Tree(Old) == original && Tree(Staged) == staged && !std::filesystem::exists(Backup));
    Require(Run(executable, {"check", "old"}) == 0);
    Require(Tree(Backup) == original);
    NoCopies();
    Write(Staged / "partial", "incomplete");
    const auto partial = Tree(Staged);
    Require(Run(executable, {"check", "invalid", "backup.recover.cleanup", "fail"}) == 0);
    Require(Tree(Backup) == original && Tree(Staged) == partial);
    Require(Run(executable, {"check", "old"}) == 0);
    Require(Tree(Backup) == original);
    NoCopies();
    Require(Run(executable, {"backup", "new", "backup.cleanup", "fail"}) == 0);
    Require(Tree(Old) == original);
    Require(Run(executable, {"check", "new"}) == 0);
    NoCopies();
    for (const char* count : {"1", "2"}) {
        const auto previous = Start(executable);
        Require(Run(executable, {"backup", "new", "backup.flush", "fail", count}) == 0);
        Require(Tree(Backup) == previous);
        NoCopies();
        Require(Run(executable, {"check", "old"}) == 0);
    }
    const auto previous = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    Require(Run(executable, {"check", "invalid", "backup.flush", "fail"}) == 0);
    Require(Tree(Backup) == previous && !std::filesystem::exists(Old));
    Require(Run(executable, {"check", "old"}) == 0);
    NoCopies();
}

void Retry(const std::string& executable) {
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    Require(Run(executable, {"backup", "new", "backup.copy", "fail"}) == 0);
    Require(Tree(Backup) == original);
    NoCopies();
    Require(Run(executable, {"check", "old"}) == 0);
    Require(Run(executable, {"backup", "new"}) == 0);
    Require(Run(executable, {"check", "new"}) == 0);
    NoCopies();
}

void Restore(const std::string& executable) {
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    Require(Run(executable, {"restore", "old"}) == 0);
    Require(Tree(Backup) == original);
    Require(Tree("_sd/kept") == Tree(Backup / "save"));
    Require(Read("_sd/kept.param") == Read(Backup / "save.param"));
    Require(Read("_sd/kept.blocks") == Read(Backup / "save.blocks"));
    NoCopies();
}

void Delete(const std::string& executable) {
    Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    Require(Run(executable, {"delete", "unused"}) == 0);
    NoCopies();
    NoLiveSave();
    Require(Run(executable, {"check", "missing"}) == 0);
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    const auto staged = Tree(Staged);
    Require(Run(executable, {"delete", "invalid", "backup.delete", "fail"}) == 0);
    Require(Tree(Old) == original && Tree(Staged) == staged);
    NoLiveSave();
    Require(Run(executable, {"delete", "unused"}) == 0);
    Require(!std::filesystem::exists(Backup));
    NoCopies();
    NoLiveSave();
    for (const char* count : {"1", "2"}) {
        const auto previous = Start(executable);
        Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
        Require(Run(executable, {"delete", "unused", "backup.delete", "stop", count}) == Interrupted);
        NoLiveSave();
        Require(Run(executable, {"check", "old"}) == 0);
        Require(Tree(Backup) == previous);
        NoCopies();
    }
    Start(executable);
    Require(Run(executable, {"backup", "new", "backup.cleanup"}) == Interrupted);
    const auto published = Tree(Backup);
    Write(Staged / "incomplete", "partial");
    Require(Run(executable, {"delete", "unused", "backup.delete", "stop", "3"}) == Interrupted);
    NoLiveSave();
    Require(Tree(Backup) == published);
    NoCopies();
    Require(Run(executable, {"check", "new"}) == 0);
    Require(Run(executable, {"delete", "unused"}) == 0);
    NoLiveSave();
    Require(Run(executable, {"check", "missing"}) == 0);
    NoCopies();
    Require(Run(executable, {"check", "missing"}) == 0);
    Require(!std::filesystem::exists(Backup));
}

#ifndef _WIN32
void DeleteFailure(const std::string& executable) {
    for (const bool interrupted : {false, true}) {
        const auto original = Start(executable);
        if (interrupted) Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
        const auto protectedDirectory = std::filesystem::path("_sd/kept/protected");
        Write(protectedDirectory / "surviving.bin", "keep this backup recoverable");
        const auto metadata = Read("_sd/kept.param");
        const auto blocks = Read("_sd/kept.blocks");
        std::filesystem::permissions(protectedDirectory, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
        int result = -1;
        try {
            result = Run(executable, {"delete", "invalid"});
        } catch (...) {
            std::filesystem::permissions(protectedDirectory, std::filesystem::perms::owner_all);
            throw;
        }
        std::filesystem::permissions(protectedDirectory, std::filesystem::perms::owner_all);
        Require(result == 0);
        Require(Read(protectedDirectory / "surviving.bin") == "keep this backup recoverable");
        Require(Read("_sd/kept.param") == metadata && Read("_sd/kept.blocks") == blocks);
        Require(Tree(interrupted ? Old : Backup) == original);
        Require(Run(executable, {"check", "old"}) == 0);
        Require(Tree(Backup) == original);
        NoCopies();
        Require(Run(executable, {"delete", "unused"}) == 0);
        NoLiveSave();
        NoCopies();
        Require(!std::filesystem::exists(Backup));
        Require(Run(executable, {"check", "missing"}) == 0);
    }
}
#endif

void Rollback(const std::string& executable) {
    for (const char* count : {"1", "2"}) {
        const auto original = Start(executable);
        Require(Run(executable, {"backup", "new", "backup.flush", "fail", "2", count}) == 0);
        Require(Tree(Old) == original);
        const auto expected = std::strcmp(count, "1") == 0 ? "new" : "old";
        const auto selected = std::strcmp(expected, "new") == 0 ? Tree(Backup) : original;
        Require(Run(executable, {"check", expected}) == 0);
        Require(Tree(Backup) == selected);
        NoCopies();
    }
}

void DeleteSidecarFailure(const std::string& executable) {
    for (const char* sidecar : {"_sd/kept.param", "_sd/kept.blocks"}) {
        const auto original = Start(executable);
        Require(std::filesystem::remove(sidecar));
        Write(std::filesystem::path(sidecar) / "blocking.bin", "failed removal");
        Require(Run(executable, {"delete", "invalid"}) == 0);
        Require(!std::filesystem::exists("_sd/kept"));
        Require(Read(std::filesystem::path(sidecar) / "blocking.bin") == "failed removal");
        Require(Tree(Backup) == original);
        Require(Run(executable, {"check", "old"}) == 0);
        NoCopies();
        std::filesystem::remove_all(sidecar);
        Require(Run(executable, {"delete", "unused"}) == 0);
        NoLiveSave();
        NoCopies();
        Require(!std::filesystem::exists(Backup));
    }
}

void Invalid(const std::string& executable) {
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    const auto staged = Tree(Staged);
    const auto metadata = Read(Old / "save.param");
    Write(Old / "save.param", "invalid");
    const auto invalid = Tree(Old);
    Require(Run(executable, {"check", "invalid"}) == 0);
    Require(Tree(Old) == invalid && Tree(Staged) == staged && !std::filesystem::exists(Backup));
    Write(Old / "save.param", metadata);
    Write(Backup / "save.param", "invalid published copy");
    const auto published = Tree(Backup);
    Require(Run(executable, {"check", "invalid"}) == 0);
    Require(Tree(Old) == original && Tree(Staged) == staged && Tree(Backup) == published);
    std::filesystem::remove_all(Backup);
    Require(Run(executable, {"check", "old"}) == 0);
    Require(Tree(Backup) == original);
    NoCopies();
}

void Published(const std::string& executable) {
    Start(executable);
    Require(Run(executable, {"backup", "new"}) == 0);
    const auto complete = Tree(Backup);
    Write(Old / "remaining", "interrupted cleanup");
    Write(Staged / "incomplete", "uncommitted staging");
    Require(Run(executable, {"check", "new"}) == 0);
    Require(Tree(Backup) == complete);
    NoCopies();
    Start(executable);
    Require(Run(executable, {"backup", "new", "backup.cleanup"}) == Interrupted);
    const auto committed = Tree(Backup);
    Require(std::filesystem::exists(Old));
    Require(Run(executable, {"check", "new"}) == 0);
    Require(Tree(Backup) == committed);
    NoCopies();
    std::filesystem::create_directories(Old / "partially-deleted");
    Write(Old / "remaining", "old cleanup was interrupted");
    Write(Staged / "incomplete", "uncommitted staging");
    Require(Run(executable, {"check", "new", "backup.recover.cleanup"}) == Interrupted);
    Require(Tree(Backup) == committed);
    Require(Run(executable, {"check", "new"}) == 0);
    Require(Tree(Backup) == committed);
    NoCopies();
}

void Unpublished(const std::string& executable) {
    Clean();
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    Require(!std::filesystem::exists(Backup) && !std::filesystem::exists(Old) && std::filesystem::exists(Staged));
    Require(Run(executable, {"check", "missing"}) == 0);
    NoCopies();
    Require(Run(executable, {"backup", "new"}) == 0);
    Require(Run(executable, {"check", "new"}) == 0);
}

bool Links(const std::string& executable) {
    const auto original = Start(executable);
    Require(Run(executable, {"backup", "new", "backup.place"}) == Interrupted);
    const auto victim = std::filesystem::absolute("victim");
    std::filesystem::rename(Old, victim);
    std::error_code error;
    std::filesystem::create_directory_symlink(victim, Old, error);
    if (error) {
        std::filesystem::rename(victim, Old);
        std::fprintf(stderr, "directory symlink unavailable: %s\n", error.message().c_str());
        return false;
    }
    const auto staged = Tree(Staged);
    Require(Run(executable, {"check", "invalid"}) == 0);
    Require(Tree(victim) == original && Tree(Staged) == staged && std::filesystem::is_symlink(Old));
    std::filesystem::remove(Old);
    std::filesystem::rename(victim, Old);
    Require(Run(executable, {"check", "old"}) == 0);
    Require(Tree(Backup) == original);
    NoCopies();
    return true;
}

}

int main(int argc, char** argv) {
    try {
        if (argc >= 4 && std::strcmp(argv[1], "--child") == 0) return Child(argc, argv);
        Require(argc == 2);
        const auto executable = std::filesystem::absolute(argv[0]).string();
        const auto previous = std::filesystem::current_path();
        std::random_device random;
        const auto root = std::filesystem::temp_directory_path() / ("anyps5-backup-recovery-" + std::to_string(random()) + "-" + std::to_string(random()));
        Require(std::filesystem::create_directory(root));
        std::filesystem::current_path(root);
        Write("sce_sys/param.json", "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Recovery fixture\"}}}");
        const std::string operation = argv[1];
        int result = 0;
        if (operation == "replacement") InterruptedReplacement(executable);
        else if (operation == "recovery") InterruptedRecovery(executable);
        else if (operation == "failure") RecoveryFailure(executable);
        else if (operation == "retry") Retry(executable);
        else if (operation == "restore") Restore(executable);
        else if (operation == "delete") Delete(executable);
#ifndef _WIN32
        else if (operation == "delete_failure") {
            if (geteuid() == 0) result = 77;
            else DeleteFailure(executable);
        }
#endif
        else if (operation == "rollback") Rollback(executable);
        else if (operation == "delete_sidecar_failure") DeleteSidecarFailure(executable);
        else if (operation == "invalid") Invalid(executable);
        else if (operation == "published") Published(executable);
        else if (operation == "unpublished") Unpublished(executable);
        else if (operation == "links") result = Links(executable) ? 0 : 77;
        else throw std::runtime_error("unknown backup recovery operation");
        std::filesystem::current_path(previous);
        std::filesystem::remove_all(root);
        return result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
