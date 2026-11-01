#pragma once
#include <cstddef>
#include <cstdint>

struct KernelStatfs {
    std::uint32_t f_version;
    std::uint32_t f_type;
    std::uint64_t f_flags;
    std::uint64_t f_bsize;
    std::uint64_t f_iosize;
    std::uint64_t f_blocks;
    std::uint64_t f_bfree;
    std::int64_t f_bavail;
    std::uint64_t f_files;
    std::int64_t f_ffree;
    std::uint64_t f_syncwrites;
    std::uint64_t f_asyncwrites;
    std::uint64_t f_syncreads;
    std::uint64_t f_asyncreads;
    std::uint64_t f_spare[10];
    std::uint32_t f_namemax;
    std::uint32_t f_owner;
    std::int32_t f_fsid[2];
    char f_charspare[80];
    char f_fstypename[16];
    char f_mntfromname[88];
    char f_mntonname[88];
};
static_assert(sizeof(KernelStatfs) == 472);
static_assert(offsetof(KernelStatfs, f_bavail) == 48);
static_assert(offsetof(KernelStatfs, f_namemax) == 184);
static_assert(offsetof(KernelStatfs, f_fstypename) == 280);
static_assert(offsetof(KernelStatfs, f_mntonname) == 384);
