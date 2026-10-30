#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libSceNet/libSceNet.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <optional>
#include <barrier>
#include <filesystem>
#include <fstream>
#include "prx/libc/include/FileStream.hpp"

#ifdef _WIN32
#include <io.h>
#include "prx/libc/include/WindowsDescriptor.hpp"
static int NativeDescriptor(std::FILE* file) { return ::_fileno(file); }
static int NativeDuplicate(int descriptor) { return ::_dup(descriptor); }
static void NativeClose(int descriptor) { ::_close(descriptor); }
static int NativeReplace(int from, int to) { return ::_dup2(from, to); }
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
static int NativeDescriptor(std::FILE* file) { return ::fileno(file); }
static int NativeDuplicate(int descriptor) { return ::dup(descriptor); }
static void NativeClose(int descriptor) { ::close(descriptor); }
static int NativeReplace(int from, int to) { return ::dup2(from, to) < 0 ? -1 : 0; }
#endif

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s, guest errno %d, host errno %d\n", message, *__error_nid_postfix(), errno);
    std::exit(1);
}

static std::FILE* OpenTemporary() {
    auto* file = std::tmpfile();
    Require(file != nullptr, "create file");
    Require(std::fwrite("xyz", 1, 3, file) == 3 && std::fflush(file) == 0 && std::fseek(file, 0, SEEK_SET) == 0, "seed file");
    return file;
}

static void Capacity() {
    auto* file = OpenTemporary();
    const int descriptor = NativeDescriptor(file);
    int sockets[2]{};
    Require(socketpair_nid_postfix(1, 1, 0, sockets) == 0, "create socket pair");
    Require(send_nid_postfix(sockets[1], "s", 1, 0) == 1, "seed socket");
    scePthreadSelf();
    std::vector<int> occupied;
    occupied.reserve(1024);
#ifdef _WIN32
    const int previous = ::_getmaxstdio();
    Require(::_setmaxstdio(512) == 512, "bound descriptor table");
#else
    rlimit previous{};
    Require(::getrlimit(RLIMIT_NOFILE, &previous) == 0, "read descriptor limit");
    auto limit = previous;
    limit.rlim_cur = 64;
    Require(::setrlimit(RLIMIT_NOFILE, &limit) == 0, "bound descriptor table");
#endif
    for (int copy; (copy = NativeDuplicate(descriptor)) >= 0;) occupied.push_back(copy);
    Require(errno == EMFILE, "fill descriptor table");
    char byte = 0;
    Require(read_nid_postfix(descriptor, &byte, 1) == 1 && byte == 'x', "read at capacity");
    KernelIovec vector{&byte, 1};
    Require(readv_nid_postfix(descriptor, &vector, 1) == 1 && byte == 'y', "readv at capacity");
    Require(pread_nid_postfix(descriptor, &byte, 1, 2) == 1 && byte == 'z', "pread at capacity");
    Require(preadv_nid_postfix(descriptor, &vector, 1, 0) == 1 && byte == 'x', "preadv at capacity");
    Require(read_nid_postfix(descriptor, &byte, 0) == 0, "zero read at capacity");
    Require(read_nid_postfix(sockets[0], &byte, 1) == 1 && byte == 's', "socket read at capacity");
    for (const int copy : occupied) NativeClose(copy);
#ifdef _WIN32
    Require(::_setmaxstdio(previous) == previous, "restore descriptor table");
#else
    Require(::setrlimit(RLIMIT_NOFILE, &previous) == 0, "restore descriptor limit");
#endif
    Require(close_nid_postfix(sockets[0]) == 0 && close_nid_postfix(sockets[1]) == 0, "close sockets");
    Require(std::fclose(file) == 0, "close file");
}

