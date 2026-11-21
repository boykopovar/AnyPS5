#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr std::uint64_t DeviceTypeRequest = 0x4004667a;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
    std::abort();
}

void AuditCharacterDeviceTypeQuery() {
#ifndef _WIN32
    const int null = ::open("/dev/null", O_RDONLY);
    Require(null >= 0, "opening the host null device failed");

    for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
        int type = 0x5a5a5a5a;
        Require(call(null, DeviceTypeRequest, &type) == 0 && type == 0,
            "FIODTYPE on a character device with no device type must succeed and answer 0, as devfs_ioctl_f answers d_flags & D_TYPEMASK and the null device sets none");
        type = 0x5a5a5a5a;
        Require(call(null, DeviceTypeRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault && type == 0x5a5a5a5a,
            "FIODTYPE without an output argument must fail with EFAULT (14) and leave the caller's value alone");
        Require(call(null, DeviceTypeRequest, reinterpret_cast<void*>(std::uintptr_t{0x10})) == -1 &&
                *__error_nid_postfix() == ErrorFault,
            "FIODTYPE with an unmapped output argument must fail with EFAULT (14)");
    }

    Require(::close(null) == 0, "closing the host null device failed");
    for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
        int type = 0;
        Require(call(null, DeviceTypeRequest, &type) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "FIODTYPE on a closed character device must fail with EBADF (9)");
    }
#endif
}

} // namespace

int main() {
#ifndef _WIN32
    AuditCharacterDeviceTypeQuery();
    std::puts("FIODTYPE on a character device with no device type answers 0");
#else
    std::puts("no guest path opens the null device on Windows, so FIODTYPE on a character device stays unverified");
#endif
    return 0;
}
