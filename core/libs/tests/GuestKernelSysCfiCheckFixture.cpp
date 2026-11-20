#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cstdint>

extern "C" {

std::uint64_t CfiFixtureCalls_nid_no_patch = 0;
std::uint64_t CfiFixtureId_nid_no_patch = 0;
void* CfiFixtureTarget_nid_no_patch = nullptr;
void* CfiFixtureDiag_nid_no_patch = nullptr;
unsigned char CfiFixtureData_nid_no_patch[64] = {1};

#ifdef CFI_FIXTURE_NID
int APS5_VABI CfiFixtureCheck_nid_no_patch(std::uint64_t id, void* target, void* diag) {
#else
int APS5_VABI __cfi_check(std::uint64_t id, void* target, void* diag) {
#endif
    ++CfiFixtureCalls_nid_no_patch;
    CfiFixtureId_nid_no_patch = id;
    CfiFixtureTarget_nid_no_patch = target;
    CfiFixtureDiag_nid_no_patch = diag;
    return 0;
}

#ifdef CFI_FIXTURE_NID
APS5_EXPORT("iSB5fJWVqIU#guest", CfiFixtureCheck_nid_no_patch);
void* CfiFixtureCheckAddress_nid_no_patch() {
    return reinterpret_cast<void*>(&CfiFixtureCheck_nid_no_patch);
}
#else
void* CfiFixtureCheckAddress_nid_no_patch() {
    return reinterpret_cast<void*>(&__cfi_check);
}
#endif

}
