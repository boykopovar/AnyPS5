#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_THREADNAME_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_THREADNAME_HPP

#ifdef _WIN32
#include <cstddef>
#include <cstring>
#include <windows.h>

namespace ThreadName {

template<typename TFunction>
inline TFunction Resolve(const char* name) {
    TFunction function = nullptr;
    const auto kernel = GetModuleHandleW(L"Kernel32.dll");
    if (kernel == nullptr) return function;
    const FARPROC address = GetProcAddress(kernel, name);
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

inline HRESULT SetCurrent(const char* name) {
    if (name == nullptr) return E_INVALIDARG;
    wchar_t wideName[64];
    std::size_t length = 0;
    while (name[length] != '\0' && length < (sizeof(wideName) / sizeof(wideName[0])) - 1) {
        wideName[length] = static_cast<unsigned char>(name[length]);
        ++length;
    }
    if (name[length] != '\0') return E_INVALIDARG;
    wideName[length] = L'\0';
    using SetThreadDescriptionFunction = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const auto setThreadDescription = Resolve<SetThreadDescriptionFunction>("SetThreadDescription");
    if (setThreadDescription == nullptr) return S_FALSE;
    return setThreadDescription(GetCurrentThread(), wideName);
}

}
#elif defined(__linux__)
#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <pthread.h>
#include <string>
#include <unistd.h>

namespace ThreadName {

inline constexpr std::size_t HostNameBytes = 15;

inline std::string HostName(const char* name) {
    std::string text(name);
    if (text.size() > HostNameBytes) std::erase(text, ' ');
    if (text.size() > HostNameBytes) {
        std::size_t end = HostNameBytes;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0u) == 0x80u) --end;
        text.resize(end);
    }
    return text;
}

inline int SetCurrent(const char* name) {
    if (name == nullptr) return EINVAL;
    return pthread_setname_np(pthread_self(), HostName(name).c_str());
}

inline int SetTask(int tid, const char* name) {
    if (name == nullptr || tid <= 0) return EINVAL;
    const auto path = "/proc/self/task/" + std::to_string(tid) + "/comm";
    const int file = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (file < 0) return errno;
    const auto text = HostName(name);
    const auto written = write(file, text.data(), text.size());
    const int error = written == static_cast<ssize_t>(text.size()) ? 0 : written < 0 ? errno : EIO;
    close(file);
    return error;
}

}
#endif

#endif
