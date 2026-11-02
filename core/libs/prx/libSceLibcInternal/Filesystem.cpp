#include "prx/libc/include/GuestFilesystemPrimitives.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI _ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return GuestFilesystem::StatType(path, perms, error);
}

int APS5_VABI _ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return GuestFilesystem::StatType(path, perms, error);
}

std::uint64_t APS5_VABI _ZNSt10filesystem10_File_sizeEPKcPi_nid_postfix(const char* path, int* error) {
    FileStat status{};
    if (stat_nid_postfix(path, &status) != 0) {
        if (error) *error = GuestFilesystem::GuestErrno();
        return UINT64_MAX;
    }
    return static_cast<std::uint64_t>(status.st_size);
}

int APS5_VABI _ZNSt10filesystem16_Last_write_timeEPKcPlS2__nid_postfix(const char* path, std::int64_t* seconds, std::int64_t* nanoseconds) {
    FileStat status{};
    if (stat_nid_postfix(path, &status) != 0) return GuestFilesystem::GuestErrno();
    *seconds = status.st_mtim.tv_sec;
    *nanoseconds = status.st_mtim.tv_nsec;
    return 0;
}

int APS5_VABI _ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(const char* path, const char* existing) {
    GuestFilesystem::GuestErrno() = 0;
    std::uint16_t mode = GuestFilesystem::DefaultDirectoryMode;
    if (existing) {
        FileStat status{};
        if (stat_nid_postfix(existing, &status) != 0) return -GuestFilesystem::GuestErrno();
        mode = status.st_mode;
    }
    if (mkdir_nid_postfix(path, mode) == 0) return 1;
    if (GuestFilesystem::GuestErrno() == GuestFilesystem::Exists) return 0;
    return -GuestFilesystem::GuestErrno();
}

bool APS5_VABI _ZNSt10filesystem11_Remove_dirEPKc_nid_postfix(const char* path) {
    return rmdir_nid_postfix(path) == 0;
}

int APS5_VABI _ZNSt10filesystem7_UnlinkEPKc_nid_postfix(const char* path) {
    GuestFilesystem::GuestErrno() = 0;
    return unlink_nid_postfix(path) == 0 ? 0 : GuestFilesystem::GuestErrno();
}

int APS5_VABI _ZNSt10filesystem6_ChmodEPKcNS_5permsE_nid_postfix(const char* path, int perms) {
    return chmod_nid_postfix(path, static_cast<std::uint16_t>(perms));
}

bool APS5_VABI _ZNSt10filesystem12_Current_getERA260_c_nid_postfix(char* target) {
    return GuestFilesystem::CurrentGet(target);
}

char* APS5_VABI _ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(char* target, void* handle, int* type) {
    return GuestFilesystem::ReadDir(target, handle, type);
}

void* APS5_VABI _ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(char* target, const char* path, int* error, int* type) {
    return GuestFilesystem::OpenDir(target, path, error, type);
}

int APS5_VABI _ZNSt10filesystem10_Close_dirEPv_nid_postfix(void* handle) {
    return GuestFilesystem::CloseDir(handle);
}

}
