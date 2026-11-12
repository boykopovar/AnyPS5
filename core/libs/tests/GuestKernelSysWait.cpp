#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <nid/NidCompute.hpp>
#include <array>
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
#include <unistd.h>
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
    if (argc == 2 && std::strcmp(argv[1], "child") == 0) return 42;
    Require(argc == 4);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    const std::string test = argv[3];
    auto* sys = LoadLibraryFile(directory / "libkernel_sys.prx");
    auto* kernel = LoadLibraryFile(directory / "libkernel.prx");
    auto* libc = LoadLibraryFile(directory / "libc.prx");
    const auto error = LoadFunction<int* (APS5_VABI*)()>(libc, "__error", patched);
    const auto wait3 = LoadFunction<int (APS5_VABI*)(int*, int, void*)>(sys, "wait3", patched);
    const auto wait4 = LoadFunction<int (APS5_VABI*)(int, int*, int, void*)>(sys, "wait4", patched);
    int status = 0x12345678;
    std::array<unsigned char, 144> usage;
    usage.fill(0xa5);
    for (const int pid : {-123, -1, 0, 1, 123, std::numeric_limits<int>::min()}) {
        for (const unsigned options : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 63u, 0x80000000u, 0x8000003fu, 64u, 0xffffffffu}) {
            *error() = 71;
            const int result = test == "wait3" ? wait3(&status, static_cast<int>(options), usage.data()) : wait4(pid, &status, static_cast<int>(options), usage.data());
            int expected = (options & ~0x8000003fu) ? 22 : 10;
#ifdef __linux__
            if ((options & ~0x8000003fu) == 0 && test == "wait4" && pid == std::numeric_limits<int>::min()) expected = 3;
#endif
            if (result != -1 || *error() != expected)
                std::fprintf(stderr, "pid=%d options=%u result=%d errno=%d expected=%d\n", pid, options, result, *error(), expected);
            Require(result == -1 && *error() == expected);
            Require(status == 0x12345678);
            for (auto byte : usage) Require(byte == 0xa5);
        }
    }
    const auto call = [&](int* out, void* ru) { return test == "wait3" ? wait3(out, 0, ru) : wait4(-1, out, 0, ru); };
    Require(call(nullptr, nullptr) == -1 && *error() == 10);
    Require(call(reinterpret_cast<int*>(1), reinterpret_cast<void*>(1)) == -1 && *error() == 10);
#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    Require(length != 0 && length < executable.size());
    executable.resize(length);
    std::wstring command = L"\"" + executable + L"\" child";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    Require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &child) != FALSE);
    Require(CloseHandle(child.hThread) != FALSE);
    const auto registerChild = reinterpret_cast<void (*)(std::int32_t, std::int64_t)>(GetProcAddress(static_cast<HMODULE>(libc), "RegisterChildProcess_nid_no_patch"));
    Require(registerChild != nullptr);
    registerChild(static_cast<std::int32_t>(child.dwProcessId), reinterpret_cast<std::int64_t>(child.hProcess));
    const int reaped = test == "wait3" ? wait3(&status, 0, nullptr) : wait4(static_cast<int>(child.dwProcessId), &status, 0, nullptr);
    Require(reaped == static_cast<int>(child.dwProcessId) && status == (42 << 8));
#else
    int barrier[2]{};
    Require(::pipe(barrier) == 0);
    const int child = ::fork();
    Require(child >= 0);
    if (child == 0) {
        ::close(barrier[1]);
        char byte;
        if (::read(barrier[0], &byte, 1) != 1) ::_exit(99);
        ::_exit(42);
    }
    Require(::close(barrier[0]) == 0);
    const auto waitChild = [&](int* out, int options, void* resources) {
        return test == "wait3" ? wait3(out, options, resources) : wait4(child, out, options, resources);
    };
    Require(waitChild(&status, 1, usage.data()) == 0);
    Require(status == 0x12345678);
    for (auto byte : usage) Require(byte == 0xa5);
    Require(::write(barrier[1], "x", 1) == 1 && ::close(barrier[1]) == 0);
    Require(waitChild(&status, 0, usage.data()) == child && status == (42 << 8));
    GuestResourceUsage resources{};
    static_assert(sizeof(resources) == usage.size());
    std::memcpy(&resources, usage.data(), sizeof(resources));
    Require(resources.ru_utime.tv_sec >= 0 && resources.ru_stime.tv_sec >= 0);
    Require(resources.ru_utime.tv_usec >= 0 && resources.ru_utime.tv_usec < 1000000);
    Require(resources.ru_stime.tv_usec >= 0 && resources.ru_stime.tv_usec < 1000000);
    Require(resources.ru_maxrss >= 0 && resources.ru_minflt >= 0 && resources.ru_nvcsw >= 0);
    const int second = ::fork();
    Require(second >= 0);
    if (second == 0) ::_exit(7);
    const int failed = test == "wait3" ? wait3(&status, 0, reinterpret_cast<void*>(1)) : wait4(second, &status, 0, reinterpret_cast<void*>(1));
    Require(failed == -1 && *error() == 14 && status == (7 << 8));
    const int third = ::fork();
    Require(third >= 0);
    if (third == 0) ::_exit(9);
    const int badStatus = test == "wait3" ? wait3(reinterpret_cast<int*>(1), 0, nullptr) : wait4(third, reinterpret_cast<int*>(1), 0, nullptr);
    Require(badStatus == -1 && *error() == 14);
#endif
    return 0;
}
