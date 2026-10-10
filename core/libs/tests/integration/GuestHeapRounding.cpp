#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

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
#include <vector>

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t);
void APS5_VABI free_nid_postfix(void*);
void* APS5_VABI realloc_nid_postfix(void*, std::size_t);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::size_t headerBytes = 2 * sizeof(void*);
constexpr std::size_t maximumSmallBlockBytes = 64u * 1024u;

void RegisterDefaultHeap() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void RequireThresholdAllocation(std::size_t bytes, unsigned char value) {
    RegisterDefaultHeap();
    auto* pointer = static_cast<unsigned char*>(malloc_nid_postfix(bytes));
    Require(pointer != nullptr, "allocation of " + std::to_string(bytes) + " bytes");
    const bool aligned = reinterpret_cast<std::uintptr_t>(pointer) % 16 == 0;
    pointer[0] = value;
    pointer[bytes - 1] = static_cast<unsigned char>(value + 1);
    const bool preserved = pointer[0] == value && pointer[bytes - 1] == static_cast<unsigned char>(value + 1);
    free_nid_postfix(pointer);
    Require(aligned, "16-byte alignment of " + std::to_string(bytes) + " bytes");
    Require(preserved, "first and last bytes preserved for " + std::to_string(bytes) + " bytes");
}

DWORD RunChild(const wchar_t* mode) {
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

int RunMallocOverflow() {
    RegisterDefaultHeap();
    try {
        auto* pointer = malloc_nid_postfix(std::numeric_limits<std::size_t>::max() - headerBytes);
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

int RunReallocOverflow() {
    RegisterDefaultHeap();
    constexpr std::size_t bytes = 16;
    auto* original = static_cast<unsigned char*>(malloc_nid_postfix(bytes));
    std::memset(original, 0x5a, bytes);
    bool rejectedWithExpectedError = false;
    try {
        auto* result = realloc_nid_postfix(original, std::numeric_limits<std::size_t>::max() - headerBytes);
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

const Case largestSmallBlock{"Malloc_LargestSmallBlock_AlignedAndUsable", [] {
    RequireThresholdAllocation(maximumSmallBlockBytes - headerBytes, 0x31);
}};

const Case smallestLargeBlock{"Malloc_SmallestLargeBlock_AlignedAndUsable", [] {
    RequireThresholdAllocation(maximumSmallBlockBytes - headerBytes + 1, 0x72);
}};

const Case mallocOverflow{"Malloc_SizeOverflowingHeader_ThrowsLengthError", [] {
    RequireEqual(RunChild(L"overflow-malloc"), DWORD {0}, "overflow-malloc child exit code");
}};

const Case reallocOverflow{"Realloc_SizeOverflowingHeader_ThrowsLengthErrorAndKeepsOriginal", [] {
    RequireEqual(RunChild(L"overflow-realloc"), DWORD {0}, "overflow-realloc child exit code (bit mask of failed checks)");
}};

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "overflow-malloc") == 0) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        return RunMallocOverflow();
    }
    if (argc == 2 && std::strcmp(argv[1], "overflow-realloc") == 0) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        return RunReallocOverflow();
    }
    return Testing::Run(argc, argv);
}
