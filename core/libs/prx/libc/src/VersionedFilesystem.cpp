#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestFilesystemPrimitives.hpp"

#include <cerrno>
#include <filesystem>
#include <system_error>

namespace {

GuestFilesystem::FileType FromNativeType(std::filesystem::file_type type) {
    switch (type) {
        case std::filesystem::file_type::none: return GuestFilesystem::TypeNone;
        case std::filesystem::file_type::not_found: return GuestFilesystem::TypeNotFound;
        case std::filesystem::file_type::regular: return GuestFilesystem::TypeRegular;
        case std::filesystem::file_type::directory: return GuestFilesystem::TypeDirectory;
        case std::filesystem::file_type::symlink: return GuestFilesystem::TypeSymlink;
        case std::filesystem::file_type::block: return GuestFilesystem::TypeBlock;
        case std::filesystem::file_type::character: return GuestFilesystem::TypeCharacter;
        case std::filesystem::file_type::fifo: return GuestFilesystem::TypeFifo;
        case std::filesystem::file_type::socket: return GuestFilesystem::TypeSocket;
        default: return GuestFilesystem::TypeUnknown;
    }
}

bool IsNotFound(int code) {
    return code == GuestFilesystem::NoSuchFile || code == GuestFilesystem::NotDirectory ||
           code == GuestFilesystem::Invalid || code == GuestFilesystem::NameTooLong;
}

int StatType(const char* path, int* perms, int* error) {
    errno = 0;
    std::filesystem::path native;
    int code = ResolvePathError_nid_no_patch(path, native);
    int type;
    if (code != 0) {
        errno = code;
        type = IsNotFound(code) ? GuestFilesystem::TypeNotFound : GuestFilesystem::TypeNone;
    } else {
        std::error_code failure;
        const auto status = std::filesystem::status(native, failure);
        if (!failure && status.type() != std::filesystem::file_type::not_found) {
            errno = 0;
            if (perms) *perms = static_cast<int>(status.permissions()) & GuestFilesystem::PermissionBits;
            type = FromNativeType(status.type());
        } else {
            code = failure ? FilesystemError(failure) : GuestFilesystem::NoSuchFile;
            if (code == GuestFilesystem::NoSuchFile) code = NotFoundError_nid_no_patch(native);
            errno = code;
            type = GuestFilesystem::TypeNotFound;
        }
    }
    if (error) *error = errno;
    return type;
}

}

extern "C" {

int APS5_VABI _ZNSt8__sce_v210filesystem5_StatEPKcPNS0_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return StatType(path, perms, error);
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
