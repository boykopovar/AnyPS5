#include "prx/libkernel/File/include/Seek.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <string>

#ifdef _WIN32
#include <stdio.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr int GuestEnxio = 6;
constexpr int GuestEinval = 22;
constexpr int GuestEnotty = 25;
constexpr int GuestEspipe = 29;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t Fifo = 0010000;
constexpr std::uint16_t Directory = 0040000;
constexpr std::uint16_t Regular = 0100000;

}

namespace File {

int UnknownWhenceError(int fd) {
    FileStat status{};
    if (!FillFileStatFromDescriptor(fd, &status)) return errno;
    return (status.st_mode & TypeMask) == Fifo ? GuestEspipe : GuestEinval;
}

SeekRegion ResolveSeekRegion(int fd, std::int64_t offset, int whence) {
    const bool data = whence == GuestSeekData;
    if (IsSharedMemoryDescriptor_nid_no_patch(fd)) NotImplemented_nid_no_patch("lseek SEEK_DATA or SEEK_HOLE on a shared memory object");
#ifdef _WIN32
    if (DirectoryDescriptorPath(fd)) return {GuestEnotty, 0, 0};
#endif
    FileStat status{};
    if (!FillFileStatFromDescriptor(fd, &status)) return {errno, 0, 0};
    const auto type = status.st_mode & TypeMask;
    if (type == Fifo) return {GuestEspipe, 0, 0};
    if (type == Directory) return {GuestEnotty, 0, 0};
    if (type != Regular) NotImplemented_nid_no_patch(("lseek SEEK_DATA or SEEK_HOLE on a non-regular file, fd=" + std::to_string(fd)).c_str());
    if (offset < 0) NotImplemented_nid_no_patch("lseek SEEK_DATA or SEEK_HOLE with a negative offset");
    if (offset >= status.st_size) return {GuestEnxio, 0, 0};
#ifdef _WIN32
    return {0, SEEK_SET, data ? offset : status.st_size};
#else
    return {0, data ? SEEK_DATA : SEEK_HOLE, offset};
#endif
}

}
