#include "prx/libc/include/GuestFilesystemPrimitives.hpp"

#include <cstdio>
#include <cstring>

namespace {

int fakeErrno = 0;
int fakeDirectory = 0;
int readStatus = 0;
const char* entryName = nullptr;
int closeCount = 0;

int Fail(int code, const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return code;
}

}

extern "C" {

int* APS5_VABI __error_nid_postfix() {
    return &fakeErrno;
}

void* APS5_VABI opendir_nid_postfix(const char*) {
    return &fakeDirectory;
}

int APS5_VABI readdir_r_nid_postfix(void* handle, GuestDirectoryEntry* entry, GuestDirectoryEntry** result) {
    if (handle != &fakeDirectory) return 9;
    if (readStatus != 0) return readStatus;
    if (!entryName) {
        *result = nullptr;
        return 0;
    }
    std::strncpy(entry->name, entryName, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entryName = nullptr;
    *result = entry;
    return 0;
}

int APS5_VABI closedir_nid_postfix(void*) {
    ++closeCount;
    return 0;
}

}

int main() {
    char target[GuestFilesystem::NameCapacity];
    int error = -1;
    int type = -1;

    std::memset(target, 'x', sizeof(target));
    readStatus = 5;
    void* handle = GuestFilesystem::OpenDir(target, "dir", &error, &type);
    if (handle) return Fail(10, "OpenDir returned a handle after readdir_r failed");
    if (error != 5) return Fail(11, "OpenDir did not report the readdir_r error");
    if (target[0] != '\0') return Fail(12, "OpenDir left a name after readdir_r failed");
    if (type != GuestFilesystem::TypeUnknown) return Fail(13, "OpenDir type changed after readdir_r failed");
    if (closeCount != 1) return Fail(14, "OpenDir did not close the handle after readdir_r failed");

    error = -1;
    readStatus = 0;
    entryName = nullptr;
    handle = GuestFilesystem::OpenDir(target, "dir", &error, &type);
    if (handle) return Fail(20, "OpenDir returned a handle for an empty directory");
    if (error != 0) return Fail(21, "OpenDir reported an error for an empty directory");
    if (closeCount != 2) return Fail(22, "OpenDir did not close an empty directory");

    error = -1;
    entryName = "save";
    handle = GuestFilesystem::OpenDir(target, "dir", &error, &type);
    if (handle != &fakeDirectory) return Fail(30, "OpenDir lost the handle of a readable directory");
    if (error != 0) return Fail(31, "OpenDir reported an error for a readable directory");
    if (std::strcmp(target, "save") != 0) return Fail(32, "OpenDir returned the wrong first name");
    if (closeCount != 2) return Fail(33, "OpenDir closed a readable directory");

    std::memset(target, 'x', sizeof(target));
    readStatus = 5;
    if (GuestFilesystem::ReadDir(target, &fakeDirectory, &type) != target) return Fail(40, "ReadDir returned another buffer");
    if (target[0] != '\0') return Fail(41, "ReadDir left a name after readdir_r failed");
    return 0;
}
