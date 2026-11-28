#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

constexpr int SentinelErrno = 77;
constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;
constexpr int GuestEexist = 17;
constexpr int GuestEnotdir = 20;
constexpr int GuestEinval = 22;
constexpr int GuestEnotempty = 66;

void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "GuestKernelFileErrno failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}

#define CHECK(expr) Require((expr), __LINE__)

template <typename TCall>
void CheckFails(TCall call, int expectedErrno, int line) {
    *__error_nid_postfix() = SentinelErrno;
    const auto result = call();
    Require(result == SceKernelError(expectedErrno), line);
    Require(*__error_nid_postfix() == expectedErrno, line);
}

#define CHECK_FAIL(call, err) CheckFails([&] { return (call); }, (err), __LINE__)

struct ScopedTempDirectory {
    std::filesystem::path previous;
    std::filesystem::path root;

    ScopedTempDirectory() : previous(std::filesystem::current_path()) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() / ("anyps5-kernel-file-errno-" + std::to_string(stamp));
        std::filesystem::create_directories(root);
        std::filesystem::current_path(root);
    }

    ~ScopedTempDirectory() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        std::filesystem::remove_all(root, error);
    }
};

}

int main() {
    ScopedTempDirectory sandbox;

    *__error_nid_postfix() = SentinelErrno;
    CHECK(sceKernelMkdir("dir", 0777) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    *__error_nid_postfix() = GuestEnoent;
    CHECK(sceKernelMkdir("dir", 0777) == SCE_KERNEL_ERROR_EEXIST);
    CHECK(*__error_nid_postfix() == GuestEexist);

    CHECK_FAIL(sceKernelMkdir("", 0777), GuestEnoent);
    CHECK_FAIL(sceKernelMkdir(nullptr, 0777), GuestEfault);
    CHECK_FAIL(sceKernelMkdir("missing/child", 0777), GuestEnoent);

    *__error_nid_postfix() = SentinelErrno;
    const int fd = sceKernelOpen("dir/sample.bin", SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0644);
    CHECK(fd >= 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    CHECK_FAIL(sceKernelMkdir("dir/sample.bin/child", 0777), GuestEnotdir);
    CHECK_FAIL(sceKernelOpen("missing.bin", SCE_KERNEL_O_RDONLY, 0), GuestEnoent);
    CHECK_FAIL(sceKernelOpen("dir/sample.bin", SCE_KERNEL_O_ACCMODE, 0), GuestEinval);
    CHECK_FAIL(sceKernelOpen(nullptr, SCE_KERNEL_O_RDONLY, 0), GuestEfault);

    const char payload[] = "hello";
    *__error_nid_postfix() = SentinelErrno;
    CHECK(sceKernelWrite(fd, payload, 5) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelLseek(fd, 0, 0) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    char buffer[8]{};
    CHECK(sceKernelRead(fd, buffer, 5) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelPwrite(fd, payload, 5, 0) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelPread(fd, buffer, 5, 0) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    KernelIovec readVec{buffer, 5};
    const KernelIovec writeVec{const_cast<char*>(payload), 5};
    CHECK(sceKernelPwritev(fd, &writeVec, 1, 0) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelPreadv(fd, &readVec, 1, 0) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelLseek(fd, 0, 0) == 0);
    CHECK(sceKernelWritev(fd, &writeVec, 1) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelLseek(fd, 0, 0) == 0);
    CHECK(sceKernelReadv(fd, &readVec, 1) == 5);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    FileStat status{};
    CHECK(sceKernelStat("dir/sample.bin", &status) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelFstat(fd, &status) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelChmod_nid_postfix("dir/sample.bin", 0644) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelFchmod(fd, 0644) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelFsync(fd) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelFdatasync(fd) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelUtimes_nid_postfix("dir/sample.bin", nullptr) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelFutimes(fd, nullptr) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelFtruncate(fd, 5) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    const int dupFd = sceKernelRdup(0, fd);
    CHECK(dupFd >= 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelClose(dupFd) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    CHECK_FAIL(sceKernelClose(-1), GuestEbadf);
    CHECK_FAIL(sceKernelRead(-1, buffer, 1), GuestEbadf);
    CHECK_FAIL(sceKernelWrite(-1, payload, 1), GuestEbadf);
    CHECK_FAIL(sceKernelLseek(-1, 0, 0), GuestEbadf);
    CHECK_FAIL(sceKernelPread(-1, buffer, 1, 0), GuestEbadf);
    CHECK_FAIL(sceKernelPwrite(-1, payload, 1, 0), GuestEbadf);
    CHECK_FAIL(sceKernelPread(fd, buffer, 1, -1), GuestEinval);
    CHECK_FAIL(sceKernelPwrite(fd, payload, 1, -1), GuestEinval);
    CHECK_FAIL(sceKernelReadv(-1, &readVec, 1), GuestEbadf);
    CHECK_FAIL(sceKernelWritev(-1, &writeVec, 1), GuestEbadf);
    CHECK_FAIL(sceKernelPreadv(-1, &readVec, 1, 0), GuestEbadf);
    CHECK_FAIL(sceKernelPwritev(-1, &writeVec, 1, 0), GuestEbadf);
    CHECK_FAIL(sceKernelPreadv(fd, &readVec, 1, -1), GuestEinval);
    CHECK_FAIL(sceKernelPwritev(fd, &writeVec, 1, -1), GuestEinval);
    CHECK_FAIL(sceKernelStat("missing.bin", &status), GuestEnoent);
    CHECK_FAIL(sceKernelFstat(-1, &status), GuestEbadf);
    CHECK_FAIL(sceKernelFstat(fd, nullptr), GuestEfault);
    CHECK_FAIL(sceKernelFsync(-1), GuestEbadf);
    CHECK_FAIL(sceKernelFdatasync(-1), GuestEbadf);
    CHECK_FAIL(sceKernelChmod_nid_postfix("missing.bin", 0644), GuestEnoent);
    CHECK_FAIL(sceKernelFchmod(-1, 0644), GuestEbadf);
    CHECK_FAIL(sceKernelTruncate_nid_postfix("missing.bin", 0), GuestEnoent);
    CHECK_FAIL(sceKernelFtruncate(-1, 0), GuestEbadf);
    CHECK_FAIL(sceKernelUtimes_nid_postfix("missing.bin", nullptr), GuestEnoent);
    CHECK_FAIL(sceKernelFutimes(-1, nullptr), GuestEbadf);
    CHECK_FAIL(sceKernelGetdirentries(-1, buffer, sizeof(buffer), nullptr), GuestEbadf);
    CHECK_FAIL(sceKernelGetdents(-1, buffer, sizeof(buffer)), GuestEbadf);
    CHECK_FAIL(sceKernelRdup(0, -1), GuestEbadf);

    CHECK_FAIL(sceKernelRmdir("dir"), GuestEnotempty);
    CHECK_FAIL(sceKernelRmdir("missing-dir"), GuestEnoent);
    CHECK_FAIL(sceKernelUnlink("missing.bin"), GuestEnoent);
    CHECK_FAIL(sceKernelRename("missing.bin", "moved.bin"), GuestEnoent);
    CHECK_FAIL(sceKernelRename(nullptr, "moved.bin"), GuestEfault);

    *__error_nid_postfix() = SentinelErrno;
    CHECK(sceKernelClose(fd) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelTruncate_nid_postfix("dir/sample.bin", 3) == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelRename("dir/sample.bin", "dir/moved.bin") == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelUnlink("dir/moved.bin") == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);
    CHECK(sceKernelRmdir("dir") == 0);
    CHECK(*__error_nid_postfix() == SentinelErrno);

    return 0;
}
