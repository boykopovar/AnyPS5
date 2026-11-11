#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int ErrorEntryNotFound = 2;
constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorFileExists = 17;
constexpr int ErrorInvalidArgument = 22;
constexpr int ErrorNameTooLong = 63;

constexpr int FlagReadOnly = 0x0000;
constexpr int FlagWriteOnly = 0x0001;
constexpr int FlagReadWrite = 0x0002;
constexpr int FlagAccessMask = 0x0003;
constexpr int FlagCreate = 0x0200;
constexpr int FlagTruncate = 0x0400;
constexpr int FlagExclusive = 0x0800;

const char* const ShmAnonymous = reinterpret_cast<const char*>(1);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno=%d)\n", message, *__error_nid_postfix());
    std::abort();
}

bool OpenFails(const char* name, int flags, int mode, int expectedError) {
    *__error_nid_postfix() = 0;
    const int result = shm_open_nid_postfix(name, flags, mode);
    return result == -1 && *__error_nid_postfix() == expectedError;
}

bool UnlinkFails(const char* name, int expectedError) {
    *__error_nid_postfix() = 0;
    const int result = shm_unlink_nid_postfix(name);
    return result == -1 && *__error_nid_postfix() == expectedError;
}

std::string MakeTestName(const std::string& suffix) {
    return "/anyps5_shm_test_" + std::to_string(getpid_nid_postfix()) + "_" + suffix;
}

void TestNameValidation() {
    Require(OpenFails(nullptr, FlagReadWrite, 0600, ErrorFault), "shm_open nullptr must fail with EFAULT");
    Require(UnlinkFails(nullptr, ErrorFault), "shm_unlink nullptr must fail with EFAULT");

    Require(OpenFails("", FlagReadWrite, 0600, ErrorInvalidArgument), "shm_open empty name must fail with EINVAL");
    Require(UnlinkFails("", ErrorInvalidArgument), "shm_unlink empty name must fail with EINVAL");

    Require(OpenFails("non_slash_shm", FlagReadWrite, 0600, ErrorInvalidArgument), "shm_open without leading slash must fail with EINVAL");
    Require(UnlinkFails("non_slash_shm", ErrorInvalidArgument), "shm_unlink without leading slash must fail with EINVAL");

    Require(OpenFails("foo/bar", FlagReadWrite, 0600, ErrorInvalidArgument), "shm_open relative path must fail with EINVAL");
    Require(UnlinkFails("foo/bar", ErrorInvalidArgument), "shm_unlink relative path must fail with EINVAL");

    std::string tooLong(1024, 'x');
    tooLong[0] = '/';
    Require(OpenFails(tooLong.c_str(), FlagReadWrite | FlagCreate, 0600, ErrorNameTooLong), "shm_open 1024 chars must fail with ENAMETOOLONG");
    Require(UnlinkFails(tooLong.c_str(), ErrorNameTooLong), "shm_unlink 1024 chars must fail with ENAMETOOLONG");

    std::string maxName(1023, 'y');
    maxName[0] = '/';
    const int maxFd = shm_open_nid_postfix(maxName.c_str(), FlagReadWrite | FlagCreate, 0600);
    Require(maxFd >= 0, "shm_open 1023 chars creation must succeed");
    Require(shm_unlink_nid_postfix(maxName.c_str()) == 0, "shm_unlink 1023 chars must succeed");
    Require(close_nid_postfix(maxFd) == 0, "close maxFd must succeed");
}

