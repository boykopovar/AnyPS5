#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/WindowsDescriptor.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <io.h>

extern "C" {

extern FileStream* __stdinp_nid_postfix;
extern FileStream* __stdoutp_nid_postfix;
extern FileStream* __stderrp_nid_postfix;

}

namespace {

constexpr int GuestGetDescriptorFlags = 1;
constexpr int GuestGetFlags = 3;
constexpr int GuestAccessMode = 3;
constexpr int GuestReadOnly = 0;
constexpr int GuestWriteOnly = 1;

bool Check(bool value, const char* message) {
    if (value) return true;
    std::fprintf(stderr, "WindowsDescriptor: %s\n", message);
    return false;
}

template <typename Fn>
bool ExpectRuntimeError(Fn&& fn, const char* message) {
    bool threw = false;
    try {
        (void)fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    return Check(threw, message);
}

bool TestWindowsDescriptor() {
    if (!Check(WindowsDescriptorHandle_nid_no_patch(-1) == INVALID_HANDLE_VALUE, "invalid descriptor handle"))
        return false;
    if (!Check(WindowsDescriptorHandle_nid_no_patch(-2) == INVALID_HANDLE_VALUE, "no-console descriptor handle"))
        return false;
    const HANDLE pseudoThread = reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-2));
    const HANDLE badHandles[] = { nullptr, INVALID_HANDLE_VALUE, pseudoThread };
    for (HANDLE bad : badHandles) {
        if (!ExpectRuntimeError([&]() { return WindowsHandleAccess_nid_no_patch(bad, -1, "test"); }, "WindowsHandleAccess rejects pseudo or null handle"))
            return false;
        if (!ExpectRuntimeError([&]() { return WindowsDescriptorAccess_nid_no_patch(bad, -1, "test"); }, "WindowsDescriptorAccess rejects pseudo or null handle"))
            return false;
    }
    int pipeFds[2] = { -1, -1 };
    if (!Check(::_pipe(pipeFds, 256, _O_BINARY) == 0, "_pipe"))
        return false;
    const HANDLE readHandle = WindowsDescriptorHandle_nid_no_patch(pipeFds[0]);
    const HANDLE writeHandle = WindowsDescriptorHandle_nid_no_patch(pipeFds[1]);
    bool ok = Check(readHandle != nullptr && readHandle != INVALID_HANDLE_VALUE && readHandle != pseudoThread, "pipe read handle") &&
              Check(writeHandle != nullptr && writeHandle != INVALID_HANDLE_VALUE && writeHandle != pseudoThread, "pipe write handle") &&
              Check(WindowsHandleAccess_nid_no_patch(readHandle, pipeFds[0], "test") != 0, "pipe read access mask") &&
              Check(WindowsHandleAccess_nid_no_patch(writeHandle, pipeFds[1], "test") != 0, "pipe write access mask");
    if (ok) {
        const int readAccess = WindowsDescriptorAccess_nid_no_patch(readHandle, pipeFds[0], "test");
        const int writeAccess = WindowsDescriptorAccess_nid_no_patch(writeHandle, pipeFds[1], "test");
        ok = Check(readAccess == 0 || readAccess == 2, "pipe read descriptor access") &&
             Check(writeAccess == 1 || writeAccess == 2, "pipe write descriptor access");
    }
    if (::_close(pipeFds[0]) != 0) ok = false;
    if (::_close(pipeFds[1]) != 0) ok = false;
    return ok;
}

