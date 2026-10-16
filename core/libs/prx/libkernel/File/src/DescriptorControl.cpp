#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/FileLock.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI dup2_nid_postfix(int from, int to);

namespace {

constexpr int GuestEintr = 4;
constexpr int GuestEbadf = 9;
constexpr int GuestEdeadlk = 11;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestEnotty = 25;
constexpr int GuestEagain = 35;
constexpr int GuestEnolck = 77;
constexpr int GuestEoverflow = 84;

constexpr int DuplicateCommand = 0;
constexpr int GetDescriptorFlags = 1;
constexpr int SetDescriptorFlags = 2;
constexpr int GetStatusFlags = 3;
constexpr int SetStatusFlags = 4;
constexpr int GetOwner = 5;
constexpr int SetOwner = 6;
constexpr int OldGetLock = 7;
constexpr int OldSetLock = 8;
constexpr int OldSetLockWait = 9;
constexpr int DuplicateToCommand = 10;
constexpr int GetLock = 11;
constexpr int SetLock = 12;
constexpr int SetLockWait = 13;
constexpr int SetLockRemote = 14;
constexpr int ReadAhead = 15;
constexpr int DarwinReadAhead = 16;
constexpr int DuplicateCloseOnExec = 17;
constexpr int DuplicateToCloseOnExec = 18;

constexpr int CloseOnExec = 1;
constexpr int OpenCloseOnExec = 0x00100000;

constexpr int ReadOnly = 0;
constexpr int WriteOnly = 1;
constexpr int ReadWrite = 2;
constexpr int Nonblocking = 0x0004;
constexpr int Append = 0x0008;
constexpr int Asynchronous = 0x0040;
constexpr int Synchronous = 0x0080;
constexpr int ReadAheadFlag = 0x0200;
constexpr int Direct = 0x00010000;
constexpr int OpenStatusFlags = Nonblocking | Append | Asynchronous | Synchronous | Direct;
constexpr int SettableStatusFlags = OpenStatusFlags | ReadAheadFlag;

constexpr std::int16_t ReadLock = 1;
constexpr std::int16_t Unlock = 2;
constexpr std::int16_t WriteLock = 3;
constexpr std::int16_t FromStart = 0;
constexpr std::int16_t FromCurrent = 1;
constexpr std::int16_t FromEnd = 2;

constexpr std::uint32_t CloseOnExecRequest = 0x20006601;
constexpr std::uint32_t KeepOnExecRequest = 0x20006602;
constexpr std::uint32_t NonblockingRequest = 0x8004667e;
constexpr std::uint32_t ReadableRequest = 0x4004667f;
constexpr std::uint32_t RequestIn = 0x80000000u;
constexpr std::uint32_t TerminalGroup = 't';

struct GuestLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};
static_assert(sizeof(GuestLock) == 32);
static_assert(offsetof(GuestLock, type) == 20);
constexpr std::size_t OldGuestLockSize = 24;

enum class Kind { File, Directory, Pipe, Device, RandomDevice, ArchivedFile };

struct Entry {
    bool closeOnExec = false;
    std::shared_ptr<int> status;
};

std::mutex g_mutex;
std::map<int, Entry> g_entries;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

const char* KindName(Kind kind) {
    switch (kind) {
        case Kind::File: return "file";
        case Kind::Directory: return "directory";
        case Kind::Pipe: return "pipe";
        case Kind::Device: return "character device";
        case Kind::RandomDevice: return "random device";
        case Kind::ArchivedFile: return "file read from app0.zar";
    }
    return "descriptor";
}

[[noreturn]] void Unsupported(const char* function, const std::string& what, Kind kind, int fd) {
    throw std::runtime_error(std::string(function) + ": " + what + " on a " + KindName(kind) + " is not implemented, fd=" + std::to_string(fd));
}

