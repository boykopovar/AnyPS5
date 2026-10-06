#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
extern "C" int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t);
#ifdef _WIN32
#include <windows.h>

namespace {
std::uint16_t ModuleId() {
    wchar_t value[16];
    const DWORD length = GetEnvironmentVariableW(L"ANYPS5_SYSMODULE_TEST_ID", value, 16);
    return length && length < 16 ? static_cast<std::uint16_t>(std::wcstoul(value, nullptr, 0)) : 6;
}
void Trace(char value) {
    wchar_t path[32768];
    const DWORD length = GetEnvironmentVariableW(L"ANYPS5_SYSMODULE_TRACE", path, 32768);
    if (!length || length >= 32768) return;
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, &value, 1, &written, nullptr);
    CloseHandle(file);
}
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Trace(sceSysmoduleIsLoaded(ModuleId()) == static_cast<int>(0x80A90002) ? 'A' : 'a');
        wchar_t failure[2];
        return GetEnvironmentVariableW(L"ANYPS5_SYSMODULE_FAIL_ATTACH", failure, 2) == 0;
    }
    if (reason == DLL_PROCESS_DETACH) {
        Trace(sceSysmoduleIsLoaded(ModuleId()) == static_cast<int>(0x80A90002) ? 'D' : 'd');
    }
    return TRUE;
}
#else
namespace {
std::uint16_t ModuleId() {
    const char* value = std::getenv("ANYPS5_SYSMODULE_TEST_ID");
    return value ? static_cast<std::uint16_t>(std::strtoul(value, nullptr, 0)) : 6;
}
void Trace(char value) {
    const char* path = std::getenv("ANYPS5_SYSMODULE_TRACE");
    if (!path) return;
    if (FILE* file = std::fopen(path, "ab")) {
        std::fwrite(&value, 1, 1, file);
        std::fclose(file);
    }
}
struct ProviderLifecycle {
    ProviderLifecycle() {
        Trace(sceSysmoduleIsLoaded(ModuleId()) == static_cast<int>(0x80A90002) ? 'A' : 'a');
    }
    ~ProviderLifecycle() {
        Trace(sceSysmoduleIsLoaded(ModuleId()) == static_cast<int>(0x80A90002) ? 'D' : 'd');
    }
};
ProviderLifecycle gLifecycle;
}
#endif
