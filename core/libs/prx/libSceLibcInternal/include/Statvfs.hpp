#ifndef CORE_LIBS_PRX_LIBSCELIBCINTERNAL_INCLUDE_STATVFS_HPP
#define CORE_LIBS_PRX_LIBSCELIBCINTERNAL_INCLUDE_STATVFS_HPP

#include "prx/libkernel/File/include/KernelStatfs.hpp"
#include <cstddef>
#include <cstdint>

struct LibcStatvfs {
    std::uint64_t f_bavail;
    std::uint64_t f_bfree;
    std::uint64_t f_blocks;
    std::uint64_t f_favail;
    std::uint64_t f_ffree;
    std::uint64_t f_files;
    std::uint64_t f_bsize;
    std::uint64_t f_flag;
    std::uint64_t f_frsize;
    std::uint64_t f_fsid;
    std::uint64_t f_namemax;
};
static_assert(sizeof(LibcStatvfs) == 88);
static_assert(offsetof(LibcStatvfs, f_bsize) == 48);
static_assert(offsetof(LibcStatvfs, f_namemax) == 80);

inline LibcStatvfs StatvfsFromStatfs(const KernelStatfs& volume) {
    return LibcStatvfs{
        static_cast<std::uint64_t>(volume.f_bavail), volume.f_bfree, volume.f_blocks,
        static_cast<std::uint64_t>(volume.f_ffree), static_cast<std::uint64_t>(volume.f_ffree), volume.f_files,
        volume.f_iosize, (volume.f_flags & 1) | ((volume.f_flags & 8) >> 2), volume.f_bsize,
        0, volume.f_namemax
    };
}

#endif
