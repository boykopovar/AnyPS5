#include "prx/libc/include/GuestAllocations.hpp"

#include <Testing/Test.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace GuestImageAnonymousData {

int initialized = 0x5a5a5a5a;

} // namespace GuestImageAnonymousData

namespace {

using GuestImageAnonymousData::initialized;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

class InitializedRestorer {
public:
    InitializedRestorer() = default;
    ~InitializedRestorer() { initialized = 0x5a5a5a5a; }
    InitializedRestorer(const InitializedRestorer&) = delete;
    InitializedRestorer& operator=(const InitializedRestorer&) = delete;
};

const Case generation{"GuestAllocations_Generation_IsNonZero", [] {
    Require(GuestAllocations::GuestAllocationsGeneration_nid_postfix() != 0, "allocation generation");
}};

const Case initializedData{"ImageData_InitializedGlobal_KeepsInitialValueAndIsWritable", [] {
    const InitializedRestorer restorer;
    RequireEqual(initialized, 0x5a5a5a5a, "initial value");
    initialized = 1;
    RequireEqual(initialized, 1, "written value");
}};

const Case mappings{"ImageMappings_PrivateWritable_AreNotBackedByTheExecutable", [] {
    const auto image = std::filesystem::read_symlink("/proc/self/exe").string();
    std::ifstream maps("/proc/self/maps");
    Require(maps.is_open(), "open /proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
        std::istringstream fields(line);
        std::string span, permissions, offset, device, inode, path;
        fields >> span >> permissions >> offset >> device >> inode >> std::ws;
        std::getline(fields, path);
        Require(permissions != "rw-p" || path != image, "file-backed writable image mapping: " + line);
    }
}};

} // namespace
