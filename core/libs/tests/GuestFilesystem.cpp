#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
extern "C" {
int APS5_VABI remove_nid_postfix(const char*);
int APS5_VABI rename_nid_postfix(const char*, const char*);
int APS5_VABI sceKernelChmod_nid_postfix(const char*, unsigned short);
int APS5_VABI sceKernelFchmod(int, unsigned short);
int APS5_VABI fchmod_nid_postfix(int, int);
int APS5_VABI futimes_nid_postfix(int, const KernelTimeval*);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI sceKernelFsync(int);
int APS5_VABI fdatasync_nid_postfix(int);
int APS5_VABI sceKernelWriteThrottlingStatus(std::uint64_t*);
int APS5_VABI sceKernelFtruncate(int, long long);
int APS5_VABI sceKernelTruncate_nid_postfix(const char*, long long);
int APS5_VABI sceKernelUtimes_nid_postfix(const char*, const void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI _open_nid_postfix(const char*, int, ...);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI flock_nid_postfix(int, int);
int APS5_VABI stat_nid_postfix(const char*, FileStat*);
int APS5_VABI lstat_nid_postfix(const char*, FileStat*);
int APS5_VABI unlink_nid_postfix(const char*);
int APS5_VABI rmdir_nid_postfix(const char*);
int APS5_VABI mkdir_nid_postfix(const char*, unsigned short);
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI sceKernelFstat(int, FileStat*);
int APS5_VABI sceKernelUnlink(const char*);
int APS5_VABI sceKernelRmdir(const char*);
int APS5_VABI sceKernelMkdir(const char*, unsigned short);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI pipe_nid_postfix(int*);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::size_t);
std::int64_t APS5_VABI write_nid_postfix(int, const void*, std::size_t);
int APS5_VABI sceKernelDebugOutText(int, const char*);
}
static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Filesystem check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
int main() {
    Require(sceKernelDebugOutText(-1, "text") == static_cast<int>(0x80020016u));
    std::uint64_t throttling[4] = {1, 2, 3, 4};
    Require(sceKernelWriteThrottlingStatus(throttling) == 0);
    Require(throttling[0] == 0xffffffffu && throttling[1] == 0 && throttling[2] == 0 && throttling[3] == 0);
    Require(sceKernelDebugOutText(0, nullptr) == static_cast<int>(0x8002000eu));
    auto* captured = std::tmpfile();
    Require(captured != nullptr);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stderr));
    Require(saved >= 0 && ::_dup2(::_fileno(captured), ::_fileno(stderr)) == 0);
#else
    const int saved = ::dup(::fileno(stderr));
    Require(saved >= 0 && ::dup2(::fileno(captured), ::fileno(stderr)) == ::fileno(stderr));
