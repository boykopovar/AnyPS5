#pragma once

#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

inline std::filesystem::path ArgumentPath([[maybe_unused]] char** argv, int index) {
#ifdef _WIN32
    int count = 0;
    const auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) throw std::runtime_error("Cannot read the command line");
    const std::filesystem::path path = index < count ? arguments[index] : L"";
    LocalFree(arguments);
    return path;
#else
    return argv[index];
#endif
}