#ifdef _WIN32
std::optional<Kind> Classify(int fd, const char* function) {
    if (fd < 0 || !DescriptorIsOpen_nid_no_patch(fd)) return std::nullopt;
    if (File::DirectoryDescriptorPath(fd)) return Kind::Directory;
    if (File::IsRandomDevice(fd)) return Kind::RandomDevice;
    if (File::IsArchivedFile(fd)) return Kind::ArchivedFile;
    switch (::GetFileType(WindowsDescriptorHandle_nid_no_patch(fd))) {
        case FILE_TYPE_DISK: return Kind::File;
        case FILE_TYPE_PIPE: return Kind::Pipe;
        case FILE_TYPE_CHAR: return Kind::Device;
        default: throw std::runtime_error(std::string(function) + ": descriptor " + std::to_string(fd) + " has an unknown Windows file type, error=" + std::to_string(::GetLastError()));
    }
}

int AccessMode(int fd, const char* function) {
    return WindowsDescriptorAccess_nid_no_patch(WindowsDescriptorHandle_nid_no_patch(fd), fd, function);
}

DWORD PipeState(int fd, const char* function) {
    DWORD state = 0;
    if (!::GetNamedPipeHandleStateW(WindowsDescriptorHandle_nid_no_patch(fd), &state, nullptr, nullptr, nullptr, nullptr, 0))
        throw std::runtime_error(std::string(function) + ": cannot read the mode of pipe descriptor " + std::to_string(fd) + ", error=" + std::to_string(::GetLastError()));
    return state;
}

void SetPipeNonblocking(int fd, bool nonblocking, const char* function) {
    DWORD mode = (PipeState(fd, function) & PIPE_READMODE_MESSAGE) | (nonblocking ? PIPE_NOWAIT : PIPE_WAIT);
    if (!::SetNamedPipeHandleState(WindowsDescriptorHandle_nid_no_patch(fd), &mode, nullptr, nullptr))
        throw std::runtime_error(std::string(function) + ": cannot change the mode of pipe descriptor " + std::to_string(fd) + ", error=" + std::to_string(::GetLastError()));
}

Entry InitialEntry(int fd) {
    int status = 0;
    if (::GetFileType(WindowsDescriptorHandle_nid_no_patch(fd)) == FILE_TYPE_PIPE && (PipeState(fd, "fcntl") & PIPE_NOWAIT) != 0) status |= Nonblocking;
    return {false, std::make_shared<int>(status)};
}

void ApplyHostStatus(int fd, Kind kind, int changed, int next, const char* function) {
    if ((changed & Append) && kind == Kind::File) Unsupported(function, "changing O_APPEND", kind, fd);
    if ((changed & Nonblocking) && kind == Kind::Device) Unsupported(function, "changing O_NONBLOCK", kind, fd);
    if ((changed & Nonblocking) && kind == Kind::Pipe) SetPipeNonblocking(fd, (next & Nonblocking) != 0, function);
}

int Readable(int fd, Kind kind, const char* function) {
    if (kind == Kind::File) {
        const auto size = ::_filelengthi64(fd);
        const auto offset = ::_telli64(fd);
        if (size < 0 || offset < 0)
            throw std::runtime_error(std::string(function) + ": cannot read the size and offset of descriptor " + std::to_string(fd) + ", errno=" + std::to_string(errno));
        return static_cast<int>(size - offset);
    }
    if (AccessMode(fd, function) == WriteOnly) return 0;
    DWORD available = 0;
    if (!::PeekNamedPipe(WindowsDescriptorHandle_nid_no_patch(fd), nullptr, 0, nullptr, &available, nullptr)) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_BROKEN_PIPE) return 0;
        throw std::runtime_error(std::string(function) + ": cannot peek pipe descriptor " + std::to_string(fd) + ", error=" + std::to_string(error));
    }
    return static_cast<int>(available);
}

std::int64_t LockOffset(int fd, Kind kind) {
    if (kind == Kind::Directory) {
        const auto cursor = File::SeekDirectoryDescriptor(fd, 0, FromCurrent);
        if (!cursor) throw std::runtime_error("fcntl: cannot read the offset of directory descriptor " + std::to_string(fd));
        return *cursor;
    }
    const auto offset = ::_telli64(fd);
    if (offset < 0) throw std::runtime_error("fcntl: cannot read the offset of descriptor " + std::to_string(fd) + ", errno=" + std::to_string(errno));
    return offset;
}

std::int64_t LockFileSize(int fd, Kind kind) {
    if (kind == Kind::Directory) Unsupported("fcntl", "a record lock from the end", kind, fd);
    const auto size = ::_filelengthi64(fd);
    if (size < 0) throw std::runtime_error("fcntl: cannot read the size of descriptor " + std::to_string(fd) + ", errno=" + std::to_string(errno));
    return size;
}