#endif
    const int debugResult = sceKernelDebugOutText(3, "%s%d literal\n");
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stderr)) == ::fileno(stderr) && ::close(saved) == 0);
#endif
    Require(debugResult == 0);
    std::rewind(captured);
    char debugText[64]{};
    Require(std::fread(debugText, 1, sizeof(debugText), captured) == 23);
    Require(std::strcmp(debugText, "[debug:3] %s%d literal\n") == 0);
    Require(std::fclose(captured) == 0);
    Require(pipe_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    int descriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0 && descriptors[0] >= 0 && descriptors[1] >= 0);
    const char payload[] = {'A', '\0', '\r', '\n', '\x1a', 'Z'};
    Require(write_nid_postfix(descriptors[1], payload, sizeof(payload)) == sizeof(payload));
    Require(close_nid_postfix(descriptors[1]) == 0);
    char received[sizeof(payload)]{};
    Require(read_nid_postfix(descriptors[0], received, sizeof(received)) == sizeof(received));
    Require(std::memcmp(received, payload, sizeof(payload)) == 0);
    Require(read_nid_postfix(descriptors[0], received, sizeof(received)) == 0);
    Require(close_nid_postfix(descriptors[0]) == 0);
    const auto root = std::filesystem::path("anyps5-filesystem-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(mkdir_nid_postfix(root.string().c_str(), 0700) == 0);
    Require(mkdir_nid_postfix(root.string().c_str(), 0700) == -1 && *__error_nid_postfix() == 17);
    Require(mkdir_nid_postfix((root / "missing" / "child").string().c_str(), 0700) == -1 && *__error_nid_postfix() == 2);
    Require(mkdir_nid_postfix(nullptr, 0700) == -1 && *__error_nid_postfix() == 14);
    const auto file = root / "file.txt";
    { std::ofstream stream(file); stream << "retained until removal"; }
    Require(remove_nid_postfix(root.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 66);
    Require(std::filesystem::is_regular_file(file));
    Require(remove_nid_postfix((file / "invalid").string().c_str()) == -1);
    Require(remove_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(remove_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    const auto readOnly = root / "read-only.txt";
    { std::ofstream stream(readOnly); stream << "removable"; }
    Require(sceKernelChmod_nid_postfix(readOnly.string().c_str(), 0400) == 0);
    Require(remove_nid_postfix(readOnly.string().c_str()) == 0 && !std::filesystem::exists(readOnly));
    const auto renamed = root / "renamed.txt";
    { std::ofstream stream(renamed); stream << "old contents"; }
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    { std::ifstream stream(renamed); std::string contents; std::getline(stream, contents);
      Require(contents == "retained until removal"); }
    Require(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()) == 0);
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 2);
    Require(rename_nid_postfix(renamed.string().c_str(), file.string().c_str()) == 0);
    Require(remove_nid_postfix(file.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    Require(remove_nid_postfix(file.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    const auto sized = root / "sized.txt";
    { std::ofstream stream(sized); stream << "0123456789abcdef"; }
    Require(sceKernelChmod_nid_postfix(sized.string().c_str(), 0600) == 0);
    Require(sceKernelTruncate_nid_postfix(sized.string().c_str(), 6) == 0);
    Require(std::filesystem::file_size(sized) == 6);
    { std::ifstream stream(sized); std::string contents; std::getline(stream, contents);
      Require(contents == "012345"); }
    Require(sceKernelTruncate_nid_postfix((sized / "missing").string().c_str(), 6) == static_cast<int>(0x80020002u));
    Require(sceKernelUtimes_nid_postfix(sized.string().c_str(), nullptr) == 0);
    std::FILE* native = std::fopen(sized.string().c_str(), "r+b");
    Require(native != nullptr);
#ifdef _WIN32
    const int descriptor = _fileno(native);
#else
    const int descriptor = ::fileno(native);
#endif
    Require(descriptor >= 0 && sceKernelFsync(descriptor) == 0);
    Require(fdatasync_nid_postfix(descriptor) == 0);
    const auto ownerWrite = [&] {
        return (std::filesystem::status(sized).permissions() & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
    };
    Require(sceKernelFchmod(descriptor, 0400) == 0 && !ownerWrite());
    Require(fchmod_nid_postfix(descriptor, 0600) == 0 && ownerWrite());
    Require(sceKernelFtruncate(descriptor, 3) == 0);
    FileStat times{};
    const KernelTimeval past[2]{{1000000000, 0}, {1000000000, 500000}};
    Require(futimes_nid_postfix(descriptor, past) == 0);
    Require(stat_nid_postfix(sized.string().c_str(), &times) == 0 && times.st_mtim.tv_sec == 1000000000);
    Require(futimes_nid_postfix(descriptor, nullptr) == 0);
    Require(stat_nid_postfix(sized.string().c_str(), &times) == 0 && times.st_mtim.tv_sec > 1000000000);
    const KernelTimeval overflow[2]{{0, 0}, {0, 1000000}};
    Require(futimes_nid_postfix(descriptor, overflow) == -1 && *__error_nid_postfix() == 22);
    const KernelTimeval negative[2]{{0, -1}, {0, 0}};
    Require(futimes_nid_postfix(descriptor, negative) == -1 && *__error_nid_postfix() == 22);
    Require(std::fclose(native) == 0);
    Require(std::filesystem::file_size(sized) == 3);
#ifndef _WIN32
    Require(sceKernelFchmod(descriptor, 0600) == static_cast<int>(0x80020009u));
    Require(fchmod_nid_postfix(descriptor, 0600) == -1 && *__error_nid_postfix() == 9);
    Require(futimes_nid_postfix(descriptor, nullptr) == -1 && *__error_nid_postfix() == 9);
    Require(fdatasync_nid_postfix(descriptor) == -1 && *__error_nid_postfix() == 9);
#endif
    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0);
    Require(sceKernelFchmod(socket, 0600) == static_cast<int>(0x80020016u));
    Require(fchmod_nid_postfix(socket, 0600) == -1 && *__error_nid_postfix() == 22);
    Require(futimes_nid_postfix(socket, nullptr) == -1 && *__error_nid_postfix() == 22);
    Require(fdatasync_nid_postfix(socket) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(socket) == 0);
    Require(fchmod_nid_postfix(socket, 0600) == -1 && *__error_nid_postfix() == 9);
    Require(futimes_nid_postfix(socket, nullptr) == -1 && *__error_nid_postfix() == 9);
    Require(fdatasync_nid_postfix(socket) == -1 && *__error_nid_postfix() == 9);
    Require(remove_nid_postfix(sized.string().c_str()) == 0);
    const auto present = root / "present.txt";
    const auto presentName = present.string();
    const auto missingName = (root / "missing.txt").string();
    const auto rootName = root.string();
    { std::ofstream stream(present); stream << "posix"; }
    FileStat status{};
    Require(stat_nid_postfix(presentName.c_str(), &status) == 0 && status.st_size == 5);
    Require(stat_nid_postfix(missingName.c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelStat(missingName.c_str(), &status) == static_cast<int>(0x80020002u));
    Require(stat_nid_postfix("", &status) == -1 && *__error_nid_postfix() == 2);
    Require(stat_nid_postfix((presentName + "/").c_str(), &status) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelStat((presentName + "/").c_str(), &status) == static_cast<int>(0x80020014u));
    FileStat dirStatus{};
    Require(stat_nid_postfix((rootName + "/").c_str(), &dirStatus) == 0 && (dirStatus.st_mode & 0170000) == 0040000);
    Require(stat_nid_postfix(nullptr, &status) == -1 && *__error_nid_postfix() == 14);
    Require(stat_nid_postfix(presentName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
    FileStat linkStatus{};
    Require(lstat_nid_postfix(presentName.c_str(), &linkStatus) == 0 && linkStatus.st_size == 5);
    Require((linkStatus.st_mode & 0170000) == 0100000 && linkStatus.st_ino == status.st_ino);
    Require(lstat_nid_postfix(rootName.c_str(), &linkStatus) == 0 && (linkStatus.st_mode & 0170000) == 0040000);
    Require(lstat_nid_postfix(missingName.c_str(), &linkStatus) == -1 && *__error_nid_postfix() == 2);
    Require(lstat_nid_postfix("", &linkStatus) == -1 && *__error_nid_postfix() == 2);
    Require(lstat_nid_postfix(nullptr, &linkStatus) == -1 && *__error_nid_postfix() == 14);
    Require(lstat_nid_postfix(presentName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
#ifndef _WIN32
    const auto link = root / "link";
    std::filesystem::create_symlink("present.txt", link);
    Require(lstat_nid_postfix(link.string().c_str(), &linkStatus) == 0);
    Require((linkStatus.st_mode & 0170000) == 0120000 && linkStatus.st_size == 11);
    Require(stat_nid_postfix(link.string().c_str(), &status) == 0 && (status.st_mode & 0170000) == 0100000);
    const auto dangling = root / "dangling";
    std::filesystem::create_symlink("missing.txt", dangling);
    Require(lstat_nid_postfix(dangling.string().c_str(), &linkStatus) == 0 && (linkStatus.st_mode & 0170000) == 0120000);
    Require(stat_nid_postfix(dangling.string().c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(lstat_nid_postfix((root / "present.txt" / "child").string().c_str(), &linkStatus) == -1 && *__error_nid_postfix() == 20);
    Require(unlink_nid_postfix(link.string().c_str()) == 0 && unlink_nid_postfix(dangling.string().c_str()) == 0);
#endif
    FileStat identity{};
    FileStat byDescriptor{};
    const int identityDescriptor = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(identityDescriptor >= 0 && stat_nid_postfix(presentName.c_str(), &identity) == 0);
    Require(sceKernelFstat(identityDescriptor, &byDescriptor) == 0 && close_nid_postfix(identityDescriptor) == 0);
    Require(identity.st_ino != 0 && byDescriptor.st_dev == identity.st_dev && byDescriptor.st_ino == identity.st_ino);
    const auto sibling = root / "sibling.txt";
    { std::ofstream stream(sibling); stream << "other"; }
    FileStat siblingStatus{};
    Require(stat_nid_postfix(sibling.string().c_str(), &siblingStatus) == 0);
    Require(siblingStatus.st_dev == identity.st_dev && siblingStatus.st_ino != identity.st_ino);
    Require(unlink_nid_postfix(sibling.string().c_str()) == 0);
    const auto hardLink = root / "hardlink.txt";
    std::error_code linkError;
    std::filesystem::create_hard_link(present, hardLink, linkError);
    if (!linkError) {
        FileStat linked{};
        Require(stat_nid_postfix(hardLink.string().c_str(), &linked) == 0);
        Require(linked.st_dev == identity.st_dev && linked.st_ino == identity.st_ino && linked.st_nlink == 2);
        Require(unlink_nid_postfix(hardLink.string().c_str()) == 0);
    }
    const int opened = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(opened >= 0 && close_nid_postfix(opened) == 0);
    const int reopened = _open_nid_postfix(presentName.c_str(), 0);
    Require(reopened >= 0 && close_nid_postfix(reopened) == 0);
    const int locked = open_nid_postfix(presentName.c_str(), 0, 0);
    const int other = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(locked >= 0 && other >= 0);
    Require(flock_nid_postfix(locked, 8) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(flock_nid_postfix(locked, 1 | 4) == 0);
    Require(flock_nid_postfix(other, 1 | 4) == 0);
    Require(flock_nid_postfix(other, 8) == 0);
    Require(flock_nid_postfix(locked, 1 | 4) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(flock_nid_postfix(locked, 8) == 0);
    Require(flock_nid_postfix(locked, 8) == 0);
    Require(flock_nid_postfix(other, 2 | 4) == 0);
    Require(flock_nid_postfix(other, 8) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(close_nid_postfix(locked) == 0);
    const int recycled = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(recycled >= 0 && flock_nid_postfix(recycled, 8) == 0);
    Require(flock_nid_postfix(recycled, 2 | 4) == 0 && sceKernelClose(recycled) == 0);
    const int kernelRecycled = sceKernelOpen(presentName.c_str(), 0, 0);
    Require(kernelRecycled >= 0 && flock_nid_postfix(kernelRecycled, 8) == 0);
    Require(flock_nid_postfix(other, 0) == -1 && *__error_nid_postfix() == 9);
    Require(flock_nid_postfix(other, 4 | 0x10) == -1 && *__error_nid_postfix() == 9);
    Require(flock_nid_postfix(kernelRecycled, 2 | 4 | 0x10) == 0);
    Require(flock_nid_postfix(kernelRecycled, 8 | 2) == 0);
    Require(flock_nid_postfix(other, 2 | 4) == 0 && flock_nid_postfix(other, 8) == 0);
    Require(sceKernelClose(kernelRecycled) == 0 && close_nid_postfix(other) == 0);
    const int holder = open_nid_postfix(presentName.c_str(), 0, 0);
    const int contender = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(holder >= 0 && contender >= 0 && flock_nid_postfix(holder, 2 | 4) == 0);
    Require(flock_nid_postfix(contender, 2 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(contender, 1 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(holder, 1 | 4) == 0 && flock_nid_postfix(contender, 1 | 4) == 0);
    Require(flock_nid_postfix(contender, 2 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(holder, 8) == 0 && flock_nid_postfix(contender, 2 | 4) == 0);
    Require(flock_nid_postfix(contender, 8) == 0);
    Require(flock_nid_postfix(holder, 1 | 2 | 4 | 0x10) == 0);
    Require(flock_nid_postfix(contender, 1 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(holder, 8) == 0);
    Require(close_nid_postfix(contender) == 0 && close_nid_postfix(holder) == 0);
    Require(open_nid_postfix(missingName.c_str(), 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(_open_nid_postfix(missingName.c_str(), 0) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelOpen(missingName.c_str(), 0, 0) == static_cast<int>(0x80020002u));
    Require(open_nid_postfix(presentName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(_open_nid_postfix(presentName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix(rootName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix(rootName.c_str(), 0x0a00, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix(rootName.c_str(), 0x0200, 0644) == -1 && *__error_nid_postfix() == 21);
    Require(open_nid_postfix(rootName.c_str(), 0x0002, 0) == -1 && *__error_nid_postfix() == 21);
    Require(open_nid_postfix(rootName.c_str(), 0x0001, 0) == -1 && *__error_nid_postfix() == 21);
    const int directoryDescriptor = open_nid_postfix(rootName.c_str(), 0, 0);
    Require(directoryDescriptor >= 0 && close_nid_postfix(directoryDescriptor) == 0);
    Require(open_nid_postfix("", 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(_open_nid_postfix("", 0) == -1 && *__error_nid_postfix() == 2);
    Require(open_nid_postfix(nullptr, 0, 0) == -1 && *__error_nid_postfix() == 14);
    Require(_open_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(rmdir_nid_postfix(rootName.c_str()) == -1 && *__error_nid_postfix() == 66);
    Require(sceKernelRmdir(rootName.c_str()) == static_cast<int>(0x80020042u));
    Require(rmdir_nid_postfix(presentName.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix(missingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    const auto underFile = presentName + "/child";
    const auto deepUnderFile = presentName + "/child/leaf";
    Require(mkdir_nid_postfix(underFile.c_str(), 0700) == -1 && *__error_nid_postfix() == 20);
    Require(mkdir_nid_postfix(deepUnderFile.c_str(), 0700) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelMkdir(underFile.c_str(), 0700) == static_cast<int>(0x80020014u));
    Require(sceKernelMkdir(deepUnderFile.c_str(), 0700) == static_cast<int>(0x80020014u));
    Require(rmdir_nid_postfix(underFile.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelRmdir(deepUnderFile.c_str()) == static_cast<int>(0x80020014u));
    Require(rmdir_nid_postfix((presentName + "/").c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix((missingName + "/").c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelRmdir((presentName + "/").c_str()) == static_cast<int>(0x80020014u));
    Require(sceKernelRmdir((missingName + "/").c_str()) == static_cast<int>(0x80020002u));
    const auto missingParentLeaf = rootName + "/missing/leaf";
    Require(sceKernelMkdir(missingParentLeaf.c_str(), 0700) == static_cast<int>(0x80020002u));
    const auto invalidNameUnderFile = presentName + "/a:b/x";
    Require(mkdir_nid_postfix(invalidNameUnderFile.c_str(), 0700) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelMkdir(invalidNameUnderFile.c_str(), 0700) == static_cast<int>(0x80020014u));
    const auto trailingPosix = root / "trailing_posix";
    const auto trailingKernel = root / "trailing_kernel";
    const auto trailingPosixName = trailingPosix.string() + "/";
    const auto trailingKernelName = trailingKernel.string() + "/";
    Require(mkdir_nid_postfix(trailingPosixName.c_str(), 0700) == 0 && std::filesystem::is_directory(trailingPosix));
    Require(sceKernelMkdir(trailingKernelName.c_str(), 0700) == 0 && std::filesystem::is_directory(trailingKernel));
    Require(rmdir_nid_postfix(trailingPosix.string().c_str()) == 0 && sceKernelRmdir(trailingKernel.string().c_str()) == 0);
    Require(!std::filesystem::exists(trailingPosix) && !std::filesystem::exists(trailingKernel));
    Require(std::filesystem::is_regular_file(present));
    Require(rmdir_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(rmdir_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(unlink_nid_postfix(missingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelUnlink(missingName.c_str()) == static_cast<int>(0x80020002u));
    Require(unlink_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(unlink_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(unlink_nid_postfix(rootName.c_str()) == -1 && *__error_nid_postfix() == 1 && std::filesystem::is_directory(root));
    Require(sceKernelUnlink(rootName.c_str()) == static_cast<int>(0x80020001u));
    const int closable = sceKernelOpen(presentName.c_str(), 0, 0);
    Require(closable >= 0 && sceKernelClose(closable) == 0);
    Require(sceKernelClose(closable) == static_cast<int>(0x80020009u));
    Require(sceKernelClose(-1) == static_cast<int>(0x80020009u));
    for (const char* device : {"/dev/urandom", "/dev/random"}) {
        const int random = open_nid_postfix(device, 0, 0);
        Require(random >= 0);
        unsigned char first[64]{};
        unsigned char second[64]{};
        Require(read_nid_postfix(random, first, sizeof(first)) == sizeof(first));
        Require(read_nid_postfix(random, second, 3) == 3);
        Require(std::memcmp(first, second, sizeof(first)) != 0 && second[3] == 0);
        Require(close_nid_postfix(random) == 0);
    }
    Require(unlink_nid_postfix(presentName.c_str()) == 0 && !std::filesystem::exists(present));
    const auto empty = root / "empty";
    Require(std::filesystem::create_directory(empty));
    Require(rmdir_nid_postfix(empty.string().c_str()) == 0 && !std::filesystem::exists(empty));
    Require(remove_nid_postfix(root.string().c_str()) == 0);
    Require(!std::filesystem::exists(root));
}
