#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int PosixEnoent = 2;
constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

constexpr int PcLinkMax = 1;
constexpr int PcNameMax = 4;
constexpr int PcPathMax = 5;
constexpr int PcPipeBuf = 6;
constexpr int PcChownRestricted = 7;
constexpr int PcNoTrunc = 8;
constexpr int PcAllocSizeMin = 10;
constexpr int PcFileSizeBits = 12;
constexpr int PcRecIncrXferSize = 14;
constexpr int PcRecMaxXferSize = 15;
constexpr int PcRecMinXferSize = 16;
constexpr int PcRecXferAlign = 17;
constexpr int PcSymlinkMax = 18;
constexpr int PcMinHoleSize = 21;
constexpr int PcAsyncIo = 53;
constexpr int PcPrioIo = 54;
constexpr int PcSyncIo = 55;
constexpr int PcAclExtended = 59;
constexpr int PcAclPathMax = 60;
constexpr int PcMacPresent = 63;
constexpr int PcAclNfs4 = 64;

constexpr std::int64_t PosixAsynchronousIo = 200112;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest_kernel_pathconf_bounds check failed at line %u\n", location.line());
        std::abort();
    }
}

int GetError() {
    return *__error_nid_postfix();
}

void TestPathconfParameterValidation() {
    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix(nullptr, PcNameMax) == -1);
    Require(GetError() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix(nullptr, PcPathMax) == -1);
    Require(GetError() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix(nullptr, PcPipeBuf) == -1);
    Require(GetError() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix(nullptr, -1) == -1);
    Require(GetError() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix(nullptr, 9999) == -1);
    Require(GetError() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("", PcNameMax) == -1);
    Require(GetError() == PosixEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("", PcPathMax) == -1);
    Require(GetError() == PosixEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("", -1) == -1);
    Require(GetError() == PosixEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5_nonexistent_directory/nonexistent_file", PcNameMax) == -1);
    Require(GetError() == PosixEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5_nonexistent_directory/nonexistent_file", PcPathMax) == -1);
    Require(GetError() == PosixEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5_nonexistent_directory/nonexistent_file", PcPipeBuf) == -1);
    Require(GetError() == PosixEnoent);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/anyps5_nonexistent_directory/nonexistent_file", -1) == -1);
    Require(GetError() == PosixEnoent);

    Require(pathconf_nid_postfix("/", PcNameMax) == 255);
    Require(pathconf_nid_postfix("/", PcPathMax) == 1024);
    Require(pathconf_nid_postfix("/", PcPipeBuf) == 512);
    Require(pathconf_nid_postfix("/", PcLinkMax) == 32767);
    Require(pathconf_nid_postfix("/", PcChownRestricted) == 1);
    Require(pathconf_nid_postfix("/", PcNoTrunc) == 1);
    Require(pathconf_nid_postfix("/", PcFileSizeBits) == 64);
    Require(pathconf_nid_postfix("/", PcSymlinkMax) == 1024);
    Require(pathconf_nid_postfix("/", PcAllocSizeMin) == 65536);
    Require(pathconf_nid_postfix("/", PcRecIncrXferSize) == 65536);
    Require(pathconf_nid_postfix("/", PcRecMinXferSize) == 65536);
    Require(pathconf_nid_postfix("/", PcRecXferAlign) == 16384);
    Require(pathconf_nid_postfix("/", PcMinHoleSize) == 65536);
    Require(pathconf_nid_postfix("/", PcAclPathMax) == 3);
    Require(pathconf_nid_postfix("/", PcPrioIo) == 0);
    Require(pathconf_nid_postfix("/", PcSyncIo) == 0);
    Require(pathconf_nid_postfix("/", PcAclExtended) == 0);
    Require(pathconf_nid_postfix("/", PcMacPresent) == 0);
    Require(pathconf_nid_postfix("/", PcAclNfs4) == 0);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix("/", PcRecMaxXferSize) == -1);
    Require(GetError() == 0);

    for (const int invalidName : {-1, 0, 2, 3, 9, 11, 53, 61, 62, 65, 999}) {
        *__error_nid_postfix() = 0;
        Require(pathconf_nid_postfix("/", invalidName) == -1);
        Require(GetError() == PosixEinval);
    }
}

void TestFpathconfParameterValidation() {
    for (const int badFd : {-100, -1, 9999, 12345}) {
        *__error_nid_postfix() = 0;
        Require(fpathconf_nid_postfix(badFd, PcNameMax) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(_fpathconf_nid_postfix(badFd, PcNameMax) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(fpathconf_nid_postfix(badFd, PcPathMax) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(_fpathconf_nid_postfix(badFd, PcPathMax) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(fpathconf_nid_postfix(badFd, PcPipeBuf) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(_fpathconf_nid_postfix(badFd, PcPipeBuf) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(fpathconf_nid_postfix(badFd, -1) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(_fpathconf_nid_postfix(badFd, -1) == -1);
        Require(GetError() == PosixEbadf);
    }

    const int rootFd = open_nid_postfix("/", 0, 0);
    Require(rootFd >= 0);

    Require(fpathconf_nid_postfix(rootFd, PcNameMax) == 255);
    Require(_fpathconf_nid_postfix(rootFd, PcNameMax) == 255);
    Require(fpathconf_nid_postfix(rootFd, PcPathMax) == 1024);
    Require(_fpathconf_nid_postfix(rootFd, PcPathMax) == 1024);
    Require(fpathconf_nid_postfix(rootFd, PcPipeBuf) == 512);
    Require(_fpathconf_nid_postfix(rootFd, PcPipeBuf) == 512);
    Require(fpathconf_nid_postfix(rootFd, PcLinkMax) == 32767);
    Require(_fpathconf_nid_postfix(rootFd, PcLinkMax) == 32767);
    Require(fpathconf_nid_postfix(rootFd, PcFileSizeBits) == 64);
    Require(_fpathconf_nid_postfix(rootFd, PcFileSizeBits) == 64);
    Require(fpathconf_nid_postfix(rootFd, PcAsyncIo) == PosixAsynchronousIo);
    Require(_fpathconf_nid_postfix(rootFd, PcAsyncIo) == PosixAsynchronousIo);

    for (const int invalidName : {-1, 0, 2, 3, 9, 11, 61, 62, 65, 999}) {
        *__error_nid_postfix() = 0;
        Require(fpathconf_nid_postfix(rootFd, invalidName) == -1);
        Require(GetError() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(_fpathconf_nid_postfix(rootFd, invalidName) == -1);
        Require(GetError() == PosixEinval);
    }

    Require(close_nid_postfix(rootFd) == 0);

    *__error_nid_postfix() = 0;
    Require(fpathconf_nid_postfix(rootFd, PcNameMax) == -1);
    Require(GetError() == PosixEbadf);

    *__error_nid_postfix() = 0;
    Require(_fpathconf_nid_postfix(rootFd, PcNameMax) == -1);
    Require(GetError() == PosixEbadf);
}

void TestPipeFpathconfAudit() {
    int pipeEnds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeEnds) == 0);
    Require(pipeEnds[0] >= 0);
    Require(pipeEnds[1] >= 0);

    for (const int end : pipeEnds) {
        Require(fpathconf_nid_postfix(end, PcPipeBuf) == 512);
        Require(_fpathconf_nid_postfix(end, PcPipeBuf) == 512);
        Require(fpathconf_nid_postfix(end, PcAsyncIo) == PosixAsynchronousIo);
        Require(_fpathconf_nid_postfix(end, PcAsyncIo) == PosixAsynchronousIo);

        for (const int invalidName : {PcLinkMax, PcNameMax, PcPathMax, PcChownRestricted, PcNoTrunc, PcFileSizeBits, PcSymlinkMax, -1, 0, 999}) {
            *__error_nid_postfix() = 0;
            Require(fpathconf_nid_postfix(end, invalidName) == -1);
            Require(GetError() == PosixEinval);

            *__error_nid_postfix() = 0;
            Require(_fpathconf_nid_postfix(end, invalidName) == -1);
            Require(GetError() == PosixEinval);
        }
    }

    Require(close_nid_postfix(pipeEnds[0]) == 0);
    Require(close_nid_postfix(pipeEnds[1]) == 0);

    for (const int end : pipeEnds) {
        *__error_nid_postfix() = 0;
        Require(fpathconf_nid_postfix(end, PcPipeBuf) == -1);
        Require(GetError() == PosixEbadf);

        *__error_nid_postfix() = 0;
        Require(_fpathconf_nid_postfix(end, PcPipeBuf) == -1);
        Require(GetError() == PosixEbadf);
    }
}

void TestCreatedFileLifecycleAudit() {
    constexpr char TestFileName[] = "anyps5_pathconf_lifecycle_test.tmp";
    unlink_nid_postfix(TestFileName);

    const int fd = open_nid_postfix(TestFileName, 0x202, 0644);
    Require(fd >= 0);

    Require(fpathconf_nid_postfix(fd, PcNameMax) == 255);
    Require(_fpathconf_nid_postfix(fd, PcNameMax) == 255);
    Require(fpathconf_nid_postfix(fd, PcPathMax) == 1024);
    Require(_fpathconf_nid_postfix(fd, PcPathMax) == 1024);
    Require(fpathconf_nid_postfix(fd, PcPipeBuf) == 512);
    Require(_fpathconf_nid_postfix(fd, PcPipeBuf) == 512);

    Require(pathconf_nid_postfix(TestFileName, PcNameMax) == 255);
    Require(pathconf_nid_postfix(TestFileName, PcPathMax) == 1024);
    Require(pathconf_nid_postfix(TestFileName, PcPipeBuf) == 512);

    Require(close_nid_postfix(fd) == 0);

    Require(pathconf_nid_postfix(TestFileName, PcNameMax) == 255);
    Require(pathconf_nid_postfix(TestFileName, PcPathMax) == 1024);
    Require(unlink_nid_postfix(TestFileName) == 0);

    *__error_nid_postfix() = 0;
    Require(pathconf_nid_postfix(TestFileName, PcNameMax) == -1);
    Require(GetError() == PosixEnoent);
}

} // namespace

int main() {
    TestPathconfParameterValidation();
    TestFpathconfParameterValidation();
    TestPipeFpathconfAudit();
    TestCreatedFileLifecycleAudit();
    return 0;
}
