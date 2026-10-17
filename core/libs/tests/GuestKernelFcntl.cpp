#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI __sys_socketex_nid_postfix(const char*, int, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int APS5_VABI _fcntl_nid_postfix(int, int, ...);
int APS5_VABI ioctl_nid_postfix(int, std::uint64_t, void*);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceKernelFcntl(int, int, ...);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
}
static void Require(bool value) { if (!value) std::abort(); }
static constexpr int DuplicateCommand = 0;
static constexpr int GetDescriptorFlags = 1;
static constexpr int SetDescriptorFlags = 2;
static constexpr int GetFlags = 3;
static constexpr int SetFlags = 4;
static constexpr int OldGetLock = 7;
static constexpr int OldSetLock = 8;
static constexpr int OldSetLockWait = 9;
static constexpr int GetLock = 11;
static constexpr int SetLock = 12;
static constexpr int SetLockWait = 13;
static constexpr int ReadAhead = 15;
static constexpr int DarwinReadAhead = 16;
static constexpr int DuplicateCloseOnExec = 17;
static constexpr int ReadOnly = 0;
static constexpr int WriteOnly = 1;
static constexpr int ReadWrite = 2;
static constexpr int NonBlocking = 4;
static constexpr int Append = 8;
static constexpr int ReadAheadFlag = 0x200;
static constexpr int CreateReadWrite = 0x202;
static constexpr int CreateWriteAppend = 0x209;
static constexpr int OpenDirectory = 0x20000;
static constexpr std::int16_t ReadLock = 1;
static constexpr std::int16_t Unlock = 2;
static constexpr std::int16_t WriteLock = 3;
static constexpr std::int16_t FromStart = 0;
static constexpr std::int16_t FromCurrent = 1;
static constexpr std::int16_t FromEnd = 2;
static constexpr int GuestEbadf = 9;
static constexpr int GuestEfault = 14;
static constexpr int GuestEinval = 22;
static constexpr int GuestEnotty = 25;
static constexpr int GuestEagain = 35;
static constexpr int GuestEoverflow = 84;
static constexpr std::uint64_t NonblockingRequest = 0x8004667e;
static constexpr std::uint64_t ReadableRequest = 0x4004667f;
static constexpr std::uint64_t WindowSizeRequest = 0x40087468;
static constexpr std::uint64_t AsyncRequest = 0x8004667d;
static constexpr std::uint64_t CloseOnExecRequest = 0x20006601;
static constexpr std::uint64_t KeepOnExecRequest = 0x20006602;
static constexpr int SceEbadf = static_cast<int>(0x80020009u);
static constexpr int SceEinval = static_cast<int>(0x80020016u);
static constexpr int SceEnotsup = static_cast<int>(0x8002002du);

struct GuestLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

template <typename TCall> static bool ThrowsWith(TCall call, const std::string& prefix) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).rfind(prefix, 0) == 0;
    }
    return false;
}

