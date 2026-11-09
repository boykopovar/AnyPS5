#include "prx/libc/include/GuestAllocations.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

int initialized = 0x5a5a5a5a;

int main() {
    Require(GuestAllocations::GuestAllocationsGeneration_nid_postfix() != 0);
    Require(initialized == 0x5a5a5a5a);
    initialized = 1;
    Require(initialized == 1);
    const auto image = std::filesystem::read_symlink("/proc/self/exe").string();
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
        std::istringstream fields(line);
        std::string span, permissions, offset, device, inode, path;
        fields >> span >> permissions >> offset >> device >> inode >> std::ws;
        std::getline(fields, path);
        Require(permissions != "rw-p" || path != image);
    }
    return 0;
}
