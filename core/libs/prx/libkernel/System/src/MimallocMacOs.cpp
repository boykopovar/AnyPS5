#ifdef __APPLE__

#include <crt_externs.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path Allocator() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&Allocator), &info) == 0 || info.dli_fname == nullptr) return {};
    return std::filesystem::path(info.dli_fname).parent_path() / "libmimalloc.dylib";
}

std::string Executable() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    if (_NSGetExecutablePath(executable.data(), &size) != 0) return {};
    return executable.data();
}

[[gnu::constructor]] void RestartWithMimalloc() {
    const bool restarted = std::getenv("APS5_MIMALLOC_RESTARTED") != nullptr;
    unsetenv("APS5_MIMALLOC_RESTARTED");
    if (std::getenv("APS5_MIMALLOC") == nullptr || dlsym(RTLD_DEFAULT, "mi_version") != nullptr) return;
    if (restarted) {
        std::fprintf(stderr, "[memory] APS5_MIMALLOC: dyld did not load libmimalloc.dylib, the title keeps the system allocator\n");
        return;
    }
    const auto allocator = Allocator();
    std::error_code error;
    if (allocator.empty() || !std::filesystem::is_regular_file(allocator, error)) return;
    const auto executable = Executable();
    if (executable.empty()) return;
    const char* inserted = std::getenv("DYLD_INSERT_LIBRARIES");
    const std::string previous = inserted == nullptr ? std::string() : inserted;
    const auto libraries = previous.empty() ? allocator.string() : allocator.string() + ":" + previous;
    setenv("DYLD_INSERT_LIBRARIES", libraries.c_str(), 1);
    setenv("APS5_MIMALLOC_RESTARTED", "1", 1);
    execv(executable.c_str(), *_NSGetArgv());
    std::perror("[memory] APS5_MIMALLOC: execv");
    unsetenv("APS5_MIMALLOC_RESTARTED");
    if (inserted == nullptr) unsetenv("DYLD_INSERT_LIBRARIES");
    else setenv("DYLD_INSERT_LIBRARIES", previous.c_str(), 1);
}

}

#endif
