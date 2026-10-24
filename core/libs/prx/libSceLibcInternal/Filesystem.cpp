#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {
int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb);
int APS5_VABI mkdir_nid_postfix(const char* path, std::uint16_t mode);
int APS5_VABI rmdir_nid_postfix(const char* path);
int APS5_VABI unlink_nid_postfix(const char* path);
int APS5_VABI chmod_nid_postfix(const char* path, int mode);
char* APS5_VABI getcwd_nid_postfix(char* buffer, std::size_t size);
void* APS5_VABI opendir_nid_postfix(const char* path);
int APS5_VABI readdir_r_nid_postfix(void* handle, GuestDirectoryEntry* entry, GuestDirectoryEntry** result);
int APS5_VABI closedir_nid_postfix(void* handle);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr std::size_t NameCapacity = 260;
constexpr std::size_t CurrentCapacity = 256;
constexpr int NoSuchFile = 2;
constexpr int Exists = 17;
constexpr int NotDirectory = 20;
constexpr int Invalid = 22;
constexpr int NameTooLong = 63;
constexpr std::uint16_t DefaultDirectoryMode = 0777;
constexpr std::uint16_t PermissionBits = 07777;

enum FileType : int {
    TypeNotFound = -1,
    TypeNone = 0,
    TypeRegular = 1,
    TypeDirectory = 2,
    TypeSymlink = 3,
    TypeBlock = 4,
    TypeCharacter = 5,
    TypeFifo = 6,
    TypeSocket = 7,
    TypeUnknown = 8,
};

int& GuestErrno() {
    return *__error_nid_postfix();
}

int TypeOfMode(std::uint16_t mode) {
    switch (mode & 0170000) {
        case 0010000: return TypeFifo;
        case 0020000: return TypeCharacter;
        case 0040000: return TypeDirectory;
        case 0060000: return TypeBlock;
        case 0100000: return TypeRegular;
        case 0120000: return TypeSymlink;
        case 0140000: return TypeSocket;
        default: return TypeUnknown;
    }
}

int StatType(const char* path, int* perms, int* error) {
    GuestErrno() = 0;
    FileStat status{};
    int type;
    if (stat_nid_postfix(path, &status) == 0) {
        GuestErrno() = 0;
        if (perms) *perms = status.st_mode & PermissionBits;
        type = TypeOfMode(status.st_mode);
    } else {
        const int code = GuestErrno();
        type = code == NoSuchFile || code == NotDirectory || code == Invalid || code == NameTooLong ? TypeNotFound : TypeNone;
    }
    if (error) *error = GuestErrno();
    return type;
}

void CopyName(char* target, const char* source) {
    std::size_t length = 0;
    while (length < NameCapacity - 1 && source[length] != '\0') {
        target[length] = source[length];
        ++length;
    }
    target[length] = '\0';
}

bool IsDotEntry(const char* name) {
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

}

extern "C" {

int APS5_VABI _ZNSt10filesystem5_StatEPKcPNS_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return StatType(path, perms, error);
}

int APS5_VABI _ZNSt10filesystem6_LstatEPKcPNS_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return StatType(path, perms, error);
}

std::uint64_t APS5_VABI _ZNSt10filesystem10_File_sizeEPKcPi_nid_postfix(const char* path, int* error) {
    FileStat status{};
    if (stat_nid_postfix(path, &status) != 0) {
        if (error) *error = GuestErrno();
        return UINT64_MAX;
    }
    return static_cast<std::uint64_t>(status.st_size);
}

int APS5_VABI _ZNSt10filesystem16_Last_write_timeEPKcPlS2__nid_postfix(const char* path, std::int64_t* seconds, std::int64_t* nanoseconds) {
    FileStat status{};
    if (stat_nid_postfix(path, &status) != 0) return GuestErrno();
    *seconds = status.st_mtim.tv_sec;
    *nanoseconds = status.st_mtim.tv_nsec;
    return 0;
}

int APS5_VABI _ZNSt10filesystem9_Make_dirEPKcS1__nid_postfix(const char* path, const char* existing) {
    GuestErrno() = 0;
    std::uint16_t mode = DefaultDirectoryMode;
    if (existing) {
        FileStat status{};
        if (stat_nid_postfix(existing, &status) != 0) return -GuestErrno();
        mode = status.st_mode;
    }
    if (mkdir_nid_postfix(path, mode) == 0) return 1;
    if (GuestErrno() == Exists) return 0;
    return -GuestErrno();
}

bool APS5_VABI _ZNSt10filesystem11_Remove_dirEPKc_nid_postfix(const char* path) {
    return rmdir_nid_postfix(path) == 0;
}

int APS5_VABI _ZNSt10filesystem7_UnlinkEPKc_nid_postfix(const char* path) {
    GuestErrno() = 0;
    return unlink_nid_postfix(path) == 0 ? 0 : GuestErrno();
}

int APS5_VABI _ZNSt10filesystem6_ChmodEPKcNS_5permsE_nid_postfix(const char* path, int perms) {
    return chmod_nid_postfix(path, static_cast<std::uint16_t>(perms));
}

bool APS5_VABI _ZNSt10filesystem12_Current_getERA260_c_nid_postfix(char* target) {
    char current[CurrentCapacity];
    const bool found = getcwd_nid_postfix(current, sizeof(current)) != nullptr;
    if (found) CopyName(target, current);
    else target[0] = '\0';
    return found;
}

char* APS5_VABI _ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(char* target, void* handle, int* type) {
    *type = TypeUnknown;
    GuestDirectoryEntry entry{};
    GuestDirectoryEntry* result = nullptr;
    while (readdir_r_nid_postfix(handle, &entry, &result) == 0 && result) {
        if (IsDotEntry(entry.name)) continue;
        CopyName(target, entry.name);
        return target;
    }
    target[0] = '\0';
    return target;
}

void* APS5_VABI _ZNSt10filesystem9_Open_dirERA260_cPKcRiRNS_9file_typeE_nid_postfix(char* target, const char* path, int* error, int* type) {
    GuestErrno() = 0;
    void* handle = opendir_nid_postfix(path);
    if (!handle) {
        *error = GuestErrno();
        return nullptr;
    }
    *error = 0;
    _ZNSt10filesystem9_Read_dirERA260_cPvRNS_9file_typeE_nid_postfix(target, handle, type);
    if (target[0] != '\0') return handle;
    closedir_nid_postfix(handle);
    return nullptr;
}

int APS5_VABI _ZNSt10filesystem10_Close_dirEPv_nid_postfix(void* handle) {
    return closedir_nid_postfix(handle);
}

void APS5_VABI _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathESt10error_code_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