static void SocketsKeepTheirPath() {
    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0);
    Require(sceKernelFcntl(socket, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(socket, SetFlags, ReadWrite | NonBlocking) == 0);
    Require(sceKernelFcntl(socket, GetFlags) == (ReadWrite | NonBlocking));
    Require(sceKernelFcntl(socket, SetFlags, 0) == 0);
    Require(sceKernelFcntl(socket, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(socket, SetFlags, 0x8000) == SceEnotsup);
    Require(sceKernelFcntl(socket, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(socket, 5) == SceEinval);
    Require(sceKernelFcntl(socket + 4096, GetFlags) == SceEbadf);
    int queued = -1;
    Require(ioctl_nid_postfix(socket, ReadableRequest, &queued) == 0 && queued == 0);
    Require(close_nid_postfix(socket) == 0);
    Require(sceKernelFcntl(socket, GetFlags) == SceEbadf);
    Require(sceKernelFcntl(-1, GetFlags) == SceEbadf);
    Require(sceKernelFcntl(12345, GetFlags) == SceEbadf);
    const int named = __sys_socketex_nid_postfix("probe", 2, 1, 0);
    Require(named >= 0 && sceKernelFcntl(named, GetFlags) == ReadWrite && close_nid_postfix(named) == 0);
}

static void FileFlagsRoundTrip() {
    const int file = sceKernelOpen("kernel_fcntl_probe.bin", CreateReadWrite, 0644);
    Require(file >= 0);
    Require(sceKernelFcntl(file, GetFlags) == ReadWrite);
    Require(fcntl_nid_postfix(file, SetFlags, ReadWrite | NonBlocking | 0x8000) == 0);
    Require(fcntl_nid_postfix(file, GetFlags) == (ReadWrite | NonBlocking));
    Require(fcntl_nid_postfix(file, SetFlags, 0) == 0);
    Require(fcntl_nid_postfix(file, GetFlags) == ReadWrite);
    Require(fcntl_nid_postfix(file, GetDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(file, SetDescriptorFlags, 1) == 0);
    Require(fcntl_nid_postfix(file, GetDescriptorFlags) == 1);
    Require(sceKernelFcntl(file, SetDescriptorFlags, 0) == 0);
    Require(sceKernelFcntl(file, GetDescriptorFlags) == 0);
    Require(ioctl_nid_postfix(file, CloseOnExecRequest, nullptr) == 0);
    Require(fcntl_nid_postfix(file, GetDescriptorFlags) == 1);
    Require(ioctl_nid_postfix(file, KeepOnExecRequest, nullptr) == 0);
    Require(fcntl_nid_postfix(file, GetDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(file, DarwinReadAhead, 1) == 0);
    Require(fcntl_nid_postfix(file, GetFlags) == (ReadWrite | ReadAheadFlag));
    Require(fcntl_nid_postfix(file, ReadAhead, -1) == 0);
    Require(fcntl_nid_postfix(file, GetFlags) == ReadWrite);
    Require(sceKernelFcntl(file, ReadAhead, 4096) == 0);
    Require(fcntl_nid_postfix(file, GetFlags) == (ReadWrite | ReadAheadFlag));
    Require(sceKernelFcntl(file, ReadAhead, -1) == 0);
    Require(sceKernelFcntl(file, GetFlags) == ReadWrite);
    Require(fcntl_nid_postfix(file, DarwinReadAhead, 1) == 0);

    const int duplicate = fcntl_nid_postfix(file, DuplicateCommand, 20);
    Require(duplicate >= 20);
    Require(fcntl_nid_postfix(duplicate, GetDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(file, SetFlags, NonBlocking) == 0);
    Require(fcntl_nid_postfix(duplicate, GetFlags) == (ReadWrite | NonBlocking));
    const int closing = fcntl_nid_postfix(file, DuplicateCloseOnExec, duplicate);
    Require(closing > duplicate);
    Require(fcntl_nid_postfix(closing, GetDescriptorFlags) == 1);
    Require(fcntl_nid_postfix(file, GetDescriptorFlags) == 0);

    Require(write_nid_postfix(file, "abcde", 5) == 5);
    Require(sceKernelLseek(file, 2, 0) == 2);
    int readable = -1;
    Require(ioctl_nid_postfix(file, ReadableRequest, &readable) == 0 && readable == 3);
    char size[8] = {};
    RequireFailure(ioctl_nid_postfix(file, WindowSizeRequest, size), GuestEnotty);
    RequireFailure(ioctl_nid_postfix(file, NonblockingRequest, nullptr), GuestEfault);
    Require(ThrowsWith([file] { int on = 1; ioctl_nid_postfix(file, AsyncRequest, &on); }, "ioctl: request 0x8004667D on a file is not implemented"));

    RequireFailure(fcntl_nid_postfix(file, DuplicateCommand, -1), GuestEinval);
    Require(sceKernelFcntl(file, DuplicateCommand, -1) == SceEinval);
    RequireFailure(fcntl_nid_postfix(file, 99), GuestEinval);
    RequireFailure(fcntl_nid_postfix(file, GetLock, nullptr), GuestEfault);
    GuestLock lock{0, 0, 0, 3, 0, 0};
    Require(fcntl_nid_postfix(file, SetLock, &lock) == 0);
    GuestLock probe{0, 0, 0, 1, 0, 0};
    Require(fcntl_nid_postfix(file, GetLock, &probe) == 0 && probe.type == 2);
    static GuestLock imageProbe{0, 0, 0, 1, 0, 0};
    Require(_fcntl_nid_postfix(file, GetLock, &imageProbe) == 0 && imageProbe.type == 2);
    Require(_fcntl_nid_postfix(file, SetDescriptorFlags, 1) == 0 && _fcntl_nid_postfix(file, GetDescriptorFlags) == 1);
    Require(_fcntl_nid_postfix(file, SetDescriptorFlags, 0) == 0);
    lock.type = 2;
    Require(fcntl_nid_postfix(file, SetLock, &lock) == 0);
    lock.type = 7;
    RequireFailure(fcntl_nid_postfix(file, SetLock, &lock), GuestEinval);

    for (const int descriptor : {closing, duplicate, file}) Require(close_nid_postfix(descriptor) == 0);
    RequireFailure(fcntl_nid_postfix(file, GetDescriptorFlags), GuestEbadf);
    RequireFailure(fcntl_nid_postfix(file, DuplicateCommand, 0), GuestEbadf);
    RequireFailure(ioctl_nid_postfix(file, ReadableRequest, &readable), GuestEbadf);
    Require(sceKernelFcntl(file, GetFlags) == SceEbadf);

    const int appending = sceKernelOpen("kernel_fcntl_probe.bin", CreateWriteAppend, 0644);
    Require(appending >= 0);
    Require(fcntl_nid_postfix(appending, GetFlags) == (WriteOnly | Append));
    Require(fcntl_nid_postfix(appending, SetFlags, Append | NonBlocking) == 0);
    Require(fcntl_nid_postfix(appending, GetFlags) == (WriteOnly | Append | NonBlocking));
#ifdef _WIN32
    Require(ThrowsWith([appending] { fcntl_nid_postfix(appending, SetFlags, 0); }, "fcntl: changing O_APPEND on a file is not implemented"));
#else
    Require(fcntl_nid_postfix(appending, SetFlags, 0) == 0);
    Require(fcntl_nid_postfix(appending, GetFlags) == WriteOnly);
#endif
    Require(ThrowsWith([appending] { fcntl_nid_postfix(appending, SetFlags, Append | 0x80); }, "fcntl: changing O_FSYNC on a file is not implemented"));
    Require(sceKernelClose(appending) == 0);
    std::remove("kernel_fcntl_probe.bin");
}

static bool NoConflict(int fd, GuestLock request) {
    const GuestLock asked = request;
    if (fcntl_nid_postfix(fd, GetLock, &request) != 0) return false;
    return request.type == Unlock && request.start == asked.start && request.length == asked.length && request.pid == asked.pid && request.whence == asked.whence && request.system == asked.system;
}

static void RecordLocksBelongToTheProcess() {
    const int file = sceKernelOpen("kernel_fcntl_locks.bin", CreateReadWrite, 0644);
    Require(file >= 0);
    Require(write_nid_postfix(file, "0123456789", 10) == 10);

    GuestLock whole{0, 0, 0, WriteLock, FromStart, 0};
    Require(fcntl_nid_postfix(file, SetLock, &whole) == 0);
    Require(NoConflict(file, {4, 2, 0, WriteLock, FromStart, 77}));
    GuestLock middle{2, 3, 0, ReadLock, FromStart, 0};
    Require(fcntl_nid_postfix(file, SetLock, &middle) == 0);
    GuestLock tail{5, 0, 0, Unlock, FromStart, 0};
    Require(fcntl_nid_postfix(file, SetLock, &tail) == 0);
    GuestLock merged{0, 10, 0, WriteLock, FromStart, 0};
    Require(fcntl_nid_postfix(file, SetLockWait, &merged) == 0);
    Require(NoConflict(file, {0, 0, 0, WriteLock, FromStart, 0}));

    const int other = sceKernelOpen("kernel_fcntl_locks.bin", ReadWrite, 0);
    Require(other >= 0);
    GuestLock same{0, 10, 0, WriteLock, FromStart, 0};
    Require(fcntl_nid_postfix(other, SetLock, &same) == 0);
    Require(fcntl_nid_postfix(other, SetLockWait, &same) == 0);
    Require(NoConflict(other, {0, 0, 0, WriteLock, FromStart, 0}));

    int threadSet = -1;
    int threadWait = -1;
    bool threadSeesNoConflict = false;
    std::thread thread([&] {
        GuestLock request{3, 4, 0, WriteLock, FromStart, 0};
        threadSet = fcntl_nid_postfix(other, SetLock, &request);
        threadWait = fcntl_nid_postfix(file, SetLockWait, &request);
        threadSeesNoConflict = NoConflict(file, {0, 0, 0, WriteLock, FromStart, 0});
    });
    thread.join();
    Require(threadSet == 0 && threadWait == 0 && threadSeesNoConflict);

    Require(sceKernelLseek(file, 6, 0) == 6);
    GuestLock current{-2, 0, 0, ReadLock, FromCurrent, 5};
    Require(fcntl_nid_postfix(file, GetLock, &current) == 0);
    Require(current.type == Unlock && current.start == 4 && current.whence == FromCurrent && current.length == 0 && current.system == 5);
    GuestLock end{-3, 0, 0, WriteLock, FromEnd, 0};
    Require(fcntl_nid_postfix(file, SetLock, &end) == 0);
    GuestLock backwards{5, -5, 0, ReadLock, FromStart, 0};
    Require(fcntl_nid_postfix(file, SetLock, &backwards) == 0);

    unsigned char old[32];
    std::memset(old, 0xa5, sizeof(old));
    GuestLock oldLock{0, 1, 0, WriteLock, FromStart, 0};
    std::memcpy(old, &oldLock, 24);
    Require(fcntl_nid_postfix(file, OldSetLock, old) == 0);
    Require(fcntl_nid_postfix(file, OldSetLockWait, old) == 0);
    Require(fcntl_nid_postfix(file, OldGetLock, old) == 0);
    std::memcpy(&oldLock, old, 24);
    Require(oldLock.type == Unlock && oldLock.start == 0 && oldLock.length == 1);
    for (std::size_t index = 24; index < sizeof(old); ++index) Require(old[index] == 0xa5);

    GuestLock badType{0, 0, 0, 0, FromStart, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &badType), GuestEinval);
    badType.type = 4;
    RequireFailure(fcntl_nid_postfix(file, SetLock, &badType), GuestEinval);
    badType.type = 7;
    RequireFailure(fcntl_nid_postfix(file, GetLock, &badType), GuestEinval);
    GuestLock negative{-1, 0, 0, WriteLock, FromStart, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &negative), GuestEinval);
    GuestLock beforeStart{0, -1, 0, WriteLock, FromStart, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &beforeStart), GuestEinval);
    GuestLock pastStart{5, -6, 0, WriteLock, FromStart, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &pastStart), GuestEinval);
    GuestLock unknownWhence{0, 0, 0, WriteLock, 3, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &unknownWhence), GuestEinval);
    Require(sceKernelFcntl(file, SetLock, &unknownWhence) == SceEinval);

    const int reader = sceKernelOpen("kernel_fcntl_locks.bin", ReadOnly, 0);
    Require(reader >= 0);
    GuestLock write{0, 0, 0, WriteLock, FromStart, 0};
    RequireFailure(fcntl_nid_postfix(reader, SetLock, &write), GuestEbadf);
    RequireFailure(fcntl_nid_postfix(reader, SetLockWait, &write), GuestEbadf);
    Require(sceKernelFcntl(reader, SetLock, &write) == SceEbadf);
    Require(NoConflict(reader, write));
    GuestLock read{0, 0, 0, ReadLock, FromStart, 0};
    Require(fcntl_nid_postfix(reader, SetLock, &read) == 0);
    const int writer = sceKernelOpen("kernel_fcntl_locks.bin", WriteOnly, 0);
    Require(writer >= 0);
    RequireFailure(fcntl_nid_postfix(writer, SetLock, &read), GuestEbadf);
    Require(fcntl_nid_postfix(writer, SetLock, &write) == 0);
    GuestLock release{0, 0, 0, Unlock, FromStart, 0};
    Require(fcntl_nid_postfix(writer, SetLock, &release) == 0);
    Require(fcntl_nid_postfix(reader, SetLock, &release) == 0);

#ifdef _WIN32
    Require(NoConflict(file, {0, 0, 0, Unlock, FromStart, 0}));
    GuestLock tooLong{std::numeric_limits<std::int64_t>::max(), 2, 0, WriteLock, FromStart, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &tooLong), GuestEoverflow);
    RequireFailure(fcntl_nid_postfix(file, GetLock, &tooLong), GuestEoverflow);
    GuestLock pastEnd{std::numeric_limits<std::int64_t>::max(), 0, 0, WriteLock, FromEnd, 0};
    RequireFailure(fcntl_nid_postfix(file, SetLock, &pastEnd), GuestEoverflow);
    GuestLock pastCurrent{std::numeric_limits<std::int64_t>::max(), 0, 0, ReadLock, FromCurrent, 0};
    Require(fcntl_nid_postfix(reader, SetLock, &pastCurrent) == 0);
    pastCurrent.type = WriteLock;
    Require(sceKernelLseek(reader, 1, 0) == 1);
    RequireFailure(fcntl_nid_postfix(reader, SetLock, &pastCurrent), GuestEoverflow);
    GuestLock writeBeforeWhence{0, 0, 0, WriteLock, 3, 0};
    RequireFailure(fcntl_nid_postfix(reader, SetLock, &writeBeforeWhence), GuestEbadf);
    RequireFailure(fcntl_nid_postfix(reader, GetLock, &writeBeforeWhence), GuestEinval);

    std::filesystem::create_directory("kernel_fcntl_locks_dir");
    const int directory = sceKernelOpen("kernel_fcntl_locks_dir", OpenDirectory, 0);
    Require(directory >= 0);
    Require(fcntl_nid_postfix(directory, SetLock, &read) == 0);
    RequireFailure(fcntl_nid_postfix(directory, SetLock, &write), GuestEbadf);
    Require(NoConflict(directory, {0, 0, 0, WriteLock, FromCurrent, 0}));
    GuestLock directoryEnd{0, 0, 0, ReadLock, FromEnd, 0};
    Require(ThrowsWith([directory, &directoryEnd] { fcntl_nid_postfix(directory, SetLock, &directoryEnd); }, "fcntl: a record lock from the end on a directory is not implemented"));
    Require(close_nid_postfix(directory) == 0);
    std::filesystem::remove("kernel_fcntl_locks_dir");
