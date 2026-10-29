#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "nid/Sha1.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(std::string(message) + " (guest errno=" + std::to_string(*__error_nid_postfix()) + ")");
    }
}

struct Object {
    std::string name;
    std::string hostName = "/anyps5-shm-";
    explicit Object(std::string suffix) : name("/anyps5-test-" + std::to_string(getpid()) + suffix) {
        const auto digest = Nid::Sha1(std::vector<std::uint8_t>(name.begin(), name.end()));
        for (const auto byte : digest) {
            hostName += "0123456789abcdef"[byte >> 4];
            hostName += "0123456789abcdef"[byte & 15];
        }
        ::shm_unlink(hostName.c_str());
    }
    ~Object() { ::shm_unlink(hostName.c_str()); }
    int Open(int flags, int mode = 0600) const { return shm_open_nid_postfix(name.c_str(), flags, mode); }
};

static void RequireError(const char* name, int flags, int error) {
    *__error_nid_postfix() = 0;
    Require(shm_open_nid_postfix(name, flags, 0600) == -1, "expected open failure");
    Require(*__error_nid_postfix() == error, "incorrect guest errno");
}

static void Run() {
    constexpr int readWrite = 2, create = 0x200, exclusive = 0x800, truncate = 0x400;
    Object object("/nested");
    RequireError(object.name.c_str(), readWrite, 2);
    *__error_nid_postfix() = 73;
    const int first = object.Open(readWrite | create | exclusive);
    Require(first >= 0, "named creation failed");
    Require(*__error_nid_postfix() == 73, "successful creation overwrote guest errno");
    Require((fcntl(first, F_GETFD) & FD_CLOEXEC) != 0, "descriptor is not close-on-exec");
    struct stat info{};
    Require(fstat(first, &info) == 0 && info.st_size == 0, "new object is not empty");
    Require(ftruncate(first, 0x4000) == 0, "resize failed");
    auto* mapped = static_cast<char*>(::mmap(nullptr, 0x4000, PROT_READ | PROT_WRITE, MAP_SHARED, first, 0));
    Require(mapped != MAP_FAILED, "host shared mapping failed");
    std::memcpy(mapped, "shared", 7);
    RequireError(object.name.c_str(), readWrite | create | exclusive, 17);
    const int reader = object.Open(truncate);
    Require(reader >= 0, "read-only reopen failed");
    Require(fstat(reader, &info) == 0 && info.st_size == 0x4000, "read-only O_TRUNC truncated the object");
    char bytes[7]{};
    Require(pread(reader, bytes, sizeof(bytes), 0) == sizeof(bytes) && std::memcmp(bytes, "shared", 7) == 0, "reopened object has different contents");
    Require(pwrite(reader, bytes, sizeof(bytes), 0) == -1, "read-only descriptor accepted a write");
    Require(::mmap(nullptr, 0x4000, PROT_READ | PROT_WRITE, MAP_SHARED, reader, 0) == MAP_FAILED, "read-only descriptor accepted a writable shared mapping");
    Object separate("/different");
    const int different = separate.Open(readWrite | create | exclusive, 0400);
    Require(different >= 0 && fstat(different, &info) == 0 && info.st_size == 0, "different names share an object");
    Require(ftruncate(different, 0x4000) == 0 && pwrite(different, "y", 1, 0) == 1, "creation permissions incorrectly restrict the new descriptor");
    Require(mapped[0] == 's', "different object write changed the first object");
    Require(close(different) == 0, "different object close failed");
    const auto child = fork();
    Require(child >= 0, "fork failed");
    if (child == 0) {
        const int reopened = object.Open(readWrite);
        char contents[7]{};
        const bool okay = reopened >= 0 && pread(reopened, contents, sizeof(contents), 0) == sizeof(contents) && std::memcmp(contents, "shared", 7) == 0;
        if (reopened >= 0) close(reopened);
        _exit(okay ? 0 : 1);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "cross-process reopen failed");
    Require(::munmap(mapped, 0x4000) == 0, "host unmap failed");
    Require(close(reader) == 0 && close(first) == 0, "close failed");
    const int truncated = object.Open(readWrite | truncate);
    Require(truncated >= 0 && fstat(truncated, &info) == 0 && info.st_size == 0, "read-write truncation failed");
    Require(close(truncated) == 0, "close after truncation failed");
    RequireError(nullptr, readWrite, 14);
    RequireError("", readWrite, 22);
    RequireError("relative", readWrite, 22);
    RequireError(object.name.c_str(), 1, 22);
    RequireError(object.name.c_str(), readWrite | 8, 22);
    const std::string tooLong(1024, '/');
    RequireError(tooLong.c_str(), readWrite, 63);
    const auto prefixLength = std::string("/anyps5-test-").size() + std::to_string(getpid()).size();
    Object longName(std::string(1023 - prefixLength, 'x'));
    Require(longName.name.size() == 1023, "incorrect boundary test length");
    const auto previousMask = umask(0077);
    const int longDescriptor = longName.Open(create | readWrite, 07777);
    umask(previousMask);
    Require(longDescriptor >= 0 && fstat(longDescriptor, &info) == 0, "long name creation failed");
    Require((info.st_mode & 07777) == 0755, "default guest umask was not applied independently of host umask");
    Require(close(longDescriptor) == 0, "long name close failed");
    const int longReader = longName.Open(0);
    Require(longReader >= 0 && close(longReader) == 0, "maximum-length name reopen failed");
    const auto* anonymous = reinterpret_cast<const char*>(1);
    const int anonymousFirst = shm_open_nid_postfix(anonymous, readWrite, 0);
    const int anonymousSecond = shm_open_nid_postfix(anonymous, readWrite | create | exclusive | truncate, 0400);
    Require(anonymousFirst >= 0 && anonymousSecond >= 0, "anonymous creation regressed");
    Require(ftruncate(anonymousFirst, 0x4000) == 0 && pwrite(anonymousFirst, "x", 1, 0) == 1, "anonymous write failed");
    Require(pread(anonymousSecond, bytes, 1, 0) == 0, "anonymous objects share contents");
    Require(close(anonymousFirst) == 0 && close(anonymousSecond) == 0, "anonymous close failed");
    RequireError(anonymous, 0, 22);
    RequireError(anonymous, 3, 22);
    std::puts("named shared-memory checks passed");
}

int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
