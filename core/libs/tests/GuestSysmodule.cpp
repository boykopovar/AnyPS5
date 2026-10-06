#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleLoadModuleInternalWithArg(std::uint32_t, int, void*, std::uint64_t, int*);
void* APS5_VABI dlopen_nid_postfix(const char*, int);
int APS5_VABI dlclose_nid_postfix(void*);
}

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "sysmodule fixture: %s\n", message);
        std::abort();
    }
}

constexpr int kNotLoadedQuery = static_cast<int>(0x80A90002);
constexpr int kModuleNotLoaded = static_cast<int>(0x80A90003);

void* NativeOpen(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return ::dlopen(path, RTLD_NOW);
#endif
}
bool NativeClose(void* module) {
#ifdef _WIN32
    return FreeLibrary(static_cast<HMODULE>(module)) != 0;
#else
    return ::dlclose(module) == 0;
#endif
}
bool NativePresent(const char* path) {
#ifdef _WIN32
    return GetModuleHandleA(path) != nullptr;
#else
    void* module = ::dlopen(path, RTLD_NOW | RTLD_NOLOAD);
    if (!module) return false;
    return NativeClose(module);
#endif
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "lifecycle";
    Require(argc == 4, "provider path and module ID are required");
    char* end = nullptr;
    const unsigned long parsedId = std::strtoul(argv[3], &end, 0);
    Require(end != argv[3] && *end == '\0' && parsedId <= UINT16_MAX, "invalid module ID");
    const auto moduleId = static_cast<std::uint16_t>(parsedId);
    Require(sceSysmoduleIsLoaded(moduleId) == kNotLoadedQuery, "initial state must be unloaded");
    if (mode == "missing" || mode == "failed-attach" || mode == "invalid-image") {
        sceSysmoduleLoadModule(moduleId);
        std::fprintf(stderr, "unexpected load success\n");
        return 79;
    }
    if (mode == "unknown") {
        sceSysmoduleLoadModule(0x7fff);
        std::fprintf(stderr, "unexpected unknown-ID success\n");
        return 79;
    }
    if (mode == "unknown-query" || mode == "unknown-unload") {
        if (mode == "unknown-query") sceSysmoduleIsLoaded(0x7fff);
        else sceSysmoduleUnloadModule(0x7fff);
        std::fprintf(stderr, "unexpected unknown-ID success\n");
        return 79;
    }
    if (mode == "with-arguments") {
        int result = 0x12345678;
        int argument = 42;
        sceSysmoduleLoadModuleInternalWithArg(moduleId, 1, &argument, 0, &result);
        std::fprintf(stderr, "unexpected argument success: %x\n", result);
        return 79;
    }
    const char* path = argv[2];
    void* borrowed = nullptr;
    void* shared = nullptr;
    if (mode == "preloaded") {
        borrowed = NativeOpen(path);
        Require(borrowed != nullptr, "external provider load failed");
    } else if (mode == "shared") {
        shared = dlopen_nid_postfix(path, 2);
        Require(shared != nullptr, "shared provider load failed");
    } else {
        Require(mode == "lifecycle", "invalid fixture mode");
    }
    Require(sceSysmoduleLoadModuleInternal(moduleId) == 0, "first internal acquire failed");
    Require(NativePresent(path), "success was published without a native image");
    Require(sceSysmoduleIsLoaded(moduleId) == 0, "acquired module must be reported loaded");
    Require(sceSysmoduleLoadModule(moduleId) == 0, "public acquire failed");
    int result = 0x12345678;
    Require(sceSysmoduleLoadModuleInternalWithArg(moduleId, 0, nullptr, 0, &result) == 0,
            "argument-free internal acquire failed");
    Require(result == 0, "successful internal result was not written");
    Require(sceSysmoduleUnloadModule(moduleId) == 0, "first release failed");
    Require(sceSysmoduleUnloadModuleInternal(moduleId) == 0, "second release failed");
    Require(NativePresent(path), "provider detached before final reference");
    if (shared) {
        Require(dlclose_nid_postfix(shared) == 0, "shared reference close failed");
        Require(NativePresent(path), "shared close detached retained provider");
    }
    Require(sceSysmoduleUnloadModuleInternal(moduleId) == 0, "final release failed");
    Require(sceSysmoduleIsLoaded(moduleId) == kNotLoadedQuery, "released module still reported loaded");
    Require(sceSysmoduleUnloadModuleInternal(moduleId) == kModuleNotLoaded, "release underflow must fail");
    if (borrowed) {
        Require(NativePresent(path), "external preloaded reference was lost");
        Require(NativeClose(borrowed), "external release failed");
    }
    Require(!NativePresent(path), "final provider reference leaked");
}
