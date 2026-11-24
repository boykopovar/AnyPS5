#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestEnotty = 25;
constexpr int GuestEaddrnotavail = 49;

int Reject(int32_t fd) {
    int error = GuestEbadf;
    if (GuestSocketIsOpen_nid_no_patch(fd)) error = GuestEaddrnotavail;
    else if (IsKqueueDescriptor_nid_no_patch(fd) || IsDirectoryDescriptor_nid_no_patch(fd) != 0 || DescriptorIsOpen_nid_no_patch(fd)) error = GuestEnotty;
    *__error_nid_postfix() = error;
    return SceKernelError(error);
}

} // namespace

int APS5_VABI sceKernelLwfsAllocateBlock(int32_t fd, int64_t size) {
    return Reject(fd);
}

int64_t APS5_VABI sceKernelLwfsLseek(int32_t fd, int64_t offset, int32_t whence) {
    return Reject(fd);
}

int APS5_VABI sceKernelLwfsSetAttribute(int32_t fd, int32_t attribute) {
    return Reject(fd);
}

int64_t APS5_VABI sceKernelLwfsWrite(int32_t fd, const void* data, std::size_t size) {
    const auto start = reinterpret_cast<std::uintptr_t>(data);
    if (size != 0 && (start == 0 || size > UINTPTR_MAX - start)) {
        *__error_nid_postfix() = 14;
        return SceKernelError(14);
    }
    std::array<unsigned char, 4096> buffer;
    for (std::size_t offset = 0; offset < size;) {
        const auto count = std::min(buffer.size(), size - offset);
        if (!GuestCopyIn_nid_no_patch(buffer.data(), reinterpret_cast<const void*>(start + offset), count)) {
            *__error_nid_postfix() = 14;
            return SceKernelError(14);
        }
        offset += count;
    }
    return Reject(fd);
}

int APS5_VABI sceKernelSetCompressionAttribute(int32_t fd, int32_t attribute) {
    if (attribute != 0 && attribute != 1) return SceKernelError(GuestEinval);
    return Reject(fd);
}
