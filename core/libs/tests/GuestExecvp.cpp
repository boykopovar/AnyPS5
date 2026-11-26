#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

static void Require(bool value) {
    if (!value) std::abort();
}

static std::filesystem::path VerbatimPath(const std::filesystem::path& path) {
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error);
    Require(!error);
    return verbatim;
}

int main() {
    char name[] = "EXECVP_ABSENT_BINARY";
    char* arguments[] = {name, nullptr};
    char unpairedEntry[] = "EXECVP_SECOND_STAGE";
    char* unpaired[] = {unpairedEntry, nullptr};
    std::string huge(300000, 'X');
    char* oversized[] = {name, &huge[0], nullptr};
    char* const empty[] = {nullptr};
    static std::string firstVector(132000, 'K');
    static std::string secondVector(132000, 'M');
    std::string firstEntry = "EXECVP_LARGE_FIRST=" + firstVector;
    std::string secondEntry = "EXECVP_LARGE_SECOND=" + secondVector;
    char* largeEnvironment[] = {&firstEntry[0], &secondEntry[0], nullptr};
    Require(execvp_nid_postfix(nullptr, nullptr) == -1 && *__error_nid_postfix() == 9);
    Require(execvp_nid_postfix(name, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(execvp_nid_postfix("", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execv_nid_postfix(nullptr, arguments) == -1 && *__error_nid_postfix() == 9);
    Require(execv_nid_postfix(name, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(execv_nid_postfix("", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execv_nid_postfix(name, empty) == -1 && *__error_nid_postfix() == 2);
    Require(execve_nid_postfix(name, arguments, nullptr) == -1 && *__error_nid_postfix() == 2);
    Require(execve_nid_postfix(name, arguments, unpaired) == -1 && *__error_nid_postfix() == 2);
    Require(execvp_nid_postfix("EXECVP_NO_SUCH_LIBRARY", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execv_nid_postfix("nul", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execve_nid_postfix("nul", arguments, nullptr) == -1 && *__error_nid_postfix() == 2);
    Require(execvp_nid_postfix("nul", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execv_nid_postfix("nul.bin", arguments) == -1 && *__error_nid_postfix() == 2);
    Require(execve_nid_postfix("nul.bin", arguments, nullptr) == -1 && *__error_nid_postfix() == 2);
    Require(execvp_nid_postfix("nul.bin", arguments) == -1 && *__error_nid_postfix() == 2);
    std::error_code deepError;
    std::vector<std::filesystem::path> deepDirectories;
    std::filesystem::path deepHost = std::filesystem::current_path() / "guest_execvp_deep";
    std::string deepGuest = "guest_execvp_deep";
    std::filesystem::create_directory(VerbatimPath(deepHost), deepError);
    Require(!deepError);
    deepDirectories.push_back(deepHost);
    for (int index = 0; deepHost.native().size() < 260; ++index) {
        const std::string segment = "seg_" + std::to_string(index) + "_0123456789abcdef0123456789abcdef";
        deepGuest += "/" + segment;
        deepHost /= segment;
        std::filesystem::create_directory(VerbatimPath(deepHost), deepError);
        Require(!deepError);
        deepDirectories.push_back(deepHost);
    }
    const auto deepFileHost = deepHost / "file.bin";
    {
        std::ofstream file(VerbatimPath(deepFileHost), std::ios::binary);
        file << "plain";
        file.close();
        Require(file.good());
    }
    const std::string deepBeyondGuest = deepGuest + "/file.bin/child";
    Require(execv_nid_postfix(deepBeyondGuest.c_str(), arguments) == -1 && *__error_nid_postfix() == 20);
    Require(execve_nid_postfix((deepGuest + "/file.bin/").c_str(), arguments, nullptr) == -1 && *__error_nid_postfix() == 20);
    const auto deepDirCandidate = deepHost / "dir_candidate.bin";
    std::filesystem::create_directory(VerbatimPath(deepDirCandidate), deepError);
    Require(!deepError);
    Require(execv_nid_postfix((deepGuest + "/dir_candidate.bin").c_str(), arguments) == -1 && *__error_nid_postfix() == 13);
    Require(setenv_nid_postfix("PATH", ("/" + deepGuest).c_str(), 1) == 0);
    Require(execvp_nid_postfix("dir_candidate.bin", arguments) == -1 && *__error_nid_postfix() == 13);
    unsetenv_nid_postfix("PATH");
    std::filesystem::remove(VerbatimPath(deepFileHost), deepError);
    std::filesystem::remove(VerbatimPath(deepDirCandidate), deepError);
    for (auto it = deepDirectories.rbegin(); it != deepDirectories.rend(); ++it) {
        std::filesystem::remove(VerbatimPath(*it), deepError);
    }
    Require(execv_nid_postfix(name, oversized) == -1 && *__error_nid_postfix() == 7);
    Require(execve_nid_postfix(name, arguments, largeEnvironment) == -1 && *__error_nid_postfix() == 7);
}
