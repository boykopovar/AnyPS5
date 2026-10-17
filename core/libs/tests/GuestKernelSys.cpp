#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#endif
#ifdef __linux__
#include <cerrno>
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI fstatfs_nid_postfix(int, void*);
int APS5_VABI _fstatfs_nid_postfix(int, void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI readlink_nid_postfix(const char*, char*, std::size_t);
std::int64_t APS5_VABI pathconf_nid_postfix(const char*, int);
int APS5_VABI fchdir_nid_postfix(int);
int APS5_VABI chdir_nid_postfix(const char*);
int APS5_VABI chroot_nid_postfix(const char*);
char* APS5_VABI getcwd_nid_postfix(char*, std::size_t);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int APS5_VABI setsid_nid_postfix(void);
int APS5_VABI link_nid_postfix(const char*, const char*);
int APS5_VABI symlink_nid_postfix(const char*, const char*);
int APS5_VABI openat_nid_postfix(int, const char*, int, ...);
int APS5_VABI unlinkat_nid_postfix(int, const char*, int);
std::int64_t APS5_VABI readlinkat_nid_postfix(int, const char*, char*, std::size_t);
int APS5_VABI mkdirat_nid_postfix(int, const char*, std::uint16_t);
int APS5_VABI renameat_nid_postfix(int, const char*, int, const char*);
int APS5_VABI fchmodat_nid_postfix(int, const char*, int, int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::uint16_t APS5_VABI umask_nid_postfix(std::uint16_t);
int* APS5_VABI __error_nid_postfix();
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Kernel sys check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

bool PathconfThrows(const char* path, int name) {
    try {
        pathconf_nid_postfix(path, name);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool ChrootThrows(const std::string& path) {
    try {
        chroot_nid_postfix(path.c_str());
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), path.c_str()) != nullptr;
    }
    return false;
}

void CheckPathconf(const std::filesystem::path& root, const std::string& file) {
    const auto directory = root.generic_string();
    for (const auto& path : {file, directory}) {
        Require(pathconf_nid_postfix(path.c_str(), 1) == 32767);
        Require(pathconf_nid_postfix(path.c_str(), 4) == 255);
        Require(pathconf_nid_postfix(path.c_str(), 5) == 1024);
        Require(pathconf_nid_postfix(path.c_str(), 6) == 512);
        Require(pathconf_nid_postfix(path.c_str(), 7) == 1);
        Require(pathconf_nid_postfix(path.c_str(), 8) == 1);
        Require(pathconf_nid_postfix(path.c_str(), 12) == 64);
        *__error_nid_postfix() = 0;
        Require(pathconf_nid_postfix(path.c_str(), 15) == -1 && Error() == 0);
        Require(pathconf_nid_postfix(path.c_str(), 18) == 1024);
        Require(pathconf_nid_postfix(path.c_str(), 54) == 0);
        Require(pathconf_nid_postfix(path.c_str(), 55) == 0);
    }
    for (const int name : {0, 11, 61, 62, 65, -1})
        Require(pathconf_nid_postfix(file.c_str(), name) == -1 && Error() == 22);
    for (const int name : {2, 3, 9, 10, 14, 16, 17, 21, 53, 59, 60, 63, 64})
        Require(PathconfThrows(file.c_str(), name));
    const auto missing = (root / "missing").string();
    Require(pathconf_nid_postfix(missing.c_str(), 4) == -1 && Error() == 2);
    Require(pathconf_nid_postfix(missing.c_str(), 0) == -1 && Error() == 2);
    Require(pathconf_nid_postfix((root / "missing" / "file").string().c_str(), 4) == -1 && Error() == 2);
    Require(pathconf_nid_postfix("", 4) == -1 && Error() == 2);
    Require(pathconf_nid_postfix(nullptr, 4) == -1 && Error() == 14);
    const auto longName = directory + "/" + std::string(256, 'n');
    Require(pathconf_nid_postfix(longName.c_str(), 4) == -1 && Error() == 63);
    Require(pathconf_nid_postfix((directory + "/missing/" + std::string(256, 'n')).c_str(), 4) == -1 && Error() == 2);
    const auto longPath = directory + "/" + std::string(1024, 'p');
    Require(pathconf_nid_postfix(longPath.c_str(), 4) == -1 && Error() == 63);
}

int main() {
    Require(setsid_nid_postfix() == -1 && Error() == 1);
    Require(umask_nid_postfix(0) == 022);
    Require(umask_nid_postfix(07777) == 0);
    Require(umask_nid_postfix(022) == 07777);
    const auto root = std::filesystem::path("anyps5-kernel-sys-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = (root / "file.txt").string();
    std::ofstream(file).put('x');
    CheckPathconf(root, file);
    const int descriptor = open_nid_postfix(file.c_str(), 0, 0);
    Require(descriptor >= 0);
    unsigned char viaPublic[472] = {};
    unsigned char viaSyscall[472] = {};
    Require(fstatfs_nid_postfix(descriptor, viaPublic) == 0 && _fstatfs_nid_postfix(descriptor, viaSyscall) == 0);
    Require(std::memcmp(viaPublic, viaSyscall, 4) == 0 && std::memcmp(viaPublic + 16, viaSyscall + 16, 8) == 0);
    Require(std::memcmp(viaPublic + 280, viaSyscall + 280, 16) == 0);
    Require(fstatfs_nid_postfix(descriptor, nullptr) == -1 && Error() == 14);
    Require(close_nid_postfix(descriptor) == 0);
    Require(fstatfs_nid_postfix(-1, viaPublic) == -1 && Error() == 9);
    char buffer[64];
    for (const auto& component : {std::string("missing"), std::string("file.txt")}) {
        const auto prefix = root.generic_string() + "/" + component + "/../";
        const auto invalid = prefix + "file.txt";
        const auto created = prefix + "new.txt";
        const int expected = component == "missing" ? 2 : 20;
        Require(readlink_nid_postfix(invalid.c_str(), buffer, sizeof(buffer)) == -1 && Error() == expected);
        Require(link_nid_postfix(invalid.c_str(), created.c_str()) == -1 && Error() == expected);
        Require(link_nid_postfix(file.c_str(), created.c_str()) == -1 && Error() == expected);
        Require(symlink_nid_postfix("file.txt", created.c_str()) == -1 && Error() == expected);
        Require(std::filesystem::hard_link_count(file) == 1 && !std::filesystem::exists(root / "new.txt"));
    }
    Require(readlink_nid_postfix(file.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlink_nid_postfix(root.string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlink_nid_postfix((root / "missing").string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlink_nid_postfix("", buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlink_nid_postfix(nullptr, buffer, sizeof(buffer)) == -1 && Error() == 14);
#ifndef _WIN32
    Require(readlink_nid_postfix((root / std::string(300, 'n')).string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 63);
#endif
    for (const std::size_t length : {std::size_t{1023}, std::size_t{1024}, std::size_t{1025}}) {
        const auto longPath = root.generic_string() + std::string(length - root.generic_string().size(), '/');
        std::memset(buffer, '#', sizeof(buffer));
        const int expected = length < 1024 ? 22 : 63;
        Require(readlink_nid_postfix(longPath.c_str(), buffer, sizeof(buffer)) == -1 && Error() == expected);
        Require(readlinkat_nid_postfix(-100, longPath.c_str(), buffer, sizeof(buffer)) == -1 && Error() == expected);
        Require(buffer[0] == '#');
    }
    const int pathLimitDirectory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(pathLimitDirectory >= 0);
    for (const std::size_t length : {std::size_t{1023}, std::size_t{1024}, std::size_t{1025}}) {
        const auto longPath = std::string(".") + std::string(length - 1, '/');
        const int expected = length < 1024 ? 22 : 63;
        Require(readlinkat_nid_postfix(pathLimitDirectory, longPath.c_str(), buffer, sizeof(buffer)) == -1 && Error() == expected);
        Require(buffer[0] == '#');
    }
    Require(sceKernelClose(pathLimitDirectory) == 0);
    const auto oversizedPath = std::string(1024, '/');
    Require(readlinkat_nid_postfix(-1, oversizedPath.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 63);
    const auto oversizedRelativePath = std::string(".") + std::string(1023, '/');
    Require(readlinkat_nid_postfix(-1, oversizedRelativePath.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 63);
    std::error_code error;
    const auto link = (root / "link").string();
#ifdef _WIN32
    const bool createdLink = CreateSymbolicLinkW(std::filesystem::path(link).wstring().c_str(), L"sub\\target.txt", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
    std::filesystem::create_symlink("sub/target.txt", link, error);
    const bool createdLink = !error;
#endif
    if (createdLink) {
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlink_nid_postfix(link.c_str(), buffer, sizeof(buffer)) == 14);
        Require(std::memcmp(buffer, "sub/target.txt", 14) == 0 && buffer[14] == '#');
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlink_nid_postfix(link.c_str(), buffer, 3) == 3);
        Require(std::memcmp(buffer, "sub", 3) == 0 && buffer[3] == '#');
        Require(readlink_nid_postfix(link.c_str(), nullptr, 0) == 0);
        Require(readlink_nid_postfix(link.c_str(), nullptr, sizeof(buffer)) == -1 && Error() == 14);
        Require(readlink_nid_postfix((root / "LINK").string().c_str(), buffer, sizeof(buffer)) == 14);
        const int linkDir = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
        Require(linkDir >= 0);
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlinkat_nid_postfix(linkDir, "link", buffer, sizeof(buffer)) == 14);
        Require(std::memcmp(buffer, "sub/target.txt", 14) == 0 && buffer[14] == '#');
        Require(readlinkat_nid_postfix(linkDir, "link", buffer, 3) == 3 && std::memcmp(buffer, "sub", 3) == 0);
        Require(readlinkat_nid_postfix(linkDir, "link", nullptr, 0) == 0);
        Require(readlinkat_nid_postfix(linkDir, "link", nullptr, sizeof(buffer)) == -1 && Error() == 14);
        const auto guestLink = "/" + root.filename().string() + "/link";
        Require(readlinkat_nid_postfix(-1, guestLink.c_str(), buffer, sizeof(buffer)) == 14 && std::memcmp(buffer, "sub/target.txt", 14) == 0);
        Require(readlinkat_nid_postfix(-100, link.c_str(), buffer, sizeof(buffer)) == 14 && std::memcmp(buffer, "sub/target.txt", 14) == 0);
        Require(sceKernelClose(linkDir) == 0);
#ifndef _WIN32
        std::filesystem::create_symlink("loopB", root / "loopA");
        std::filesystem::create_symlink("loopA", root / "loopB");
        Require(readlink_nid_postfix((root / "loopA" / "inner").string().c_str(), buffer, sizeof(buffer)) == -1 && Error() == 62);
#endif
    } else {
        std::fprintf(stderr, "symbolic links unavailable: the link cases not tested\n");
    }
    const auto original = (root / "original.bin").string();
    const auto linked = (root / "linked.bin").string();
    std::ofstream(original) << "abc";
    Require(link_nid_postfix(original.c_str(), linked.c_str()) == 0);
    Require(std::filesystem::hard_link_count(original) == 2 && std::filesystem::file_size(linked) == 3);
    std::ofstream(linked, std::ios::trunc).close();
    Require(std::filesystem::file_size(original) == 0);
    Require(link_nid_postfix(original.c_str(), linked.c_str()) == -1 && Error() == 17);
    Require(link_nid_postfix((root / "missing").string().c_str(), (root / "other").string().c_str()) == -1 && Error() == 2);
    Require(link_nid_postfix(original.c_str(), (root / "missing" / "linked.bin").string().c_str()) == -1 && Error() == 2);
    Require(link_nid_postfix(root.string().c_str(), (root / "directory").string().c_str()) == -1 && Error() == 1);
    Require(link_nid_postfix(nullptr, linked.c_str()) == -1 && Error() == 14);
    Require(link_nid_postfix(original.c_str(), "") == -1 && Error() == 2);
    Require(std::filesystem::remove(linked) && std::filesystem::hard_link_count(original) == 1);
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    const int fileDescriptor = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fileDescriptor >= 0);
    Require(fchdir_nid_postfix(-1) == -1 && Error() == 9);
    Require(fchdir_nid_postfix(fileDescriptor) == -1 && Error() == 20);
    Require(fchdir_nid_postfix(directory) == 0);
    char directoryPath[1024];
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath);
    Require(std::strcmp(directoryPath, ("/" + root.filename().string()).c_str()) == 0);
#ifdef __linux__
    Require(chdir_nid_postfix("/") == 0);
    const auto originalPermissions = std::filesystem::status(root).permissions();
    std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    const bool searchDenied = ::access(root.c_str(), X_OK) == -1 && errno == EACCES;
    const int deniedResult = searchDenied ? fchdir_nid_postfix(directory) : 0;
    const int deniedError = Error();
    const bool cwdUnchanged = getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && std::strcmp(directoryPath, "/") == 0;
    std::filesystem::permissions(root, std::filesystem::perms::owner_exec);
    const int searchOnlyResult = fchdir_nid_postfix(directory);
    std::filesystem::permissions(root, originalPermissions);
    if (searchDenied) Require(deniedResult == -1 && deniedError == EACCES && cwdUnchanged);
    Require(searchOnlyResult == 0);
#endif
    Require(sceKernelClose(fileDescriptor) == 0);
    Require(sceKernelClose(directory) == 0);
    Require(fchdir_nid_postfix(directory) == -1 && Error() == 9);
    Require(chdir_nid_postfix("/") == 0);
    Require(chroot_nid_postfix(nullptr) == -1 && Error() == 14);
    Require(chroot_nid_postfix("") == -1 && Error() == 2);
    Require(chroot_nid_postfix((root / "missing").string().c_str()) == -1 && Error() == 2);
    Require(chroot_nid_postfix(file.c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((root / "file.txt" / "child").string().c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((file + "/").c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((root / "file.txt" / "..").string().c_str()) == -1 && Error() == 20);
    Require(chroot_nid_postfix((root / std::string(256, 'n')).string().c_str()) == -1 && Error() == 63);
    Require(chroot_nid_postfix(("/" + std::string(1024, 'p')).c_str()) == -1 && Error() == 63);
    Require(ChrootThrows(root.string()));
    Require(ChrootThrows("/" + root.filename().string() + "/."));
    const auto guestRoot = "/" + root.filename().string();
    const auto chrootFile = guestRoot + "/file.txt";
    const int top = sceKernelOpen("/", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(top >= 0);
    Require(chdir_nid_postfix(guestRoot.c_str()) == 0);
    Require(fchdir_nid_postfix(top) == 0 && chroot_nid_postfix(".") == 0);
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && std::strcmp(directoryPath, "/") == 0);
    Require(sceKernelClose(top) == 0);
    Require(chroot_nid_postfix("/") == 0 && chroot_nid_postfix("/..") == 0 && chroot_nid_postfix("//./") == 0);
    Require(chdir_nid_postfix(guestRoot.c_str()) == 0);
    Require(chroot_nid_postfix("..") == 0 && chroot_nid_postfix("../..") == 0);
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && guestRoot == directoryPath);
    Require(ChrootThrows("."));
    for (const auto& path : {std::string("file.txt"), chrootFile, "/../../.." + chrootFile, "../.." + chrootFile}) {
        const int opened = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
        char byte = 0;
        Require(opened >= 0 && sceKernelRead(opened, &byte, 1) == 1 && byte == 'x');
        Require(sceKernelClose(opened) == 0);
    }
    Require(chdir_nid_postfix("/") == 0);
    Require(getcwd_nid_postfix(directoryPath, sizeof(directoryPath)) == directoryPath && std::strcmp(directoryPath, "/") == 0);
    Require(readlink_nid_postfix(file.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    const auto symbolic = (root / "symbolic").string();
    const int created = symlink_nid_postfix("original.bin", symbolic.c_str());
    if (created == 0) {
        const auto dangling = (root / "dangling").string();
#ifdef _WIN32
        Require((GetFileAttributesW(std::filesystem::path(symbolic).wstring().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        Require(symlink_nid_postfix("original.bin", symbolic.c_str()) == -1 && Error() == 17);
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == 0);
        Require((GetFileAttributesW(std::filesystem::path(dangling).wstring().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == -1 && Error() == 17);
        Require(readlink_nid_postfix(symbolic.c_str(), buffer, sizeof(buffer)) == 12 && std::memcmp(buffer, "original.bin", 12) == 0);
        Require(readlink_nid_postfix(dangling.c_str(), buffer, sizeof(buffer)) == 11 && std::memcmp(buffer, "missing.bin", 11) == 0);
        const auto nested = (root / "nested").string();
        Require(symlink_nid_postfix("sub/target.txt", nested.c_str()) == 0);
        std::memset(buffer, '#', sizeof(buffer));
        Require(readlink_nid_postfix(nested.c_str(), buffer, sizeof(buffer)) == 14);
        Require(std::memcmp(buffer, "sub/target.txt", 14) == 0 && buffer[14] == '#');
        Require(readlink_nid_postfix(nested.c_str(), buffer, 3) == 3 && std::memcmp(buffer, "sub", 3) == 0);
        Require(readlink_nid_postfix(nested.c_str(), nullptr, 0) == 0);
        Require(readlink_nid_postfix(nested.c_str(), nullptr, sizeof(buffer)) == -1 && Error() == 14);
        const auto accented = (root / "accented").string();
        Require(symlink_nid_postfix("a\xc3\xa7\xc3\xa3o.bin", accented.c_str()) == 0);
        Require(readlink_nid_postfix(accented.c_str(), buffer, sizeof(buffer)) == 10 && std::memcmp(buffer, "a\xc3\xa7\xc3\xa3o.bin", 10) == 0);
        Require(std::filesystem::create_directory(root / "sub"));
        const auto directoryLink = (root / "directory-link").string();
        Require(symlink_nid_postfix("sub", directoryLink.c_str()) == 0);
        Require(readlink_nid_postfix(directoryLink.c_str(), buffer, sizeof(buffer)) == 3 && std::memcmp(buffer, "sub", 3) == 0);
        const auto absolute = std::filesystem::absolute(root / "absolute");
        Require(CreateSymbolicLinkW(absolute.wstring().c_str(), std::filesystem::absolute(original).wstring().c_str(),
            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        bool refused = false;
        try {
            readlink_nid_postfix((root / "absolute").string().c_str(), buffer, sizeof(buffer));
        } catch (const std::runtime_error& failure) {
            refused = std::string(failure.what()) == "readlink of a Windows reparse point other than a relative symbolic link not implemented";
        }
        Require(refused);
#else
        Require(std::filesystem::is_symlink(std::filesystem::symlink_status(symbolic)));
        Require(readlink_nid_postfix(symbolic.c_str(), buffer, sizeof(buffer)) == 12 && std::memcmp(buffer, "original.bin", 12) == 0);
        Require(std::filesystem::file_size(symbolic) == std::filesystem::file_size(original));
        Require(symlink_nid_postfix("original.bin", symbolic.c_str()) == -1 && Error() == 17);
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == 0);
        Require(readlink_nid_postfix(dangling.c_str(), buffer, sizeof(buffer)) == 11 && !std::filesystem::exists(dangling));
        Require(symlink_nid_postfix("missing.bin", dangling.c_str()) == -1 && Error() == 17);
#endif
    } else {
        Require(created == -1 && Error() == 1);
        std::fprintf(stderr, "symbolic links unavailable: the symlink success cases not tested\n");
    }
    Require(symlink_nid_postfix("original.bin", (root / "missing" / "symbolic").string().c_str()) == -1 && Error() == 2);
    Require(symlink_nid_postfix(nullptr, symbolic.c_str()) == -1 && Error() == 14);
    Require(symlink_nid_postfix("original.bin", "") == -1 && Error() == 2);
    const int base = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(base >= 0);
    const int relative = openat_nid_postfix(base, "file.txt", SCE_KERNEL_O_RDONLY);
    char byte = 0;
    Require(relative >= 0 && sceKernelRead(relative, &byte, 1) == 1 && byte == 'x');
    Require(sceKernelClose(relative) == 0);
    const int made = openat_nid_postfix(base, "made.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644);
    Require(made >= 0 && std::filesystem::is_regular_file(root / "made.bin"));
    Require(sceKernelClose(made) == 0);
    Require(openat_nid_postfix(base, "made.bin", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644) == -1 && Error() == 17);
    Require(openat_nid_postfix(base, "missing.bin", SCE_KERNEL_O_RDONLY) == -1 && Error() == 2);
    const auto guestFile = "/" + root.filename().string() + "/file.txt";
    const int absolute = openat_nid_postfix(-1, guestFile.c_str(), SCE_KERNEL_O_RDONLY);
    Require(absolute >= 0 && sceKernelClose(absolute) == 0);
    const int current = openat_nid_postfix(-100, file.c_str(), SCE_KERNEL_O_RDONLY);
    Require(current >= 0 && sceKernelClose(current) == 0);
    const int plain = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(plain >= 0);
    Require(openat_nid_postfix(plain, "file.txt", SCE_KERNEL_O_RDONLY) == -1 && Error() == 20);
    Require(readlinkat_nid_postfix(base, "file.txt", buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(base, "missing.bin", buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlinkat_nid_postfix(plain, "file.txt", buffer, sizeof(buffer)) == -1 && Error() == 20);
    Require(readlinkat_nid_postfix(-1, "file.txt", buffer, sizeof(buffer)) == -1 && Error() == 9);
    Require(readlinkat_nid_postfix(base, nullptr, buffer, sizeof(buffer)) == -1 && Error() == 14);
    Require(readlinkat_nid_postfix(base, "", buffer, sizeof(buffer)) == -1 && Error() == 2);
    Require(readlinkat_nid_postfix(-1, guestFile.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(readlinkat_nid_postfix(-100, file.c_str(), buffer, sizeof(buffer)) == -1 && Error() == 22);
    Require(mkdirat_nid_postfix(base, "dir", 0700) == 0 && std::filesystem::is_directory(root / "dir"));
    Require(mkdirat_nid_postfix(base, "dir", 0700) == -1 && Error() == 17);
    Require(mkdirat_nid_postfix(base, "file.txt", 0700) == -1 && Error() == 17);
    Require(mkdirat_nid_postfix(base, "missing/child", 0700) == -1 && Error() == 2);
    Require(mkdirat_nid_postfix(base, "file.txt/child", 0700) == -1 && Error() == 20);
    Require(mkdirat_nid_postfix(base, "slash_dir/", 0700) == 0 && std::filesystem::is_directory(root / "slash_dir"));
    const auto guestSubdir = "/" + root.filename().string() + "/abs_dir";
    Require(mkdirat_nid_postfix(-1, guestSubdir.c_str(), 0700) == 0 && std::filesystem::is_directory(root / "abs_dir"));
    const auto cwdDir = (root / "cwd_dir").string();
    Require(mkdirat_nid_postfix(-100, cwdDir.c_str(), 0700) == 0 && std::filesystem::is_directory(root / "cwd_dir"));
    Require(mkdirat_nid_postfix(plain, "child", 0700) == -1 && Error() == 20);
    Require(mkdirat_nid_postfix(-1, "child", 0700) == -1 && Error() == 9);
    Require(mkdirat_nid_postfix(base, nullptr, 0700) == -1 && Error() == 14);
    Require(mkdirat_nid_postfix(base, "", 0700) == -1 && Error() == 2);
    const std::string longName(256, 'a');
    Require(mkdirat_nid_postfix(base, longName.c_str(), 0700) == -1 && Error() == 63);
    Require(mkdirat_nid_postfix(base, "dir/../dotdot_dir", 0700) == 0 && std::filesystem::is_directory(root / "dotdot_dir"));
    Require(unlinkat_nid_postfix(plain, "file.txt", 0) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(-1, "file.txt", 0) == -1 && Error() == 9);
    Require(sceKernelClose(plain) == 0);
    Require(unlinkat_nid_postfix(base, "file.txt", 1) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, nullptr, 1) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, nullptr, 0) == -1 && Error() == 14);
    Require(unlinkat_nid_postfix(base, "", 0) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "missing.bin", 0) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "missing.bin", 0x800) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "missing/child", 0) == -1 && Error() == 2);
    Require(unlinkat_nid_postfix(base, "file.txt/child", 0) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, "file.txt/child", 0x800) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, "file.txt/", 0) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, longName.c_str(), 0) == -1 && Error() == 63);
    Require(unlinkat_nid_postfix(base, "file.txt", 0x800) == -1 && Error() == 20);
    Require(unlinkat_nid_postfix(base, ".", 0x800) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, "..", 0x800) == -1 && Error() == 22);
    Require(unlinkat_nid_postfix(base, ".", 0) == -1 && Error() == 1);
    std::filesystem::create_directory(root / "usub");
    Require(unlinkat_nid_postfix(base, "usub", 0) == -1 && Error() == 1);
    std::filesystem::create_directory(root / "nonempty");
    { std::ofstream out(root / "nonempty" / "f.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(base, "nonempty", 0x800) == -1 && Error() == 66);
    Require(unlinkat_nid_postfix(base, "nonempty/f.bin", 0) == 0 && !std::filesystem::exists(root / "nonempty" / "f.bin"));
    Require(unlinkat_nid_postfix(base, "nonempty", 0x800) == 0 && !std::filesystem::exists(root / "nonempty"));
    { std::ofstream out(root / "to_unlink.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(base, "to_unlink.bin", 0) == 0 && !std::filesystem::exists(root / "to_unlink.bin"));
    { std::ofstream out(root / "dotdot.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(base, "usub/../dotdot.bin", 0) == 0 && !std::filesystem::exists(root / "dotdot.bin"));
    std::filesystem::create_directory(root / "uslash_sub");
    Require(unlinkat_nid_postfix(base, "uslash_sub/", 0x800) == 0 && !std::filesystem::exists(root / "uslash_sub"));
    Require(unlinkat_nid_postfix(base, "usub", 0x800) == 0 && !std::filesystem::exists(root / "usub"));
    { std::ofstream out(root / "abs.bin"); out << 'x'; }
    const auto guestAbsFile = "/" + root.filename().string() + "/abs.bin";
    Require(unlinkat_nid_postfix(-1, guestAbsFile.c_str(), 0) == 0 && !std::filesystem::exists(root / "abs.bin"));
    std::filesystem::create_directory(root / "unlink_abs_dir");
    const auto guestAbsDir = "/" + root.filename().string() + "/unlink_abs_dir";
    Require(unlinkat_nid_postfix(-1, guestAbsDir.c_str(), 0x800) == 0 && !std::filesystem::exists(root / "unlink_abs_dir"));
    { std::ofstream out(root / "cwd.bin"); out << 'x'; }
    Require(unlinkat_nid_postfix(-100, (root / "cwd.bin").string().c_str(), 0) == 0 && !std::filesystem::exists(root / "cwd.bin"));
    std::filesystem::create_directory(root / "unlink_cwd_dir");
    Require(unlinkat_nid_postfix(-100, (root / "unlink_cwd_dir").string().c_str(), 0x800) == 0 && !std::filesystem::exists(root / "unlink_cwd_dir"));
    Require(openat_nid_postfix(-1, "file.txt", SCE_KERNEL_O_RDONLY) == -1 && Error() == 9);
    Require(openat_nid_postfix(base, nullptr, SCE_KERNEL_O_RDONLY) == -1 && Error() == 14);
    Require(openat_nid_postfix(base, "", SCE_KERNEL_O_RDONLY) == -1 && Error() == 2);
    Require(renameat_nid_postfix(base, nullptr, base, "renamed.txt") == -1 && Error() == 14);
    Require(renameat_nid_postfix(base, "made.bin", base, nullptr) == -1 && Error() == 14);
    Require(renameat_nid_postfix(base, "", base, "renamed.txt") == -1 && Error() == 2);
    Require(renameat_nid_postfix(base, "made.bin", base, "") == -1 && Error() == 2);
    const int bad = sceKernelOpen(file.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(bad >= 0);
    Require(renameat_nid_postfix(-1, "made.bin", base, "renamed.txt") == -1 && Error() == 9);
    Require(renameat_nid_postfix(base, "made.bin", -1, "renamed.txt") == -1 && Error() == 9);
    Require(renameat_nid_postfix(bad, "made.bin", base, "renamed.txt") == -1 && Error() == 20);
    Require(renameat_nid_postfix(base, "made.bin", bad, "renamed.txt") == -1 && Error() == 20);
    Require(sceKernelClose(bad) == 0);
    const auto longPart = std::string(256, 'y');
    Require(renameat_nid_postfix(base, longPart.c_str(), base, "renamed.txt") == -1 && Error() == 63);
    Require(renameat_nid_postfix(base, "made.bin", base, longPart.c_str()) == -1 && Error() == 63);
    Require(renameat_nid_postfix(base, ".", base, "renamed.txt") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "..", base, "renamed.txt") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "made.bin", base, ".") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "made.bin", base, "..") == -1 && Error() == 22);
    Require(renameat_nid_postfix(base, "missing.bin", base, "renamed.txt") == -1 && Error() == 2);
    Require(renameat_nid_postfix(base, "made.bin", base, "renamed.bin") == 0);
    Require(!std::filesystem::exists(root / "made.bin"));
    Require(std::filesystem::is_regular_file(root / "renamed.bin"));
    Require(renameat_nid_postfix(base, "renamed.bin", base, "renamed.bin") == 0);
    const auto subPath = root / "sub_dir";
    std::filesystem::create_directory(subPath);
    Require(renameat_nid_postfix(base, "renamed.bin", base, "sub_dir") == -1 && Error() == 21);
    Require(renameat_nid_postfix(base, "sub_dir", base, "renamed.bin") == -1 && Error() == 20);
    Require(renameat_nid_postfix(base, "sub_dir", base, "sub_renamed") == 0);
    Require(!std::filesystem::exists(subPath));
    Require(std::filesystem::is_directory(root / "sub_renamed"));
    const int subFd = sceKernelOpen((root / "sub_renamed").string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(subFd >= 0);
    Require(renameat_nid_postfix(base, "renamed.bin", subFd, "nested.bin") == 0);
    Require(!std::filesystem::exists(root / "renamed.bin"));
    Require(std::filesystem::is_regular_file(root / "sub_renamed" / "nested.bin"));
    Require(renameat_nid_postfix(base, "sub_renamed", subFd, "child") == -1 && Error() == 22);
    const auto otherDir = root / "other_dir";
    std::filesystem::create_directory(otherDir);
    { std::ofstream stream(otherDir / "item.txt"); }
    Require(renameat_nid_postfix(base, "sub_renamed", base, "other_dir") == -1 && Error() == 66);
    Require(renameat_nid_postfix(subFd, "nested.bin/", base, "back.bin") == -1 && Error() == 20);
    Require(renameat_nid_postfix(subFd, "nested.bin", base, "nonexistent/") == -1 && Error() == 20);
    Require(renameat_nid_postfix(subFd, "nested.bin", base, "made.bin") == 0);
    Require(std::filesystem::is_regular_file(root / "made.bin"));
    Require(sceKernelClose(subFd) == 0);
    Require(renameat_nid_postfix(-100, (root / "made.bin").string().c_str(), -100, (root / "final.bin").string().c_str()) == 0);
    Require(std::filesystem::is_regular_file(root / "final.bin"));
    Require(fchmodat_nid_postfix(base, "file.txt", 0600, 0x0200) == 0);
    Require(fchmodat_nid_postfix(-100, (root / "file.txt").string().c_str(), 0644, 0) == 0);
#ifndef _WIN32
    Require((std::filesystem::status(root / "file.txt").permissions() & std::filesystem::perms::all) ==
        (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read | std::filesystem::perms::others_read));
#endif
    Require(fchmodat_nid_postfix(base, "missing.bin", 0600, 0) == -1 && Error() == 2);
    Require(fchmodat_nid_postfix(base, nullptr, 0600, 0) == -1 && Error() == 14);
    Require(fchmodat_nid_postfix(base, "", 0600, 0) == -1 && Error() == 2);
    Require(fchmodat_nid_postfix(base, "file.txt", 0600, 0x4000) == -1 && Error() == 22);
    Require(fchmodat_nid_postfix(-1, "file.txt", 0600, 0) == -1 && Error() == 9);
#ifndef _WIN32
    Require(symlink_nid_postfix("loop", (root / "loop").string().c_str()) == 0);
    Require(fchmodat_nid_postfix(base, "loop/x", 0600, 0) == -1 && Error() == 62);
    Require(fchmodat_nid_postfix(base, longName.c_str(), 0600, 0) == -1 && Error() == 63);
#endif
    Require(sceKernelClose(base) == 0);
#ifndef _WIN32
    if (created == 0) {
        const auto throughLink = (root / "through-link.bin").string();
        Require(link_nid_postfix(symbolic.c_str(), throughLink.c_str()) == 0);
        Require(!std::filesystem::is_symlink(std::filesystem::symlink_status(throughLink)));
        Require(std::filesystem::equivalent(throughLink, original) && std::filesystem::hard_link_count(original) == 2);
        Require(std::filesystem::remove(throughLink) && std::filesystem::hard_link_count(original) == 1);
        Require(link_nid_postfix((root / "dangling").string().c_str(), throughLink.c_str()) == -1 && Error() == 2);
        const auto directoryLink = (root / "directory-link").string();
        Require(symlink_nid_postfix(".", directoryLink.c_str()) == 0);
        Require(link_nid_postfix(directoryLink.c_str(), throughLink.c_str()) == -1 && Error() == 1);
        Require(!std::filesystem::exists(std::filesystem::symlink_status(throughLink)));
    }
#endif
    std::filesystem::remove_all(root);
    return 0;
}