#endif

    for (const int descriptor : {writer, reader, other}) Require(close_nid_postfix(descriptor) == 0);
    Require(fcntl_nid_postfix(file, SetLock, &whole) == 0);
    Require(NoConflict(file, {0, 0, 0, WriteLock, FromStart, 0}));
    Require(close_nid_postfix(file) == 0);
    RequireFailure(fcntl_nid_postfix(file, SetLock, &whole), GuestEbadf);
    RequireFailure(fcntl_nid_postfix(file, GetLock, &whole), GuestEbadf);
    std::remove("kernel_fcntl_locks.bin");
}

static void PipeFlagsRoundTrip() {
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    Require(fcntl_nid_postfix(pipe[0], GetFlags) == ReadOnly);
    Require(fcntl_nid_postfix(pipe[1], GetFlags) == WriteOnly);
    Require(fcntl_nid_postfix(pipe[1], SetDescriptorFlags, 1) == 0);
    Require(fcntl_nid_postfix(pipe[1], GetDescriptorFlags) == 1);
    Require(fcntl_nid_postfix(pipe[0], GetDescriptorFlags) == 0);
    Require(ioctl_nid_postfix(pipe[0], CloseOnExecRequest, nullptr) == 0);
    Require(fcntl_nid_postfix(pipe[0], GetDescriptorFlags) == 1);
    Require(ioctl_nid_postfix(pipe[1], KeepOnExecRequest, nullptr) == 0);
    Require(fcntl_nid_postfix(pipe[1], GetDescriptorFlags) == 0);

    Require(fcntl_nid_postfix(pipe[0], SetFlags, fcntl_nid_postfix(pipe[0], GetFlags) | NonBlocking) == 0);
    Require(fcntl_nid_postfix(pipe[0], GetFlags) == (ReadOnly | NonBlocking));
    char buffer[8] = {};
    RequireFailure(static_cast<int>(read_nid_postfix(pipe[0], buffer, sizeof(buffer))), GuestEagain);
    Require(write_nid_postfix(pipe[1], "abc", 3) == 3);
    int readable = -1;
    Require(ioctl_nid_postfix(pipe[0], ReadableRequest, &readable) == 0 && readable == 3);
    Require(ioctl_nid_postfix(pipe[1], ReadableRequest, &readable) == 0 && readable == 0);
    Require(read_nid_postfix(pipe[0], buffer, sizeof(buffer)) == 3);
    RequireFailure(static_cast<int>(read_nid_postfix(pipe[0], buffer, sizeof(buffer))), GuestEagain);

    int off = 0;
    Require(ioctl_nid_postfix(pipe[0], NonblockingRequest, &off) == 0);
    Require(fcntl_nid_postfix(pipe[0], GetFlags) == ReadOnly);
    int on = 1;
    Require(ioctl_nid_postfix(pipe[0], NonblockingRequest, &on) == 0);
    Require(fcntl_nid_postfix(pipe[0], GetFlags) == (ReadOnly | NonBlocking));

    Require(fcntl_nid_postfix(pipe[1], SetFlags, NonBlocking) == 0);
    char block[1024] = {};
    std::int64_t written = 0;
    std::int64_t result = 0;
    while (written < (1 << 22) && (result = write_nid_postfix(pipe[1], block, sizeof(block))) > 0) written += result;
    Require(written > 0);
    RequireFailure(static_cast<int>(result), GuestEagain);

    RequireFailure(fcntl_nid_postfix(pipe[0], DarwinReadAhead, 1), GuestEbadf);
    GuestLock lock{0, 0, 0, 3, 0, 0};
    RequireFailure(fcntl_nid_postfix(pipe[1], SetLock, &lock), GuestEbadf);
    char size[8] = {};
    RequireFailure(ioctl_nid_postfix(pipe[0], WindowSizeRequest, size), GuestEnotty);
    Require(ThrowsWith([&pipe] { fcntl_nid_postfix(pipe[0], SetFlags, NonBlocking | 0x40); }, "fcntl: O_ASYNC on a pipe is not implemented"));
    for (const int descriptor : pipe) Require(close_nid_postfix(descriptor) == 0);
}

static void StandardInputFlags() {
    const int flags = fcntl_nid_postfix(0, GetFlags);
    Require(flags >= 0);
    Require((flags & 3) != WriteOnly);
    Require(fcntl_nid_postfix(0, SetFlags, fcntl_nid_postfix(0, GetFlags) & ~NonBlocking) == 0);
    Require(fcntl_nid_postfix(0, GetFlags) == (flags & ~NonBlocking));
    Require(fcntl_nid_postfix(0, SetDescriptorFlags, 1) == 0);
    Require(fcntl_nid_postfix(0, GetDescriptorFlags) == 1);
    Require(fcntl_nid_postfix(0, SetDescriptorFlags, 0) == 0);
    Require(fcntl_nid_postfix(0, GetDescriptorFlags) == 0);
}

int main() {
    SocketsKeepTheirPath();
    FileFlagsRoundTrip();
    RecordLocksBelongToTheProcess();
    PipeFlagsRoundTrip();
    StandardInputFlags();
    return 0;
}
