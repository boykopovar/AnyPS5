#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cwchar>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include <io.h>

extern "C" {
_invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);

}

namespace {
int invalidCalls = 0;

void InvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++invalidCalls;
}

DWORD RunPrebound(const std::wstring& library, HANDLE report) {
    wchar_t self[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 0xffffffffu;
    HANDLE inherited = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), report, GetCurrentProcess(), &inherited, 0, TRUE, DUPLICATE_SAME_ACCESS)) return 0xfffffffeu;
    std::wstring command = L"\"" + std::wstring(self) + L"\" \"" + library + L"\" --prebound";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdError = inherited;
    PROCESS_INFORMATION process{};
    const bool created = CreateProcessW(self, command.data(), nullptr, nullptr, TRUE, DETACHED_PROCESS, nullptr, nullptr, &startup, &process) != 0;
    CloseHandle(inherited);
    if (!created) return 0xfffffffdu;
    DWORD code = 0xfffffffcu;
    if (WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
    else TerminateProcess(process.hProcess, code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code;
}
}

int main() {
    HANDLE report = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE), GetCurrentProcess(), &report, 0, FALSE, DUPLICATE_SAME_ACCESS)) return 1;
    const auto finish = [&](int code, const std::string& message) {
        DWORD written = 0;
        WriteFile(report, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
        CloseHandle(report);
        return code;
    };
    int count = 0;
    auto** const arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments || (count != 2 && count != 3) || (count == 3 && std::wcscmp(arguments[2], L"--prebound") != 0)) {
        if (arguments) LocalFree(arguments);
        return finish(2, "expected one libc path argument\n");
    }
    const std::wstring library(arguments[1]);
    const bool prebound = count == 3;
    LocalFree(arguments);
    int outputDescriptor = 1;
    if (prebound) {
        if (::_fileno(stdout) != -2 || ::_wfreopen(L"NUL", L"wb", stdout) != stdout)
            return finish(11, "cannot prepare an already rebound stdout\n");
        outputDescriptor = ::_fileno(stdout);
        if (outputDescriptor < 3 || ::_get_osfhandle(1) != -2)
            return finish(11, "stdout did not retain its separate descriptor\n");
    } else {
        const DWORD child = RunPrebound(library, report);
        if (child != 0) return finish(12, "prebound stdout child exited with " + std::to_string(child) + "\n");
        for (int descriptor : {1, 2}) {
            if (::_fileno(__acrt_iob_func(descriptor)) != descriptor || ::_close(descriptor) != 0)
                return finish(4, "cannot close an inherited standard descriptor\n");
        }
    }
    const auto previous = _set_thread_local_invalid_parameter_handler(InvalidParameter);
    const auto module = LoadLibraryW(library.c_str());
    const DWORD loadError = module == nullptr ? GetLastError() : ERROR_SUCCESS;
    _set_thread_local_invalid_parameter_handler(previous);
    if (module == nullptr) return finish(5, "cannot load libc: " + std::to_string(loadError) + "\n");
    if (invalidCalls != 0) return finish(6, "restoring bound standard streams invoked the invalid parameter handler " + std::to_string(invalidCalls) + " times\n");
    const auto put = reinterpret_cast<decltype(&fputc_nid_postfix)>(GetProcAddress(module, "fputc_nid_postfix"));
    const auto flush = reinterpret_cast<decltype(&fflush_nid_postfix)>(GetProcAddress(module, "fflush_nid_postfix"));
    if (!put || !flush) return finish(7, "cannot find guest stream operations\n");
    if (::_write(1, "x", 1) != 1) return finish(13, "the restored stdout descriptor cannot write\n");
    for (int descriptor : {1, 2}) {
        const auto pointer = GetProcAddress(module, descriptor == 1 ? "__stdoutp_nid_postfix" : "__stderrp_nid_postfix");
        if (!pointer) return finish(8, "cannot find a guest standard stream\n");
        auto* const guest = *reinterpret_cast<FileStream**>(pointer);
        auto* const host = __acrt_iob_func(descriptor);
        const int expected = descriptor == 1 ? outputDescriptor : descriptor;
        if (!guest || guest->GuestState().descriptor != expected || ::_fileno(host) != expected)
            return finish(9, "a restored stream has the wrong descriptor\n");
        if (std::fputc('x', host) != 'x' || std::fflush(host) != 0 || put('x', guest) != 'x' || flush(guest) != 0)
            return finish(10, "a restored stream cannot write to NUL\n");
    }
    return finish(0, "bound standard streams are restored without invalid parameter callbacks\n");
}
