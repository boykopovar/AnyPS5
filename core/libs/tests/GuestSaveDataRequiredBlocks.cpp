#include "prx/libSceSaveData/include/InventoryExports.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int RequiredBlocksNull = static_cast<int>(0x809F8004u);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

}

int main() {
    Require(sceSaveDataGetRequiredBlocksForCdlg(nullptr) == RequiredBlocksNull, "a null output was not refused with 0x809f8004");
    std::uint64_t blocks = 0x5a5a5a5a5a5a5a5aull;
    Require(sceSaveDataGetRequiredBlocksForCdlg(&blocks) == 0 && blocks == 0, "the required block count was not 0 before any no-space mount");
    errno = 71;
    Require(sceSaveDataGetRequiredBlocksForCdlg(reinterpret_cast<std::uint64_t*>(0x10)) == RequiredBlocksNull && errno == 71, "an unwritable output was not refused with errno kept");
    std::puts("guest save data required blocks tests passed");
    return 0;
}
