#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include <cerrno>
#include <filesystem>
#include <stdexcept>

static constexpr int GUEST_ENOENT = 2;
static constexpr int GUEST_EIO = 5;
static constexpr int GUEST_EEXIST = 17;
static constexpr int GUEST_ENOTDIR = 20;
static constexpr int GUEST_ENOTEMPTY = 66;

static int SceErrorFromErrno(int error) {
    return static_cast<int>(0x80020000u | static_cast<unsigned>(error > 0 && error <= 34 ? error : error == GUEST_ENOTEMPTY ? error : GUEST_EIO));
}

extern "C" {

int APS5_VABI chmod_nid_postfix(const char* path, int mode) {
 (void)path;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI close_nid_postfix(int d) {
 (void)d;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI flock_nid_postfix(int d, int operation) {
 (void)d;
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
 (void)d;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI ftruncate_nid_postfix(int d, int64_t length) {
 (void)d;
 (void)length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI lseek_nid_postfix(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
 (void)path;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
 (void)path;
 (void)flags;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI pread_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI read_nid_postfix(int d, void* buf, uint64_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
 (void)d;
 (void)str;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
    if (sb == nullptr) throw std::invalid_argument("sceKernelFstat: sb is null");
    if (!File::FillFileStatFromDescriptor(d, sb)) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelFsync(int fd) {
 (void)fd;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 (void)basep;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    (void)mode;
    if (path == nullptr) throw std::invalid_argument("sceKernelMkdir: path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (std::filesystem::exists(native, error)) return SceErrorFromErrno(GUEST_EEXIST);
    if (!std::filesystem::exists(native.parent_path(), error)) return SceErrorFromErrno(GUEST_ENOENT);
    if (!std::filesystem::create_directory(native, error)) return SceErrorFromErrno(error.value() ? error.value() : GUEST_EIO);
    return 0;
}

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    if (from == nullptr || to == nullptr) throw std::invalid_argument("sceKernelRename: path is null");
    const auto source = ResolvePath_nid_no_patch(from);
    std::error_code error;
    if (!std::filesystem::exists(source, error)) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::rename(source, ResolvePath_nid_no_patch(to), error);
    if (error) return SceErrorFromErrno(GUEST_EIO);
    return 0;
}

int APS5_VABI sceKernelRmdir(const char* path) {
    if (path == nullptr) throw std::invalid_argument("sceKernelRmdir: path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (!std::filesystem::is_directory(native, error)) return SceErrorFromErrno(std::filesystem::exists(native, error) ? GUEST_ENOTDIR : GUEST_ENOENT);
    if (!std::filesystem::is_empty(native, error)) return SceErrorFromErrno(GUEST_ENOTEMPTY);
    if (!std::filesystem::remove(native, error)) return SceErrorFromErrno(GUEST_EIO);
    return 0;
}

}
