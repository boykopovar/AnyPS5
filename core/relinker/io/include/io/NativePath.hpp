#ifndef IO_NATIVEPATH_HPP
#define IO_NATIVEPATH_HPP

#include <filesystem>
#include <string>

namespace Io {

std::filesystem::path NativePath(const std::string& path);
std::string Utf8Path(const std::filesystem::path& path);

}

#endif