int LockRangeError(int fd, Kind kind, const GuestLock& lock) {
    constexpr auto Largest = std::numeric_limits<std::int64_t>::max();
    std::int64_t start = lock.start;
    if (lock.whence == FromEnd) {
        const auto size = LockFileSize(fd, kind);
        if (lock.start > 0 && size > Largest - lock.start) return GuestEoverflow;
        start = size + lock.start;
    } else if (lock.whence != FromStart && lock.whence != FromCurrent) {
        return GuestEinval;
    }
    if (start < 0) return GuestEinval;
    if (lock.length < 0 && (start == 0 || start + lock.length < 0)) return GuestEinval;
    if (lock.length > 0 && lock.length - 1 > Largest - start) return GuestEoverflow;
    return 0;
}

int ProcessRecordLock(int fd, Kind kind, int command, GuestLock& lock, void* pointer, std::size_t size) {
    const bool known = lock.type == ReadLock || lock.type == WriteLock || lock.type == Unlock;
    if (command == GetLock && !known) return Fail(GuestEinval);
    if (lock.whence == FromCurrent) {
        const auto offset = LockOffset(fd, kind);
        if (lock.start > 0 && offset > std::numeric_limits<std::int64_t>::max() - lock.start) return Fail(GuestEoverflow);
        lock.start += offset;
    }
    if (command != GetLock) {
        if (!known) return Fail(GuestEinval);
        const int access = lock.type == Unlock ? ReadWrite : AccessMode(fd, "fcntl");
        if ((lock.type == ReadLock && access == WriteOnly) || (lock.type == WriteLock && access == ReadOnly)) return Fail(GuestEbadf);
    }
    if (const int error = LockRangeError(fd, kind, lock); error != 0) return Fail(error);
    if (command != GetLock) return 0;
    lock.type = Unlock;
    const GuestArena::HostWrite destination(pointer, size);
    if (!destination.Open()) return Fail(GuestEfault);
    std::memcpy(pointer, &lock, size);
    return 0;
}
#else
std::optional<Kind> Classify(int fd, const char* function) {
    if (fd < 0) return std::nullopt;
    struct stat status{};
    if (::fstat(fd, &status) != 0) {
        if (errno == EBADF) return std::nullopt;
        throw std::runtime_error(std::string(function) + ": fstat failed, fd=" + std::to_string(fd) + ", errno=" + std::to_string(errno));
    }
    if (File::IsRandomDevice(fd)) return Kind::RandomDevice;
    if (File::IsArchivedFile(fd)) return Kind::ArchivedFile;
    if (S_ISREG(status.st_mode)) return Kind::File;
    if (S_ISDIR(status.st_mode)) return Kind::Directory;
    if (S_ISFIFO(status.st_mode)) return Kind::Pipe;
    if (S_ISCHR(status.st_mode)) return Kind::Device;
    throw std::runtime_error(std::string(function) + ": descriptor " + std::to_string(fd) + " is neither a file, a directory, a pipe nor a character device");
}

int HostStatus(int fd, const char* function) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0) throw std::runtime_error(std::string(function) + ": host F_GETFL failed, fd=" + std::to_string(fd) + ", errno=" + std::to_string(errno));
    return flags;
}

int AccessMode(int fd, const char* function) {
    return HostStatus(fd, function) & O_ACCMODE;
}

Entry InitialEntry(int fd) {
    const int descriptorFlags = ::fcntl(fd, F_GETFD);
    if (descriptorFlags < 0) throw std::runtime_error("fcntl: host F_GETFD failed, fd=" + std::to_string(fd) + ", errno=" + std::to_string(errno));
    const int host = HostStatus(fd, "fcntl");
    int status = 0;
    if (host & O_NONBLOCK) status |= Nonblocking;
    if (host & O_APPEND) status |= Append;
    if (host & O_ASYNC) status |= Asynchronous;
    if ((host & O_SYNC) == O_SYNC) status |= Synchronous;
#ifdef O_DIRECT
    if (host & O_DIRECT) status |= Direct;
#endif
    return {(descriptorFlags & FD_CLOEXEC) != 0, std::make_shared<int>(status)};
}

