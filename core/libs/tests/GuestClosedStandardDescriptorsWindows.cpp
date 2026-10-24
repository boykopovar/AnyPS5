#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstring>
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
        if (descriptor == 0) {
            if (std::fgetc(host) != EOF || std::feof(host) == 0 || std::ferror(host) != 0) return 88;
            std::clearerr(host);
            if (fgetc_nid_postfix(guest) != EOF || feof_nid_postfix(guest) == 0 || ferror_nid_postfix(guest) != 0) return 89;
            clearerr_nid_postfix(guest);
        } else {
            if (std::fputc('x', host) == EOF || std::fflush(host) != 0 || std::ferror(host) != 0) return 90 + descriptor;
            if (fputc_nid_postfix('x', guest) == EOF || fflush_nid_postfix(guest) != 0 || ferror_nid_postfix(guest) != 0) return 93 + descriptor;
        }
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
    wchar_t self[MAX_PATH];
    if (GetModuleFileNameW(nullptr, self, MAX_PATH) == 0) return 2;
    const struct { bool input, output, error; const char* name; } cases[] = {
        {true, false, false, "stdin"},
        {false, true, false, "stdout"},
        {false, false, true, "stderr"},
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
