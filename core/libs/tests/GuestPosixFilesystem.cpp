#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <source_location>
#include <string>
#include <stdexcept>
#include "prx/libkernel/File/include/FileFlags.hpp"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "POSIX filesystem check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int main() {
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-posix-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    AddPathAlias_nid_no_patch("/anyps5-posix-test", root.string().c_str());
    constexpr int shared = 0x10;
    constexpr int exclusive = 0x20;
    constexpr int closeOnExec = 0x100000;
    const std::array flagsToTest{0, SCE_KERNEL_O_APPEND, SCE_KERNEL_O_SYNC, shared, exclusive,
        closeOnExec, shared | exclusive, shared | closeOnExec,
        exclusive | SCE_KERNEL_O_APPEND | SCE_KERNEL_O_SYNC | closeOnExec};
    for (const int flags : flagsToTest) {
        std::string temporary = "/anyps5-posix-test/mkostemp-XXXXXXXX";
        const int fd = mkostemp_nid_postfix(temporary.data(), flags);
        Require(fd >= 0);
        Require(temporary.starts_with("/anyps5-posix-test/mkostemp-") && temporary.size() == std::strlen("/anyps5-posix-test/mkostemp-XXXXXXXX"));
        Require(temporary.substr(temporary.size() - 8).find_first_not_of(
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos);
        Require(fcntl_nid_postfix(fd, 1) == ((flags & closeOnExec) ? 1 : 0));
        Require((fcntl_nid_postfix(fd, 3) & (SCE_KERNEL_O_APPEND | SCE_KERNEL_O_SYNC)) ==
            (flags & (SCE_KERNEL_O_APPEND | SCE_KERNEL_O_SYNC)));
#ifdef _WIN32
        DWORD handleFlags = 0;
        Require(GetHandleInformation(reinterpret_cast<HANDLE>(::_get_osfhandle(fd)), &handleFlags));
        if (flags & closeOnExec) Require((handleFlags & HANDLE_FLAG_INHERIT) == 0);
#else
        Require((::fcntl(fd, F_GETFD) & FD_CLOEXEC) == ((flags & closeOnExec) ? FD_CLOEXEC : 0));
        const int nativeStatus = ::fcntl(fd, F_GETFL);
        Require(nativeStatus >= 0);
        Require((nativeStatus & O_APPEND) == ((flags & SCE_KERNEL_O_APPEND) ? O_APPEND : 0));
        Require((nativeStatus & O_SYNC) == ((flags & SCE_KERNEL_O_SYNC) ? O_SYNC : 0));
        struct stat temporaryStatus{};
        Require(::fstat(fd, &temporaryStatus) == 0 && (temporaryStatus.st_mode & 0777) == 0600);
#endif
        Require(write_nid_postfix(fd, "ab", 2) == 2);
        Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
        Require(write_nid_postfix(fd, "c", 1) == 1);
        Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
        std::array<char, 3> content{};
        Require(read_nid_postfix(fd, content.data(), content.size()) == ((flags & SCE_KERNEL_O_APPEND) ? 3 : 2));
        Require(std::string(content.data(), (flags & SCE_KERNEL_O_APPEND) ? 3 : 2) ==
            ((flags & SCE_KERNEL_O_APPEND) ? "abc" : "cb"));
        const int secondFd = open_nid_postfix(temporary.c_str(), SCE_KERNEL_O_RDWR, 0);
        Require(secondFd >= 0);
        const int lockResult = flock_nid_postfix(secondFd, 1 | 4);
        if (flags & exclusive) Require(lockResult == -1 && *__error_nid_postfix() == 35);
        else {
            Require(lockResult == 0);
            Require(flock_nid_postfix(secondFd, 8) == 0);
        }
        if (flags & (shared | exclusive)) {
            Require(flock_nid_postfix(secondFd, 2 | 4) == -1 && *__error_nid_postfix() == 35);
        }
        Require(close_nid_postfix(fd) == 0);
        Require(flock_nid_postfix(secondFd, 2 | 4) == 0);
        Require(flock_nid_postfix(secondFd, 8) == 0);
        Require(close_nid_postfix(secondFd) == 0);
        Require(unlink_nid_postfix(temporary.c_str()) == 0);
    }
    for (const int invalid : {1, 2, 4, 0x200, 0x400, 0x800, 0x40, -1, 0x10001}) {
        std::string untouched = "/anyps5-posix-test/invalid-XXXXXX";
        const auto before = untouched;
        Require(mkostemp_nid_postfix(untouched.data(), invalid) == -1 && *__error_nid_postfix() == 22);
        Require(untouched == before);
        Require(mkostemp_nid_postfix(nullptr, invalid) == -1 && *__error_nid_postfix() == 22);
    }
    bool nullThrew = false;
    try {
        mkostemp_nid_postfix(nullptr, 0);
    } catch (const std::invalid_argument&) {
        nullThrew = true;
    }
    Require(nullThrew);
    const std::array optionalFlags{SCE_KERNEL_O_APPEND, SCE_KERNEL_O_SYNC, shared, exclusive, closeOnExec};
    for (unsigned int combination = 0; combination < 32; ++combination) {
        int flags = 0;
        for (unsigned int bit = 0; bit < optionalFlags.size(); ++bit)
            if (combination & (1u << bit)) flags |= optionalFlags[bit];
        std::string combined = "/anyps5-posix-test/combined-XXXXXX";
        const int fd = mkostemp_nid_postfix(combined.data(), flags);
        Require(fd >= 0);
        Require(fcntl_nid_postfix(fd, 1) == ((flags & closeOnExec) ? 1 : 0));
        Require((fcntl_nid_postfix(fd, 3) & (SCE_KERNEL_O_APPEND | SCE_KERNEL_O_SYNC)) ==
            (flags & (SCE_KERNEL_O_APPEND | SCE_KERNEL_O_SYNC)));
        Require(close_nid_postfix(fd) == 0);
        Require(unlink_nid_postfix(combined.c_str()) == 0);
    }
    for (const int flags : {0, SCE_KERNEL_O_APPEND | SCE_KERNEL_O_SYNC | closeOnExec}) {
        std::string absent = "/anyps5-posix-test/absent/file-XXXXXX";
        Require(mkostemp_nid_postfix(absent.data(), flags) == -1 && *__error_nid_postfix() == 2);
        Require(absent.substr(absent.size() - 6).find_first_not_of(
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos);
        std::string emptyTemplate;
        Require(mkostemp_nid_postfix(emptyTemplate.data(), flags) == -1 && *__error_nid_postfix() == 22);
        std::string longTemplate(1024, 'X');
        Require(mkostemp_nid_postfix(longTemplate.data(), flags) == -1 && *__error_nid_postfix() == 63);
        Require(longTemplate == std::string(1024, 'X'));
        std::string invalidLong(1024, 'X');
        Require(mkostemp_nid_postfix(invalidLong.data(), flags | 4) == -1 && *__error_nid_postfix() == 22);
        Require(invalidLong == std::string(1024, 'X'));
    }
    for (const unsigned int count : {1u, 2u, 6u, 17u}) {
        const std::string base = "/anyps5-posix-test/trailing-";
        std::string arbitrary = base + std::string(count, 'X');
        const int fd = mkostemp_nid_postfix(arbitrary.data(), closeOnExec);
        Require(fd >= 0 && arbitrary.starts_with(base) && arbitrary.size() == base.size() + count);
        Require(arbitrary.substr(base.size()).find_first_not_of(
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos);
        Require(close_nid_postfix(fd) == 0);
        Require(unlink_nid_postfix(arbitrary.c_str()) == 0);
    }
    Require(std::filesystem::create_directory(root / "directory-X"));
    for (const int flags : {0, closeOnExec | exclusive}) {
        std::string nested = "/anyps5-posix-test/directory-X/nested-XXX";
        const int fd = mkostemp_nid_postfix(nested.data(), flags);
        Require(fd >= 0 && nested.starts_with("/anyps5-posix-test/directory-X/nested-"));
        Require(std::filesystem::is_directory(root / "directory-X"));
        Require(close_nid_postfix(fd) == 0);
        Require(unlink_nid_postfix(nested.c_str()) == 0);
    }
    Require(std::filesystem::remove(root / "directory-X"));
    std::string prefixWithX = "/anyps5-posix-test/embedded-X-fixed";
    const int embedded = mkostemp_nid_postfix(prefixWithX.data(), closeOnExec);
    Require(embedded >= 0 && prefixWithX == "/anyps5-posix-test/embedded-X-fixed");
    Require(close_nid_postfix(embedded) == 0);
    std::string underFile = prefixWithX + "/child-XXXXXX";
    Require(mkostemp_nid_postfix(underFile.data(), 0) == -1 && *__error_nid_postfix() == 20);
    Require(unlink_nid_postfix(prefixWithX.c_str()) == 0);
    std::string directoryTemplate = "/anyps5-posix-test/";
    Require(mkostemp_nid_postfix(directoryTemplate.data(), 0) == -1 && *__error_nid_postfix() == 17);
    const std::string prefix = "/anyps5-posix-test/file-";
    std::string pattern = prefix + "XXXXXXXXX";
    const int descriptor = mkstemp_nid_postfix(pattern.data());
    Require(descriptor >= 0);
    Require(pattern.starts_with(prefix) && pattern.size() == prefix.size() + 9);
    Require(pattern.substr(prefix.size()).find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos);
    Require(std::filesystem::file_size(ResolvePath_nid_no_patch(pattern.c_str())) == 0);
#ifndef _WIN32
    struct stat status{};
    Require(::fstat(descriptor, &status) == 0 && (status.st_mode & 0777) == 0600);
#endif
    const char payload[] = {'a', '\0', '\r', '\n', '\x1a', 'z'};
    Require(write_nid_postfix(descriptor, payload, sizeof(payload)) == sizeof(payload));
    Require(lseek_nid_postfix(descriptor, 0, SEEK_SET) == 0);
    std::array<char, sizeof(payload)> received{};
    Require(read_nid_postfix(descriptor, received.data(), received.size()) == static_cast<std::int64_t>(received.size()));
    Require(std::memcmp(payload, received.data(), sizeof(payload)) == 0);
    Require(isatty_nid_postfix(descriptor) == 0 && *__error_nid_postfix() == 25);
    Require(close_nid_postfix(descriptor) == 0);
    Require(isatty_nid_postfix(descriptor) == 0 && *__error_nid_postfix() == 9);
    std::string second = prefix + "XXXXXXXXX";
    const int other = mkstemp_nid_postfix(second.data());
    Require(other >= 0 && second != pattern);
    Require(close_nid_postfix(other) == 0);
    const std::string candidates = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (const char character : candidates) {
        std::ofstream stream(root / (std::string("occupied-") + character));
        stream << "preserved";
        Require(stream.good());
    }
    std::string occupied = "/anyps5-posix-test/occupied-X";
    Require(mkstemp_nid_postfix(occupied.data()) == -1 && *__error_nid_postfix() == 17);
    for (const char character : candidates) {
        std::ifstream stream(root / (std::string("occupied-") + character));
        std::string contents;
        stream >> contents;
        Require(contents == "preserved");
    }
    Require(std::filesystem::remove(root / "occupied-7"));
    occupied = "/anyps5-posix-test/occupied-X";
    const int collision = mkstemp_nid_postfix(occupied.data());
    Require(collision >= 0 && occupied == "/anyps5-posix-test/occupied-7");
    Require(close_nid_postfix(collision) == 0);
    occupied = "/anyps5-posix-test/occupied-X";
    Require(mkostemp_nid_postfix(occupied.data(), closeOnExec) == -1 && *__error_nid_postfix() == 17);
    Require(std::filesystem::remove(root / "occupied-8"));
    occupied = "/anyps5-posix-test/occupied-X";
    const int flaggedCollision = mkostemp_nid_postfix(occupied.data(), closeOnExec);
    Require(flaggedCollision >= 0 && occupied == "/anyps5-posix-test/occupied-8");
    Require(fcntl_nid_postfix(flaggedCollision, 1) == 1);
    Require(close_nid_postfix(flaggedCollision) == 0);
    std::string flaggedFixed = "/anyps5-posix-test/flagged-fixed";
    const int fixedFd = mkostemp_nid_postfix(flaggedFixed.data(), 0);
    Require(fixedFd >= 0 && flaggedFixed == "/anyps5-posix-test/flagged-fixed");
    Require(write_nid_postfix(fixedFd, "kept", 4) == 4);
    Require(close_nid_postfix(fixedFd) == 0);
    Require(mkostemp_nid_postfix(flaggedFixed.data(), 0) == -1 && *__error_nid_postfix() == 17);
    const int preservedFd = open_nid_postfix(flaggedFixed.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(preservedFd >= 0);
    std::array<char, 4> preserved{};
    Require(read_nid_postfix(preservedFd, preserved.data(), preserved.size()) == 4);
    Require(std::string(preserved.data(), preserved.size()) == "kept");
    Require(close_nid_postfix(preservedFd) == 0);
    std::string fixed = "/anyps5-posix-test/fixed";
    const int unchanged = mkstemp_nid_postfix(fixed.data());
    Require(unchanged >= 0 && fixed == "/anyps5-posix-test/fixed");
    Require(close_nid_postfix(unchanged) == 0);
    Require(mkstemp_nid_postfix(fixed.data()) == -1 && *__error_nid_postfix() == 17);
    std::string missing = "/anyps5-posix-test/missing/file-XXXXXX";
    Require(mkstemp_nid_postfix(missing.data()) == -1 && *__error_nid_postfix() == 2);
    std::string notDirectory = fixed + "/file-XXXXXX";
    Require(mkstemp_nid_postfix(notDirectory.data()) == -1);
    Require(*__error_nid_postfix() == 20 || *__error_nid_postfix() == 2);
    std::string empty;
    Require(mkstemp_nid_postfix(empty.data()) == -1 && *__error_nid_postfix() == 22);
    Require(mkstemp_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    std::string tooLong(1024, 'X');
    Require(mkstemp_nid_postfix(tooLong.data()) == -1 && *__error_nid_postfix() == 63);
    Require(tooLong == std::string(1024, 'X'));
    Require(isatty_nid_postfix(-1) == 0 && *__error_nid_postfix() == 9);
    Require(isatty_nid_postfix(100000) == 0 && *__error_nid_postfix() == 9);
    int pipes[2]{};
    Require(pipe_nid_postfix(pipes) == 0);
    for (const int pipe : pipes) {
        Require(isatty_nid_postfix(pipe) == 0 && *__error_nid_postfix() == 25);
        Require(close_nid_postfix(pipe) == 0);
    }
    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0 && socket < 1024);
    Require(isatty_nid_postfix(socket) == 0 && *__error_nid_postfix() == 25);
    Require(close_nid_postfix(socket) == 0);
    Require(isatty_nid_postfix(socket) == 0 && *__error_nid_postfix() == 9);
#ifdef _WIN32
    const int nullDevice = ::_open("NUL", _O_RDWR);
    Require(nullDevice >= 0);
    Require(isatty_nid_postfix(nullDevice) == 0 && *__error_nid_postfix() == 25);
    Require(::_close(nullDevice) == 0);
    const int console = ::_open("CONIN$", _O_RDONLY);
    if (console >= 0) {
        *__error_nid_postfix() = 123;
        Require(isatty_nid_postfix(console) == 1 && *__error_nid_postfix() == 123);
        Require(::_close(console) == 0);
    }
#else
    const int terminal = ::posix_openpt(O_RDWR | O_NOCTTY);
    Require(terminal >= 0 && ::grantpt(terminal) == 0 && ::unlockpt(terminal) == 0);
    const int slave = ::open(::ptsname(terminal), O_RDWR | O_NOCTTY);
    Require(slave >= 0);
    *__error_nid_postfix() = 123;
    Require(isatty_nid_postfix(slave) == 1 && *__error_nid_postfix() == 123);
    Require(::close(slave) == 0 && ::close(terminal) == 0);
#endif
    Require(setenv_nid_postfix("WEBKIT_WRAP_SET", "set", 1) == 0);
    char* const variable = getenv_nid_postfix("WEBKIT_WRAP_SET");
    Require(variable != nullptr && std::strcmp(variable, "set") == 0 && __wrap_getenv("WEBKIT_WRAP_SET") == variable);
    Require(getenv_nid_postfix("WEBKIT_WRAP_UNSET") == nullptr && __wrap_getenv("WEBKIT_WRAP_UNSET") == nullptr);
    Require(access_nid_postfix(pattern.c_str(), 0) == 0 && __wrap_access(pattern.c_str(), 0) == 0);
    Require(access_nid_postfix("/anyps5-posix-test/absent", 0) == -1);
    const int accessError = *__error_nid_postfix();
    *__error_nid_postfix() = 0;
    Require(__wrap_access("/anyps5-posix-test/absent", 0) == -1 && *__error_nid_postfix() == accessError && accessError == 2);
    std::array<char, 1024> expectedDirectory{};
    std::array<char, 1024> wrappedDirectory{};
    Require(getcwd_nid_postfix(expectedDirectory.data(), expectedDirectory.size()) == expectedDirectory.data());
    Require(__wrap_getcwd(wrappedDirectory.data(), wrappedDirectory.size()) == wrappedDirectory.data());
    Require(std::strcmp(expectedDirectory.data(), wrappedDirectory.data()) == 0);
    Require(getcwd_nid_postfix(wrappedDirectory.data(), 1) == nullptr);
    const int directoryError = *__error_nid_postfix();
    *__error_nid_postfix() = 0;
    Require(__wrap_getcwd(wrappedDirectory.data(), 1) == nullptr && *__error_nid_postfix() == directoryError && directoryError == 34);
    FileStat expectedStatus{};
    FileStat wrappedStatus{};
    Require(lstat_nid_postfix(pattern.c_str(), &expectedStatus) == 0 && __wrap_lstat(pattern.c_str(), &wrappedStatus) == 0);
    Require((wrappedStatus.st_mode & 0170000) == 0100000 && wrappedStatus.st_size == static_cast<std::int64_t>(sizeof(payload)));
    Require(std::memcmp(&expectedStatus, &wrappedStatus, sizeof(FileStat)) == 0);
    const auto notAfter = [](const KernelTimespec& earlier, const KernelTimespec& later) {
        return earlier.tv_sec < later.tv_sec || (earlier.tv_sec == later.tv_sec && earlier.tv_nsec <= later.tv_nsec);
    };
    KernelTimespec before{};
    KernelTimespec during{};
    KernelTimespec after{};
    Require(clock_gettime_nid_postfix(4, &before) == 0 && __wrap_clock_gettime(4, &during) == 0 && clock_gettime_nid_postfix(4, &after) == 0);
    Require(notAfter(before, during) && notAfter(during, after));
    Require(clock_gettime_nid_postfix(4, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(__wrap_clock_gettime(4, nullptr) == -1 && *__error_nid_postfix() == 14);
    constexpr std::size_t page = 0x4000;
    void* const failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    void* const mapped = __wrap_mmap(nullptr, page, 3, 0x1002, -1, 0);
    Require(mapped != failed);
    auto* const bytes = static_cast<volatile unsigned char*>(mapped);
    bytes[0] = 0x5a;
    bytes[page - 1] = 0xa5;
    Require(bytes[0] == 0x5a && bytes[page - 1] == 0xa5);
    Require(munmap_nid_postfix(mapped, page) == 0);
    Require(mmap_nid_postfix(nullptr, page, 8, 0x1002, -1, 0) == failed && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(__wrap_mmap(nullptr, page, 8, 0x1002, -1, 0) == failed && *__error_nid_postfix() == 22);
    Require(opendir_nid_disambig1_nid_postfix(nullptr) == nullptr && *__error_nid_postfix() == 14);
    Require(opendir_nid_disambig1_nid_postfix("") == nullptr && *__error_nid_postfix() == 2);
    Require(opendir_nid_disambig1_nid_postfix("/anyps5-posix-test/missing-directory") == nullptr && *__error_nid_postfix() == 2);
    void* const testDirectory = opendir_nid_disambig1_nid_postfix("/anyps5-posix-test");
    Require(testDirectory != nullptr);
    bool foundDot = false;
    bool foundDotDot = false;
    bool foundFixed = false;
    while (auto* const entry = readdir_nid_disambig1_nid_postfix(testDirectory)) {
        Require(entry->nameLength == std::strlen(entry->name));
        Require(entry->recordLength >= 8 + entry->nameLength);
        Require(entry->fileNumber != 0);
        if (std::strcmp(entry->name, ".") == 0) foundDot = true;
        if (std::strcmp(entry->name, "..") == 0) foundDotDot = true;
        if (std::strcmp(entry->name, "fixed") == 0) foundFixed = true;
    }
    Require(foundDot && foundDotDot && foundFixed);
    Require(readdir_nid_disambig1_nid_postfix(testDirectory) == nullptr);
    Require(readdir_nid_disambig1_nid_postfix(nullptr) == nullptr && *__error_nid_postfix() == 9);
    Require(closedir_nid_disambig1_nid_postfix(testDirectory) == 0);
    Require(closedir_nid_disambig1_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 9);
    void* const readdirDirectory = opendir_nid_disambig1_nid_postfix("/anyps5-posix-test");
    Require(readdirDirectory != nullptr);
    GuestDirectoryEntry readdirEntry{};
    GuestDirectoryEntry* readdirResult = nullptr;
    Require(readdir_r_nid_disambig1_nid_postfix(readdirDirectory, &readdirEntry, &readdirResult) == 0);
    Require(readdirResult == &readdirEntry);
    Require(readdirEntry.nameLength == std::strlen(readdirEntry.name));
    Require(closedir_nid_disambig1_nid_postfix(readdirDirectory) == 0);
    Require(readdir_r_nid_disambig1_nid_postfix(nullptr, &readdirEntry, &readdirResult) == 9);
    RemovePathAlias_nid_no_patch("/anyps5-posix-test");
    Require(std::filesystem::remove_all(root) > 0);
}
