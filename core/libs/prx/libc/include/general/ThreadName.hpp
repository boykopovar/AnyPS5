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
#endif

#endif
