#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint16_t GuestRegularMode = 0100444;
constexpr std::uint16_t GuestDirectoryMode = 0040555;

int OpenPlaceholder() {
#ifdef _WIN32
    return ::_open("NUL", _O_RDONLY | _O_BINARY);
#else
    return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
#endif
}

void FillNode(const AppArchiveNode& node, FileStat* sb) {
    File::FillFileStat(AppArchivePath_nid_no_patch(), sb);
    sb->st_mode = node.Directory ? GuestDirectoryMode : GuestRegularMode;
    sb->st_ino = node.Handle + 1;
    sb->st_nlink = node.Directory ? 2 : 1;
    sb->st_size = static_cast<std::int64_t>(node.Size);
    sb->st_blocks = static_cast<std::int64_t>((node.Size + 511) / 512);
}

}

namespace File {

std::optional<int> OpenArchivedFile(const std::filesystem::path& native, int flags) {
    if (!AppArchiveOnly_nid_no_patch(native)) return std::nullopt;
    AppArchiveNode node{};
    if (!AppArchiveLookup_nid_no_patch(native, &node)) throw std::logic_error("OpenArchivedFile: " + native.string() + " left app0.zar while opening");
    if ((flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) == (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) return -EEXIST;
    if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY || (flags & SCE_KERNEL_O_TRUNC) != 0) return -EROFS;
    if (node.Directory) NotImplemented_nid_no_patch(("opening the app0.zar directory " + native.string()).c_str());
    if ((flags & SCE_KERNEL_O_DIRECTORY) != 0) return -ENOTDIR;
    const int fd = OpenPlaceholder();
    if (fd < 0) return -errno;
    AppArchiveAttach_nid_no_patch(fd, node);
    return fd;
}

void RefuseMergedDirectory(const std::filesystem::path& native) {
    if (!AppArchiveHasDirectory_nid_no_patch(native)) return;
    std::error_code error;
    if (std::filesystem::is_directory(native, error)) NotImplemented_nid_no_patch(("opening " + native.string() + " merged with app0.zar").c_str());
}

bool IsArchivedFile(int fd) {
    return AppArchiveDescriptor_nid_no_patch(fd, nullptr, nullptr);
}

void RefuseArchivedFile(int fd, const char* operation) {
    if (IsArchivedFile(fd)) NotImplemented_nid_no_patch((std::string(operation) + " on a file read from app0.zar, fd=" + std::to_string(fd)).c_str());
}

std::optional<std::int64_t> ReadArchivedFile(int fd, void* buf, std::size_t nbytes, const std::int64_t* offset) {
    const AppArchiveBuffer buffer{buf, nbytes};
    return ReadArchivedIovecs(fd, &buffer, 1, offset);
}

std::optional<std::int64_t> ReadArchivedIovecs(int fd, const void* iov, int iovcnt, const std::int64_t* offset) {
    if (!IsArchivedFile(fd)) return std::nullopt;
    if (offset != nullptr && *offset < 0) return -EINVAL;
    std::uint64_t read = 0;
    if (!AppArchiveDescriptorRead_nid_no_patch(fd, static_cast<const AppArchiveBuffer*>(iov), iovcnt, offset, &read)) return std::nullopt;
    return static_cast<std::int64_t>(read);
}

std::optional<std::int64_t> SeekArchivedFile(int fd, std::int64_t offset, int whence) {
    std::int64_t position = 0;
    if (!AppArchiveDescriptorSeek_nid_no_patch(fd, offset, whence, &position)) return std::nullopt;
    return position < 0 ? -EINVAL : position;
}

std::optional<std::int64_t> ArchivedFileRemaining(int fd) {
    AppArchiveNode node{};
    std::uint64_t position = 0;
    if (!AppArchiveDescriptor_nid_no_patch(fd, &node, &position)) return std::nullopt;
    return position >= node.Size ? 0 : static_cast<std::int64_t>(node.Size - position);
}

bool FillArchivedStat(const std::filesystem::path& native, FileStat* sb) {
    if (!AppArchiveOnly_nid_no_patch(native)) return false;
    AppArchiveNode node{};
    if (!AppArchiveLookup_nid_no_patch(native, &node)) throw std::logic_error("FillArchivedStat: " + native.string() + " left app0.zar while reading its status");
    FillNode(node, sb);
    return true;
}

bool FillArchivedStatFromDescriptor(int fd, FileStat* sb) {
    AppArchiveNode node{};
    if (!AppArchiveDescriptor_nid_no_patch(fd, &node, nullptr)) return false;
    FillNode(node, sb);
    return true;
}

void ShareArchivedFile(int from, int to) {
    AppArchiveShare_nid_no_patch(from, to);
}

void ForgetArchivedFile(int fd) {
    AppArchiveDetach_nid_no_patch(fd);
}

}