template <class TPredicate>
static bool Observe(TPredicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

struct BlockedRead {
    int descriptor;
    char byte = 0;
    std::int64_t result = -2;
};

static void* APS5_VABI Reader(void* argument) {
    auto& read = *static_cast<BlockedRead*>(argument);
    read.result = read_nid_postfix(read.descriptor, &read.byte, 1);
    return nullptr;
}

static std::array<bool, 1024> OpenDescriptors() {
    std::array<bool, 1024> result{};
    for (std::size_t descriptor = 0; descriptor < result.size(); ++descriptor) {
#ifdef _WIN32
        result[descriptor] = WindowsDescriptorHandle_nid_no_patch(static_cast<int>(descriptor)) != INVALID_HANDLE_VALUE;
#else
        result[descriptor] = ::fcntl(static_cast<int>(descriptor), F_GETFD) >= 0;
#endif
    }
    return result;
}

static void NoAliases() {
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create alias probe pipe");
    Require(fcntl_nid_postfix(pipe[0], 2, 1) == 0 && fcntl_nid_postfix(pipe[1], 2, 1) == 0, "set close-on-exec originals");
    BlockedRead read{pipe[0]};
    const auto before = OpenDescriptors();
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, Reader, &read, nullptr) == 0, "start alias probe");
    const bool registered = Observe([&] {
        std::lock_guard lock(reader->cancelLock);
        return reader->cancelIo != nullptr;
    });
    const auto during = OpenDescriptors();
    Require(write_nid_postfix(pipe[1], "x", 1) == 1, "release alias probe");
    Require(Observe([&] { return reader->_finished.load(); }), "alias probe did not finish");
    Require(scePthreadJoin(reader, nullptr) == 0, "join alias probe");
    Require(registered && read.result == 1 && read.byte == 'x', "alias probe did not read");
    Require(before == during, "blocked read allocated a publicly reusable or inheritable descriptor");
    Require(before == OpenDescriptors(), "read changed descriptor ownership");
    Require(fcntl_nid_postfix(pipe[0], 1) == 1 && fcntl_nid_postfix(pipe[1], 1) == 1, "read changed close-on-exec flags");
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0, "close alias probe pipe");
}

#ifdef _WIN32
static void AppendOwnership(bool stream) {
    auto* file = OpenTemporary();
    const int original = NativeDescriptor(file);
    const int descriptor = dup_nid_postfix(original);
    Require(descriptor >= 0, "duplicate append ownership file");
    const auto before = WindowsDescriptorHandle_nid_no_patch(descriptor);
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    FileStream* converted = nullptr;
    std::thread changer;
    bool premature = false;
    {
        const GuestDescriptorAccess admission(original, GuestDescriptorAccess::Mode::Read);
        const GuestDescriptorAccess aliasAdmission(descriptor, GuestDescriptorAccess::Mode::Read);
        changer = std::thread([&] {
            started.store(true);
            if (stream) {
                converted = fdopen_nid_postfix(descriptor, "a+");
                Require(converted != nullptr, "append fdopen");
            } else Require(fcntl_nid_postfix(original, 4, 8) == 0, "change append status");
            finished.store(true);
        });
        Require(Observe([&] { return started.load(); }), "start append replacement");
        premature = Observe([&] { return finished.load(); });
        Require(WindowsDescriptorHandle_nid_no_patch(descriptor) == before, "append replaced an admitted handle");
        Require(fcntl_nid_postfix(original, 3) >= 0, "append waiter held the status table lock");
    }
    Require(Observe([&] { return finished.load(); }), "append replacement did not resume");
    changer.join();
    Require(!premature, "append replacement bypassed read admission");
    if (converted) Require(fclose_nid_postfix(converted) == 0, "close converted stream");
    else Require(close_nid_postfix(descriptor) == 0, "close append alias");
    Require(std::fclose(file) == 0, "close append file");
}
#endif

static bool Registered(Pthread reader) {
    return Observe([&] {
        std::lock_guard lock(reader->cancelLock);
        return reader->cancelIo != nullptr;
    });
}

struct CloseRead {
    int descriptor;
    int api;
    bool disabled;
    PthreadKey key;
    std::int64_t result = -2;
    int error = 0;
    std::atomic<bool> returned{false};
    std::atomic<int> cleanups{0}, destructors{0};
    std::array<char, 4> data{};
};

static void APS5_VABI CloseCleanup(void* argument) { ++static_cast<CloseRead*>(argument)->cleanups; }
static void APS5_VABI CloseDestructor(void* argument) { ++static_cast<CloseRead*>(argument)->destructors; }

