#include <io/FileWriter.hpp>
#include <domain/Types.hpp>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

template<typename TValue>
void requireWriteFailure(const TValue& data) {
    Io::FileWriter writer;
    writer.Write("/dev/null", data);
    try {
        writer.Write("/dev/full", data);
    } catch (const Domain::RelinkerException& e) {
        if (std::string(e.what()) != "Failed to write file: /dev/full")
            throw std::runtime_error("Unexpected write failure: " + std::string(e.what()));
        return;
    }
    throw std::runtime_error("Write to /dev/full reported success");
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<std::string> siblings(const std::filesystem::path& output) {
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(output.parent_path())) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(output.filename().string() + ".") || name.starts_with(".anyps5-"))
            names.push_back(name);
    }
    return names;
}

template<typename TValue>
void requireRunningReplacement(const std::filesystem::path& directory, const TValue& data, const bool leftover, const std::string& name = "running") {
    const auto output = directory / (leftover ? "leftover" : name);
    if (leftover)
        std::ofstream(directory / "leftover.tmp", std::ios::binary) << "killed";
    std::filesystem::copy_file(std::filesystem::read_symlink("/proc/self/exe"), output);
    const auto permissions = std::filesystem::perms::owner_all | std::filesystem::perms::group_all;
    std::filesystem::permissions(output, permissions);
    const auto previous = readFile(output);
    const pid_t child = fork();
    if (child < 0)
        throw std::runtime_error("Cannot start the previous executable");
    if (child == 0) {
        execl(output.c_str(), output.c_str(), "wait", nullptr);
        _exit(127);
    }
    std::error_code error;
    bool started = false;
    for (int attempt = 0; attempt < 2000 && !started; ++attempt) {
        started = std::filesystem::read_symlink("/proc/" + std::to_string(child) + "/exe", error) == output;
        if (!started)
            usleep(1000);
    }
    const int descriptor = open(output.c_str(), O_RDONLY);
    std::string failure;
    if (started) {
        const auto mask = umask(077);
        try {
            Io::FileWriter().Write(output.string(), data);
        } catch (const std::exception& e) {
            failure = e.what();
        }
        umask(mask);
    }
    std::string kept(previous.size(), '\0');
    const bool keptPrevious = pread(descriptor, kept.data(), kept.size(), 0) == static_cast<ssize_t>(kept.size()) && kept == previous;
    close(descriptor);
    const bool running = waitpid(child, nullptr, WNOHANG) == 0;
    if (running) {
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
    }
    if (!started)
        throw std::runtime_error("The previous executable did not start");
    if (!failure.empty())
        throw std::runtime_error("Replacing a running executable failed: " + failure);
    if (readFile(output) != std::string(data.begin(), data.end()))
        throw std::runtime_error("Running executable was not replaced");
    if ((std::filesystem::status(output).permissions() & std::filesystem::perms::all) != permissions)
        throw std::runtime_error("Replacement did not keep the previous permissions");
    if (!keptPrevious)
        throw std::runtime_error("An open descriptor no longer reads the previous file");
    if (!running)
        throw std::runtime_error("Running executable stopped");
    if (siblings(output) != (leftover ? std::vector<std::string>{"leftover.tmp"} : std::vector<std::string>{}))
        throw std::runtime_error("Temporary file left behind");
    if (leftover && readFile(directory / "leftover.tmp") != "killed")
        throw std::runtime_error("A file left by an earlier run was changed");
}

template<typename TValue>
void requireFailedReplacement(const std::filesystem::path& directory, const TValue& data, const std::string& name = "limited") {
    const auto output = directory / name;
    std::ofstream(output, std::ios::binary) << "previous";
    rlimit limit{};
    getrlimit(RLIMIT_FSIZE, &limit);
    rlimit lowered = limit;
    lowered.rlim_cur = 1;
    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
    setrlimit(RLIMIT_FSIZE, &lowered);
    std::string failure;
    try {
        Io::FileWriter().Write(output.string(), data);
    } catch (const Domain::RelinkerException& e) {
        failure = e.what();
    }
    setrlimit(RLIMIT_FSIZE, &limit);
    std::signal(SIGXFSZ, handler);
    if (failure != "Failed to write file: " + output.string())
        throw std::runtime_error("Write past the file size limit did not fail: " + failure);
    if (readFile(output) != "previous" || !siblings(output).empty())
        throw std::runtime_error("Failed write did not keep the previous file");
}

template<typename TValue>
void requireInPlaceWrites(const std::filesystem::path& directory, const TValue& data) {
    const std::string expected(data.begin(), data.end());
    const auto target = directory / "target";
    const auto link = directory / "link";
    std::ofstream(target, std::ios::binary) << "previous";
    std::filesystem::create_symlink(target, link);
    Io::FileWriter().Write(link.string(), data);
    if (!std::filesystem::is_symlink(link) || readFile(target) != expected)
        throw std::runtime_error("Symbolic link output was not written through");
    const auto reserved = directory / "reserved";
    std::ofstream(reserved, std::ios::binary) << "previous";
    std::ofstream(directory / "reserved.tmp", std::ios::binary) << "unrelated";
    Io::FileWriter().Write(reserved.string(), data);
    if (readFile(reserved) != expected || readFile(directory / "reserved.tmp") != "unrelated")
        throw std::runtime_error("Existing temporary file name was not left alone");
}

template<typename TValue>
void requireReplacement(const TValue& data) {
    const auto directory = std::filesystem::canonical(std::filesystem::temp_directory_path()) / ("anyps5-file-writer-" + std::to_string(getpid()));
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directory(directory);
    try {
        requireRunningReplacement(directory, data, false);
        requireRunningReplacement(directory, data, true);
        requireFailedReplacement(directory, data);
        const auto maximumName = pathconf(directory.c_str(), _PC_NAME_MAX);
        if (maximumName <= 0)
            throw std::runtime_error("Cannot determine the output filename limit");
        requireFailedReplacement(directory, data, std::string(static_cast<std::size_t>(maximumName), 'f'));
        requireRunningReplacement(directory, data, false, std::string(static_cast<std::size_t>(maximumName), 'r'));
        requireInPlaceWrites(directory, data);
    } catch (...) {
        std::filesystem::remove_all(directory, error);
        throw;
    }
    std::filesystem::remove_all(directory, error);
}

}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::runtime_error("Expected binary or text");
        const std::string mode = argv[1];
        if (mode == "wait") {
            sleep(30);
            return 0;
        }
        for (const std::size_t size : {std::size_t{3}, std::size_t{65536}}) {
            if (mode == "binary") {
                const std::vector<std::uint8_t> data(size, 0x41);
                requireWriteFailure(data);
                requireReplacement(data);
            } else if (mode == "text") {
                const std::string data(size, 'A');
                requireWriteFailure(data);
                requireReplacement(data);
            } else {
                throw std::runtime_error("Unknown test mode: " + mode);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