void TestFlagValidation() {
    const std::string name = MakeTestName("flags");

    Require(OpenFails(name.c_str(), FlagWriteOnly, 0600, ErrorInvalidArgument), "shm_open O_WRONLY must fail with EINVAL");
    Require(OpenFails(name.c_str(), FlagWriteOnly | FlagCreate, 0600, ErrorInvalidArgument), "shm_open O_WRONLY|O_CREAT must fail with EINVAL");
    Require(OpenFails(name.c_str(), FlagAccessMask, 0600, ErrorInvalidArgument), "shm_open access mode 3 must fail with EINVAL");
    Require(OpenFails(name.c_str(), FlagReadWrite | 0x0008, 0600, ErrorInvalidArgument), "shm_open O_APPEND must fail with EINVAL");
    Require(OpenFails(name.c_str(), FlagReadWrite | 0x0004, 0600, ErrorInvalidArgument), "shm_open O_NONBLOCK must fail with EINVAL");
    Require(OpenFails(name.c_str(), FlagReadWrite | 0x0080, 0600, ErrorInvalidArgument), "shm_open O_SYNC must fail with EINVAL");
    Require(OpenFails(name.c_str(), FlagReadWrite | 0x1000, 0600, ErrorInvalidArgument), "shm_open O_DSYNC must fail with EINVAL");

    Require(OpenFails(ShmAnonymous, FlagReadOnly, 0600, ErrorInvalidArgument), "anonymous shm O_RDONLY must fail with EINVAL");
    Require(OpenFails(ShmAnonymous, FlagWriteOnly, 0600, ErrorInvalidArgument), "anonymous shm O_WRONLY must fail with EINVAL");
    Require(OpenFails(ShmAnonymous, FlagAccessMask, 0600, ErrorInvalidArgument), "anonymous shm access mode 3 must fail with EINVAL");
    Require(OpenFails(ShmAnonymous, FlagReadWrite | 0x0008, 0600, ErrorInvalidArgument), "anonymous shm O_APPEND must fail with EINVAL");
    Require(OpenFails(ShmAnonymous, FlagReadWrite | 0x0004, 0600, ErrorInvalidArgument), "anonymous shm O_NONBLOCK must fail with EINVAL");
}

void TestNonexistentShm() {
    const std::string nonExistent = MakeTestName("nonexistent");
    Require(OpenFails(nonExistent.c_str(), FlagReadWrite, 0600, ErrorEntryNotFound), "open nonexistent O_RDWR without O_CREAT must fail with ENOENT");
    Require(OpenFails(nonExistent.c_str(), FlagReadOnly, 0600, ErrorEntryNotFound), "open nonexistent O_RDONLY without O_CREAT must fail with ENOENT");
    Require(UnlinkFails(nonExistent.c_str(), ErrorEntryNotFound), "shm_unlink nonexistent must fail with ENOENT");
}

void TestLifecycleAndOperations() {
    const std::string objName = MakeTestName("lifecycle");
    static_cast<void>(shm_unlink_nid_postfix(objName.c_str()));

    *__error_nid_postfix() = 42;
    const int fd = shm_open_nid_postfix(objName.c_str(), FlagReadWrite | FlagCreate, 0600);
    Require(fd >= 0, "creation with O_CREAT | O_RDWR failed");
    Require(*__error_nid_postfix() == 42, "successful shm_open modified errno");

    Require(OpenFails(objName.c_str(), FlagReadWrite | FlagCreate | FlagExclusive, 0600, ErrorFileExists), "O_CREAT | O_EXCL on existing object must fail with EEXIST");

    const int fdReadWrite = shm_open_nid_postfix(objName.c_str(), FlagReadWrite, 0);
    Require(fdReadWrite >= 0 && fdReadWrite != fd, "reopen with O_RDWR failed");

    const int fdReadOnly = shm_open_nid_postfix(objName.c_str(), FlagReadOnly, 0);
    Require(fdReadOnly >= 0 && fdReadOnly != fd && fdReadOnly != fdReadWrite, "reopen with O_RDONLY failed");

    const int fdRecreate = shm_open_nid_postfix(objName.c_str(), FlagReadWrite | FlagCreate, 0600);
    Require(fdRecreate >= 0 && fdRecreate != fd, "reopen with O_CREAT without O_EXCL failed");
    Require(close_nid_postfix(fdRecreate) == 0, "close fdRecreate failed");

    Require(ftruncate_nid_postfix(fd, 0x4000) == 0, "ftruncate 16KB on shm failed");

    const char helloMsg[] = "ps5_shared_memory_payload";
    constexpr std::size_t msgLen = sizeof(helloMsg);
    Require(pwrite_nid_disambig1_nid_postfix(fd, helloMsg, msgLen, 0) == static_cast<std::int64_t>(msgLen), "pwrite at offset 0 failed");

    char readBuf[sizeof(helloMsg)]{};
    Require(pread_nid_postfix(fdReadWrite, readBuf, msgLen, 0) == static_cast<std::int64_t>(msgLen), "pread via second fd failed");
    Require(std::memcmp(readBuf, helloMsg, msgLen) == 0, "pread data mismatch on read-write reopen");

    std::memset(readBuf, 0, sizeof(readBuf));
    Require(pread_nid_postfix(fdReadOnly, readBuf, msgLen, 0) == static_cast<std::int64_t>(msgLen), "pread via read-only fd failed");
    Require(std::memcmp(readBuf, helloMsg, msgLen) == 0, "pread data mismatch on read-only reopen");

    const char endMarker[] = "tail";
    Require(pwrite_nid_disambig1_nid_postfix(fd, endMarker, 4, 0x3ffc) == 4, "pwrite near end of shm failed");
    char endBuf[4]{};
    Require(pread_nid_postfix(fdReadWrite, endBuf, 4, 0x3ffc) == 4, "pread near end of shm failed");
    Require(std::memcmp(endBuf, endMarker, 4) == 0, "pread near end data mismatch");

    Require(pread_nid_postfix(fdReadWrite, endBuf, 4, 0x4000) == 0, "pread at EOF must return 0");

    Require(pwrite_nid_disambig1_nid_postfix(fdReadOnly, helloMsg, msgLen, 0) == -1, "pwrite on read-only fd must fail");
    Require(*__error_nid_postfix() == ErrorBadDescriptor, "pwrite on read-only fd must fail with EBADF");

    Require(shm_unlink_nid_postfix(objName.c_str()) == 0, "shm_unlink on open object failed");
    Require(UnlinkFails(objName.c_str(), ErrorEntryNotFound), "shm_unlink on already-unlinked object must fail with ENOENT");

    Require(OpenFails(objName.c_str(), FlagReadWrite, 0, ErrorEntryNotFound), "reopen unlinked object without O_CREAT must fail with ENOENT");

    char stillReadable[sizeof(helloMsg)]{};
    Require(pread_nid_postfix(fd, stillReadable, msgLen, 0) == static_cast<std::int64_t>(msgLen), "pread on fd after unlink failed");
    Require(std::memcmp(stillReadable, helloMsg, msgLen) == 0, "content preserved on fd after unlink");

    Require(close_nid_postfix(fd) == 0, "close original fd failed");
    Require(close_nid_postfix(fdReadWrite) == 0, "close fdReadWrite failed");
    Require(close_nid_postfix(fdReadOnly) == 0, "close fdReadOnly failed");

    Require(close_nid_postfix(fd) == -1, "close already closed fd must fail");
    Require(*__error_nid_postfix() == ErrorBadDescriptor, "close already closed fd must set EBADF");
    Require(close_nid_postfix(-1) == -1, "close -1 must fail");
    Require(*__error_nid_postfix() == ErrorBadDescriptor, "close -1 must set EBADF");
}

