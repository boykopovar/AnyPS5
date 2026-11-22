#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>

extern "C" {

int APS5_VABI _ZNSt10filesystem5_StatEPKcPNS_5permsE_nid_postfix(const char* path, int* perms) {
    return _ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(path, perms, nullptr);
}

int APS5_VABI _ZNSt10filesystem6_LstatEPKcPNS_5permsE_nid_postfix(const char* path, int* perms) {
    return _ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix(path, perms, nullptr);
}

std::uint64_t APS5_VABI _ZNSt10filesystem10_File_sizeEPKc_nid_postfix(const char* path) {
    return _ZNSt10filesystem10_File_sizeEPKcPi_nid_postfix(path, nullptr);
}

std::int64_t APS5_VABI _ZNSt10filesystem16_Last_write_timeEPKc_nid_postfix(const char* path) {
    std::int64_t seconds = 0;
    std::int64_t nanoseconds = 0;
    return _ZNSt10filesystem16_Last_write_timeEPKcPlS2__nid_postfix(path, &seconds, &nanoseconds) != 0 ? -1 : seconds;
}

bool APS5_VABI _ZNSt10filesystem20_Set_last_write_timeEPKcl_nid_postfix(const char* path, std::int64_t microseconds) {
    return _ZNSt8__sce_v210filesystem20_Set_last_write_timeEPKcl_nid_postfix(path, microseconds);
}

int APS5_VABI _ZNSt10filesystem7_RenameEPKcS1__nid_postfix(const char* from, const char* to) {
    return _ZNSt8__sce_v210filesystem7_RenameEPKcS2__nid_postfix(from, to);
}

int APS5_VABI _ZNSt10filesystem7_ResizeEPKcm_nid_postfix(const char* path, std::uint64_t size) {
    return _ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix(path, size);
}

int APS5_VABI _ZNSt10filesystem10_Copy_fileEPKcS1__nid_postfix(const char* from, const char* to) {
    return _ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix(from, to);
}

int APS5_VABI _ZNSt10filesystem8_StatvfsEPKcRNS_10space_infoE_nid_postfix(const char* path, std::uint64_t* space) {
    return _ZNSt8__sce_v210filesystem8_StatvfsEPKcRNS0_10space_infoE_nid_postfix(path, space);
}

int APS5_VABI _ZNSt10filesystem13_Current_set2EPKc_nid_postfix(const char* path) {
    return _ZNSt8__sce_v210filesystem13_Current_set2EPKc_nid_postfix(path);
}

}
