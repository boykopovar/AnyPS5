#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t);
void APS5_VABI free_nid_postfix(void*);
void* APS5_VABI realloc_nid_postfix(void*, std::size_t);
}

namespace {

constexpr std::size_t HeaderBytes = 2 * sizeof(void*);
constexpr std::size_t MaximumSmallBlockBytes = 64u * 1024u;

void registerDefaultHeap() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

bool checkThreshold(std::size_t bytes, unsigned char value) {
    auto* pointer = static_cast<unsigned char*>(malloc_nid_postfix(bytes));
    if (pointer == nullptr || reinterpret_cast<std::uintptr_t>(pointer) % 16 != 0) return false;
    pointer[0] = value;
    pointer[bytes - 1] = static_cast<unsigned char>(value + 1);
    const bool preserved = pointer[0] == value && pointer[bytes - 1] == static_cast<unsigned char>(value + 1);
    free_nid_postfix(pointer);
    return preserved;
}

DWORD runChild(const wchar_t* mode) {
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return 0xffffffffu;
    std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" " + mode;
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.data(), commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &process)) return 0xffffffffu;
    const DWORD wait = WaitForSingleObject(process.hProcess, 30000);
    if (wait != WAIT_OBJECT_0) {
        const DWORD exitCode = wait == WAIT_TIMEOUT ? 0xfffffffeu : 0xffffffffu;
        TerminateProcess(process.hProcess, exitCode);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return exitCode;
    }
    DWORD exitCode = 0xffffffffu;
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) exitCode = 0xffffffffu;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode;
}

int runMallocOverflow() {
    registerDefaultHeap();
    try {
        auto* pointer = malloc_nid_postfix(std::numeric_limits<std::size_t>::max() - HeaderBytes);
        free_nid_postfix(pointer);
        return 1;
    } catch (const std::length_error& error) {
        std::fprintf(stderr, "malloc overflow rejected: %s\n", error.what());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "malloc failed with an unexpected exception: %s\n", error.what());
        return 2;
    } catch (...) {
        std::fputs("malloc failed with a non-standard exception\n", stderr);
        return 3;
    }
}

int runReallocOverflow() {
    registerDefaultHeap();
    constexpr std::size_t bytes = 16;
    auto* original = static_cast<unsigned char*>(malloc_nid_postfix(bytes));
    std::memset(original, 0x5a, bytes);
    bool rejectedWithExpectedError = false;
    try {
        auto* result = realloc_nid_postfix(original, std::numeric_limits<std::size_t>::max() - HeaderBytes);
        if (result != nullptr) {
            free_nid_postfix(result);
            return 4;
        }
    } catch (const std::length_error& error) {
        rejectedWithExpectedError = true;
        std::fprintf(stderr, "realloc overflow rejected: %s\n", error.what());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "realloc failed with an unexpected exception: %s\n", error.what());
    } catch (...) {
        std::fputs("realloc failed with a non-standard exception\n", stderr);
    }
    bool originalPreserved = true;
    for (std::size_t index = 0; index < bytes; ++index) {
        if (original[index] != 0x5a) originalPreserved = false;
    }
    auto* replacement = static_cast<unsigned char*>(nullptr);
    bool followUpAllocationThrew = false;
    try {
        replacement = static_cast<unsigned char*>(malloc_nid_postfix(bytes));
    } catch (const std::exception& error) {
        followUpAllocationThrew = true;
        std::fprintf(stderr, "follow-up allocation failed with an exception: %s\n", error.what());
    } catch (...) {
        followUpAllocationThrew = true;
        std::fputs("follow-up allocation failed with a non-standard exception\n", stderr);
    }
    unsigned result = 0;
    if (!rejectedWithExpectedError) result |= 1;
    if (!originalPreserved) result |= 2;
    if (replacement == nullptr) result |= 4;
    if (replacement == original) result |= 8;
    if (followUpAllocationThrew) result |= 16;
    if (result != 0) {
        std::fprintf(stderr, "realloc checks failed: rejected=%d preserved=%d replacement=%d distinct=%d followUpThrew=%d\n",
                     rejectedWithExpectedError, originalPreserved, replacement != nullptr, replacement != original, followUpAllocationThrew);
    }
    if (result == 0) {
        free_nid_postfix(replacement);
        free_nid_postfix(original);
    }
    return static_cast<int>(result);
}

int runGuardedCases() {
    bool success = true;
    for (const auto& [mode, label] : std::array<std::pair<const wchar_t*, const char*>, 2>{
             std::pair{L"overflow-malloc", "overflow malloc"},
             std::pair{L"overflow-realloc", "overflow realloc"}}) {
        const DWORD exitCode = runChild(mode);
        if (exitCode != 0) {
            std::fprintf(stderr, "%s child exited with 0x%08lx\n", label, static_cast<unsigned long>(exitCode));
            success = false;
        }
    }
    return success ? 0 : 1;
}

}

int main(int argc, char** argv) {
    if (argc == 2) SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc == 2 && std::strcmp(argv[1], "overflow-malloc") == 0) return runMallocOverflow();
    if (argc == 2 && std::strcmp(argv[1], "overflow-realloc") == 0) return runReallocOverflow();
    if (argc != 1) return 10;
    registerDefaultHeap();
    if (!checkThreshold(MaximumSmallBlockBytes - HeaderBytes, 0x31) ||
        !checkThreshold(MaximumSmallBlockBytes - HeaderBytes + 1, 0x72)) return 11;
    return runGuardedCases();
}
