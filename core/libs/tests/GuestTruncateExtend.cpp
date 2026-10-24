#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr int ReadOnly = 0;
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;
constexpr std::int64_t Length = 64ll * 1024 * 1024;
constexpr char FileName[] = "anyps5_truncate_extend_file";

void Cleanup() {
    unlink_nid_postfix(FileName);

}

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    Cleanup();
    std::abort();
}

std::uint64_t BytesWritten() {
#ifdef _WIN32
    IO_COUNTERS counters{};
    return GetProcessIoCounters(GetCurrentProcess(), &counters) ? counters.WriteTransferCount : 0;
#else
    return 0;
#endif
}

}

int main() {
    Cleanup();
    const int descriptor = open_nid_postfix(FileName, Create | ReadWrite, 0644);
    Require(descriptor >= 0, "open failed");
    Require(write_nid_postfix(descriptor, "head", 4) == 4, "write failed");

    const std::uint64_t before = BytesWritten();
    Require(ftruncate_nid_postfix(descriptor, Length) == 0, "ftruncate to 64 MiB failed");
    const std::uint64_t written = BytesWritten() - before;
    std::printf("extending by 64 MiB wrote %llu bytes\n", static_cast<unsigned long long>(written));
#ifdef _WIN32
    Require(written < static_cast<std::uint64_t>(Length) / 16, "extending a file must not write its new length as zero bytes: the process wrote about as many bytes as the new length");
#endif

    Require(lseek_nid_postfix(descriptor, 0, 2) == Length, "the extended file has the wrong size");
    Require(lseek_nid_postfix(descriptor, 0, 1) == Length, "ftruncate must not move the file offset");
    char head[4] = {};
    char tail[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    Require(pread_nid_postfix(descriptor, head, 4, 0) == 4 && std::memcmp(head, "head", 4) == 0, "extending must keep the existing bytes");
    Require(pread_nid_postfix(descriptor, tail, 8, Length - 8) == 8 && tail[0] == 0 && tail[7] == 0, "the extension must read as zero bytes");

    Require(ftruncate_nid_postfix(descriptor, 2) == 0, "shrinking failed");
    Require(lseek_nid_postfix(descriptor, 0, 2) == 2, "the shrunk file has the wrong size");
    Require(ftruncate_nid_postfix(descriptor, -1) == -1 && *__error_nid_postfix() == 22, "a negative length must fail with EINVAL (22)");
    Require(close_nid_postfix(descriptor) == 0, "close failed");

    const int readOnly = open_nid_postfix(FileName, ReadOnly, 0);
    Require(readOnly >= 0, "reopen failed");
    Require(ftruncate_nid_postfix(readOnly, 8) == -1 && *__error_nid_postfix() == 22, "ftruncate on a read-only descriptor must fail with EINVAL (22)");
    Require(close_nid_postfix(readOnly) == 0, "close failed");

    Require(truncate_nid_postfix(FileName, Length) == 0, "truncate to 64 MiB failed");
    const int check = open_nid_postfix(FileName, ReadOnly, 0);
    Require(check >= 0 && lseek_nid_postfix(check, 0, 2) == Length, "truncate by path left the wrong size");
    Require(close_nid_postfix(check) == 0, "close failed");
    Require(truncate_nid_postfix(FileName, -1) == -1 && *__error_nid_postfix() == 22, "truncate by path with negative length must fail with EINVAL (22)");
    Require(truncate_nid_postfix(FileName, -100) == -1 && *__error_nid_postfix() == 22, "truncate by path with negative length must fail with EINVAL (22)");
    Cleanup();
    return 0;
}
