#pragma once

#include <cstddef>
#include <string>

#ifdef _WIN32
#include <windows.h>

inline std::string GuestTimeZoneNameToUtf8(const char* name, unsigned int codePage) {
    if (name == nullptr) return {};
    struct LastErrorGuard {
        DWORD savedError;
        LastErrorGuard() : savedError(GetLastError()) {
            SetLastError(ERROR_SUCCESS);
        }
        ~LastErrorGuard() {
            SetLastError(savedError);
        }
        LastErrorGuard(const LastErrorGuard&) = delete;
        LastErrorGuard& operator=(const LastErrorGuard&) = delete;
    };
    const LastErrorGuard lastErrorGuard;
    DWORD mbFlags = MB_ERR_INVALID_CHARS;
    int wideLength = MultiByteToWideChar(codePage, mbFlags, name, -1, nullptr, 0);
    if (wideLength <= 0 && GetLastError() == ERROR_INVALID_FLAGS) {
        mbFlags = 0;
        wideLength = MultiByteToWideChar(codePage, mbFlags, name, -1, nullptr, 0);
    }
    if (wideLength <= 0) return name;
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(codePage, mbFlags, name, -1, wide.data(), wideLength) <= 0) return name;
    const int utf8Length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) return name;
    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), -1, utf8.data(), utf8Length, nullptr, nullptr) <= 0) return name;
    utf8.resize(static_cast<std::size_t>(utf8Length - 1));
    return utf8;
}
#endif
