#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#ifdef __APPLE__
#include <sys/uio.h>
#endif

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/DescriptorAccess.hpp"

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
class IoDescriptor {
public:
    explicit IoDescriptor(int descriptor);
    ~IoDescriptor();
    IoDescriptor(const IoDescriptor&) = delete;
    IoDescriptor& operator=(const IoDescriptor&) = delete;
    int Value() const { return value; }

private:
    int value;
    GuestDescriptorAccess access;
    std::optional<GuestDescriptorAccess::Operation> operation;
};

template <class TCall>
std::int64_t WithIoDescriptor(int descriptor, TCall&& call) {
    IoDescriptor retained(descriptor);
    if (retained.Value() < 0) return -1;
    return call(retained.Value());
}

#ifdef _WIN32
std::optional<std::filesystem::path> WindowsDescriptorPath_nid_no_patch(int descriptor);
#endif
int HostStandardError();
void ForgetClosedDescriptor(int descriptor);
class PositionedTransferLock {
public:
    explicit PositionedTransferLock(std::shared_ptr<std::mutex> mutex) : _mutex(std::move(mutex)), _lock(*_mutex) {}
    PositionedTransferLock(std::shared_ptr<std::mutex> mutex, std::unique_lock<std::mutex> lock)
        : _mutex(std::move(mutex)), _lock(std::move(lock)) {}
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
std::optional<PositionedTransferLock> IoPositionedTransfer(int descriptor);
std::int64_t WriteAtEnd(int descriptor, const void* buf, std::size_t nbytes);
#elif defined(__APPLE__)
std::int64_t WritevAtEnd(int descriptor, const iovec* iov, int iovcnt);
#endif
}

#endif
