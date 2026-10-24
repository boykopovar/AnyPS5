#include <Cli.hpp>
#include <domain/Types.hpp>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::filesystem::path ExecutablePath(const char* argument) {
#ifdef _WIN32
    static_cast<void>(argument);
    std::vector<wchar_t> filename(32768);
    const auto size = GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    if (size == 0 || size >= filename.size()) throw std::runtime_error("Cannot locate the autorun test executable");
    return std::filesystem::path(filename.data());
#else
    return std::filesystem::absolute(argument);
#endif
}

}

int main(int, char* argv[]) {
    const auto executable = ExecutablePath(argv[0]);
    const auto name = executable.stem().string();
    if (name.starts_with("anyps5-autorun-child-")) {
        if (name.find("signal") != std::string::npos) std::raise(SIGILL);
        return 42;
    }
    auto child = std::filesystem::temp_directory_path() / ("anyps5-autorun-child-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + " with spaces" + executable.extension().string());
#ifndef _WIN32
    const auto signaled = child.parent_path() / ("anyps5-autorun-child-signal-" + child.filename().string());
#endif
    std::istringstream input("\n\n\n");
    auto* savedInput = std::cin.rdbuf(input.rdbuf());
    int result = 0;
    try {
        std::filesystem::copy_file(executable, child);
        std::vector<std::string> suffixes{""};
#ifndef _WIN32
        suffixes.insert(suffixes.end(), {" $HOME", " $(printf substituted)", " `printf substituted`",
            " 'single'", " \"double\"", " \\backslash", " line\nbreak"});
#endif
        const auto originalChild = child;
        for (const auto& suffix : suffixes) {
            const auto renamed = std::filesystem::path(originalChild.string() + suffix);
            if (renamed != child) std::filesystem::rename(child, renamed);
            child = renamed;
            for (const bool windows : {false, true}) {
                const int code = Cli::Autorun(child.string(), windows);
                if (code != 42) throw std::runtime_error("Autorun changed exit code 42 to " +
                    std::to_string(code) + " for " + child.string());
            }
        }
#ifndef _WIN32
        const auto savedHandler = std::signal(SIGCHLD, SIG_IGN);
        if (savedHandler == SIG_ERR) throw std::runtime_error("Cannot ignore SIGCHLD for the autorun failure test");
        try {
            for (const bool windows : {false, true}) {
                bool rejected = false;
                try {
                    Cli::Autorun(child.string(), windows);
                } catch (const Domain::RelinkerException& error) {
                    const std::string message = error.what();
                    if (message.find(child.string()) == std::string::npos ||
                        message.find("(error " + std::to_string(ECHILD) + ")") == std::string::npos)
                        throw std::runtime_error("Autorun lost the failed wait's path or ECHILD error: " + message);
                    rejected = true;
                }
                if (!rejected) throw std::runtime_error("Autorun reported success after losing the child exit status");
            }
        } catch (...) {
            std::signal(SIGCHLD, savedHandler);
            throw;
        }
        if (std::signal(SIGCHLD, savedHandler) == SIG_ERR)
            throw std::runtime_error("Cannot restore SIGCHLD after the autorun failure test");
        std::filesystem::rename(child, signaled);
        const int code = Cli::Autorun(signaled.string(), false);
        if (code != 128 + SIGILL) throw std::runtime_error("Autorun lost the child signal");
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::cin.rdbuf(savedInput);
    std::filesystem::remove(child);
#ifndef _WIN32
    std::filesystem::remove(signaled);
#endif
    return result;
}
