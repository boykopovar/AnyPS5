#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/KernelStatfs.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
KernelStatfs backend;
int backendError;
int guestError;
int calls;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fstatvfs conversion check failed at line %d\n", line);
        std::abort();
    }
}
}
#define Require(value) Check((value), __LINE__)

extern "C" int* APS5_VABI __error_nid_postfix() { return &guestError; }

extern "C" int APS5_VABI _fstatfs_nid_postfix(int descriptor, KernelStatfs* output) {
    Require(descriptor == 42);
    ++calls;
    *output = backend;
    guestError = backendError ? backendError : 71;
    return backendError ? -1 : 0;
}

int main() {
    backend.f_blocks = 0x100000002ull;
    backend.f_bfree = 0x100000001ull;
    backend.f_bavail = 0x100000000ll;
    backend.f_files = 0x200000002ull;
    backend.f_ffree = 0x200000001ll;
    backend.f_iosize = 65536;
    backend.f_bsize = 4096;
    backend.f_namemax = 255;
    backend.f_fsid[0] = 123;
    for (std::uint64_t flags = 0; flags != 16; ++flags) {
        backend.f_flags = flags;
        std::array<std::uint64_t, 13> output;
        output.fill(0xA5A5A5A5A5A5A5A5ull);
        const std::array<std::uint64_t, 11> expected{
            0x100000000ull, 0x100000001ull, 0x100000002ull,
            0x200000001ull, 0x200000001ull, 0x200000002ull,
            65536, (flags & 1) | ((flags & 8) ? 2u : 0u), 4096, 0, 255
        };
        const int before = calls;
        guestError = 12345;
        Require(fstatvfs_nid_postfix(42, reinterpret_cast<LibcStatvfs*>(output.data() + 1)) == 0);
        Require(calls == before + 1 && guestError == 12345);
        Require(output.front() == 0xA5A5A5A5A5A5A5A5ull && output.back() == output.front());
        Require(std::memcmp(output.data() + 1, expected.data(), sizeof(expected)) == 0);
    }
    backend.f_bavail = -1;
    backend.f_ffree = -2;
    std::array<std::uint64_t, 11> output{};
    Require(fstatvfs_nid_postfix(42, reinterpret_cast<LibcStatvfs*>(output.data())) == 0);
    Require(output[0] == std::numeric_limits<std::uint64_t>::max());
    Require(output[3] == std::numeric_limits<std::uint64_t>::max() - 1 && output[4] == output[3]);
    Require(fstatvfs_nid_postfix(42, nullptr) == -1 && guestError == 14);
    for (int failure : {5, 9, 22, 45}) {
        backendError = failure;
        output.fill(0x5A5A5A5A5A5A5A5Aull);
        const auto saved = output;
        Require(fstatvfs_nid_postfix(42, reinterpret_cast<LibcStatvfs*>(output.data())) == -1 && guestError == failure);
        Require(output == saved);
        Require(fstatvfs_nid_postfix(42, nullptr) == -1 && guestError == failure);
    }
}
