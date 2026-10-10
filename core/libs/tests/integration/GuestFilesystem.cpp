#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI remove_nid_postfix(const char*);
int APS5_VABI rename_nid_postfix(const char*, const char*);
int APS5_VABI sceKernelChmod_nid_postfix(const char*, unsigned short);
int APS5_VABI sceKernelFchmod(int, unsigned short);
int APS5_VABI fchmod_nid_postfix(int, int);
int APS5_VABI futimes_nid_postfix(int, const KernelTimeval*);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI sceKernelFsync(int);
int APS5_VABI fdatasync_nid_postfix(int);
int APS5_VABI sceKernelWriteThrottlingStatus(std::uint64_t*);
int APS5_VABI sceKernelFtruncate(int, long long);
int APS5_VABI sceKernelTruncate_nid_postfix(const char*, long long);
int APS5_VABI sceKernelUtimes_nid_postfix(const char*, const void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI _open_nid_postfix(const char*, int, ...);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI stat_nid_postfix(const char*, FileStat*);
int APS5_VABI lstat_nid_postfix(const char*, FileStat*);
int APS5_VABI unlink_nid_postfix(const char*);
int APS5_VABI rmdir_nid_postfix(const char*);
int APS5_VABI mkdir_nid_postfix(const char*, unsigned short);
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI sceKernelUnlink(const char*);
int APS5_VABI sceKernelRmdir(const char*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI pipe_nid_postfix(int*);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::size_t);
std::int64_t APS5_VABI write_nid_postfix(int, const void*, std::size_t);
int APS5_VABI sceKernelDebugOutText(int, const char*);
}

namespace {

using Testing::Case;
using Testing::Fail;
using Testing::Require;
using Testing::RequireEqual;

constexpr int eperm = 1;
constexpr int enoent = 2;
constexpr int ebadf = 9;
constexpr int efault = 14;
constexpr int eexist = 17;
constexpr int enotdir = 20;
constexpr int eisdir = 21;
constexpr int einval = 22;
constexpr int enotempty = 66;
constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001u);
constexpr int SCE_KERNEL_ERROR_ENOENT = static_cast<int>(0x80020002u);
constexpr int SCE_KERNEL_ERROR_EBADF = static_cast<int>(0x80020009u);
constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000eu);
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
constexpr int SCE_KERNEL_ERROR_ENOTEMPTY = static_cast<int>(0x80020042u);
constexpr int openWriteOnly = 0x0001;
constexpr int openReadWrite = 0x0002;
constexpr int openCreate = 0x0200;
constexpr int openCreateExclusive = 0x0a00;
constexpr int openCreateExclusiveReadWrite = 0x0a02;
constexpr std::uint16_t fileTypeMask = 0170000;
constexpr std::uint16_t regularFile = 0100000;
constexpr std::uint16_t directoryFile = 0040000;
constexpr std::int64_t pastSeconds = 1000000000;

#ifdef _WIN32
int Fileno(std::FILE* file) { return ::_fileno(file); }
int DuplicateDescriptor(int descriptor) { return ::_dup(descriptor); }
bool RedirectDescriptor(int from, int to) { return ::_dup2(from, to) == 0; }
int CloseDescriptor(int descriptor) { return ::_close(descriptor); }
#else
int Fileno(std::FILE* file) { return ::fileno(file); }
int DuplicateDescriptor(int descriptor) { return ::dup(descriptor); }
bool RedirectDescriptor(int from, int to) { return ::dup2(from, to) == to; }
int CloseDescriptor(int descriptor) { return ::close(descriptor); }
#endif

int GuestErrno() {
    return *__error_nid_postfix();
}

void RequireFailure(int result, int expectedError, const std::string& message) {
    RequireEqual(result, -1, message);
    RequireEqual(GuestErrno(), expectedError, message + " errno");
}

void RequireOpenFailure(int result, int expectedError, const std::string& message) {
    if (result >= 0) {
        close_nid_postfix(result);
        Fail(message + ": open unexpectedly succeeded");
    }
    RequireFailure(result, expectedError, message);
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
}

std::string ReadLine(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::string contents;
    std::getline(stream, contents);
    return contents;
}

