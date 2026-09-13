#include <stdexcept>
#include <string>
#include <filesystem>

#include "prx/libc/include/General.hpp"

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path) {
    if (path == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    std::string s(path);
    std::size_t start = 0;
    while (start < s.size() && (s[start] == '/' || s[start] == '\\')) {
        ++start;
    }
    std::filesystem::path result = std::filesystem::current_path() / std::filesystem::path(s.substr(start));
    return result;
}

extern "C" void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName) + " not implemented");
}
