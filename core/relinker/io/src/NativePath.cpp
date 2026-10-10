#include <io/NativePath.hpp>
#include <domain/Types.hpp>

#ifdef _WIN32
#include <windows.h>
#include <utility>
#endif

namespace Io {

#ifdef _WIN32

std::filesystem::path NativePath(const std::string& path) {
    if (path.empty()) return {};
    const int wideSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0);
    if (wideSize <= 0) throw Domain::RelinkerException("Path is not valid UTF-8: " + path);
    std::wstring wide(static_cast<std::size_t>(wideSize), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), wide.data(), wideSize) != wideSize)
        throw Domain::RelinkerException("Path is not valid UTF-8: " + path);
    return std::filesystem::path(std::move(wide));
}

std::string Utf8Path(const std::filesystem::path& path) {
    const std::wstring wide = path.wstring();
    if (wide.empty()) return {};
    const int utf8Size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) throw Domain::RelinkerException("Path cannot be encoded as UTF-8");
    std::string utf8(static_cast<std::size_t>(utf8Size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), utf8.data(), utf8Size, nullptr, nullptr) != utf8Size)
        throw Domain::RelinkerException("Path cannot be encoded as UTF-8");
    return utf8;
}

#else

std::filesystem::path NativePath(const std::string& path) {
    return std::filesystem::path(path);
}

std::string Utf8Path(const std::filesystem::path& path) {
    return path.string();
}

#endif

}
