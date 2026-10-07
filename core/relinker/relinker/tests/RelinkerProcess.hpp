#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace RelinkerTests {

class TempDirectory {
public:
    TempDirectory() {
        auto pattern = (std::filesystem::temp_directory_path() / "anyps5-relinker-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) throw std::runtime_error("Cannot create a temporary directory");
        path = pattern;
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

inline RelinkerRun RunRelinker(const std::filesystem::path& relinker, const std::vector<std::string>& arguments, const std::filesystem::path& logPath) {
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
