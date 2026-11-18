#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifndef _WIN32
#include <csignal>
#endif

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixSigpipe = 13;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEspipe = 29;
constexpr int PosixEpipe = 32;
constexpr int PosixEagain = 35;

constexpr int FcntlSetFl = 4;
constexpr int FcntlONonBlock = 4;
constexpr int FcntlORdWr = 2;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

void SetError(int err) {
    *__error_nid_postfix() = err;
}

struct PipeHandle {
    int readFd = -1;
    int writeFd = -1;

    PipeHandle() {
        int fds[2] = {-1, -1};
        Require(pipe_nid_postfix(fds) == 0);
        Require(fds[0] >= 0 && fds[1] >= 0);
        Require(fds[0] != fds[1]);
        readFd = fds[0];
        writeFd = fds[1];
    }

    ~PipeHandle() {
        if (readFd >= 0) {
            close_nid_postfix(readFd);
        }
        if (writeFd >= 0) {
            close_nid_postfix(writeFd);
        }
    }

    PipeHandle(const PipeHandle&) = delete;
    PipeHandle& operator=(const PipeHandle&) = delete;
};

void TestBasicWriteReadPayloadBounds() {
    PipeHandle pipe;

    Require(write_nid_postfix(pipe.writeFd, nullptr, 0) == 0);
    Require(_write_nid_postfix(pipe.writeFd, nullptr, 0) == 0);
    Require(sceKernelWrite(pipe.writeFd, nullptr, 0) == 0);

    Require(read_nid_postfix(pipe.readFd, nullptr, 0) == 0);
    Require(_read_nid_postfix(pipe.readFd, nullptr, 0) == 0);
    Require(sceKernelRead(pipe.readFd, nullptr, 0) == 0);

    const char sampleA[] = "PipePayloadAlpha123";
    constexpr std::int64_t lenA = sizeof(sampleA);
    Require(write_nid_postfix(pipe.writeFd, sampleA, lenA) == lenA);
    char bufA[32]{};
    Require(read_nid_postfix(pipe.readFd, bufA, sizeof(bufA)) == lenA);
    Require(std::memcmp(sampleA, bufA, static_cast<std::size_t>(lenA)) == 0);

    const char sampleB[] = "PipePayloadBeta456789";
    constexpr std::size_t lenB = sizeof(sampleB);
    Require(_write_nid_postfix(pipe.writeFd, sampleB, lenB) == static_cast<std::int64_t>(lenB));
    char bufB[32]{};
    Require(_read_nid_postfix(pipe.readFd, bufB, sizeof(bufB)) == static_cast<std::int64_t>(lenB));
    Require(std::memcmp(sampleB, bufB, lenB) == 0);

    const char sampleC[] = "PipePayloadGammaKernelWriteRead";
    constexpr std::size_t lenC = sizeof(sampleC);
    Require(sceKernelWrite(pipe.writeFd, sampleC, lenC) == static_cast<std::int64_t>(lenC));
    char bufC[64]{};
    Require(sceKernelRead(pipe.readFd, bufC, sizeof(bufC)) == static_cast<std::int64_t>(lenC));
    Require(std::memcmp(sampleC, bufC, lenC) == 0);
}

