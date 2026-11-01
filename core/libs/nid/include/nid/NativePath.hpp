#ifndef NID_NATIVEPATH_HPP
#define NID_NATIVEPATH_HPP

#include <filesystem>
#include <string>

namespace Nid::Internal {

inline std::filesystem::path NativePath(const std::string& path) {
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

}

#endif
