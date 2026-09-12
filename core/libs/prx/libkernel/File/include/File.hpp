#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP

#include <cstdint>
#include <cstddef>

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

int APS5_VABI sceKernelClose(int d);
int APS5_VABI sceKernelLseek(int d, int64_t offset, int whence);
int APS5_VABI sceKernelOpen(const char* path, int flags, uint16_t mode);
int64_t APS5_VABI sceKernelRead(int d, void* buf, size_t nbytes);
int APS5_VABI sceKernelStat(const char* path, FileStat* sb);
int APS5_VABI sceKernelUnlink(const char* path);
int64_t APS5_VABI sceKernelWrite(int d, const void* buf, size_t nbytes);

}

#endif