void TestWriteReadPointerAndSizeBounds() {
    PipeHandle pipe;

    SetError(0);
    Require(write_nid_postfix(pipe.writeFd, nullptr, 16) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(_write_nid_postfix(pipe.writeFd, nullptr, 16) == -1);
    Require(Error() == PosixEfault);

    Require(sceKernelWrite(pipe.writeFd, nullptr, 16) == SCE_KERNEL_ERROR_EFAULT);

    SetError(0);
    Require(read_nid_postfix(pipe.readFd, nullptr, 16) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(_read_nid_postfix(pipe.readFd, nullptr, 16) == -1);
    Require(Error() == PosixEfault);

    Require(sceKernelRead(pipe.readFd, nullptr, 16) == SCE_KERNEL_ERROR_EFAULT);

    for (const std::uintptr_t faultAddr : {std::uintptr_t{0x10}, ~std::uintptr_t{0xfff}}) {
        const auto* ptr = reinterpret_cast<const char*>(faultAddr);
        auto* mutPtr = reinterpret_cast<char*>(faultAddr);

        SetError(0);
        Require(write_nid_postfix(pipe.writeFd, ptr, 8) == -1);
        Require(Error() == PosixEfault);

        SetError(0);
        Require(_write_nid_postfix(pipe.writeFd, ptr, 8) == -1);
        Require(Error() == PosixEfault);

        Require(sceKernelWrite(pipe.writeFd, ptr, 8) == SCE_KERNEL_ERROR_EFAULT);

        SetError(0);
        Require(read_nid_postfix(pipe.readFd, mutPtr, 8) == -1);
        Require(Error() == PosixEfault);

        SetError(0);
        Require(_read_nid_postfix(pipe.readFd, mutPtr, 8) == -1);
        Require(Error() == PosixEfault);

        Require(sceKernelRead(pipe.readFd, mutPtr, 8) == SCE_KERNEL_ERROR_EFAULT);
    }

    char validBuf[16]{};
    SetError(0);
    Require(write_nid_postfix(pipe.writeFd, validBuf, -1) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(write_nid_postfix(pipe.writeFd, validBuf, -500) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(write_nid_postfix(pipe.writeFd, validBuf, std::numeric_limits<std::int64_t>::min()) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(read_nid_postfix(pipe.readFd, validBuf, static_cast<std::uint64_t>(-1)) == -1);
    Require(Error() == PosixEinval);
}

void TestVectorWriteReadBounds() {
    PipeHandle pipe;

    SetError(0);
    Require(writev_nid_postfix(pipe.writeFd, nullptr, 1) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(_writev_nid_postfix(pipe.writeFd, nullptr, 1) == -1);
    Require(Error() == PosixEfault);

    Require(sceKernelWritev(pipe.writeFd, nullptr, 1) == SCE_KERNEL_ERROR_EFAULT);

    SetError(0);
    Require(readv_nid_postfix(pipe.readFd, nullptr, 1) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(_readv_nid_postfix(pipe.readFd, nullptr, 1) == -1);
    Require(Error() == PosixEfault);

    Require(sceKernelReadv(pipe.readFd, nullptr, 1) == SCE_KERNEL_ERROR_EFAULT);

    char tempBuf[16]{};
    KernelIovec dummyIov[1] = {{tempBuf, sizeof(tempBuf)}};

    SetError(0);
    Require(writev_nid_postfix(pipe.writeFd, dummyIov, -1) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(_writev_nid_postfix(pipe.writeFd, dummyIov, -1) == -1);
    Require(Error() == PosixEinval);

    Require(sceKernelWritev(pipe.writeFd, dummyIov, -1) == SCE_KERNEL_ERROR_EINVAL);

    SetError(0);
    Require(readv_nid_postfix(pipe.readFd, dummyIov, -1) == -1);
    Require(Error() == PosixEinval);

    SetError(0);
    Require(_readv_nid_postfix(pipe.readFd, dummyIov, -1) == -1);
    Require(Error() == PosixEinval);

    Require(sceKernelReadv(pipe.readFd, dummyIov, -1) == SCE_KERNEL_ERROR_EINVAL);

    Require(writev_nid_postfix(pipe.writeFd, dummyIov, 0) == 0);
    Require(_writev_nid_postfix(pipe.writeFd, dummyIov, 0) == 0);
    Require(sceKernelWritev(pipe.writeFd, dummyIov, 0) == 0);

    Require(readv_nid_postfix(pipe.readFd, dummyIov, 0) == 0);
    Require(_readv_nid_postfix(pipe.readFd, dummyIov, 0) == 0);
    Require(sceKernelReadv(pipe.readFd, dummyIov, 0) == 0);

    SetError(0);
    Require(writev_nid_postfix(pipe.writeFd, dummyIov, 1025) == -1);
    Require(Error() == PosixEinval);

    Require(sceKernelWritev(pipe.writeFd, dummyIov, 1025) == SCE_KERNEL_ERROR_EINVAL);

    SetError(0);
    Require(readv_nid_postfix(pipe.readFd, dummyIov, 1025) == -1);
    Require(Error() == PosixEinval);

    Require(sceKernelReadv(pipe.readFd, dummyIov, 1025) == SCE_KERNEL_ERROR_EINVAL);

    KernelIovec badBaseIov[1] = {{nullptr, 32}};
    SetError(0);
    Require(writev_nid_postfix(pipe.writeFd, badBaseIov, 1) == -1);
    Require(Error() == PosixEfault);

    Require(sceKernelWritev(pipe.writeFd, badBaseIov, 1) == SCE_KERNEL_ERROR_EFAULT);

    SetError(0);
    Require(readv_nid_postfix(pipe.readFd, badBaseIov, 1) == -1);
    Require(Error() == PosixEfault);

    Require(sceKernelReadv(pipe.readFd, badBaseIov, 1) == SCE_KERNEL_ERROR_EFAULT);

    char part1[] = "Hello-";
    char part2[] = "Vector-";
    char part3[] = "World";
    KernelIovec writeIov[3] = {
        {part1, std::strlen(part1)},
        {part2, std::strlen(part2)},
        {part3, std::strlen(part3)}
    };
    const std::int64_t totalBytes = static_cast<std::int64_t>(std::strlen(part1) + std::strlen(part2) + std::strlen(part3));
    Require(sceKernelWritev(pipe.writeFd, writeIov, 3) == totalBytes);

    char in1[8]{};
    char in2[8]{};
    char in3[8]{};
    KernelIovec readIov[3] = {
        {in1, std::strlen(part1)},
        {in2, std::strlen(part2)},
        {in3, std::strlen(part3)}
    };
    Require(sceKernelReadv(pipe.readFd, readIov, 3) == totalBytes);
    Require(std::memcmp(in1, part1, std::strlen(part1)) == 0);
    Require(std::memcmp(in2, part2, std::strlen(part2)) == 0);
    Require(std::memcmp(in3, part3, std::strlen(part3)) == 0);
}

void TestPositionedNonSeekableBounds() {
    PipeHandle pipe;
    char buffer[16]{};
    KernelIovec iov[1] = {{buffer, sizeof(buffer)}};

    SetError(0);
    Require(pread_nid_postfix(pipe.readFd, buffer, sizeof(buffer), 0) == -1);
    Require(Error() == PosixEspipe);

    SetError(0);
    Require(pread_nid_postfix(pipe.readFd, buffer, sizeof(buffer), 128) == -1);
    Require(Error() == PosixEspipe);

    Require(sceKernelPread(pipe.readFd, buffer, sizeof(buffer), 0) == SCE_KERNEL_ERROR_ESPIPE);
    Require(sceKernelPread(pipe.readFd, buffer, sizeof(buffer), 128) == SCE_KERNEL_ERROR_ESPIPE);

    SetError(0);
    Require(pwrite_nid_disambig1_nid_postfix(pipe.writeFd, buffer, sizeof(buffer), 0) == -1);
    Require(Error() == PosixEspipe);

    SetError(0);
    Require(pwrite_nid_disambig1_nid_postfix(pipe.writeFd, buffer, sizeof(buffer), 128) == -1);
    Require(Error() == PosixEspipe);

    Require(sceKernelPwrite(pipe.writeFd, buffer, sizeof(buffer), 0) == SCE_KERNEL_ERROR_ESPIPE);
    Require(sceKernelPwrite(pipe.writeFd, buffer, sizeof(buffer), 128) == SCE_KERNEL_ERROR_ESPIPE);

    SetError(0);
    Require(preadv_nid_postfix(pipe.readFd, iov, 1, 0) == -1);
    Require(Error() == PosixEspipe);

    Require(sceKernelPreadv(pipe.readFd, iov, 1, 0) == SCE_KERNEL_ERROR_ESPIPE);

    SetError(0);
    Require(pwritev_nid_postfix(pipe.writeFd, iov, 1, 0) == -1);
    Require(Error() == PosixEspipe);

    Require(sceKernelPwritev(pipe.writeFd, iov, 1, 0) == SCE_KERNEL_ERROR_ESPIPE);

    SetError(0);
    Require(lseek_nid_postfix(pipe.readFd, 0, 0) == -1);
    Require(Error() == PosixEspipe);

    SetError(0);
    Require(lseek_nid_postfix(pipe.writeFd, 0, 0) == -1);
    Require(Error() == PosixEspipe);

    Require(sceKernelLseek(pipe.readFd, 0, 0) == SCE_KERNEL_ERROR_ESPIPE);
    Require(sceKernelLseek(pipe.writeFd, 0, 0) == SCE_KERNEL_ERROR_ESPIPE);
}

void TestClosedAndInvalidDescriptorBounds() {
    char buf[16]{};
    KernelIovec iov[1] = {{buf, sizeof(buf)}};

    const int invalidFds[] = {-1, -100, 99999};
    for (const int fd : invalidFds) {
        SetError(0);
        Require(read_nid_postfix(fd, buf, sizeof(buf)) == -1);
        Require(Error() == PosixEbadf);

        SetError(0);
        Require(_read_nid_postfix(fd, buf, sizeof(buf)) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelRead(fd, buf, sizeof(buf)) == SCE_KERNEL_ERROR_EBADF);

        SetError(0);
        Require(write_nid_postfix(fd, buf, sizeof(buf)) == -1);
        Require(Error() == PosixEbadf);

        SetError(0);
        Require(_write_nid_postfix(fd, buf, sizeof(buf)) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelWrite(fd, buf, sizeof(buf)) == SCE_KERNEL_ERROR_EBADF);

        SetError(0);
        Require(readv_nid_postfix(fd, iov, 1) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelReadv(fd, iov, 1) == SCE_KERNEL_ERROR_EBADF);

        SetError(0);
        Require(writev_nid_postfix(fd, iov, 1) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelWritev(fd, iov, 1) == SCE_KERNEL_ERROR_EBADF);
    }

    int fds[2] = {-1, -1};
    Require(pipe_nid_postfix(fds) == 0);
    Require(close_nid_postfix(fds[0]) == 0);
    Require(close_nid_postfix(fds[1]) == 0);

    for (const int fd : fds) {
        SetError(0);
        Require(read_nid_postfix(fd, buf, sizeof(buf)) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelRead(fd, buf, sizeof(buf)) == SCE_KERNEL_ERROR_EBADF);

        SetError(0);
        Require(write_nid_postfix(fd, buf, sizeof(buf)) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelWrite(fd, buf, sizeof(buf)) == SCE_KERNEL_ERROR_EBADF);

        SetError(0);
        Require(readv_nid_postfix(fd, iov, 1) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelReadv(fd, iov, 1) == SCE_KERNEL_ERROR_EBADF);

        SetError(0);
        Require(writev_nid_postfix(fd, iov, 1) == -1);
        Require(Error() == PosixEbadf);

        Require(sceKernelWritev(fd, iov, 1) == SCE_KERNEL_ERROR_EBADF);
    }
}

void TestBrokenPipeEpipeBounds() {
    PipeHandle pipe;
    Require(close_nid_postfix(pipe.readFd) == 0);
    pipe.readFd = -1;

    const char sample[] = "broken";
    constexpr std::int64_t len = sizeof(sample);

    SetError(0);
    Require(write_nid_postfix(pipe.writeFd, sample, len) == -1);
    Require(Error() == PosixEpipe);

    SetError(0);
    Require(_write_nid_postfix(pipe.writeFd, sample, len) == -1);
    Require(Error() == PosixEpipe);

    Require(sceKernelWrite(pipe.writeFd, sample, len) == SCE_KERNEL_ERROR_EPIPE);

    KernelIovec iov[1] = {{const_cast<char*>(sample), static_cast<std::size_t>(len)}};
    SetError(0);
    Require(writev_nid_postfix(pipe.writeFd, iov, 1) == -1);
    Require(Error() == PosixEpipe);

    Require(sceKernelWritev(pipe.writeFd, iov, 1) == SCE_KERNEL_ERROR_EPIPE);
}

void TestEofOnClosedWriterBounds() {
    PipeHandle pipe;
    const char data[] = "buffered";
    constexpr std::int64_t len = sizeof(data);
    Require(write_nid_postfix(pipe.writeFd, data, len) == len);

    Require(close_nid_postfix(pipe.writeFd) == 0);
    pipe.writeFd = -1;

    char readBuf[32]{};
    Require(read_nid_postfix(pipe.readFd, readBuf, sizeof(readBuf)) == len);
    Require(std::memcmp(readBuf, data, static_cast<std::size_t>(len)) == 0);

    Require(read_nid_postfix(pipe.readFd, readBuf, sizeof(readBuf)) == 0);
    Require(_read_nid_postfix(pipe.readFd, readBuf, sizeof(readBuf)) == 0);
    Require(sceKernelRead(pipe.readFd, readBuf, sizeof(readBuf)) == 0);

    KernelIovec iov[1] = {{readBuf, sizeof(readBuf)}};
    Require(readv_nid_postfix(pipe.readFd, iov, 1) == 0);
    Require(_readv_nid_postfix(pipe.readFd, iov, 1) == 0);
    Require(sceKernelReadv(pipe.readFd, iov, 1) == 0);
}

void TestNonblockingEmptyReadBounds() {
    PipeHandle pipe;
    Require(fcntl_nid_postfix(pipe.readFd, FcntlSetFl, FcntlORdWr | FcntlONonBlock) == 0);

    char buf[16]{};
    SetError(0);
    Require(read_nid_postfix(pipe.readFd, buf, sizeof(buf)) == -1);
    Require(Error() == PosixEagain);

    SetError(0);
    Require(_read_nid_postfix(pipe.readFd, buf, sizeof(buf)) == -1);
    Require(Error() == PosixEagain);

    Require(sceKernelRead(pipe.readFd, buf, sizeof(buf)) == SCE_KERNEL_ERROR_EAGAIN);

    KernelIovec iov[1] = {{buf, sizeof(buf)}};
    SetError(0);
    Require(readv_nid_postfix(pipe.readFd, iov, 1) == -1);
    Require(Error() == PosixEagain);

    Require(sceKernelReadv(pipe.readFd, iov, 1) == SCE_KERNEL_ERROR_EAGAIN);
}

} // namespace

int main() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    const auto ignore = reinterpret_cast<GuestHandler>(std::uintptr_t{1});
    signal_nid_postfix(PosixSigpipe, ignore);

    TestBasicWriteReadPayloadBounds();
    TestWriteReadPointerAndSizeBounds();
    TestVectorWriteReadBounds();
    TestPositionedNonSeekableBounds();
    TestClosedAndInvalidDescriptorBounds();
    TestBrokenPipeEpipeBounds();
    TestEofOnClosedWriterBounds();
    TestNonblockingEmptyReadBounds();
    return 0;
}
