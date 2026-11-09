#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <random>
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace RelinkerTests {

class TempDirectory {
public:
    TempDirectory() {
#ifdef _WIN32
        std::random_device random;
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = std::filesystem::temp_directory_path() / ("anyps5-relinker-" + std::to_string(random()));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                path = candidate;
                return;
            }
        }
        throw std::runtime_error("Cannot create a temporary directory");
#else
        auto pattern = (std::filesystem::temp_directory_path() / "anyps5-relinker-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) throw std::runtime_error("Cannot create a temporary directory");
        path = pattern;
#endif
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    const std::filesystem::path& Path() const {
        return path;
    }

private:
    std::filesystem::path path;
};

struct RelinkerRun {
    int ExitCode = -1;
    std::string Output;
};

#ifdef _WIN32
inline std::wstring QuoteArgument(const std::wstring& argument) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        quoted.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(character);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}
#endif

inline RelinkerRun RunRelinker(const std::filesystem::path& relinker, const std::vector<std::string>& arguments, const std::filesystem::path& logPath) {
#ifdef _WIN32
    std::wstring commandLine = QuoteArgument(relinker.wstring());
    for (const auto& argument : arguments) commandLine += L" " + QuoteArgument(std::filesystem::path(argument).wstring());
    SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
    const HANDLE logFile = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create the relinker log");
    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributeStorage(attributeBytes);
    const auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    HANDLE inherited[] = {logFile};
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeBytes)) {
        CloseHandle(logFile);
        throw std::runtime_error("Cannot prepare the relinker's inherited handles");
    }
    const bool listed = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr) != 0;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = logFile;
    startup.StartupInfo.hStdError = logFile;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    const bool spawned = listed && CreateProcessW(relinker.c_str(), commandLine.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                                                  nullptr, nullptr, &startup.StartupInfo, &process) != 0;
    DeleteProcThreadAttributeList(attributes);
    CloseHandle(logFile);
    if (!spawned) throw std::runtime_error("Cannot start the relinker");
    DWORD exitCode = 0;
    const bool waited = WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exitCode) != 0;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!waited) throw std::runtime_error("Cannot wait for the relinker");
    RelinkerRun run;
    run.ExitCode = static_cast<int>(exitCode);
#else
    std::vector<std::string> owned{relinker.string()};
    owned.insert(owned.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& argument : owned) argv.push_back(argument.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t pid = 0;
    const auto spawned = posix_spawn(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawned != 0) throw std::runtime_error("Cannot start the relinker");
    int status = 0;
    if (waitpid(pid, &status, 0) != pid) throw std::runtime_error("Cannot wait for the relinker");
    RelinkerRun run;
    if (WIFEXITED(status)) run.ExitCode = WEXITSTATUS(status);
#endif
    std::ifstream log(logPath, std::ios::binary);
    run.Output.assign(std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>());
    return run;
}

inline std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

inline void WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("Cannot write " + path.string());
}

} // namespace RelinkerTests