static void* APS5_VABI ReadUntilClose(void* argument) {
    auto& read = *static_cast<CloseRead*>(argument);
    if (read.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable close reader cancellation");
    Require(scePthreadSetspecific(read.key, &read) == 0, "set close reader TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(CloseCleanup, &read, &cleanup);
    char byte = 0;
    KernelIovec vector{&byte, 1};
    GuestMsghdr message{};
    message.iov = &vector;
    message.iovLength = 1;
    if (read.api == 0) read.result = read_nid_postfix(read.descriptor, &byte, 1);
    else if (read.api == 1) read.result = readv_nid_postfix(read.descriptor, &vector, 1);
    else if (read.api == 2) read.result = recv_nid_postfix(read.descriptor, &byte, 1, 0);
    else if (read.api == 3) read.result = recvmsg_nid_postfix(read.descriptor, &message, 0);
    else read.result = recv_nid_postfix(read.descriptor, read.data.data(), read.data.size(), 0x40);
    read.error = *__error_nid_postfix();
    read.returned.store(true);
    if (read.disabled) {
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable close reader cancellation");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return nullptr;
}

static void PosixClose(bool socket, int api, bool disabled, int entry = 0) {
    int descriptors[2]{};
    Require((socket ? socketpair_nid_postfix(1, 1, 0, descriptors) : pipe_nid_postfix(descriptors)) == 0, "create close-read transport");
    const int alias = dup_nid_postfix(descriptors[0]);
    Require(alias >= 0, "duplicate close-read endpoint");
    PthreadKey key = -1;
    Require(scePthreadKeyCreate(&key, reinterpret_cast<pthread_key_destructor_func_t>(CloseDestructor)) == 0, "create close reader TSD key");
    CloseRead read{descriptors[0], api, disabled, key};
    if (api == 4) {
        Require(write_nid_postfix(descriptors[1], "p", 1) == 1, "seed partial close read");
        Require(Observe([&] {
            int available = -1;
            Require(ioctl_nid_postfix(descriptors[0], 0x4004667f, &available) == 0, "inspect partial input");
            return available == 1;
        }), "partial input did not arrive");
    }
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, ReadUntilClose, &read, nullptr) == 0, "start close reader");
    const bool admitted = Observe([&] {
        GuestDescriptorAccess probe(descriptors[0], GuestDescriptorAccess::Mode::Update, std::defer_lock);
        return !probe.TryLock();
    });
    if (api == 4) Require(Observe([&] {
        int available = -1;
        Require(ioctl_nid_postfix(descriptors[0], 0x4004667f, &available) == 0, "inspect partial progress");
        return available == 0;
    }), "WAITALL read did not consume its partial input");
    if (disabled) Require(pthread_cancel_nid_postfix(reader) == 0, "leave cancellation pending in close reader");
    std::atomic<bool> closed{false};
    std::thread closer([&] {
        Require((entry == 1 ? GuestSockets::Close(descriptors[0]) : entry == 2 ? sceKernelClose(descriptors[0]) : close_nid_postfix(descriptors[0])) == 0, "close active read descriptor");
        closed.store(true);
    });
    const bool prompt = Observe([&] { return closed.load(); });
    if (!prompt) Require(write_nid_postfix(descriptors[1], "xxxx", api == 4 ? 4 : 1) == (api == 4 ? 4 : 1), "release failed close reader");
    Require(Observe([&] { return reader->_finished.load(); }), "close reader did not finish");
    void* result = nullptr;
    Require(scePthreadJoin(reader, &result) == 0, "join close reader");
    closer.join();
    Require(admitted && prompt && read.returned.load() && (api == 4 ? read.result == 1 && read.data[0] == 'p' : read.result == -1 && read.error == 4), "close waited for external input or returned the wrong read result");
    Require(result == (disabled ? GUEST_CANCELED : nullptr) && read.cleanups.load() == 1 && read.destructors.load() == 1, "close reader lost pending cancellation or repeated cleanup/TSD");
    Require(write_nid_postfix(descriptors[1], "a", 1) == 1, "write surviving alias");
    char byte = 0;
    Require(read_nid_postfix(alias, &byte, 1) == 1 && byte == 'a', "close shut down a surviving alias");
    Require(dup2_nid_postfix(alias, descriptors[0]) == descriptors[0], "reuse closed descriptor");
    Require(write_nid_postfix(descriptors[1], "b", 1) == 1 && read_nid_postfix(descriptors[0], &byte, 1) == 1 && byte == 'b', "old close interruption affected reused descriptor");
    Require(close_nid_postfix(descriptors[0]) == 0 && close_nid_postfix(alias) == 0 && close_nid_postfix(descriptors[1]) == 0, "close close-read endpoints");
    Require(scePthreadKeyDelete(key) == 0, "delete close reader TSD key");
}

#ifdef _WIN32
static void CloseNativeErrors() {
    int descriptors[2]{};
    Require(pipe_nid_postfix(descriptors) == 0, "create native-error close pipe");
    std::atomic<bool> admitted{false};
    std::thread reader([&] {
        bool called = false;
        const auto result = ThreadCancel::Io([&] { return File::WithIoDescriptor(descriptors[0], [&](int) -> std::int64_t {
            admitted.store(true);
            Require(Observe([] { return GuestDescriptorAccess::Operation::CurrentInterrupted(); }), "close did not interrupt pre-call reader");
            _doserrno = ERROR_NO_DATA;
            SetLastError(ERROR_HANDLE_EOF);
            const auto transferred = ThreadCancel::Native([&] { called = true; return std::int64_t{0}; });
            const DWORD error = GetLastError();
            Require(!called && transferred == -1 && errno == EINTR && _doserrno == ERROR_OPERATION_ABORTED && error == ERROR_OPERATION_ABORTED, "pre-call close interruption left stale native errors");
            return transferred;
        }); });
        Require(result == -1, "pre-call close read returned success");
    });
    Require(Observe([&] { return admitted.load(); }), "pre-call reader did not enter");
    Require(close_nid_postfix(descriptors[0]) == 0, "close pre-call descriptor");
    reader.join();
    Require(close_nid_postfix(descriptors[1]) == 0, "close pre-call peer");
}
#endif

static void CloseUnregistered(bool socket) {
    int descriptors[2]{};
    Require((socket ? socketpair_nid_postfix(1, 1, 0, descriptors) : pipe_nid_postfix(descriptors)) == 0, "create unregistered close transport");
    BlockedRead read{descriptors[0]};
    std::thread reader([&] { Reader(&read); });
    const bool admitted = Observe([&] {
        GuestDescriptorAccess probe(descriptors[0], GuestDescriptorAccess::Mode::Update, std::defer_lock);
        return !probe.TryLock();
    });
    std::atomic<bool> closed{false};
    std::thread closer([&] {
        Require(close_nid_postfix(descriptors[0]) == 0, "close unregistered read");
        closed.store(true);
    });
    const bool prompt = Observe([&] { return closed.load(); });
    if (!prompt) Require(write_nid_postfix(descriptors[1], "x", 1) == 1, "release unregistered close failure");
    closer.join();
    reader.join();
    Require(admitted && prompt && read.result == -1, "close did not interrupt an unregistered reader");
    Require(close_nid_postfix(descriptors[1]) == 0, "close unregistered peer");
}

static void CloseReaders(bool socket) {
    int descriptors[2]{};
    Require((socket ? socketpair_nid_postfix(1, 1, 0, descriptors) : pipe_nid_postfix(descriptors)) == 0, "create multiple-reader close transport");
    std::array<BlockedRead, 3> reads{{{descriptors[0]}, {descriptors[0]}, {descriptors[0]}}};
    std::array<Pthread, 3> readers{};
    for (std::size_t index = 0; index < readers.size(); ++index) {
        Require(scePthreadCreate(&readers[index], nullptr, Reader, &reads[index], nullptr) == 0, "start close cohort reader");
        Require(Registered(readers[index]), "close cohort reader did not register");
    }
    std::atomic<bool> closed{false};
    std::thread closer([&] {
        Require(close_nid_postfix(descriptors[0]) == 0, "close multiple readers");
        closed.store(true);
    });
    const bool prompt = Observe([&] { return closed.load(); });
    if (!prompt) Require(write_nid_postfix(descriptors[1], "xxx", 3) == 3, "release multiple-reader close failure");
    closer.join();
    for (std::size_t index = 0; index < readers.size(); ++index) {
        Require(Observe([&] { return readers[index]->_finished.load(); }), "close cohort did not finish");
        Require(scePthreadJoin(readers[index], nullptr) == 0 && reads[index].result == -1, "close cohort read did not fail");
    }
    Require(prompt && close_nid_postfix(descriptors[1]) == 0, "close failed to wake every admitted reader");
}

static void InvalidDuplicate() {
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create invalid duplicate pipe");
    BlockedRead read{pipe[0]};
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, Reader, &read, nullptr) == 0, "start invalid duplicate reader");
    Require(Registered(reader), "invalid duplicate reader did not block");
    const int closed = dup_nid_postfix(pipe[1]);
    Require(closed >= 0 && close_nid_postfix(closed) == 0, "create closed duplicate source");
    std::atomic<bool> finished{false};
    std::thread duplicator([&] {
        Require(dup2_nid_postfix(closed, pipe[0]) == -1 && *__error_nid_postfix() == 9, "invalid duplicate result");
        finished.store(true);
    });
    const bool prompt = Observe([&] { return finished.load(); });
    Require(write_nid_postfix(pipe[1], "x", 1) == 1, "complete invalid duplicate reader");
    duplicator.join();
    Require(Observe([&] { return reader->_finished.load(); }), "invalid duplicate reader did not finish");
    Require(scePthreadJoin(reader, nullptr) == 0, "join invalid duplicate reader");
    Require(prompt && read.result == 1 && read.byte == 'x', "invalid source waited for target admission");
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0, "close invalid duplicate pipe");
}

