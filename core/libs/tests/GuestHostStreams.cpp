#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <io.h>
#include <stdlib.h>
#include <string>
#include <vector>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEperm = 1;
constexpr int GuestEbadf = 9;
constexpr int SceKernelErrorEperm = static_cast<int>(0x80020001);
constexpr int SceKernelErrorEbadf = static_cast<int>(0x80020009);

#ifdef _WIN32
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
static void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
#endif

static bool HasAttachedHandle(int fd) {
#ifdef _WIN32
    const int savedErrno = errno;
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const std::intptr_t handle = _get_osfhandle(fd);
    _set_thread_local_invalid_parameter_handler(previous);
    errno = savedErrno;
    return handle != -1 && handle != -2 && handle != 0;
#else
    (void)fd;
    return true;
#endif
}

static bool Open(int fd) {
    const int duplicate = dup_nid_postfix(fd);
    if (duplicate < 0) return false;
    Require(close_nid_postfix(duplicate) == 0);
    return true;
}

static void RunClosedStdinChild(char* exePath) {
#ifdef _WIN32
    (void)exePath;
    std::wstring modulePath(260, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        Require(len != 0);
        if (len < modulePath.size()) {
            modulePath.resize(len);
            break;
        }
        modulePath.resize(modulePath.size() * 2);
    }
    const std::wstring cmdLine = L"\"" + modulePath + L"\" --closed-stdin";
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};
    Require(CreateProcessW(
        modulePath.c_str(),
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi) != FALSE);
    Require(WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0);
    DWORD exitCode = 1;
    Require(GetExitCodeProcess(pi.hProcess, &exitCode) != FALSE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Require(exitCode == 0);
#else
    const pid_t pid = ::fork();
    Require(pid >= 0);
    if (pid == 0) {
        ::close(0);
        char flag[] = "--closed-stdin";
        char* childArgv[] = {exePath, flag, nullptr};
        ::execvp(exePath, childArgv);
        ::_exit(127);
    }
    int status = 0;
    pid_t waited = -1;
    do {
        waited = ::waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    Require(waited == pid);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
}

int main(int argc, char** argv) {
    const int entryErrno = errno;
    if (argc > 1 && std::strcmp(argv[1], "--closed-stdin") == 0) {
        Require(entryErrno != EBADF);
        *__error_nid_postfix() = 0;
        const int firstClose = close_nid_postfix(0);
        Require(firstClose == 0 || (firstClose == -1 && *__error_nid_postfix() == GuestEbadf));
        Require(!Open(0));
        *__error_nid_postfix() = 0;
        Require(close_nid_postfix(0) == -1 && *__error_nid_postfix() == GuestEbadf);
        Require(sceKernelClose(0) == SceKernelErrorEbadf);
        return 0;
    }

    bool open[3];
    bool guarded[3];
    for (int fd = 0; fd < 3; ++fd) {
        open[fd] = Open(fd);
        guarded[fd] = open[fd] && HasAttachedHandle(fd);
    }
    for (int fd = 0; fd < 3; ++fd) {
        if (!guarded[fd]) continue;
        Require(close_nid_postfix(fd) == -1 && *__error_nid_postfix() == GuestEperm);
        Require(sceKernelClose(fd) == SceKernelErrorEperm);
        Require(Open(fd));
    }
    *__error_nid_postfix() = 0;
    Require(dup_nid_postfix(-1) == -1 && *__error_nid_postfix() == GuestEbadf);

    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    for (const int fd : pipe) Require(fd >= 3 || !open[fd]);
    const int dupRead = dup_nid_postfix(pipe[0]);
    Require(dupRead >= 0 && dupRead != pipe[0]);
    Require(close_nid_postfix(dupRead) == 0);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
    Require(close_nid_postfix(pipe[0]) == -1 && *__error_nid_postfix() == GuestEbadf);
    *__error_nid_postfix() = 0;
    Require(dup_nid_postfix(pipe[0]) == -1 && *__error_nid_postfix() == GuestEbadf);
    Require(sceKernelClose(pipe[1]) == SceKernelErrorEbadf);
    Require(close_nid_postfix(-1) == -1 && *__error_nid_postfix() == GuestEbadf);
    Require(sceKernelClose(-1) == SceKernelErrorEbadf);
    Require(sceKernelClose(-2) == SceKernelErrorEbadf);

    Require(argc >= 1 && argv[0] != nullptr);
    RunClosedStdinChild(argv[0]);
    return 0;
}
