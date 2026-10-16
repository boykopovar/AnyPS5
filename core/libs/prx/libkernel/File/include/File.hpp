#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes);
std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes);
std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence);
int APS5_VABI sceKernelStat(const char* path, FileStat* sb);
int APS5_VABI sceKernelUnlink(const char* path);
int APS5_VABI sceKernelFcntl(int d, int command, ...);
#ifdef _WIN32
std::int64_t NativePositioned_nid_no_patch(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset, bool write);
#endif

}

bool DescriptorIsOpen_nid_no_patch(int descriptor);

namespace File {
int HostStandardError();
[[nodiscard]] std::unique_lock<std::mutex> LockPositionedTransfer(int descriptor);
void SharePositionedTransfer(int from, int to);
#ifdef _WIN32
std::int64_t WriteAtEnd(int descriptor, const void* buf, std::size_t nbytes);
#endif
}

#endif