static void AccessCancellation(bool positionedWait = false) {
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create access probe pipe");
    std::optional<GuestDescriptorAccess> changing;
    std::FILE* file = nullptr;
    int input = pipe[0];
#ifdef _WIN32
    std::optional<File::PositionedTransferLock> positioned;
    if (positionedWait) {
        file = OpenTemporary();
        input = NativeDescriptor(file);
        positioned.emplace(File::LockPositionedTransfer(input));
    } else
#endif
        changing.emplace(input, GuestDescriptorAccess::Mode::Change);
    BlockedRead read{input};
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, Reader, &read, nullptr) == 0, "start access probe");
    const bool registered = Registered(reader);
    std::atomic<bool> canceled{false};
    std::thread requester([&] {
        Require(pthread_cancel_nid_postfix(reader) == 0, "cancel access probe");
        canceled.store(true);
    });
    const bool prompt = Observe([&] { return canceled.load(); });
    changing.reset();
#ifdef _WIN32
    positioned.reset();
#endif
    if (!prompt && !positionedWait) Require(write_nid_postfix(pipe[1], "x", 1) == 1, "release failed access probe");
    requester.join();
    Require(Observe([&] { return reader->_finished.load(); }), "access probe did not finish");
    void* result = nullptr;
    Require(scePthreadJoin(reader, &result) == 0, "join access probe");
    Require(registered && prompt && result == GUEST_CANCELED && read.result == -2, "descriptor access did not cancel while exclusive access was held");
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0, "close access probe pipe");
    if (file) Require(std::fclose(file) == 0, "close positioned probe file");
}

