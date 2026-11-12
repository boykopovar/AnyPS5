#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Kernel process export check failed at line %u\n", location.line());
        std::abort();
    }
}

static void* LoadLibraryFile(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

static void* FindSymbol(void* module, const char* name) {
#ifdef _WIN32
    auto* symbol = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    auto* symbol = dlsym(module, name);
#endif
    if (!symbol) std::fprintf(stderr, "libkernel.prx does not export %s\n", name);
    return symbol;
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    auto* kernel = LoadLibraryFile(directory / "libkernel.prx");
    struct Export {
        const char* name;
        const char* nid;
    };
    const Export exports[] = {
        {"cpuset_nid_postfix", "itioQwmJGys"},
        {"cpuset_getid_nid_postfix", "DGY7qWLcHGA"},
        {"cpuset_setid_nid_postfix", "BiHHifIvX88"},
        {"rfork_thread_nid_postfix", "bSDxEpGzmUE"},
        {"sceKernelRdup", "LxIacrWiJ3w"},
        {"physhm_open_nid_postfix", "n371J5cP+uo"},
        {"physhm_unlink_nid_postfix", "AUqJNkobQ1c"},
    };
    bool found = true;
    for (const auto& entry : exports) found = FindSymbol(kernel, patched ? entry.nid : entry.name) != nullptr && found;
    Require(found);
    using GetId = int (APS5_VABI*)(int, int, std::int64_t, int*);
    const auto getId = reinterpret_cast<GetId>(FindSymbol(kernel, patched ? "DGY7qWLcHGA" : "cpuset_getid_nid_postfix"));
    constexpr int LevelRoot = 1;
    constexpr int LevelCpuset = 2;
    constexpr int WhichProcess = 2;
    int setid = -1;
    Require(getId(LevelRoot, WhichProcess, -1, &setid) == 0 && setid == 0);
    Require(getId(LevelCpuset, WhichProcess, -1, &setid) == 0 && setid == 1);
    using Rfork = int (APS5_VABI*)(int, void*, int (APS5_VABI*)(void*), void*);
    const auto rfork = reinterpret_cast<Rfork>(FindSymbol(kernel, patched ? "bSDxEpGzmUE" : "rfork_thread_nid_postfix"));
    Require(rfork != nullptr);
    bool rforkThrew = false;
    try {
        rfork(0, nullptr, nullptr, nullptr);
    } catch (const std::runtime_error&) {
        rforkThrew = true;
    }
    Require(rforkThrew);
    using Physhm = int (APS5_VABI*)(void);
    const auto physhmOpen = reinterpret_cast<Physhm>(FindSymbol(kernel, patched ? "n371J5cP+uo" : "physhm_open_nid_postfix"));
    Require(physhmOpen != nullptr);
    bool physhmOpenThrew = false;
    try {
        physhmOpen();
    } catch (const std::runtime_error&) {
        physhmOpenThrew = true;
    }
    Require(physhmOpenThrew);
    const auto physhmUnlink = reinterpret_cast<Physhm>(FindSymbol(kernel, patched ? "AUqJNkobQ1c" : "physhm_unlink_nid_postfix"));
    Require(physhmUnlink != nullptr);
    bool physhmUnlinkThrew = false;
    try {
        physhmUnlink();
    } catch (const std::runtime_error&) {
        physhmUnlinkThrew = true;
    }
    Require(physhmUnlinkThrew);
    return 0;
}