void TestModes() {
    const std::array<int, 4> modes = {0600, 0644, 0666, 0777};
    for (const auto mode : modes) {
        const std::string modeObj = MakeTestName("mode_" + std::to_string(mode));
        static_cast<void>(shm_unlink_nid_postfix(modeObj.c_str()));
        const int modeFd = shm_open_nid_postfix(modeObj.c_str(), FlagReadWrite | FlagCreate, mode);
        Require(modeFd >= 0, "shm_open with mode failed");
        Require(shm_unlink_nid_postfix(modeObj.c_str()) == 0, "shm_unlink with mode failed");
        Require(close_nid_postfix(modeFd) == 0, "close modeFd failed");
    }
}

void TestAnonymousShm() {
    const int anonFd = shm_open_nid_postfix(ShmAnonymous, FlagReadWrite, 0600);
    Require(anonFd >= 0, "anonymous shm_open failed");
    Require(ftruncate_nid_postfix(anonFd, 0x1000) == 0, "ftruncate anonymous shm failed");
    const char anonData[] = "anon_content";
    Require(pwrite_nid_disambig1_nid_postfix(anonFd, anonData, sizeof(anonData), 0) == static_cast<std::int64_t>(sizeof(anonData)), "pwrite anonymous shm failed");
    char anonBuf[sizeof(anonData)]{};
    Require(pread_nid_postfix(anonFd, anonBuf, sizeof(anonData), 0) == static_cast<std::int64_t>(sizeof(anonData)), "pread anonymous shm failed");
    Require(std::memcmp(anonBuf, anonData, sizeof(anonData)) == 0, "anonymous shm data mismatch");
    Require(close_nid_postfix(anonFd) == 0, "close anonymous shm fd failed");
}

} // namespace

int main() {
    TestNameValidation();
    TestFlagValidation();
    TestNonexistentShm();
    TestLifecycleAndOperations();
    TestModes();
    TestAnonymousShm();
    return 0;
}
