#include "prx/libkernel/File/include/Path.hpp"

#include <stdexcept>
#include <string>

namespace File {

std::filesystem::path ResolvePath(const char* path) {
    if (path == nullptr) {
        throw std::invalid_argument("ResolvePath: path is null");
    }
    std::filesystem::path p(path);
    if (p.is_absolute()) {
        auto rel = p.relative_path();
        return std::filesystem::current_path() / rel;
    }
    return std::filesystem::current_path() / p;
}

}
