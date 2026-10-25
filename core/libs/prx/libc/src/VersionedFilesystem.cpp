#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestFilesystemPrimitives.hpp"

extern "C" {

int APS5_VABI _ZNSt8__sce_v210filesystem5_StatEPKcPNS0_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return GuestFilesystem::StatType(path, perms, error);
}

void* APS5_VABI _ZNSt8__sce_v210filesystem9_Open_dirERA260_cPKcRiRNS0_9file_typeE_nid_postfix(char* target, const char* path, int* error, int* type) {
    return GuestFilesystem::OpenDir(target, path, error, type);
}

char* APS5_VABI _ZNSt8__sce_v210filesystem9_Read_dirERA260_cPvRNS0_9file_typeE_nid_postfix(char* target, void* handle, int* type) {
    return GuestFilesystem::ReadDir(target, handle, type);
}

int APS5_VABI _ZNSt8__sce_v210filesystem10_Close_dirEPv_nid_postfix(void* handle) {
    return GuestFilesystem::CloseDir(handle);
}

bool APS5_VABI _ZNSt8__sce_v210filesystem12_Current_getERA260_c_nid_postfix(char* target) {
    target[0] = '/';
    target[1] = '\0';
    return true;
}

void APS5_VABI _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcNS_10error_codeE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcRKNS0_4pathENS_10error_codeE_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
