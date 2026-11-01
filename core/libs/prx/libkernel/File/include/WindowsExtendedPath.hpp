#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSEXTENDEDPATH_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSEXTENDEDPATH_HPP

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

namespace File {

inline std::filesystem::path WindowsNormalizedPath(const std::filesystem::path& path, std::error_code& error, bool* extended = nullptr) {
    error.clear();
    if (extended) *extended = false;
#ifdef _WIN32
    constexpr std::size_t maxPath = 260;
    std::wstring text = path.native();
    std::replace(text.begin(), text.end(), L'/', L'\\');
    if (text.starts_with(L"\\\\?\\") || text.starts_with(L"\\\\.\\")) return path;
    if (text.starts_with(L"\\\\")) {
        const auto serverEnd = text.find(L'\\', 2);
        const auto shareEnd = serverEnd == std::wstring::npos ? std::wstring::npos : text.find(L'\\', serverEnd + 1);
        const std::wstring root = text.substr(0, shareEnd);
        const std::wstring rest = shareEnd == std::wstring::npos ? std::wstring() : std::filesystem::path(text.substr(shareEnd)).lexically_normal().make_preferred().native();
        const std::wstring joined = root + rest;
        if (joined.size() < maxPath) return std::filesystem::path(joined);
        if (extended) *extended = true;
        return std::filesystem::path(L"\\\\?\\UNC" + joined.substr(1));
    }
    const auto absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    auto normalized = absolute.lexically_normal().make_preferred();
    if (normalized.native().size() < maxPath) return normalized;
    if (extended) *extended = true;
    return std::filesystem::path(L"\\\\?\\" + normalized.native());
#else
    return path;
#endif
}

inline std::filesystem::path WindowsExtendedPath(const std::filesystem::path& path) {
#ifdef _WIN32
    constexpr std::size_t maxPath = 260;
    if (path.native().size() < maxPath && path.is_absolute()) return path;
    std::error_code error;
    bool extended = false;
    auto result = WindowsNormalizedPath(path, error, &extended);
    if (error || !extended) return path;
    return result;
#else
    return path;
#endif
}

}  // namespace File

#endif
