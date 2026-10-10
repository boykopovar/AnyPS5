#include <Testing/Test.hpp>

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Testing::Case;
using Testing::Require;

std::string Load(const std::filesystem::path& path) {
#ifdef _WIN32
    if (LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) != nullptr) return {};
    return path.filename().string() + ": LoadLibraryEx failed with error " + std::to_string(GetLastError());
#else
    if (dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL) != nullptr) return {};
    const char* error = dlerror();
    return error != nullptr ? std::string(error) : path.filename().string() + ": dlopen failed";
#endif
}

const Case loadAll{"PatchedLibraries_EveryPrxInDirectory_LoadsWithResolvedImports", [] {
    const std::filesystem::path directory = Testing::RequireArgument(0, "patched libraries directory");
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
    int loaded = 0;
    std::string failures;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() != ".prx") continue;
        const auto error = Load(entry.path());
        if (error.empty()) {
            ++loaded;
        } else {
            failures += "\n    " + error;
        }
    }
    Require(failures.empty(), "patched libraries failed to load:" + failures);
    Require(loaded > 0, "no patched library found in " + directory.string());
}};

} // namespace
