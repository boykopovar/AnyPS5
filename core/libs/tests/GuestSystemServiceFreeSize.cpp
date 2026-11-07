#include "prx/libSceSystemService/include/SystemService.hpp"

#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint64_t freeSize = 0;
    std::uint64_t totalSize = 0;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, &totalSize) == 0);
    Require(freeSize != 0 && totalSize != 0 && freeSize <= totalSize);
    Require(sceShellCoreUtilGetFreeSizeOfAvContentsTmp() != 0);
    return 0;
}
