#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTEXECUTABLE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTEXECUTABLE_HPP

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace HostExecutable {

template <typename Fail>
std::string ConvertedName(std::string_view guest, Fail fail) {
    constexpr std::string_view root = "/app0/";
    if (!guest.starts_with(root)) fail("executables outside /app0 (" + std::string(guest) + ")");
    std::string_view relative = guest.substr(root.size());
    std::string name;
    while (true) {
        const std::size_t separator = relative.find('/');
        const std::string_view component = relative.substr(0, separator);
        if (component.empty() || component == "." || component == "..") fail("a path with an empty, . or .. component (" + std::string(guest) + ")");
        name += (name.empty() ? "" : ".") + std::string(component);
        if (separator == std::string_view::npos) return name;
        relative.remove_prefix(separator + 1);
    }
}

inline std::filesystem::path Own(const char* function) {
#if defined(_WIN32)
    std::wstring own(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, own.data(), static_cast<DWORD>(own.size()));
    if (length == 0 || length >= own.size()) throw std::system_error(GetLastError(), std::system_category(), std::string(function) + ": GetModuleFileNameW");
    own.resize(length);
    return std::filesystem::path(own);
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    std::filesystem::path own;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) own = std::filesystem::canonical(executable.data(), error);
    if (own.empty()) throw std::runtime_error(std::string(function) + ": cannot locate the executable");
    return own;
#else
    std::error_code error;
    auto own = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) throw std::system_error(error, std::string(function) + ": cannot read /proc/self/exe");
    return own;
#endif
}

#ifdef _WIN32
inline std::wstring Wide(const std::string& text, const char* function) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error(std::string(function) + ": argument is not valid UTF-8");
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

inline std::wstring QuotedArgument(const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
    std::wstring quoted = L"\"";
    for (std::size_t index = 0;; ++index) {
        std::size_t backslashes = 0;
        while (index < argument.size() && argument[index] == L'\\') {
            ++backslashes;
            ++index;
        }
        if (index == argument.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        quoted.append(argument[index] == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        quoted.push_back(argument[index]);
    }
    quoted.push_back(L'"');
    return quoted;
}
#endif

}

#endif
