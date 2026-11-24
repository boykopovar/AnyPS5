#include "prx/libSceSystemService/include/SystemService.hpp"

#include <cstdint>
#include <cerrno>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint64_t freeSize = 0;
    std::uint64_t totalSize = 0;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, &totalSize) == 0);
    errno = 71;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(reinterpret_cast<std::uint64_t*>(0x10), &totalSize) == static_cast<int>(0x80e30005u));
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, reinterpret_cast<std::uint64_t*>(0x10)) == static_cast<int>(0x80e30005u));
    Require(errno == 71);
    Require(freeSize != 0 && totalSize != 0 && freeSize <= totalSize);
    Require(sceShellCoreUtilGetFreeSizeOfAvContentsTmp() != 0);
    return 0;
}