static void ProtectedClose() {
    const int saved = NativeDuplicate(0);
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create protected-stream pipe");
    Require(NativeReplace(pipe[0], 0) == 0, "replace stdin for protected close");
    BlockedRead read{0};
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, Reader, &read, nullptr) == 0, "start protected-stream reader");
    const bool registered = Registered(reader);
    std::atomic<bool> posixDone{false}, sceDone{false};
    int posixResult = 0, posixError = 0, sceResult = 0;
    std::thread posix([&] {
        posixResult = close_nid_postfix(0);
        posixError = *__error_nid_postfix();
        posixDone.store(true);
    });
    std::thread sce([&] {
        sceResult = sceKernelClose(0);
        sceDone.store(true);
    });
    const bool prompt = Observe([&] { return posixDone.load() && sceDone.load(); });
    const bool blocked = !reader->_finished.load();
    Require(write_nid_postfix(pipe[1], "x", 1) == 1, "release protected-stream reader");
    Require(Observe([&] { return reader->_finished.load(); }), "protected-stream reader did not finish");
    Require(scePthreadJoin(reader, nullptr) == 0, "join protected-stream reader");
    posix.join();
    sce.join();
    if (saved >= 0) {
        Require(NativeReplace(saved, 0) == 0, "restore stdin");
        NativeClose(saved);
    } else NativeClose(0);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0, "close protected-stream pipe");
    Require(registered && prompt && blocked && read.result == 1, "protected close waited for input");
    Require(posixResult == -1 && posixError == 1 && sceResult == static_cast<int>(0x80020001u), "protected close returned wrong error");
}

struct StreamRead {
    FileStream* stream;
    int descriptor;
    std::atomic<bool> held{false};
    std::atomic<bool> go{false};
    std::atomic<bool> returned{false};
    std::atomic<int> unlocks{0};
    std::int64_t result = -2;
};

static void APS5_VABI UnlockStream(void* argument) {
    auto& read = *static_cast<StreamRead*>(argument);
    funlockfile_nid_postfix(read.stream);
    ++read.unlocks;
}

static void* APS5_VABI StreamReader(void* argument) {
    auto& read = *static_cast<StreamRead*>(argument);
    flockfile_nid_postfix(read.stream);
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(UnlockStream, &read, &cleanup);
    read.held.store(true);
    Require(Observe([&] { return read.go.load(); }), "release stdio reader");
    char byte = 0;
    read.result = read_nid_postfix(read.descriptor, &byte, 1);
    Require(read.result == 1 && byte == 'x', "stdio reader lost input");
    read.returned.store(true);
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return nullptr;
}

