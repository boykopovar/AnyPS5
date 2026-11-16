#if defined(__APPLE__) || defined(__linux__)

#include "prx/libc/include/Shutdown.hpp"
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <cstdint>
#else
#include <fstream>
#endif

namespace {

#ifdef __APPLE__
constexpr const char* Loader = "dyld";
constexpr const char* AllocatorName = "libmimalloc.dylib";
constexpr const char* PreloadVariable = "DYLD_INSERT_LIBRARIES";
constexpr const char* PreloadSeparators = ":";
#else
constexpr const char* Loader = "ld.so";
constexpr const char* AllocatorName = "libmimalloc.so";
constexpr const char* PreloadVariable = "LD_PRELOAD";
constexpr const char* PreloadSeparators = ": ";
#endif

std::filesystem::path Allocator() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&Allocator), &info) == 0 || info.dli_fname == nullptr) return {};
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(info.dli_fname).parent_path() / AllocatorName, error);
}

#ifdef __APPLE__
std::string Executable() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    if (_NSGetExecutablePath(executable.data(), &size) != 0) return {};
    return executable.data();
}

std::vector<std::string> Arguments() {
    std::vector<std::string> arguments;
    for (char** argument = *_NSGetArgv(); *argument != nullptr; ++argument) arguments.emplace_back(*argument);
    return arguments;
}
#else
std::string Executable() {
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error).string();
}

std::vector<std::string> Arguments() {
    std::ifstream file("/proc/self/cmdline", std::ios::binary);
    std::vector<std::string> arguments;
    for (std::string argument; std::getline(file, argument, '\0');) arguments.push_back(argument);
    return arguments;
}
#endif

[[gnu::constructor(101)]] void RestartWithMimalloc() {
    const bool restarted = std::getenv("APS5_MIMALLOC_RESTARTED") != nullptr;
    unsetenv("APS5_MIMALLOC_RESTARTED");
    if (std::getenv("APS5_MIMALLOC") == nullptr || dlsym(RTLD_DEFAULT, "mi_version") != nullptr) return;
    if (restarted) {
        std::fprintf(stderr, "[memory] APS5_MIMALLOC: %s did not load %s, the title keeps the system allocator\n", Loader, AllocatorName);
        return;
    }
    const auto allocator = Allocator();
    std::error_code error;
    if (allocator.empty() || !std::filesystem::is_regular_file(allocator, error)) return;
    if (allocator.string().find_first_of(PreloadSeparators) != std::string::npos) {
        std::fprintf(stderr, "[memory] APS5_MIMALLOC: the path %s has a separator of %s, the title keeps the system allocator\n", allocator.c_str(), PreloadVariable);
        return;
    }
    const auto executable = Executable();
    auto arguments = Arguments();
    if (executable.empty() || arguments.empty()) return;
    std::vector<char*> values;
    for (auto& argument : arguments) values.push_back(argument.data());
    values.push_back(nullptr);
    const char* inserted = std::getenv(PreloadVariable);
    const std::string previous = inserted == nullptr ? std::string() : inserted;
    const auto libraries = previous.empty() ? allocator.string() : allocator.string() + ":" + previous;
    setenv(PreloadVariable, libraries.c_str(), 1);
    setenv("APS5_MIMALLOC_RESTARTED", "1", 1);
    LogFileRelease_nid_no_patch();
    execv(executable.c_str(), values.data());
    std::perror("[memory] APS5_MIMALLOC: execv");
    unsetenv("APS5_MIMALLOC_RESTARTED");
    if (inserted == nullptr) unsetenv(PreloadVariable);
    else setenv(PreloadVariable, previous.c_str(), 1);
}

}

#endif
