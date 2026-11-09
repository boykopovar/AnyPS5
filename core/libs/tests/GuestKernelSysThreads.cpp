#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <nid/NidCompute.hpp>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <source_location>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Kernel sys API check failed at line %u\n", location.line());
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
#ifdef _WIN32
    std::wstring loaded(32768, L'\0');
    const auto length = GetModuleFileNameW(static_cast<HMODULE>(module), loaded.data(), static_cast<DWORD>(loaded.size()));
    Require(length != 0 && length < loaded.size());
    loaded.resize(length);
    Require(std::filesystem::equivalent(path, std::filesystem::path(loaded)));
#endif
    return module;
}

template <typename Function>
static Function LoadFunction(void* module, const char* name, bool patched, const char* unpatched = nullptr) {
    const auto symbol = patched ? Nid::ComputeNid(name, "") : unpatched ? std::string(unpatched) : std::string(name) + "_nid_postfix";
#ifdef _WIN32
    const auto function = reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), symbol.c_str())));
#else
    const auto function = reinterpret_cast<Function>(dlsym(module, symbol.c_str()));
#endif
    if (!function) std::fprintf(stderr, "Missing symbol %s\n", symbol.c_str());
    Require(function != nullptr);
    return function;
}


int main(int argc, char** argv) {
    Require(argc == 4);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    const std::string test = argv[3];
#ifdef __linux__
    if (test == "main_late_load") {
        int (APS5_VABI* isMain)() = nullptr;
        int (APS5_VABI* sceIsMain)() = nullptr;
        int other = -1;
        int sceOther = -1;
        std::thread loader([&] {
            auto* module = LoadLibraryFile(directory / "libkernel_sys.prx");
            isMain = LoadFunction<int (APS5_VABI*)()>(module, "pthread_main_np", patched);
            sceIsMain = LoadFunction<int (APS5_VABI*)()>(module, "scePthreadMain", patched, "scePthreadMain");
            other = isMain();
            sceOther = sceIsMain();
        });
        loader.join();
        Require(other == 0 && isMain() == 1 && sceOther == 0 && sceIsMain() == 1);
        return 0;
    }
#endif
    Require(test == "main");
    auto* sys = LoadLibraryFile(directory / "libkernel_sys.prx");
    LoadLibraryFile(directory / "libkernel.prx");
    auto* libc = LoadLibraryFile(directory / "libc.prx");
    const auto error = LoadFunction<int* (APS5_VABI*)()>(libc, "__error", patched);
    const auto isMain = LoadFunction<int (APS5_VABI*)()>(sys, "pthread_main_np", patched);
    const auto sceIsMain = LoadFunction<int (APS5_VABI*)()>(sys, "scePthreadMain", patched, "scePthreadMain");
    *error() = 71;
    int other = -1;
    int sceOther = -1;
    std::thread worker([&] {
        other = isMain();
        sceOther = sceIsMain();
    });
    worker.join();
    Require(other == 0 && isMain() == 1 && sceOther == 0 && sceIsMain() == 1 && *error() == 71);
    return 0;
}