static void StreamOrder(bool reopen) {
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create stdio ordering pipe");
    Require(write_nid_postfix(pipe[1], "x", 1) == 1, "seed stdio ordering pipe");
#ifdef _WIN32
    auto* native = ::_fdopen(pipe[0], "rb");
#else
    auto* native = ::fdopen(pipe[0], "rb");
#endif
    Require(native != nullptr, "open stdio ordering stream");
    FileStream stream(native);
    const auto path = std::filesystem::path("io-stdio-order-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp");
    if (reopen) {
        std::ofstream output(path, std::ios::binary);
        output.put('r');
        Require(output.good(), "create reopen destination");
    }
    StreamRead read{&stream, pipe[0]};
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, StreamReader, &read, nullptr) == 0, "start stdio ordering reader");
    Require(Observe([&] { return read.held.load(); }), "stdio reader did not acquire FILE lock");
    std::atomic<bool> started{false}, done{false};
    bool changed = false;
    std::thread closer([&] {
        started.store(true);
        changed = reopen ? freopen_nid_postfix(path.string().c_str(), "rb", &stream) == &stream : fclose_nid_postfix(&stream) == 0;
        done.store(true);
    });
    Require(Observe([&] { return started.load(); }), "stdio closer did not start");
    const auto observation = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    while (std::chrono::steady_clock::now() < observation) {
        GuestDescriptorAccess access(pipe[0], GuestDescriptorAccess::Mode::Read, std::defer_lock);
        if (!access.TryLock()) break;
        std::this_thread::yield();
    }
    read.go.store(true);
    const bool progressed = Observe([&] { return read.returned.load(); });
    if (!progressed) Require(pthread_cancel_nid_postfix(reader) == 0, "release failed stdio ordering reader");
    Require(Observe([&] { return reader->_finished.load(); }), "stdio ordering reader did not finish");
    Require(scePthreadJoin(reader, nullptr) == 0, "join stdio ordering reader");
    Require(Observe([&] { return done.load(); }), "stdio close or reopen did not finish");
    closer.join();
    if (reopen && changed) Require(fclose_nid_postfix(&stream) == 0, "close reopened stream");
    Require(close_nid_postfix(pipe[1]) == 0, "close stdio ordering writer");
    if (reopen) Require(std::filesystem::remove(path), "remove reopen destination");
    Require(progressed && changed && read.unlocks.load() == 1, "stdio FILE and descriptor locks inverted");
}

#ifdef __linux__
static std::uint64_t PeakResidentKiB() {
    rusage usage{};
    Require(::getrusage(RUSAGE_SELF, &usage) == 0, "read resident memory");
    return static_cast<std::uint64_t>(usage.ru_maxrss);
}

static void InvalidRegistry() {
    char byte = 0;
    for (int index = 0; index < 1024; ++index)
        Require(read_nid_postfix(1000000000 + index, &byte, 0) == -1 && *__error_nid_postfix() == 9, "warm invalid read");
    const auto before = PeakResidentKiB();
    for (int index = 1024; index < 101024; ++index) {
        Require(read_nid_postfix(1000000000 + index, &byte, 0) == -1 && *__error_nid_postfix() == 9, "invalid descriptor read");
        Require(read_nid_postfix(-index, &byte, 0) == -1 && *__error_nid_postfix() == 9, "negative descriptor read");
    }
    Require(PeakResidentKiB() - before < 8192, "invalid reads retained descriptor registry allocations");
}
#endif

#ifdef _WIN32
struct VectorRace {
    static constexpr std::size_t Readers = 3;
    static constexpr std::size_t Records = 32;
    static constexpr std::size_t Parts = 64;
    std::barrier<> start{Readers + 1};
    std::array<int, Readers> descriptors{};
    std::array<std::array<std::array<std::uint16_t, Parts>, Records>, Readers> data{};
};

struct VectorReader {
    VectorRace* race;
    std::size_t index;
};

static void* APS5_VABI ReadRecords(void* argument) {
    const auto reader = *static_cast<VectorReader*>(argument);
    auto& race = *reader.race;
    race.start.arrive_and_wait();
    for (auto& record : race.data[reader.index]) {
        std::int64_t read;
        if (reader.index == VectorRace::Readers - 1) read = read_nid_postfix(race.descriptors[reader.index], record.data(), sizeof(record));
        else {
            std::array<KernelIovec, VectorRace::Parts> vectors{};
            for (std::size_t part = 0; part < vectors.size(); ++part) vectors[part] = {&record[part], sizeof(record[part])};
            read = readv_nid_postfix(race.descriptors[reader.index], vectors.data(), static_cast<int>(vectors.size()));
        }
        Require(read == sizeof(record), "pipe record was shortened");
        for (std::size_t part = 0; part < record.size(); ++part)
            Require(record[part] == record[0] + part, "pipe readv interleaved with another reader");
    }
    return nullptr;
}

