#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_FILEFLAGS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_FILEFLAGS_HPP

#include <cstdint>

constexpr std::int32_t SCE_KERNEL_O_RDONLY    = 0x00000000;
constexpr std::int32_t SCE_KERNEL_O_WRONLY    = 0x00000001;
constexpr std::int32_t SCE_KERNEL_O_RDWR      = 0x00000002;
constexpr std::int32_t SCE_KERNEL_O_ACCMODE   = 0x00000003;
constexpr std::int32_t SCE_KERNEL_O_APPEND    = 0x00000008;
constexpr std::int32_t SCE_KERNEL_O_FSYNC     = 0x00000080;
constexpr std::int32_t SCE_KERNEL_O_SYNC      = 0x00000080;
constexpr std::int32_t SCE_KERNEL_O_CREAT     = 0x00000200;
constexpr std::int32_t SCE_KERNEL_O_TRUNC     = 0x00000400;
constexpr std::int32_t SCE_KERNEL_O_EXCL      = 0x00000800;
constexpr std::int32_t SCE_KERNEL_O_DSYNC     = 0x00001000;
constexpr std::int32_t SCE_KERNEL_O_NONBLOCK  = 0x00000004;
constexpr std::int32_t SCE_KERNEL_O_DIRECT    = 0x00010000;
constexpr std::int32_t SCE_KERNEL_O_DIRECTORY = 0x00020000;

#endif
