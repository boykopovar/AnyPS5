#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "LibcInternal runtime hooks: %s\n", message);
        std::abort();
    }
}

}

int main() {
    int locks[8];
    unsigned char info[0x40];
    std::memset(locks, 0x5a, sizeof(locks));
    std::memset(info, 0xa5, sizeof(info));
    int untouchedLocks[8];
    unsigned char untouchedInfo[sizeof(info)];
    std::memcpy(untouchedLocks, locks, sizeof(locks));
    std::memcpy(untouchedInfo, info, sizeof(info));

    libSceLibcInternalInventory_268_nid_no_patch();
    Require(libSceLibcInternalInventory_269_nid_no_patch() == 0, "_malloc_finalize_lv2 must return 0");
    libSceLibcInternalInventory_277_nid_no_patch(locks);
    libSceLibcInternalInventory_278_nid_no_patch("rtld error %d", 1);
    Require(libSceLibcInternalInventory_279_nid_no_patch() == 7, "_rtld_get_stack_prot must return 7");
    Require(libSceLibcInternalInventory_281_nid_no_patch() == 0, "_sceLibcGetMallocParam must return 0");
    sceLibcInternalUnknown6_005_PJWpmO4pj6I_nid_no_patch();
    sceLibcInternalUnknown6_065_1kIpmZX1jw8_nid_no_patch();
    sceLibcInternalUnknown6_082_Sopthb9ztZI_nid_no_patch();
    sceLibcInternalUnknown6_093_R2QKo3hBLkw_nid_no_patch(locks);
    Require(sceLibcInternalUnknown6_098_5FoEpV3Aj0c_nid_no_patch(&locks, info) == 0, "_rtld_addr_phdr must return 0");

    Require(std::memcmp(locks, untouchedLocks, sizeof(locks)) == 0, "an atfork hook wrote the lock array");
    Require(std::memcmp(info, untouchedInfo, sizeof(info)) == 0, "_rtld_addr_phdr wrote the phdr info");
    return 0;
}
