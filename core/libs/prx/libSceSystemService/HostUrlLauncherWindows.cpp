#include "prx/libSceSystemService/HostUrl.hpp"
#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include <string>
#include <utility>
#include <windows.h>
#include <shellapi.h>

HostBrowserLauncher::HostBrowserLauncher() = default;

HostBrowserLauncher::HostBrowserLauncher(std::string program) : program(std::move(program)) {}

bool HostBrowserLauncher::Launch(const std::string& url) {
    if (ValidateHostUrl(url) != HostUrlVerdict::Ok) return false;
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.c_str(), static_cast<int>(url.size()), nullptr, 0);
    if (length <= 0) return false;
    std::wstring address(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.c_str(), static_cast<int>(url.size()), address.data(), length) != length) return false;
    const auto result = reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr, L"open", address.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}