void ApplyHostStatus(int fd, Kind, int changed, int next, const char* function) {
    if ((changed & (Nonblocking | Append)) == 0) return;
    const int host = HostStatus(fd, function);
    const int wanted = (host & ~(O_NONBLOCK | O_APPEND)) | ((next & Nonblocking) ? O_NONBLOCK : 0) | ((next & Append) ? O_APPEND : 0);
    if (::fcntl(fd, F_SETFL, wanted) != 0)
        throw std::runtime_error(std::string(function) + ": host F_SETFL failed, fd=" + std::to_string(fd) + ", errno=" + std::to_string(errno));
}

int Readable(int fd, Kind kind, const char* function) {
    if (kind == Kind::File) {
        struct stat status{};
        const auto offset = ::lseek(fd, 0, SEEK_CUR);
        if (::fstat(fd, &status) != 0 || offset < 0)
            throw std::runtime_error(std::string(function) + ": cannot read the size and offset of descriptor " + std::to_string(fd) + ", errno=" + std::to_string(errno));
        return static_cast<int>(status.st_size - offset);
    }
    if (AccessMode(fd, function) == WriteOnly) return 0;
    int available = 0;
    if (::ioctl(fd, FIONREAD, &available) != 0)
        throw std::runtime_error(std::string(function) + ": host FIONREAD failed, fd=" + std::to_string(fd) + ", errno=" + std::to_string(errno));
    return available;
}

int LockError(int error) {
    switch (error) {
        case EACCES:
        case EAGAIN: return GuestEagain;
        case EDEADLK: return GuestEdeadlk;
        case EINTR: return GuestEintr;
        case ENOLCK: return GuestEnolck;
        case EBADF: return GuestEbadf;
        case EINVAL: return GuestEinval;
        case EOVERFLOW: return GuestEoverflow;
        default: throw std::runtime_error("fcntl: host record lock failed, errno=" + std::to_string(error));
    }
}
#endif

Entry& Materialize(int fd) {
    const auto found = g_entries.find(fd);
    if (found != g_entries.end()) return found->second;
    return g_entries.emplace(fd, InitialEntry(fd)).first->second;
}

int ChangeStatus(int fd, Kind kind, int mask, int value, const char* function) {
    std::lock_guard lock(g_mutex);
    auto& status = *Materialize(fd).status;
    const int next = (status & ~mask) | (value & mask);
    const int changed = status ^ next;
    if ((changed & Asynchronous) && (kind == Kind::Pipe || kind == Kind::Device)) Unsupported(function, "O_ASYNC", kind, fd);
    if ((changed & Synchronous) && kind == Kind::File) Unsupported(function, "changing O_FSYNC", kind, fd);
    ApplyHostStatus(fd, kind, changed, next, function);
    status = next;
    return 0;
}

int CurrentStatus(int fd) {
    std::lock_guard lock(g_mutex);
    return *Materialize(fd).status;
}

void SetCloseOnExec(int fd, bool value) {
    std::lock_guard lock(g_mutex);
    Materialize(fd).closeOnExec = value;
}

