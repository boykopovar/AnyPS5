#include "prx/libc/include/LogFile.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

namespace log_file {

namespace {

constexpr const char VariableName[] = "ANYPS5_LOG_FILE";

#ifdef _WIN32

HANDLE g_diverted[3] = {nullptr, nullptr, nullptr};

DWORD StandardId(int standard) {
    return standard == StandardOutput ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
}

std::string Describe(unsigned long code) {
    char text[256];
    const DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, text, sizeof(text), nullptr);
    std::string message(text, length);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ' || message.back() == '.')) message.pop_back();
    if (message.empty()) message = "unknown error";
    return message + " (Windows error " + std::to_string(code) + ")";
}

bool Valid(HANDLE handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

#else

struct Identity {
    dev_t device = 0;
    ino_t node = 0;
    bool known = false;
};

Identity g_diverted[3];

std::string Describe(int code) {
    return std::string(std::strerror(code)) + " (errno " + std::to_string(code) + ")";
}

#endif

}

#ifdef _WIN32

bool OpenAppend(const std::string& path, Handle& file, std::string& error) {
    const HANDLE handle = CreateFileW(Widen(path).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = Describe(GetLastError());
        return false;
    }
    SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
    file = reinterpret_cast<Handle>(handle);
    return true;
}

bool MakePipe(Pipe& pipe, std::string& error) {
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if (!CreatePipe(&read, &write, &attributes, 1u << 20)) {
        error = Describe(GetLastError());
        return false;
    }
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(write, HANDLE_FLAG_INHERIT, 0);
    pipe.read = reinterpret_cast<Handle>(read);
    pipe.write = reinterpret_cast<Handle>(write);
    return true;
}

Handle KeepStandard(int standard) {
    const HANDLE current = GetStdHandle(StandardId(standard));
    if (!Valid(current)) return NoHandle;
    HANDLE kept = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), current, GetCurrentProcess(), &kept, 0, FALSE, DUPLICATE_SAME_ACCESS)) return NoHandle;
    return reinterpret_cast<Handle>(kept);
}

bool Divert(int standard, Handle write) {
    HANDLE copy = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), reinterpret_cast<HANDLE>(write), GetCurrentProcess(), &copy, 0, TRUE, DUPLICATE_SAME_ACCESS)) return false;
    const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(copy), _O_BINARY | _O_WRONLY);
    if (descriptor < 0) {
        CloseHandle(copy);
        return false;
    }
    const int result = _dup2(descriptor, standard);
    _close(descriptor);
    if (result != 0) return false;
    const HANDLE installed = reinterpret_cast<HANDLE>(_get_osfhandle(standard));
    if (!SetStdHandle(StandardId(standard), installed)) return false;
    g_diverted[standard] = installed;
    return true;
}

void RestoreStandard(int standard, Handle original) {
    if (g_diverted[standard] == nullptr || GetStdHandle(StandardId(standard)) != g_diverted[standard]) return;
    g_diverted[standard] = nullptr;
    if (original == NoHandle) {
        _close(standard);
        SetStdHandle(StandardId(standard), nullptr);
        return;
    }
    HANDLE copy = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), reinterpret_cast<HANDLE>(original), GetCurrentProcess(), &copy, 0, TRUE, DUPLICATE_SAME_ACCESS)) return;
    const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(copy), _O_BINARY | _O_WRONLY);
    if (descriptor < 0) {
        CloseHandle(copy);
        return;
    }
    const int result = _dup2(descriptor, standard);
    _close(descriptor);
    if (result == 0) SetStdHandle(StandardId(standard), reinterpret_cast<HANDLE>(_get_osfhandle(standard)));
}

bool IsInteractive(Handle handle) {
    DWORD mode = 0;
    return GetConsoleMode(reinterpret_cast<HANDLE>(handle), &mode) != 0;
}

void PauseMicroseconds(long microseconds) {
    Sleep(static_cast<DWORD>(std::max(1L, microseconds / 1000)));
}

void WriteStandardDirect(int standard, const char* text, std::size_t length) {
    const HANDLE handle = GetStdHandle(StandardId(standard));
    DWORD written = 0;
    WriteFile(handle, text, static_cast<DWORD>(length), &written, nullptr);
}

bool ReadVariable(std::string& value) {
    const DWORD size = GetEnvironmentVariableW(L"ANYPS5_LOG_FILE", nullptr, 0);
    if (size <= 1) return false;
    std::wstring wide(size, L'\0');
    const DWORD length = GetEnvironmentVariableW(L"ANYPS5_LOG_FILE", wide.data(), size);
    if (length == 0 || length >= size) return false;
    wide.resize(length);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    value.assign(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), value.data(), bytes, nullptr, nullptr);
    return true;
}

void FailStartup(const std::string& message) {
    WriteStandardDirect(StandardError, message.data(), message.size());
    TerminateProcess(GetCurrentProcess(), 1);
    std::abort();
}

bool WaitReadable(Handle read, int milliseconds) {
    for (int waited = 0; waited <= milliseconds; waited += 2) {
        if (Pending(read) != 0) return true;
        Sleep(2);
    }
    return false;
}

