#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
int APS5_VABI fcntl_nid_postfix(int descriptor, int command, ...);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "record lock owner check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int GetLock = 11;
constexpr int SetLock = 12;
constexpr std::int16_t ReadLock = 1;
constexpr std::int16_t Unlock = 2;
constexpr std::int16_t WriteLock = 3;
constexpr std::int16_t FromStart = 0;
constexpr int GuestEagain = 35;

struct GuestLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};
static_assert(sizeof(GuestLock) == 32);

const char* const FileName = "owners.bin";

pid_t HoldLocks(int ready[2], int release[2]) {
    const pid_t child = ::fork();
    Check(child >= 0);
    if (child != 0) {
        ::close(ready[1]);
        ::close(release[0]);
        return child;
    }
    ::close(ready[0]);
    ::close(release[1]);
    const int fd = ::open(FileName, O_RDWR);
    struct flock read{};
    read.l_type = F_RDLCK;
    read.l_whence = SEEK_SET;
    read.l_start = 0;
    read.l_len = 10;
    struct flock write = read;
    write.l_type = F_WRLCK;
    write.l_start = 20;
    write.l_len = 10;
    struct flock tail = write;
    tail.l_start = 50;
    tail.l_len = 0;
    const bool held = fd >= 0 && ::fcntl(fd, F_SETLK, &read) == 0 && ::fcntl(fd, F_SETLK, &write) == 0 && ::fcntl(fd, F_SETLK, &tail) == 0;
    const char state = held ? 'y' : 'n';
    if (::write(ready[1], &state, 1) != 1) ::_exit(2);
    char done = 0;
    if (::read(release[0], &done, 1) != 1) ::_exit(3);
    ::_exit(held ? 0 : 1);
}

GuestLock Query(int fd, GuestLock request) {
    Check(fcntl_nid_postfix(fd, GetLock, &request) == 0);
    return request;
}

bool Unchanged(const GuestLock& reply, const GuestLock& asked) {
    return reply.type == Unlock && reply.start == asked.start && reply.length == asked.length && reply.pid == asked.pid &&
        reply.whence == asked.whence && reply.system == asked.system;
}

bool Reports(const GuestLock& reply, std::int16_t type, std::int64_t start, std::int64_t length, pid_t owner) {
    return reply.type == type && reply.start == start && reply.length == length && reply.pid == owner && reply.whence == FromStart;
}

void OtherOwner() {
    int ready[2];
    int release[2];
    Check(::pipe(ready) == 0 && ::pipe(release) == 0);
    const pid_t child = HoldLocks(ready, release);
    char state = 0;
    Check(::read(ready[0], &state, 1) == 1 && state == 'y');

    const int fd = sceKernelOpen(FileName, SCE_KERNEL_O_RDWR, 0);
    Check(fd >= 0);

    const GuestLock readRange{0, 10, 7, Unlock, FromStart, 3};
    Check(Unchanged(Query(fd, readRange), readRange));
    Check(Reports(Query(fd, {0, 0, 0, Unlock, FromStart, 0}), WriteLock, 20, 10, child));
    Check(Reports(Query(fd, {25, 1, 0, Unlock, FromStart, 0}), WriteLock, 20, 10, child));
    Check(Reports(Query(fd, {60, 5, 0, Unlock, FromStart, 0}), WriteLock, 50, 0, child));
    const GuestLock between{10, 10, 0, Unlock, FromStart, 0};
    Check(Unchanged(Query(fd, between), between));

    const GuestLock sharedRead{0, 15, 0, ReadLock, FromStart, 0};
    Check(Unchanged(Query(fd, sharedRead), sharedRead));
    Check(Reports(Query(fd, {0, 15, 0, WriteLock, FromStart, 0}), ReadLock, 0, 10, child));
    Check(Reports(Query(fd, {22, 2, 0, ReadLock, FromStart, 0}), WriteLock, 20, 10, child));

    GuestLock blocked{5, 1, 0, WriteLock, FromStart, 0};
    *__error_nid_postfix() = 0;
    Check(fcntl_nid_postfix(fd, SetLock, &blocked) == -1 && *__error_nid_postfix() == GuestEagain);

    const char done = 'd';
    Check(::write(release[1], &done, 1) == 1);
    int status = 0;
    Check(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);

    const GuestLock afterExit{0, 0, 0, Unlock, FromStart, 0};
    Check(Unchanged(Query(fd, afterExit), afterExit));
    Check(sceKernelClose(fd) == 0);
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-record-lock-owners-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    { std::ofstream file(FileName, std::ios::binary); file << std::string(100, 'x'); }
    OtherOwner();
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
