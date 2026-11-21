#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
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
    std::uintptr_t lockInfo[10];
    for (std::size_t index = 0; index < 10; ++index) lockInfo[index] = 0x10 + index;
    std::uintptr_t untouchedLockInfo[10];
    std::memcpy(untouchedLockInfo, lockInfo, sizeof(lockInfo));

    _init_tls();
    Require(_malloc_finalize_lv2() == 0, "_malloc_finalize_lv2 must return 0");
    _rtld_atfork_pre(locks);
    _rtld_error("rtld error %d", 1);
    Require(_rtld_get_stack_prot() == 7, "_rtld_get_stack_prot must return 7");
    libSceLibcInternalInventory_280_nid_no_patch(lockInfo);
    libSceLibcInternalInventory_280_nid_no_patch(nullptr);
    Require(_sceLibcGetMallocParam() == 0, "_sceLibcGetMallocParam must return 0");
    _thread_init();
    _thread_init_stub();
    _malloc_thread_cleanup();
    _rtld_atfork_post(locks);
    Require(_rtld_addr_phdr(&locks, info) == 0, "_rtld_addr_phdr must return 0");

    Require(std::memcmp(locks, untouchedLocks, sizeof(locks)) == 0, "an atfork hook wrote the lock array");
    Require(std::memcmp(info, untouchedInfo, sizeof(info)) == 0, "_rtld_addr_phdr wrote the phdr info");
    Require(std::memcmp(lockInfo, untouchedLockInfo, sizeof(lockInfo)) == 0, "libSceLibcInternalInventory_280_nid_no_patch wrote the lock information");
    return 0;
}