long Pending(Handle read) {
    DWORD available = 0;
    if (!PeekNamedPipe(reinterpret_cast<HANDLE>(read), nullptr, 0, nullptr, &available, nullptr)) return -1;
    return static_cast<long>(available);
}

long ReadAvailable(Handle read, char* buffer, std::size_t capacity) {
    const long available = Pending(read);
    if (available <= 0) return available;
    DWORD count = 0;
    if (!ReadFile(reinterpret_cast<HANDLE>(read), buffer, static_cast<DWORD>(std::min<std::size_t>(capacity, static_cast<std::size_t>(available))), &count, nullptr)) return -1;
    return static_cast<long>(count);
}

bool WriteAll(Handle destination, const char* data, std::size_t length) {
    std::size_t done = 0;
    while (done < length) {
        DWORD written = 0;
        if (!WriteFile(reinterpret_cast<HANDLE>(destination), data + done, static_cast<DWORD>(std::min<std::size_t>(length - done, 1u << 20)), &written, nullptr) || written == 0) return false;
        done += written;
    }
    return true;
}

void Close(Handle handle) {
    if (handle != NoHandle) CloseHandle(reinterpret_cast<HANDLE>(handle));
}

#else

bool OpenAppend(const std::string& path, Handle& file, std::string& error) {
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
    if (descriptor < 0) {
        error = Describe(errno);
        return false;
    }
    file = descriptor;
    return true;
}

bool MakePipe(Pipe& pipe, std::string& error) {
    int descriptors[2];
    if (::pipe(descriptors) != 0) {
        error = Describe(errno);
        return false;
    }
    for (const int descriptor : descriptors) ::fcntl(descriptor, F_SETFD, FD_CLOEXEC);
    ::fcntl(descriptors[0], F_SETFL, ::fcntl(descriptors[0], F_GETFL) | O_NONBLOCK);
#ifdef F_SETPIPE_SZ
    ::fcntl(descriptors[1], F_SETPIPE_SZ, 1 << 20);
#endif
    pipe.read = descriptors[0];
    pipe.write = descriptors[1];
    return true;
}

Handle KeepStandard(int standard) {
    const int kept = ::fcntl(standard, F_DUPFD_CLOEXEC, 3);
    return kept < 0 ? NoHandle : kept;
}

bool Divert(int standard, Handle write) {
    struct stat information{};
    if (::fstat(static_cast<int>(write), &information) != 0) return false;
    if (::dup2(static_cast<int>(write), standard) != standard) return false;
    g_diverted[standard] = {information.st_dev, information.st_ino, true};
    return true;
}

void RestoreStandard(int standard, Handle original) {
    if (!g_diverted[standard].known) return;
    struct stat information{};
    if (::fstat(standard, &information) != 0 || information.st_dev != g_diverted[standard].device || information.st_ino != g_diverted[standard].node) return;
    g_diverted[standard].known = false;
    if (original == NoHandle) ::close(standard);
    else ::dup2(static_cast<int>(original), standard);
}

bool IsInteractive(Handle handle) {
    return ::isatty(static_cast<int>(handle)) == 1;
}

void PauseMicroseconds(long microseconds) {
    const timespec pause{0, microseconds * 1000};
    ::nanosleep(&pause, nullptr);
}

void WriteStandardDirect(int standard, const char* text, std::size_t length) {
    std::size_t done = 0;
    while (done < length) {
        const ssize_t result = ::write(standard, text + done, length - done);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) break;
        done += static_cast<std::size_t>(result);
    }
}

bool ReadVariable(std::string& value) {
    const char* text = std::getenv(VariableName);
    if (text == nullptr || *text == '\0') return false;
    value = text;
    return true;
}

void FailStartup(const std::string& message) {
    WriteStandardDirect(StandardError, message.data(), message.size());
    ::_exit(1);
}

bool WaitReadable(Handle read, int milliseconds) {
    pollfd request{static_cast<int>(read), POLLIN, 0};
    const int ready = ::poll(&request, 1, milliseconds);
    if (ready > 0 && (request.revents & POLLIN) != 0) return true;
    if (ready > 0) PauseMicroseconds(20000);
    return false;
}

long Pending(Handle read) {
    int available = 0;
    if (::ioctl(static_cast<int>(read), FIONREAD, &available) != 0) return -1;
    return available;
}

long ReadAvailable(Handle read, char* buffer, std::size_t capacity) {
    const ssize_t count = ::read(static_cast<int>(read), buffer, capacity);
    if (count >= 0) return static_cast<long>(count);
    return errno == EAGAIN || errno == EINTR ? 0 : -1;
}

bool WriteAll(Handle destination, const char* data, std::size_t length) {
    std::size_t done = 0;
    int stalled = 0;
    while (done < length) {
        const ssize_t result = ::write(static_cast<int>(destination), data + done, length - done);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0 && errno == EAGAIN && stalled++ < 50) {
            pollfd request{static_cast<int>(destination), POLLOUT, 0};
            ::poll(&request, 1, 100);
            continue;
        }
        if (result <= 0) return false;
        done += static_cast<std::size_t>(result);
    }
    return true;
}

void Close(Handle handle) {
    if (handle != NoHandle) ::close(static_cast<int>(handle));
}

#endif

}