int RecordLock(int fd, int command, std::intptr_t argument) {
    const bool old = command <= OldSetLockWait;
    const int current = old ? command + (GetLock - OldGetLock) : command;
    const std::size_t size = old ? OldGuestLockSize : sizeof(GuestLock);
    auto* pointer = reinterpret_cast<void*>(argument);
    if (pointer == nullptr) return Fail(GuestEfault);
    GuestLock lock{};
    std::memcpy(&lock, pointer, size);
    const auto kind = Classify(fd, "fcntl");
    if (!kind || *kind == Kind::Pipe) return Fail(GuestEbadf);
    if (*kind != Kind::File && *kind != Kind::Directory) Unsupported("fcntl", "a record lock", *kind, fd);
#ifdef _WIN32
    return ProcessRecordLock(fd, *kind, current, lock, pointer, size);
#else
    struct flock host{};
    if (lock.type == ReadLock) host.l_type = F_RDLCK;
    else if (lock.type == WriteLock) host.l_type = F_WRLCK;
    else if (lock.type == Unlock) host.l_type = F_UNLCK;
    else return Fail(GuestEinval);
    if (current == GetLock && lock.type == Unlock)
        throw std::runtime_error("fcntl: F_GETLK with F_UNLCK is not implemented, fd=" + std::to_string(fd));
    host.l_whence = lock.whence;
    host.l_start = static_cast<off_t>(lock.start);
    host.l_len = static_cast<off_t>(lock.length);
    const int hostCommand = current == GetLock ? F_GETLK : current == SetLock ? F_SETLK : F_SETLKW;
    if (::fcntl(fd, hostCommand, &host) != 0) return Fail(LockError(errno));
    if (current != GetLock) return 0;
    if (host.l_type == F_UNLCK) {
        if (lock.whence == FromCurrent) {
            const auto offset = ::lseek(fd, 0, SEEK_CUR);
            if (offset < 0) throw std::runtime_error("fcntl: cannot read the offset of descriptor " + std::to_string(fd) + ", errno=" + std::to_string(errno));
            lock.start += offset;
        }
        lock.type = Unlock;
    } else {
        lock.type = host.l_type == F_RDLCK ? ReadLock : WriteLock;
        lock.whence = SEEK_SET;
        lock.start = host.l_start;
        lock.length = host.l_len;
        lock.pid = host.l_pid;
        lock.system = 0;
    }
    const GuestArena::HostWrite destination(pointer, size);
    if (!destination.Open()) return Fail(GuestEfault);
    std::memcpy(pointer, &lock, size);
    return 0;
#endif
}

int ReadAheadControl(int fd, int command, std::intptr_t argument) {
    const auto kind = Classify(fd, "fcntl");
    if (!kind || *kind == Kind::Pipe) return Fail(GuestEbadf);
    if (*kind != Kind::File && *kind != Kind::ArchivedFile) return Fail(GuestEnotty);
    std::lock_guard lock(g_mutex);
    auto& status = *Materialize(fd).status;
    if (command == DarwinReadAhead || argument >= 0) status |= ReadAheadFlag;
    else status &= ~ReadAheadFlag;
    return 0;
}

}

