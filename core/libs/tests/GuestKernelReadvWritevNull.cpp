#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GuestKernelReadvWritevNull check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr std::int64_t ErrorEfault = static_cast<int>(0x8002000Eu);
static constexpr std::int64_t ErrorEinval = static_cast<int>(0x80020016u);

int main() {
    const auto root = std::filesystem::path("anyps5-readv-writev-null-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = root / "data.bin";
    { std::ofstream stream(path, std::ios::binary); stream << "0123456789"; }

    const int file = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(file >= 0);

    char buf[4] = {};
    KernelIovec iov = {buf, sizeof(buf)};

    Require(sceKernelReadv(file, nullptr, 1) == ErrorEfault);
    Require(sceKernelWritev(file, nullptr, 1) == ErrorEfault);
    Require(sceKernelPreadv(file, nullptr, 1, 0) == ErrorEfault);
    Require(sceKernelPwritev(file, nullptr, 1, 0) == ErrorEfault);

    Require(readv_nid_postfix(file, nullptr, 1) == -1 && *__error_nid_postfix() == 14);
    Require(writev_nid_postfix(file, nullptr, 1) == -1 && *__error_nid_postfix() == 14);
    Require(preadv_nid_postfix(file, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(pwritev_nid_postfix(file, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);

    Require(sceKernelReadv(file, &iov, -1) == ErrorEinval);
    Require(sceKernelWritev(file, &iov, -1) == ErrorEinval);
    Require(sceKernelPreadv(file, &iov, -1, 0) == ErrorEinval);
    Require(sceKernelPwritev(file, &iov, -1, 0) == ErrorEinval);

    Require(readv_nid_postfix(file, &iov, -1) == -1 && *__error_nid_postfix() == 22);
    Require(writev_nid_postfix(file, &iov, -1) == -1 && *__error_nid_postfix() == 22);
    Require(preadv_nid_postfix(file, &iov, -1, 0) == -1 && *__error_nid_postfix() == 22);
    Require(pwritev_nid_postfix(file, &iov, -1, 0) == -1 && *__error_nid_postfix() == 22);

    Require(sceKernelReadv(file, &iov, 1025) == ErrorEinval);
    Require(sceKernelWritev(file, &iov, 1025) == ErrorEinval);
    Require(sceKernelPreadv(file, &iov, 1025, 0) == ErrorEinval);
    Require(sceKernelPwritev(file, &iov, 1025, 0) == ErrorEinval);

    KernelIovec badIov = {reinterpret_cast<void*>(0x12345678), sizeof(buf)};
    Require(sceKernelReadv(file, &badIov, 1) == ErrorEfault);
    Require(sceKernelWritev(file, &badIov, 1) == ErrorEfault);
    Require(sceKernelPreadv(file, &badIov, 1, 0) == ErrorEfault);
    Require(sceKernelPwritev(file, &badIov, 1, 0) == ErrorEfault);

    Require(readv_nid_postfix(file, &badIov, 1) == -1 && *__error_nid_postfix() == 14);
    Require(writev_nid_postfix(file, &badIov, 1) == -1 && *__error_nid_postfix() == 14);
    Require(preadv_nid_postfix(file, &badIov, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(pwritev_nid_postfix(file, &badIov, 1, 0) == -1 && *__error_nid_postfix() == 14);

    Require(sceKernelClose(file) == 0);
    std::filesystem::remove_all(root);
    std::printf("GuestKernelReadvWritevNull passed successfully!\n");
    return 0;
}
