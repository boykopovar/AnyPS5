#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>

inline std::string GuestTimeZoneNameToUtf8(const char* name, unsigned int codePage) {
    if (name == nullptr) return {};
    const int wideLength = MultiByteToWideChar(codePage, 0, name, -1, nullptr, 0);
    if (wideLength == 0) return name;
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(codePage, 0, name, -1, wide.data(), wideLength) == 0) return name;
    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0) return name;
    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, utf8.data(), utf8Length, nullptr, nullptr) == 0) return name;
    utf8.resize(static_cast<std::size_t>(utf8Length - 1));
    return utf8;
}
#endif
