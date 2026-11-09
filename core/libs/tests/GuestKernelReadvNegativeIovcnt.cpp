#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    KernelIovec iov{};
    iov.base = &iov;
    iov.length = 1;

    Require(sceKernelReadv(-1, &iov, -1) == SCE_KERNEL_ERROR_EINVAL, "negative iovcnt must return EINVAL for sceKernelReadv");
    Require(sceKernelWritev(-1, &iov, -1) == SCE_KERNEL_ERROR_EINVAL, "negative iovcnt must return EINVAL for sceKernelWritev");
    Require(sceKernelPreadv(-1, &iov, -1, 0) == SCE_KERNEL_ERROR_EINVAL, "negative iovcnt must return EINVAL for sceKernelPreadv");
    Require(sceKernelPwritev(-1, &iov, -1, 0) == SCE_KERNEL_ERROR_EINVAL, "negative iovcnt must return EINVAL for sceKernelPwritev");

    Require(readv_nid_postfix(-1, &iov, -1) == -1, "negative iovcnt must fail for readv_nid_postfix");
    Require(writev_nid_postfix(-1, &iov, -1) == -1, "negative iovcnt must fail for writev_nid_postfix");
    Require(preadv_nid_postfix(-1, &iov, -1, 0) == -1, "negative iovcnt must fail for preadv_nid_postfix");
    Require(pwritev_nid_postfix(-1, &iov, -1, 0) == -1, "negative iovcnt must fail for pwritev_nid_postfix");

    Require(sceKernelReadv(-1, &iov, 1025) == SCE_KERNEL_ERROR_EINVAL, "iovcnt > KERNEL_IOV_MAX must return EINVAL for sceKernelReadv");
    Require(sceKernelWritev(-1, &iov, 1025) == SCE_KERNEL_ERROR_EINVAL, "iovcnt > KERNEL_IOV_MAX must return EINVAL for sceKernelWritev");
    Require(sceKernelPreadv(-1, &iov, 1025, 0) == SCE_KERNEL_ERROR_EINVAL, "iovcnt > KERNEL_IOV_MAX must return EINVAL for sceKernelPreadv");
    Require(sceKernelPwritev(-1, &iov, 1025, 0) == SCE_KERNEL_ERROR_EINVAL, "iovcnt > KERNEL_IOV_MAX must return EINVAL for sceKernelPwritev");

    Require(readv_nid_postfix(-1, &iov, 1025) == -1, "iovcnt > KERNEL_IOV_MAX must fail for readv_nid_postfix");
    Require(writev_nid_postfix(-1, &iov, 1025) == -1, "iovcnt > KERNEL_IOV_MAX must fail for writev_nid_postfix");
    Require(preadv_nid_postfix(-1, &iov, 1025, 0) == -1, "iovcnt > KERNEL_IOV_MAX must fail for preadv_nid_postfix");
    Require(pwritev_nid_postfix(-1, &iov, 1025, 0) == -1, "iovcnt > KERNEL_IOV_MAX must fail for pwritev_nid_postfix");

    std::printf("All readv and writev iovcnt boundary checks passed successfully.\n");
    return 0;
}