static void PipeVectors() {
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create vector race pipe");
    VectorRace race;
    const int alias = dup_nid_postfix(pipe[0]);
    Require(alias >= 0, "duplicate vector race reader");
    race.descriptors = {pipe[0], alias, pipe[0]};
    std::array<std::uint16_t, VectorRace::Readers * VectorRace::Records * VectorRace::Parts> source{};
    for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<std::uint16_t>(index);
    Require(write_nid_postfix(pipe[1], reinterpret_cast<const char*>(source.data()), sizeof(source)) == sizeof(source), "seed vector records");
    std::array<Pthread, VectorRace::Readers> threads{};
    std::array<VectorReader, VectorRace::Readers> readers{};
    for (std::size_t index = 0; index < threads.size(); ++index) {
        readers[index] = {&race, index};
        Require(scePthreadCreate(&threads[index], nullptr, ReadRecords, &readers[index], nullptr) == 0, "start vector race reader");
    }
    race.start.arrive_and_wait();
    for (const auto thread : threads) Require(scePthreadJoin(thread, nullptr) == 0, "join vector race reader");
    std::array<bool, source.size()> seen{};
    for (const auto& records : race.data) for (const auto& record : records) for (const auto value : record) {
        Require(value < seen.size() && !seen[value], "pipe read lost ownership of a record");
        seen[value] = true;
    }
    for (const bool value : seen) Require(value, "pipe read lost data");
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(alias) == 0 && close_nid_postfix(pipe[1]) == 0, "close vector race pipe");
}

static void CloseProgress() {
    int pipe[2]{};
    Require(pipe_nid_postfix(pipe) == 0, "create close-progress pipe");
    Require(write_nid_postfix(pipe[1], "x", 1) == 1, "seed close-progress pipe");
    auto positioned = File::LockPositionedTransfer(pipe[1]);
    std::atomic<bool> closed{false};
    std::thread closer([&] {
        Require(close_nid_postfix(pipe[1]) == 0, "close positioned descriptor");
        closed.store(true);
    });
    const bool waiting = Observe([&] {
        GuestDescriptorAccess access(pipe[1], GuestDescriptorAccess::Mode::Read, std::defer_lock);
        return !access.TryLock();
    });
    BlockedRead read{pipe[0]};
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, Reader, &read, nullptr) == 0, "start drain reader");
    const bool drained = Observe([&] { return reader->_finished.load(); });
    const bool closeStayedBlocked = !closed.load();
    positioned.unlock();
    closer.join();
    Require(Observe([&] { return reader->_finished.load(); }), "drain reader did not finish");
    Require(scePthreadJoin(reader, nullptr) == 0, "join drain reader");
    Require(waiting && closeStayedBlocked && drained && read.result == 1 && read.byte == 'x', "blocked close prevented an independent pipe drain");
    Require(close_nid_postfix(pipe[0]) == 0, "close drain reader");
}
#endif

#ifndef _WIN32
struct GuestLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};

static bool ChildBlocked(int descriptor) {
    const pid_t child = ::fork();
    Require(child >= 0, "fork record-lock probe");
    if (child == 0) {
        struct flock lock{};
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        const int result = ::fcntl(descriptor, F_SETLK, &lock);
        const bool blocked = result == -1 && (errno == EACCES || errno == EAGAIN);
        ::_exit(blocked ? 7 : result == 0 ? 0 : 9);
    }
    int status = 0;
    Require(::waitpid(child, &status, 0) == child && WIFEXITED(status), "join record-lock probe");
    Require(WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 7, "record-lock probe failed");
    return WEXITSTATUS(status) == 7;
}

static void BorrowedNetClose(bool posix = false) {
    const int listener = sceNetSocket("io-read-listener", 2, 1, 0);
    Require(listener >= 0, "create borrowed listener");
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0, "bind borrowed listener");
    Require(sceNetListen(listener, 4) == 0, "listen borrowed socket");
    std::uint32_t length = address.size();
    Require(sceNetGetsockname(listener, address.data(), &length) == 0, "get borrowed listener address");
    const int client = sceNetSocket("io-read-client", 2, 1, 0);
    Require(client >= 0 && sceNetConnect(client, address.data(), length) == 0, "connect borrowed socket");
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept borrowed socket");
    BlockedRead read{accepted};
    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, Reader, &read, nullptr) == 0, "start borrowed reader");
    const bool registered = Registered(reader);
    std::atomic<bool> closed{false};
    std::thread closer([&] {
        Require((posix ? close_nid_postfix(accepted) : sceNetSocketClose(accepted)) == 0, "close borrowed socket");
        closed.store(true);
    });
    const bool prompt = Observe([&] { return closed.load(); });
    if (!prompt) Require(pthread_cancel_nid_postfix(reader) == 0, "release failed borrowed close");
    closer.join();
    Require(Observe([&] { return reader->_finished.load(); }), "borrowed reader did not finish");
    Require(scePthreadJoin(reader, nullptr) == 0, "join borrowed reader");
    Require(registered && prompt && (posix ? read.result == -1 : read.result == 0), "socket close did not wake the borrowed POSIX read");
    Require(sceNetSocketClose(client) == 0 && sceNetSocketClose(listener) == 0, "close borrowed socket pair");
}

