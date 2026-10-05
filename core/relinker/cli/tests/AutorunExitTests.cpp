#include <Cli.hpp>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

int main(int, char* argv[]) {
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto name = executable.stem().string();
    if (name.starts_with("anyps5-autorun-child-")) {
        if (name.find("signal") != std::string::npos) std::raise(SIGILL);
        return 42;
    }
    const auto child = std::filesystem::temp_directory_path() / ("anyps5-autorun-child-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + " with spaces" + executable.extension().string());
#ifndef _WIN32
    const auto signaled = child.parent_path() / ("anyps5-autorun-child-signal-" + child.filename().string());
#endif
    std::istringstream input("\n\n\n");
    auto* savedInput = std::cin.rdbuf(input.rdbuf());
    int result = 0;
    try {
        std::filesystem::copy_file(executable, child);
        for (const bool windows : {false, true}) {
            const int code = Cli::Autorun(child.string(), windows);
            if (code != 42) throw std::runtime_error("Autorun changed exit code 42 to " + std::to_string(code));
        }
#ifndef _WIN32
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
