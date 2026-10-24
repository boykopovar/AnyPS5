#include <io/NativePath.hpp>
#include <stdexcept>

namespace Io {

std::filesystem::path NativePath(const std::string& path) {
    if (path.find('\0') != std::string::npos)
        throw std::invalid_argument("File path contains a NUL byte");
    const std::u8string utf8(path.begin(), path.end());
    return std::filesystem::path(utf8);
}

std::string Utf8Path(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

}
