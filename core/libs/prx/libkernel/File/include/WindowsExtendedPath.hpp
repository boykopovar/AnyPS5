#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSEXTENDEDPATH_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSEXTENDEDPATH_HPP

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

namespace File {

inline std::filesystem::path WindowsExtendedPath(const std::filesystem::path& path) {
#ifdef _WIN32
    constexpr std::size_t maxPath = 260;
    if (path.native().size() < maxPath) return path;
    std::wstring text = path.native();
    std::replace(text.begin(), text.end(), L'/', L'\\');
    if (text.starts_with(L"\\\\?\\") || text.starts_with(L"\\\\.\\")) return path;
    if (text.starts_with(L"\\\\")) {
        const auto tail = std::filesystem::path(text.substr(1)).lexically_normal().make_preferred().native();
        if (tail.size() + 2 < maxPath) return path;
        return std::filesystem::path(L"\\\\?\\UNC" + tail);
    }
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error) return path;
    const auto normalized = absolute.lexically_normal().make_preferred();
    const auto& native = normalized.native();
    if (native.size() < maxPath) return path;
    return std::filesystem::path(L"\\\\?\\" + native);
#else
    return path;
#endif
}

}  // namespace File

#endif