bool OwnerWritable(const std::filesystem::path& path) {
    return (std::filesystem::status(path).permissions() & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
}

class ScratchDirectory {
public:
    explicit ScratchDirectory(bool create = true) : root(UniqueName()), name(root.string()) {
        if (create) Require(std::filesystem::create_directory(root), "create the test directory " + name);
    }

    ~ScratchDirectory() {
        std::error_code iteration;
        for (std::filesystem::recursive_directory_iterator entry(root, iteration), end; !iteration && entry != end;
             entry.increment(iteration)) {
            std::error_code ignored;
            std::filesystem::permissions(entry->path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add,
                                         ignored);
        }
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    const std::filesystem::path& Path() const noexcept { return root; }
    const std::string& Name() const noexcept { return name; }

    std::filesystem::path File(const char* fileName, const std::string& contents) const {
        const auto path = root / fileName;
        WriteFile(path, contents);
        return path;
    }

private:
    static std::filesystem::path UniqueName() {
        static std::atomic<unsigned> counter{0};
        return "anyps5-filesystem-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
               std::to_string(counter++);
    }

    std::filesystem::path root;
    std::string name;
};

class GuestDescriptor {
public:
    explicit GuestDescriptor(int descriptor) : descriptor(descriptor) {}

    ~GuestDescriptor() {
        if (descriptor >= 0) close_nid_postfix(descriptor);
    }

    GuestDescriptor(const GuestDescriptor&) = delete;
    GuestDescriptor& operator=(const GuestDescriptor&) = delete;

    int Descriptor() const noexcept { return descriptor; }

    int Close() {
        const int result = close_nid_postfix(descriptor);
        descriptor = -1;
        return result;
    }

private:
    int descriptor;
};

class NativeFile {
public:
    explicit NativeFile(const std::filesystem::path& path) : file(std::fopen(path.string().c_str(), "r+b")) {
        Require(file != nullptr, "open " + path.string());
        descriptor = Fileno(file);
        Require(descriptor >= 0, "native descriptor");
    }

    ~NativeFile() {
        if (file != nullptr) std::fclose(file);
    }

    NativeFile(const NativeFile&) = delete;
    NativeFile& operator=(const NativeFile&) = delete;

    int Descriptor() const noexcept { return descriptor; }

    int Close() {
        const int result = std::fclose(file);
        file = nullptr;
        return result;
    }

private:
    std::FILE* file;
    int descriptor = -1;
};

class StderrCapture {
public:
    StderrCapture() = default;

    ~StderrCapture() {
        Restore();
        if (file != nullptr) std::fclose(file);
    }

    StderrCapture(const StderrCapture&) = delete;
    StderrCapture& operator=(const StderrCapture&) = delete;

    void Start() {
        file = std::tmpfile();
        Require(file != nullptr, "create the capture file");
        saved = DuplicateDescriptor(Fileno(stderr));
        Require(saved >= 0, "save stderr");
        Require(RedirectDescriptor(Fileno(file), Fileno(stderr)), "redirect stderr");
    }

    bool Restore() {
        if (saved < 0) return true;
        const bool redirected = RedirectDescriptor(saved, Fileno(stderr));
        const bool closed = CloseDescriptor(saved) == 0;
        saved = -1;
        return redirected && closed;
    }

    std::string Text() {
        std::rewind(file);
        char text[64]{};
        const auto count = std::fread(text, 1, sizeof(text), file);
        return std::string(text, count);
    }

    int CloseFile() {
        const int result = std::fclose(file);
        file = nullptr;
        return result;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

struct SizedFile {
    ScratchDirectory directory;
    std::filesystem::path path = directory.File("sized.txt", "0123456789abcdef");
    std::string name = path.string();
};

struct OpenSizedFile {
    SizedFile sized;
    NativeFile native{sized.path};
};

struct StatFixture {
    ScratchDirectory directory;
    std::filesystem::path present = directory.File("present.txt", "posix");
    std::string presentName = present.string();
    std::string missingName = (directory.Path() / "missing.txt").string();
    const std::string& rootName = directory.Name();
};

const Case debugNegativeChannel{"DebugOutText_NegativeChannel_FailsWithEinval", [] {
    RequireEqual(sceKernelDebugOutText(-1, "text"), SCE_KERNEL_ERROR_EINVAL, "channel -1");
}};

const Case debugNullText{"DebugOutText_NullText_FailsWithEfault", [] {
    RequireEqual(sceKernelDebugOutText(0, nullptr), SCE_KERNEL_ERROR_EFAULT, "null text");
}};

const Case debugFormatText{"DebugOutText_FormatCharacters_WritesPrefixedLiteralToStderr", [] {
    StderrCapture capture;
    capture.Start();
    const int debugResult = sceKernelDebugOutText(3, "%s%d literal\n");
    Require(capture.Restore(), "restore stderr");
    RequireEqual(debugResult, 0, "debug result");
    const auto text = capture.Text();
    RequireEqual(text.size(), std::size_t{23}, "captured byte count");
    RequireEqual(text, std::string("[debug:3] %s%d literal\n"), "captured text");
    RequireEqual(capture.CloseFile(), 0, "close the capture file");
}};

const Case throttling{"WriteThrottlingStatus_Called_ReportsUnthrottled", [] {
    std::uint64_t status[4] = {1, 2, 3, 4};
    RequireEqual(sceKernelWriteThrottlingStatus(status), 0, "result");
    RequireEqual(status[0], std::uint64_t{0xffffffffu}, "status[0]");
    RequireEqual(status[1], std::uint64_t{0}, "status[1]");
    RequireEqual(status[2], std::uint64_t{0}, "status[2]");
    RequireEqual(status[3], std::uint64_t{0}, "status[3]");
}};

const Case pipeNull{"Pipe_NullDescriptors_FailsWithEfault", [] {
    RequireFailure(pipe_nid_postfix(nullptr), efault, "null descriptors");
}};

const Case pipeTransfer{"Pipe_BinaryPayload_ReadsBackUnchangedThenEof", [] {
    int descriptors[2] = {-1, -1};
    const int created = pipe_nid_postfix(descriptors);
    GuestDescriptor reader(descriptors[0]);
    GuestDescriptor writer(descriptors[1]);
    RequireEqual(created, 0, "create pipe");
    Require(descriptors[0] >= 0 && descriptors[1] >= 0, "pipe descriptors are valid");
    const char payload[] = {'A', '\0', '\r', '\n', '\x1a', 'Z'};
    const auto size = static_cast<std::int64_t>(sizeof(payload));
    RequireEqual(write_nid_postfix(writer.Descriptor(), payload, sizeof(payload)), size, "write");
    RequireEqual(writer.Close(), 0, "close writer");
    char received[sizeof(payload)]{};
    RequireEqual(read_nid_postfix(reader.Descriptor(), received, sizeof(received)), size, "read");
    RequireEqual(std::string(received, sizeof(received)), std::string(payload, sizeof(payload)), "payload");
    RequireEqual(read_nid_postfix(reader.Descriptor(), received, sizeof(received)), std::int64_t{0}, "read at eof");
    RequireEqual(reader.Close(), 0, "close reader");
}};

const Case mkdirNew{"Mkdir_NewDirectory_Succeeds", [] {
    const ScratchDirectory directory(false);
    RequireEqual(mkdir_nid_postfix(directory.Name().c_str(), 0700), 0, "mkdir");
    Require(std::filesystem::is_directory(directory.Path()), "directory exists");
}};

const Case mkdirExisting{"Mkdir_ExistingDirectory_FailsWithEexist", [] {
    const ScratchDirectory directory;
    RequireFailure(mkdir_nid_postfix(directory.Name().c_str(), 0700), eexist, "existing directory");
}};

const Case mkdirMissingParent{"Mkdir_MissingParent_FailsWithEnoent", [] {
    const ScratchDirectory directory;
    RequireFailure(mkdir_nid_postfix((directory.Path() / "missing" / "child").string().c_str(), 0700), enoent, "missing parent");
}};

const Case mkdirNull{"Mkdir_NullPath_FailsWithEfault", [] {
    RequireFailure(mkdir_nid_postfix(nullptr, 0700), efault, "null path");
}};

const Case removeNonEmpty{"Remove_NonEmptyDirectory_FailsWithEnotemptyAndKeepsFile", [] {
    const ScratchDirectory directory;
    const auto file = directory.File("file.txt", "retained until removal");
    RequireFailure(remove_nid_postfix(directory.Name().c_str()), enotempty, "non-empty directory");
    Require(std::filesystem::is_regular_file(file), "file kept");
}};

const Case removeBelowFile{"Remove_PathBelowFile_Fails", [] {
    const ScratchDirectory directory;
    const auto file = directory.File("file.txt", "retained until removal");
    RequireEqual(remove_nid_postfix((file / "invalid").string().c_str()), -1, "path below a file");
}};

const Case removeEmptyPath{"Remove_EmptyPath_FailsWithEnoent", [] {
    RequireFailure(remove_nid_postfix(""), enoent, "empty path");
}};

const Case removeNull{"Remove_NullPath_FailsWithEfault", [] {
    RequireFailure(remove_nid_postfix(nullptr), efault, "null path");
}};

const Case renameReplace{"Rename_OntoExistingFile_ReplacesTarget", [] {
    const ScratchDirectory directory;
    const auto file = directory.File("file.txt", "retained until removal");
    const auto renamed = directory.File("renamed.txt", "old contents");
    RequireEqual(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()), 0, "rename");
    Require(!std::filesystem::exists(file), "source removed");
    RequireEqual(ReadLine(renamed), std::string("retained until removal"), "target contents");
}};

const Case renameSame{"Rename_SameSourceAndTarget_Succeeds", [] {
    const ScratchDirectory directory;
    const auto renamed = directory.File("renamed.txt", "retained until removal");
    RequireEqual(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()), 0, "rename onto itself");
    RequireEqual(ReadLine(renamed), std::string("retained until removal"), "contents");
}};

const Case renameMissing{"Rename_MissingSource_FailsWithEnoent", [] {
    const ScratchDirectory directory;
    const auto renamed = directory.File("renamed.txt", "retained until removal");
    RequireFailure(rename_nid_postfix((directory.Path() / "file.txt").string().c_str(), renamed.string().c_str()), enoent,
                   "missing source");
}};

const Case renameNewName{"Rename_ToNewName_MovesFile", [] {
    const ScratchDirectory directory;
    const auto renamed = directory.File("renamed.txt", "retained until removal");
    const auto file = directory.Path() / "file.txt";
    RequireEqual(rename_nid_postfix(renamed.string().c_str(), file.string().c_str()), 0, "rename");
    Require(std::filesystem::exists(file), "target exists");
}};

const Case removeFile{"Remove_ExistingFile_DeletesIt", [] {
    const ScratchDirectory directory;
    const auto file = directory.File("file.txt", "retained until removal");
    RequireEqual(remove_nid_postfix(file.string().c_str()), 0, "remove");
    Require(!std::filesystem::exists(file), "file removed");
}};

const Case removeMissing{"Remove_MissingFile_FailsWithEnoent", [] {
    const ScratchDirectory directory;
    RequireFailure(remove_nid_postfix((directory.Path() / "file.txt").string().c_str()), enoent, "missing file");
}};

const Case removeEmptyDirectory{"Remove_EmptyDirectory_RemovesIt", [] {
    const ScratchDirectory directory;
    RequireEqual(remove_nid_postfix(directory.Name().c_str()), 0, "remove");
    Require(!std::filesystem::exists(directory.Path()), "directory removed");
}};

const Case chmodFile{"KernelChmod_ExistingFile_Succeeds", [] {
    const SizedFile sized;
    RequireEqual(sceKernelChmod_nid_postfix(sized.name.c_str(), 0600), 0, "chmod 0600");
}};

const Case truncateFile{"KernelTruncate_ExistingFile_ShrinksAndKeepsPrefix", [] {
    const SizedFile sized;
    RequireEqual(sceKernelTruncate_nid_postfix(sized.name.c_str(), 6), 0, "truncate");
    RequireEqual(std::filesystem::file_size(sized.path), std::uintmax_t{6}, "size");
    RequireEqual(ReadLine(sized.path), std::string("012345"), "contents");
}};

const Case truncateBelowFile{"KernelTruncate_PathBelowFile_FailsWithEnoent", [] {
    const SizedFile sized;
    RequireEqual(sceKernelTruncate_nid_postfix((sized.path / "missing").string().c_str(), 6), SCE_KERNEL_ERROR_ENOENT,
                 "path below a file");
}};

const Case utimesNull{"KernelUtimes_NullTimes_Succeeds", [] {
    const SizedFile sized;
    RequireEqual(sceKernelUtimes_nid_postfix(sized.name.c_str(), nullptr), 0, "utimes");
}};

const Case fsyncDescriptor{"Fsync_OpenDescriptor_Succeeds", [] {
    OpenSizedFile file;
    RequireEqual(sceKernelFsync(file.native.Descriptor()), 0, "sceKernelFsync");
    RequireEqual(fdatasync_nid_postfix(file.native.Descriptor()), 0, "fdatasync");
}};

const Case fchmodToggle{"Fchmod_ReadOnlyThenWritable_TogglesOwnerWrite", [] {
    OpenSizedFile file;
    RequireEqual(sceKernelFchmod(file.native.Descriptor(), 0400), 0, "sceKernelFchmod 0400");
    Require(!OwnerWritable(file.sized.path), "owner write cleared");
    RequireEqual(fchmod_nid_postfix(file.native.Descriptor(), 0600), 0, "fchmod 0600");
    Require(OwnerWritable(file.sized.path), "owner write restored");
}};

const Case ftruncateDescriptor{"KernelFtruncate_OpenDescriptor_ShrinksFile", [] {
    OpenSizedFile file;
    RequireEqual(sceKernelFtruncate(file.native.Descriptor(), 3), 0, "ftruncate");
    RequireEqual(file.native.Close(), 0, "close");
    RequireEqual(std::filesystem::file_size(file.sized.path), std::uintmax_t{3}, "size");
}};

const Case futimesExplicit{"Futimes_ExplicitTimes_SetsModificationTime", [] {
    OpenSizedFile file;
    const KernelTimeval past[2]{{pastSeconds, 0}, {pastSeconds, 500000}};
    RequireEqual(futimes_nid_postfix(file.native.Descriptor(), past), 0, "futimes");
    FileStat times{};
    RequireEqual(stat_nid_postfix(file.sized.name.c_str(), &times), 0, "stat");
    RequireEqual(times.st_mtim.tv_sec, pastSeconds, "modification time");
}};

const Case futimesNull{"Futimes_NullTimes_SetsCurrentTime", [] {
    OpenSizedFile file;
    const KernelTimeval past[2]{{pastSeconds, 0}, {pastSeconds, 500000}};
    RequireEqual(futimes_nid_postfix(file.native.Descriptor(), past), 0, "futimes past");
    RequireEqual(futimes_nid_postfix(file.native.Descriptor(), nullptr), 0, "futimes now");
    FileStat times{};
    RequireEqual(stat_nid_postfix(file.sized.name.c_str(), &times), 0, "stat");
    Require(times.st_mtim.tv_sec > pastSeconds, "modification time is after the past time, got " +
            std::to_string(times.st_mtim.tv_sec));
}};

const Case futimesOutOfRange{"Futimes_MicrosecondsOutOfRange_FailsWithEinval", [] {
    OpenSizedFile file;
    const KernelTimeval overflow[2]{{0, 0}, {0, 1000000}};
    RequireFailure(futimes_nid_postfix(file.native.Descriptor(), overflow), einval, "tv_usec 1000000");
    const KernelTimeval negative[2]{{0, -1}, {0, 0}};
    RequireFailure(futimes_nid_postfix(file.native.Descriptor(), negative), einval, "tv_usec -1");
}};

#ifndef _WIN32
const Case closedDescriptor{"FileDescriptorOperations_ClosedDescriptor_FailWithEbadf", [] {
    OpenSizedFile file;
    const int descriptor = file.native.Descriptor();
    RequireEqual(file.native.Close(), 0, "close");
    RequireEqual(sceKernelFchmod(descriptor, 0600), SCE_KERNEL_ERROR_EBADF, "sceKernelFchmod");
    RequireFailure(fchmod_nid_postfix(descriptor, 0600), ebadf, "fchmod");
    RequireFailure(futimes_nid_postfix(descriptor, nullptr), ebadf, "futimes");
    RequireFailure(fdatasync_nid_postfix(descriptor), ebadf, "fdatasync");
}};
#endif

const Case socketDescriptor{"FileDescriptorOperations_Socket_FailWithEinval", [] {
    GuestDescriptor socket(socket_nid_postfix(2, 2, 0));
    Require(socket.Descriptor() >= 0, "create socket");
    RequireEqual(sceKernelFchmod(socket.Descriptor(), 0600), SCE_KERNEL_ERROR_EINVAL, "sceKernelFchmod");
    RequireFailure(fchmod_nid_postfix(socket.Descriptor(), 0600), einval, "fchmod");
    RequireFailure(futimes_nid_postfix(socket.Descriptor(), nullptr), einval, "futimes");
    RequireFailure(fdatasync_nid_postfix(socket.Descriptor()), einval, "fdatasync");
    RequireEqual(socket.Close(), 0, "close");
}};

const Case closedSocket{"FileDescriptorOperations_ClosedSocket_FailWithEbadf", [] {
    GuestDescriptor socket(socket_nid_postfix(2, 2, 0));
    const int descriptor = socket.Descriptor();
    Require(descriptor >= 0, "create socket");
    RequireEqual(socket.Close(), 0, "close");
    RequireFailure(fchmod_nid_postfix(descriptor, 0600), ebadf, "fchmod");
    RequireFailure(futimes_nid_postfix(descriptor, nullptr), ebadf, "futimes");
    RequireFailure(fdatasync_nid_postfix(descriptor), ebadf, "fdatasync");
}};

const Case statFile{"Stat_ExistingFile_ReportsSize", [] {
    const StatFixture fixture;
    FileStat status{};
    RequireEqual(stat_nid_postfix(fixture.presentName.c_str(), &status), 0, "stat");
    RequireEqual(status.st_size, decltype(status.st_size){5}, "size");
}};

const Case statMissing{"Stat_MissingFile_FailsWithEnoent", [] {
    const StatFixture fixture;
    FileStat status{};
    RequireFailure(stat_nid_postfix(fixture.missingName.c_str(), &status), enoent, "stat");
    RequireEqual(sceKernelStat(fixture.missingName.c_str(), &status), SCE_KERNEL_ERROR_ENOENT, "sceKernelStat");
}};

const Case statInvalid{"Stat_EmptyOrNullArguments_Fail", [] {
    const StatFixture fixture;
    FileStat status{};
    RequireFailure(stat_nid_postfix("", &status), enoent, "empty path");
    RequireFailure(stat_nid_postfix(nullptr, &status), efault, "null path");
    RequireFailure(stat_nid_postfix(fixture.presentName.c_str(), nullptr), efault, "null status");
}};

const Case lstatFile{"Lstat_RegularFile_MatchesStat", [] {
    const StatFixture fixture;
    FileStat status{};
    RequireEqual(stat_nid_postfix(fixture.presentName.c_str(), &status), 0, "stat");
    FileStat linkStatus{};
    RequireEqual(lstat_nid_postfix(fixture.presentName.c_str(), &linkStatus), 0, "lstat");
    RequireEqual(linkStatus.st_size, decltype(linkStatus.st_size){5}, "size");
    RequireEqual(static_cast<std::uint16_t>(linkStatus.st_mode & fileTypeMask), regularFile, "file type");
    RequireEqual(linkStatus.st_ino, status.st_ino, "inode");
}};

const Case lstatDirectory{"Lstat_Directory_ReportsDirectoryType", [] {
    const StatFixture fixture;
    FileStat linkStatus{};
    RequireEqual(lstat_nid_postfix(fixture.rootName.c_str(), &linkStatus), 0, "lstat");
    RequireEqual(static_cast<std::uint16_t>(linkStatus.st_mode & fileTypeMask), directoryFile, "file type");
}};

const Case lstatInvalid{"Lstat_MissingEmptyOrNullArguments_Fail", [] {
    const StatFixture fixture;
    FileStat linkStatus{};
    RequireFailure(lstat_nid_postfix(fixture.missingName.c_str(), &linkStatus), enoent, "missing file");
    RequireFailure(lstat_nid_postfix("", &linkStatus), enoent, "empty path");
    RequireFailure(lstat_nid_postfix(nullptr, &linkStatus), efault, "null path");
    RequireFailure(lstat_nid_postfix(fixture.presentName.c_str(), nullptr), efault, "null status");
}};

#ifndef _WIN32
const Case lstatSymlink{"Lstat_Symlink_ReportsLinkAndStatFollowsIt", [] {
    const StatFixture fixture;
    const auto link = fixture.directory.Path() / "link";
    std::filesystem::create_symlink("present.txt", link);
    FileStat linkStatus{};
    RequireEqual(lstat_nid_postfix(link.string().c_str(), &linkStatus), 0, "lstat");
    RequireEqual(static_cast<std::uint16_t>(linkStatus.st_mode & fileTypeMask), std::uint16_t{0120000}, "lstat file type");
    RequireEqual(linkStatus.st_size, decltype(linkStatus.st_size){11}, "lstat size");
    FileStat status{};
    RequireEqual(stat_nid_postfix(link.string().c_str(), &status), 0, "stat");
    RequireEqual(static_cast<std::uint16_t>(status.st_mode & fileTypeMask), regularFile, "stat file type");
    RequireEqual(unlink_nid_postfix(link.string().c_str()), 0, "unlink link");
}};

const Case danglingSymlink{"Lstat_DanglingSymlink_ReportsLinkAndStatFailsWithEnoent", [] {
    const StatFixture fixture;
    const auto dangling = fixture.directory.Path() / "dangling";
    std::filesystem::create_symlink("missing.txt", dangling);
    FileStat linkStatus{};
    RequireEqual(lstat_nid_postfix(dangling.string().c_str(), &linkStatus), 0, "lstat");
    RequireEqual(static_cast<std::uint16_t>(linkStatus.st_mode & fileTypeMask), std::uint16_t{0120000}, "lstat file type");
    FileStat status{};
    RequireFailure(stat_nid_postfix(dangling.string().c_str(), &status), enoent, "stat");
    RequireEqual(unlink_nid_postfix(dangling.string().c_str()), 0, "unlink dangling link");
}};

const Case lstatBelowFile{"Lstat_PathBelowFile_FailsWithEnotdir", [] {
    const StatFixture fixture;
    FileStat linkStatus{};
    RequireFailure(lstat_nid_postfix((fixture.present / "child").string().c_str(), &linkStatus), enotdir, "path below a file");
}};
#endif

const Case openExisting{"Open_ExistingFile_Succeeds", [] {
    const StatFixture fixture;
    GuestDescriptor opened(open_nid_postfix(fixture.presentName.c_str(), 0, 0));
    Require(opened.Descriptor() >= 0, "open");
    RequireEqual(opened.Close(), 0, "close opened");
    GuestDescriptor reopened(_open_nid_postfix(fixture.presentName.c_str(), 0));
    Require(reopened.Descriptor() >= 0, "_open");
    RequireEqual(reopened.Close(), 0, "close reopened");
}};

const Case openMissing{"Open_MissingFile_FailsWithEnoent", [] {
    const StatFixture fixture;
    RequireOpenFailure(open_nid_postfix(fixture.missingName.c_str(), 0, 0), enoent, "open");
    RequireOpenFailure(_open_nid_postfix(fixture.missingName.c_str(), 0), enoent, "_open");
    const int kernelResult = sceKernelOpen(fixture.missingName.c_str(), 0, 0);
    if (kernelResult >= 0) sceKernelClose(kernelResult);
    RequireEqual(kernelResult, SCE_KERNEL_ERROR_ENOENT, "sceKernelOpen");
}};

const Case openExclusive{"Open_ExclusiveCreateOfExistingEntry_FailsWithEexist", [] {
    const StatFixture fixture;
    RequireOpenFailure(open_nid_postfix(fixture.presentName.c_str(), openCreateExclusiveReadWrite, 0644), eexist, "open file");
    RequireOpenFailure(_open_nid_postfix(fixture.presentName.c_str(), openCreateExclusiveReadWrite, 0644), eexist, "_open file");
    RequireOpenFailure(open_nid_postfix(fixture.rootName.c_str(), openCreateExclusiveReadWrite, 0644), eexist,
                       "open directory read-write");
    RequireOpenFailure(open_nid_postfix(fixture.rootName.c_str(), openCreateExclusive, 0644), eexist, "open directory read-only");
}};

const Case openDirectoryWrite{"Open_DirectoryForCreateOrWrite_FailsWithEisdir", [] {
    const StatFixture fixture;
    for (const int flags : {openCreate, openReadWrite, openWriteOnly}) {
        RequireOpenFailure(open_nid_postfix(fixture.rootName.c_str(), flags, flags == openCreate ? 0644 : 0), eisdir,
                           "flags " + std::to_string(flags));
    }
}};

const Case openDirectoryRead{"Open_DirectoryReadOnly_Succeeds", [] {
    const StatFixture fixture;
    GuestDescriptor directory(open_nid_postfix(fixture.rootName.c_str(), 0, 0));
    Require(directory.Descriptor() >= 0, "open directory");
    RequireEqual(directory.Close(), 0, "close directory");
}};

const Case openInvalid{"Open_EmptyOrNullPath_Fails", [] {
    RequireOpenFailure(open_nid_postfix("", 0, 0), enoent, "open empty path");
    RequireOpenFailure(_open_nid_postfix("", 0), enoent, "_open empty path");
    RequireOpenFailure(open_nid_postfix(nullptr, 0, 0), efault, "open null path");
    RequireOpenFailure(_open_nid_postfix(nullptr, 0), efault, "_open null path");
}};

const Case rmdirNonEmpty{"Rmdir_NonEmptyDirectory_FailsWithEnotempty", [] {
    const StatFixture fixture;
    RequireFailure(rmdir_nid_postfix(fixture.rootName.c_str()), enotempty, "rmdir");
    RequireEqual(sceKernelRmdir(fixture.rootName.c_str()), SCE_KERNEL_ERROR_ENOTEMPTY, "sceKernelRmdir");
}};

const Case rmdirInvalid{"Rmdir_FileMissingEmptyOrNullPath_Fails", [] {
    const StatFixture fixture;
    RequireFailure(rmdir_nid_postfix(fixture.presentName.c_str()), enotdir, "regular file");
    RequireFailure(rmdir_nid_postfix(fixture.missingName.c_str()), enoent, "missing path");
    RequireFailure(rmdir_nid_postfix(""), enoent, "empty path");
    RequireFailure(rmdir_nid_postfix(nullptr), efault, "null path");
}};

const Case rmdirEmpty{"Rmdir_EmptyDirectory_RemovesIt", [] {
    const ScratchDirectory directory;
    const auto empty = directory.Path() / "empty";
    Require(std::filesystem::create_directory(empty), "create the empty directory");
    RequireEqual(rmdir_nid_postfix(empty.string().c_str()), 0, "rmdir");
    Require(!std::filesystem::exists(empty), "directory removed");
}};

const Case unlinkInvalid{"Unlink_MissingEmptyOrNullPath_Fails", [] {
    const StatFixture fixture;
    RequireFailure(unlink_nid_postfix(fixture.missingName.c_str()), enoent, "missing path");
    RequireEqual(sceKernelUnlink(fixture.missingName.c_str()), SCE_KERNEL_ERROR_ENOENT, "sceKernelUnlink missing path");
    RequireFailure(unlink_nid_postfix(""), enoent, "empty path");
    RequireFailure(unlink_nid_postfix(nullptr), efault, "null path");
}};

const Case unlinkDirectory{"Unlink_Directory_FailsWithEpermAndKeepsDirectory", [] {
    const StatFixture fixture;
    RequireFailure(unlink_nid_postfix(fixture.rootName.c_str()), eperm, "unlink");
    Require(std::filesystem::is_directory(fixture.directory.Path()), "directory kept");
    RequireEqual(sceKernelUnlink(fixture.rootName.c_str()), SCE_KERNEL_ERROR_EPERM, "sceKernelUnlink");
}};

const Case unlinkFile{"Unlink_ExistingFile_RemovesIt", [] {
    const StatFixture fixture;
    RequireEqual(unlink_nid_postfix(fixture.presentName.c_str()), 0, "unlink");
    Require(!std::filesystem::exists(fixture.present), "file removed");
}};

const Case kernelCloseTwice{"KernelClose_AlreadyClosedDescriptor_FailsWithEbadf", [] {
    const StatFixture fixture;
    const int closable = sceKernelOpen(fixture.presentName.c_str(), 0, 0);
    Require(closable >= 0, "open");
    RequireEqual(sceKernelClose(closable), 0, "first close");
    RequireEqual(sceKernelClose(closable), SCE_KERNEL_ERROR_EBADF, "second close");
}};

const Case kernelCloseNegative{"KernelClose_NegativeDescriptor_FailsWithEbadf", [] {
    RequireEqual(sceKernelClose(-1), SCE_KERNEL_ERROR_EBADF, "descriptor -1");
}};

} // namespace
