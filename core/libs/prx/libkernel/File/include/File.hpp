#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#ifndef _WIN32
#include <sys/uio.h>
#endif

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"

struct KernelStatfs;

extern "C" {

int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes);
std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes);
std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence);
int PosixStatfs_nid_no_patch(const char* path, KernelStatfs* buf);
int PosixUtimesAt_nid_no_patch(int directory, const char* path, const KernelTimeval* times);
int APS5_VABI sceKernelStat(const char* path, FileStat* sb);
int APS5_VABI sceKernelUnlink(const char* path);
int APS5_VABI sceKernelFcntl(int d, int command, ...);
std::int64_t PosixPwrite_nid_no_patch(int d, const void* buf, std::size_t nbytes, std::int64_t offset);
#ifdef _WIN32
std::int64_t NativePositioned_nid_no_patch(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset, bool write);
#endif

}

extern "C" bool DescriptorIsOpen_nid_no_patch(int descriptor);

namespace File {
inline int KernelFileResult(int saved, int result) {
    *__error_nid_postfix() = result < 0 ? (result & 0xffff) : saved;
    return result;
}
inline std::int64_t KernelFileResult(int saved, std::int64_t result) {
    *__error_nid_postfix() = result < 0 ? (static_cast<int>(result) & 0xffff) : saved;
    return result;
}
template <typename TCall>
auto RunKernelFileCall(TCall call) {
    const int saved = *__error_nid_postfix();
    return KernelFileResult(saved, call());
}
#ifdef _WIN32
std::optional<std::filesystem::path> WindowsDescriptorPath_nid_no_patch(int descriptor);
#endif
int HostStandardError();
void ForgetClosedDescriptor(int descriptor);
class PositionedTransferLock {
public:
    explicit PositionedTransferLock(std::shared_ptr<std::mutex> mutex) : _mutex(std::move(mutex)), _lock(*_mutex) {}
    PositionedTransferLock(PositionedTransferLock&&) = default;
    PositionedTransferLock& operator=(PositionedTransferLock&&) = delete;
    void unlock() { _lock.unlock(); }

private:
    std::shared_ptr<std::mutex> _mutex;
    std::unique_lock<std::mutex> _lock;
};
[[nodiscard]] PositionedTransferLock LockPositionedTransfer(int descriptor);
void SharePositionedTransfer(int from, int to);
#ifdef _WIN32
std::int64_t WriteAtEnd(int descriptor, const void* buf, std::size_t nbytes);
inline std::size_t TransferPartBytes(std::size_t total, std::size_t done) {
    constexpr std::size_t singleCallBytes = 0x7ffff000;
    constexpr std::size_t partBytes = std::size_t{1} << 30;
    return total <= singleCallBytes ? total : total - done < partBytes ? total - done : partBytes;
}
#elif defined(__APPLE__)
std::int64_t WritevAtEnd(int descriptor, const iovec* iov, int iovcnt);
[[nodiscard]] std::optional<PositionedTransferLock> LockAppendingTransfer(int descriptor);
#endif
#ifndef _WIN32
std::int64_t TransferInParts(int descriptor, const iovec* iov, int iovcnt, const std::int64_t* offset, bool write);
#endif
}

#endif