static void RecordLocks() {
    auto* file = OpenTemporary();
    const int descriptor = NativeDescriptor(file);
    GuestLock lock{0, 0, 0, 3, 0, 0};
    Require(fcntl_nid_postfix(descriptor, 12, &lock) == 0, "acquire guest record lock");
    Require(ChildBlocked(descriptor), "record lock was not established");
    char byte = 0;
    KernelIovec vector{&byte, 1};
    Require(read_nid_postfix(descriptor, &byte, 1) == 1, "locked read");
    Require(ChildBlocked(descriptor), "read released record lock");
    Require(pread_nid_postfix(descriptor, &byte, 1, 0) == 1, "locked pread");
    Require(ChildBlocked(descriptor), "pread released record lock");
    Require(readv_nid_postfix(descriptor, &vector, 1) == 1, "locked readv");
    Require(ChildBlocked(descriptor), "readv released record lock");
    Require(preadv_nid_postfix(descriptor, &vector, 1, 0) == 1, "locked preadv");
    Require(ChildBlocked(descriptor), "preadv released record lock");
    lock.type = 2;
    Require(fcntl_nid_postfix(descriptor, 12, &lock) == 0 && !ChildBlocked(descriptor), "release guest record lock");
    Require(std::fclose(file) == 0, "close locked file");
}
#endif

int main(int argc, char** argv) {
    Require(argc >= 2, "expected descriptor-access case");
    if (std::strcmp(argv[1], "posix_close") == 0) {
        Require(argc == 6, "expected close transport, API, cancellation state and entry point");
        PosixClose(std::atoi(argv[2]) != 0, std::atoi(argv[3]), std::atoi(argv[4]) != 0, std::atoi(argv[5]));
        return 0;
    }
    if (std::strcmp(argv[1], "close_unregistered") == 0 || std::strcmp(argv[1], "close_readers") == 0) {
        Require(argc == 3, "expected close transport");
        if (std::strcmp(argv[1], "close_unregistered") == 0) CloseUnregistered(std::atoi(argv[2]) != 0);
        else CloseReaders(std::atoi(argv[2]) != 0);
        return 0;
    }
    if (std::strcmp(argv[1], "capacity") == 0) Capacity();
    else if (std::strcmp(argv[1], "no_aliases") == 0) NoAliases();
    else if (std::strcmp(argv[1], "access_cancel") == 0) AccessCancellation();
    else if (std::strcmp(argv[1], "protected_close") == 0) ProtectedClose();
    else if (std::strcmp(argv[1], "invalid_duplicate") == 0) InvalidDuplicate();
    else if (std::strcmp(argv[1], "stdio_close") == 0) StreamOrder(false);
    else if (std::strcmp(argv[1], "stdio_reopen") == 0) StreamOrder(true);
#ifdef __linux__
    else if (std::strcmp(argv[1], "invalid_registry") == 0) InvalidRegistry();
#endif
#ifdef _WIN32
    else if (std::strcmp(argv[1], "close_native_errors") == 0) CloseNativeErrors();
    else if (std::strcmp(argv[1], "append_status") == 0) AppendOwnership(false);
    else if (std::strcmp(argv[1], "append_stream") == 0) AppendOwnership(true);
    else if (std::strcmp(argv[1], "close_progress") == 0) CloseProgress();
    else if (std::strcmp(argv[1], "position_cancel") == 0) AccessCancellation(true);
    else if (std::strcmp(argv[1], "pipe_vectors") == 0) PipeVectors();
#endif
#ifndef _WIN32
    else if (std::strcmp(argv[1], "record_locks") == 0) RecordLocks();
    else if (std::strcmp(argv[1], "borrowed_net_close") == 0) BorrowedNetClose();
    else if (std::strcmp(argv[1], "borrowed_posix_close") == 0) BorrowedNetClose(true);
#endif
    else Require(false, "unknown descriptor-access case");
}
