#pragma once

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace GuestFilesystem {

inline constexpr std::size_t NameCapacity = 260;
inline constexpr std::size_t CurrentCapacity = 256;
inline constexpr int NoSuchFile = 2;
inline constexpr int Exists = 17;
inline constexpr int NotDirectory = 20;
inline constexpr int Invalid = 22;
inline constexpr int NameTooLong = 63;
inline constexpr std::uint16_t DefaultDirectoryMode = 0777;
inline constexpr std::uint16_t PermissionBits = 07777;

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

inline int& GuestErrno() {
    return *__error_nid_postfix();
}

inline int TypeOfMode(std::uint16_t mode) {
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

inline int StatType(const char* path, int* perms, int* error) {
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

inline void CopyName(char* target, const char* source) {
    std::size_t length = 0;
    while (length < NameCapacity - 1 && source[length] != '\0') {
        target[length] = source[length];
        ++length;
    }
    target[length] = '\0';
}

inline bool IsDotEntry(const char* name) {
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

inline bool CurrentGet(char* target) {
    char current[CurrentCapacity];
    const bool found = getcwd_nid_postfix(current, sizeof(current)) != nullptr;
    if (found) CopyName(target, current);
    else target[0] = '\0';
    return found;
}

inline char* ReadDir(char* target, void* handle, int* type) {
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

inline void* OpenDir(char* target, const char* path, int* error, int* type) {
    GuestErrno() = 0;
    void* handle = opendir_nid_postfix(path);
    if (!handle) {
        *error = GuestErrno();
        return nullptr;
    }
    *error = 0;
    ReadDir(target, handle, type);
    if (target[0] != '\0') return handle;
    closedir_nid_postfix(handle);
    return nullptr;
}

inline int CloseDir(void* handle) {
    return closedir_nid_postfix(handle);
}

}