namespace File {

FcntlArgument FcntlArgumentOf(int command) {
    switch (command) {
        case DuplicateCommand:
        case SetDescriptorFlags:
        case SetStatusFlags:
        case SetOwner:
        case DuplicateToCommand:
        case ReadAhead:
        case DarwinReadAhead:
        case DuplicateCloseOnExec:
        case DuplicateToCloseOnExec:
            return FcntlArgument::Int;
        case OldGetLock:
        case OldSetLock:
        case OldSetLockWait:
        case GetLock:
        case SetLock:
        case SetLockWait:
        case SetLockRemote:
            return FcntlArgument::Pointer;
        default:
            return FcntlArgument::None;
    }
}

int Fcntl(int fd, int command, std::intptr_t argument) {
    switch (command) {
        case DuplicateCommand:
        case DuplicateCloseOnExec: {
            if (fd < 0) return Fail(GuestEbadf);
            if (static_cast<int>(argument) < 0) return Fail(GuestEinval);
            if (!Classify(fd, "fcntl")) return Fail(GuestEbadf);
            const int duplicate = DuplicateFrom(fd, static_cast<int>(argument));
            if (duplicate >= 0 && command == DuplicateCloseOnExec) SetCloseOnExec(duplicate, true);
            return duplicate;
        }
        case DuplicateToCommand:
        case DuplicateToCloseOnExec: {
            const int duplicate = dup2_nid_postfix(fd, static_cast<int>(argument));
            if (duplicate >= 0 && command == DuplicateToCloseOnExec) SetCloseOnExec(duplicate, true);
            return duplicate;
        }
        case GetDescriptorFlags: {
            if (!Classify(fd, "fcntl")) return Fail(GuestEbadf);
            std::lock_guard lock(g_mutex);
            return Materialize(fd).closeOnExec ? CloseOnExec : 0;
        }
        case SetDescriptorFlags:
            if (!Classify(fd, "fcntl")) return Fail(GuestEbadf);
            SetCloseOnExec(fd, (argument & CloseOnExec) != 0);
            return 0;
        case GetStatusFlags: {
            if (!Classify(fd, "fcntl")) return Fail(GuestEbadf);
            const int access = AccessMode(fd, "fcntl");
            return access | CurrentStatus(fd);
        }
        case SetStatusFlags: {
            const auto kind = Classify(fd, "fcntl");
            if (!kind) return Fail(GuestEbadf);
            return ChangeStatus(fd, *kind, SettableStatusFlags, static_cast<int>(argument), "fcntl");
        }
        case GetOwner:
        case SetOwner: {
            const auto kind = Classify(fd, "fcntl");
            if (!kind) return Fail(GuestEbadf);
            Unsupported("fcntl", command == GetOwner ? "F_GETOWN" : "F_SETOWN", *kind, fd);
        }
        case OldGetLock:
        case OldSetLock:
        case OldSetLockWait:
        case GetLock:
        case SetLock:
        case SetLockWait:
            return RecordLock(fd, command, argument);
        case SetLockRemote:
            throw std::runtime_error("fcntl: F_SETLK_REMOTE needs a privilege whose state on the console is not known, fd=" + std::to_string(fd));
        case ReadAhead:
        case DarwinReadAhead:
            return ReadAheadControl(fd, command, argument);
        default:
            return Fail(GuestEinval);
    }
}

int Ioctl(int fd, std::uint64_t request, void* argument) {
    const auto command = static_cast<std::uint32_t>(request);
    const std::uint32_t length = (command >> 16) & 0x1fff;
    if ((command & RequestIn) && length != 0 && argument == nullptr) return Fail(GuestEfault);
    const auto kind = Classify(fd, "ioctl");
    if (!kind) return Fail(GuestEbadf);
    if (command == CloseOnExecRequest || command == KeepOnExecRequest) {
        SetCloseOnExec(fd, command == CloseOnExecRequest);
        return 0;
    }
    if (command == NonblockingRequest) {
        const bool enable = *static_cast<const int*>(argument) != 0;
        return ChangeStatus(fd, *kind, Nonblocking, enable ? Nonblocking : 0, "ioctl");
    }
    if (command == ReadableRequest) {
        if (*kind == Kind::RandomDevice) return Fail(GuestEnotty);
        if (*kind != Kind::File && *kind != Kind::Pipe && *kind != Kind::ArchivedFile) Unsupported("ioctl", "FIONREAD", *kind, fd);
        const auto archived = File::ArchivedFileRemaining(fd);
        const int available = archived ? static_cast<int>(std::min<std::int64_t>(*archived, std::numeric_limits<int>::max())) : Readable(fd, *kind, "ioctl");
        if (argument == nullptr) return Fail(GuestEfault);
        const GuestArena::HostWrite destination(argument, sizeof(available));
        if (!destination.Open()) return Fail(GuestEfault);
        std::memcpy(argument, &available, sizeof(available));
        return 0;
    }
    if (((command >> 8) & 0xff) == TerminalGroup) return Fail(GuestEnotty);
    char hex[11];
    std::snprintf(hex, sizeof(hex), "0x%08X", command);
    Unsupported("ioctl", std::string("request ") + hex, *kind, fd);
}

void RememberDescriptor(int fd, int openFlags) {
    std::lock_guard lock(g_mutex);
    g_entries[fd] = Entry{(openFlags & OpenCloseOnExec) != 0, std::make_shared<int>(openFlags & OpenStatusFlags)};
#ifdef _WIN32
    GuestFileLockForget_nid_no_patch(fd);
#endif
}

void ShareDescriptor(int from, int to) {
    File::ShareArchivedFile(from, to);
    std::lock_guard lock(g_mutex);
    auto status = Materialize(from).status;
    g_entries[to] = Entry{false, std::move(status)};
#ifdef _WIN32
    File::SharePositionedTransfer(from, to);
    GuestFileLockShare_nid_no_patch(from, to);
#endif
}

void ForgetDescriptor(int fd) {
    File::ForgetArchivedFile(fd);
    std::lock_guard lock(g_mutex);
    g_entries.erase(fd);
}

bool Appends(int fd) {
    std::lock_guard lock(g_mutex);
    const auto found = g_entries.find(fd);
    return found != g_entries.end() && found->second.status && (*found->second.status & Append) != 0;
}

}