int Child(const char* closed) {
    for (int descriptor = 0; descriptor <= 2; ++descriptor) {
        if (closed[descriptor] != '1') continue;
        const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
        if (handle == INVALID_HANDLE_VALUE || handle == reinterpret_cast<HANDLE>(-2)) return 70 + descriptor;
        if (::GetFileType(handle) != FILE_TYPE_CHAR) return 73 + descriptor;
        if (fcntl_nid_postfix(descriptor, GuestGetDescriptorFlags) != 0) return 76 + descriptor;
        const int expected = descriptor == 0 ? GuestReadOnly : GuestWriteOnly;
        if ((fcntl_nid_postfix(descriptor, GuestGetFlags) & GuestAccessMode) != expected) return 79 + descriptor;
        std::FILE* const host = descriptor == 0 ? stdin : descriptor == 1 ? stdout : stderr;
        FileStream* const guest = descriptor == 0 ? __stdinp_nid_postfix : descriptor == 1 ? __stdoutp_nid_postfix : __stderrp_nid_postfix;
        if (::_fileno(host) != descriptor) return 82 + descriptor;
        if (guest->GuestState().descriptor != descriptor) return 96 + descriptor;
        if (fileno_nid_postfix(guest) != descriptor) return 85 + descriptor;
        char buffer[16];
        if (descriptor == 0) {
            if (std::fgetc(host) != EOF || std::feof(host) == 0 || std::ferror(host) != 0) return 88;
            std::clearerr(host);
            if (fgetc_nid_postfix(guest) != EOF || feof_nid_postfix(guest) == 0 || ferror_nid_postfix(guest) != 0) return 89;
            clearerr_nid_postfix(guest);
            if (read_nid_postfix(0, buffer, sizeof(buffer)) != 0) return 100;
            if (_read_nid_postfix(0, buffer, sizeof(buffer)) != 0) return 101;
            *__error_nid_postfix() = 0;
            if (write_nid_postfix(0, "x", 1) != -1 || *__error_nid_postfix() != 9) return 102;
            *__error_nid_postfix() = 0;
            if (_write_nid_postfix(0, "x", 1) != -1 || *__error_nid_postfix() != 9) return 103;
        } else {
            if (std::fputc('x', host) == EOF || std::fflush(host) != 0 || std::ferror(host) != 0) return 90 + descriptor;
            if (fputc_nid_postfix('x', guest) == EOF || fflush_nid_postfix(guest) != 0 || ferror_nid_postfix(guest) != 0) return 93 + descriptor;
            if (write_nid_postfix(descriptor, "x", 1) != 1) return 104 + descriptor;
            if (_write_nid_postfix(descriptor, "x", 1) != 1) return 107 + descriptor;
            *__error_nid_postfix() = 0;
            if (read_nid_postfix(descriptor, buffer, sizeof(buffer)) != -1 || *__error_nid_postfix() != 9) return 110 + descriptor;
            *__error_nid_postfix() = 0;
            if (_read_nid_postfix(descriptor, buffer, sizeof(buffer)) != -1 || *__error_nid_postfix() != 9) return 113 + descriptor;
        }
        FileStat sb{};
        if (_fstat_nid_postfix(descriptor, &sb) != 0) return 116 + descriptor;
        if ((sb.st_mode & 0170000) != 0020000) return 119 + descriptor;
    }
    return 0;
}

DWORD RunWithout(const wchar_t* self, bool input, bool output, bool error) {
    std::wstring command = std::wstring(L"\"") + self + L"\" --closed-child " + (input ? L"1" : L"0") + (output ? L"1" : L"0") + (error ? L"1" : L"0");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input ? nullptr : GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = output ? nullptr : GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = error ? nullptr : GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(self, command.data(), nullptr, nullptr, TRUE, DETACHED_PROCESS, nullptr, nullptr, &startup, &process)) return 0xffffffffu;
    DWORD code = 0xfffffffeu;
    if (WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
    else TerminateProcess(process.hProcess, code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code;
}

}

int main(int argc, char** argv) {
    if (argc > 2 && std::strcmp(argv[1], "--closed-child") == 0 && std::strlen(argv[2]) == 3) return Child(argv[2]);
    if (!TestWindowsDescriptor()) return 1;
    wchar_t self[MAX_PATH];
    if (GetModuleFileNameW(nullptr, self, MAX_PATH) == 0) return 2;
    const struct { bool input, output, error; const char* name; } cases[] = {
        {true, false, false, "stdin"},
        {false, true, false, "stdout"},
        {false, false, true, "stderr"},
        {true, true, false, "stdin and stdout"},
        {true, false, true, "stdin and stderr"},
        {false, true, true, "stdout and stderr"},
        {true, true, true, "all three"},
    };
    for (const auto& test : cases) {
        const DWORD result = RunWithout(self, test.input, test.output, test.error);
        if (result != 0) {
            std::fprintf(stderr, "missing standard handles, %s: child exited with %lu\n", test.name, result);
            return 1;
        }
    }
    std::puts("missing standard handles are opened on NUL");
    return 0;
}

