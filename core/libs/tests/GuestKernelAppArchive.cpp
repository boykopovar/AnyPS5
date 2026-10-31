#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "AppArchiveFixture.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestFileNumber.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/KernelStatfs.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/AppArchive.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#elif defined(__linux__)
#include <csignal>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

struct KernelLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "app0.zar kernel check failed at line %d\n", line);
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

template <typename TCall> bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), "not implemented") != nullptr;
    }
    return false;
}

constexpr std::size_t BigSize = 3 * 65536 + 77;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t RegularType = 0100000;
constexpr std::uint16_t DirectoryType = 0040000;
constexpr int DuplicateFrom = 0;
constexpr std::uint64_t ReadableRequest = 0x4004667f;

void Archive(const std::filesystem::path& root) {
    std::map<std::string, std::vector<char>> files{
        {"Data/Big.bin", AppArchiveFixture::Pattern(BigSize)},
        {"Data/Small.txt", {'z', 'a', 'r'}},
        {"sce_sys/Packed.json", {'{', '}'}},
        {"eboot.bin", {'o', 'l', 'd'}},
        {"Readme.TXT", {'z'}},
        {"MaskedDir/Child.txt", {'d'}},
        {"MaskedFile", {'f'}},
    };
    for (unsigned index = 0; index < 32; ++index) {
        const auto suffix = std::string(1, static_cast<char>('a' + index / 10)) + static_cast<char>('0' + index % 10);
        files.emplace("sce_sys/Concurrent/" + suffix, std::vector<char>{static_cast<char>(index)});
    }
    AppArchiveFixture::WriteArchive(root / "app0.zar", files, {"EmptyArchive"});
    std::ofstream eboot(root / "app0" / "eboot.bin", std::ios::binary);
    eboot << "disk";
    std::ofstream readme(root / "app0" / "README.txt", std::ios::binary);
    readme << "disk";
    std::ofstream masked(root / "app0" / "MaskedDir", std::ios::binary);
    masked << "disk";
    std::filesystem::create_directory(root / "app0" / "MaskedFile");
}

void FileNumbers() {
    constexpr std::uint32_t Fold = 0x9E3779B1u;
    Check(GuestFileNumber_nid_no_patch(7) == 7);
    Check(GuestFileNumber_nid_no_patch((std::uint64_t{1} << 32) | 7) == (Fold ^ 7u));
    Check(GuestFileNumber_nid_no_patch((std::uint64_t{1} << 32) | Fold) == 1);
}

void ConcurrentArchiveDirectoryEntries() {
    constexpr int WorkerCount = 8;
    constexpr int ChildCount = 32;
    struct Entry {
        std::int64_t base;
        std::string name;
    };
    const int directory = sceKernelOpen("/app0/sce_sys/Concurrent", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(directory >= 0);
    std::barrier start(WorkerCount);
    std::mutex entriesMutex;
    std::vector<Entry> entries;
    entries.reserve(ChildCount + 2);
    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    workers.reserve(WorkerCount);
    for (int worker = 0; worker < WorkerCount; ++worker) {
        workers.emplace_back([&] {
            start.arrive_and_wait();
            for (;;) {
                std::array<char, 12> record{};
                std::int64_t base = -1;
                const int bytes = sceKernelGetdirentries(directory, record.data(), static_cast<int>(record.size()), &base);
                if (bytes == 0) break;
                if (bytes != static_cast<int>(record.size()) || base < 0 || record[7] == 0) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
                std::lock_guard lock(entriesMutex);
                entries.push_back({base, std::string(record.data() + 8)});
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Check(failures.load(std::memory_order_relaxed) == 0);
    Check(sceKernelClose(directory) == 0);
    std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) { return left.base < right.base; });
    Check(entries.size() == ChildCount + 2);
    Check(entries[0].base == 0 && entries[0].name == ".");
    Check(entries[1].base == 1 && entries[1].name == "..");
    for (int index = 0; index < ChildCount; ++index) {
        const auto name = std::string(1, static_cast<char>('a' + index / 10)) + static_cast<char>('0' + index % 10);
        Check(entries[index + 2].base == index + 2 && entries[index + 2].name == name);
    }
}

std::map<std::string, std::uint8_t> ReadDirectory(void* directory) {
    std::map<std::string, std::uint8_t> entries;
    while (auto* entry = readdir_nid_postfix(directory)) Check(entries.emplace(entry->name, entry->type).second);
    return entries;
}

std::map<std::string, std::uint8_t> ReadKernelDirectory(int fd) {
    std::array<char, 4096> bytes{};
    std::map<std::string, std::uint8_t> entries;
    const int count = sceKernelGetdents(fd, bytes.data(), static_cast<int>(bytes.size()));
    Check(count >= 0);
    for (int offset = 0; offset < count;) {
        std::uint16_t recordLength = 0;
        std::memcpy(&recordLength, bytes.data() + offset + 4, sizeof(recordLength));
        const auto nameLength = static_cast<std::uint8_t>(bytes[offset + 7]);
        const std::string name(bytes.data() + offset + 8);
        Check(name.size() == nameLength && recordLength == ((8 + nameLength + 1 + 3) & ~3));
        Check(offset + recordLength <= count && entries.emplace(name, static_cast<std::uint8_t>(bytes[offset + 6])).second);
        offset += recordLength;
    }
    return entries;
}

void Directories() {
    void* archive = opendir_nid_postfix("/app0/Data");
    Check(archive != nullptr);
    const auto archiveEntries = ReadDirectory(archive);
    Check(archiveEntries.size() == 4 && archiveEntries.at(".") == 4 && archiveEntries.at("..") == 4);
    Check(archiveEntries.at("Big.bin") == 8 && archiveEntries.at("Small.txt") == 8);
    GuestDirectoryEntry copied{};
    GuestDirectoryEntry* copiedResult = nullptr;
    rewinddir_nid_postfix(archive);
    Check(readdir_r_nid_postfix(archive, &copied, &copiedResult) == 0 && copiedResult == &copied && std::strcmp(copied.name, ".") == 0);
    rewinddir_nid_postfix(archive);
    Check(telldir_nid_postfix(archive) == 0 && readdir_nid_postfix(archive) != nullptr);
    Check(telldir_nid_postfix(archive) == 1);
    seekdir_nid_postfix(archive, 0);
    Check(readdir_nid_postfix(archive) != nullptr);
    rewinddir_nid_postfix(archive);
    Check(closedir_nid_postfix(archive) == 0);

    void* merged = opendir_nid_postfix("/app0");
    Check(merged != nullptr);
#ifdef _WIN32
    bool diskIdentityChecked = false;
    while (auto* entry = readdir_nid_postfix(merged)) {
        if (std::strcmp(entry->name, "README.txt") != 0) continue;
        FileStat identity{};
        Check(sceKernelStat("/app0/README.txt", &identity) == 0);
        Check(entry->fileNumber != 0 && entry->fileNumber == static_cast<std::uint32_t>(identity.st_ino));
        diskIdentityChecked = true;
    }
    Check(diskIdentityChecked);
    rewinddir_nid_postfix(merged);
#endif
    const auto mergedEntries = ReadDirectory(merged);
    Check(mergedEntries.at("eboot.bin") == 8 && mergedEntries.at("README.txt") == 8);
    Check(mergedEntries.at("EmptyArchive") == 4 && mergedEntries.at("Data") == 4);
    Check(mergedEntries.at("MaskedDir") == 8 && mergedEntries.at("MaskedFile") == 4);
    Check(mergedEntries.size() == 9);
    Check(closedir_nid_postfix(merged) == 0);

    void* refreshed = opendir_nid_postfix("/app0");
    Check(refreshed != nullptr && readdir_nid_postfix(refreshed) != nullptr);
    const auto addedPath = std::filesystem::current_path() / "app0" / "AfterOpen.txt";
    { std::ofstream added(addedPath); added << "disk"; }
    rewinddir_nid_postfix(refreshed);
    const auto refreshedEntries = ReadDirectory(refreshed);
    Check(refreshedEntries.contains("AfterOpen.txt"));
    Check(closedir_nid_postfix(refreshed) == 0);
    std::filesystem::remove(addedPath);

    const int fd = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(fd >= 0);
    FileStat pathStatus{};
    FileStat descriptorStatus{};
    Check(sceKernelStat("/app0/Data", &pathStatus) == 0 && sceKernelFstat(fd, &descriptorStatus) == 0);
    Check((descriptorStatus.st_mode & TypeMask) == DirectoryType && descriptorStatus.st_ino == pathStatus.st_ino && descriptorStatus.st_dev == pathStatus.st_dev);
    char rejectedByte = 0;
    Check(sceKernelRead(fd, &rejectedByte, 1) == SCE_KERNEL_ERROR_EISDIR);
    Check(sceKernelWrite(fd, &rejectedByte, 1) == SCE_KERNEL_ERROR_EISDIR);
    Check(sceKernelPread(fd, &rejectedByte, 1, 0) == SCE_KERNEL_ERROR_EISDIR);
    const KernelIovec rejectedIovec{&rejectedByte, 1};
    Check(sceKernelReadv(fd, &rejectedIovec, 1) == SCE_KERNEL_ERROR_EISDIR);
    Check(sceKernelPreadv(fd, &rejectedIovec, 1, 0) == SCE_KERNEL_ERROR_EISDIR);
    Check(sceKernelFsync(fd) == SCE_KERNEL_ERROR_EISDIR && sceKernelFdatasync(fd) == SCE_KERNEL_ERROR_EISDIR);
    Check(ftruncate_nid_postfix(fd, 0) == -1 && *__error_nid_postfix() == 22);
    Check(ftruncate_nid_postfix(fd, 8) == -1 && *__error_nid_postfix() == 22);
    Check(sceKernelFchmod(fd, 0600) == SCE_KERNEL_ERROR_EISDIR);
    Check(sceKernelFutimes(fd, nullptr) == SCE_KERNEL_ERROR_EISDIR);
    Check(flock_nid_postfix(fd, 1) == -1);
    KernelLock recordLock{};
    Check(fcntl_nid_postfix(fd, 12, &recordLock) == -1);
    Check(_fstatfs_nid_postfix(fd, nullptr) == -1);
    int available = 0;
    Check(ioctl_nid_postfix(fd, ReadableRequest, &available) == -1);
    Check(mmap_nid_postfix(nullptr, 4096, 1, 1, fd, 0) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    std::array<char, 12> first{};
    Check(sceKernelGetdents(fd, first.data(), 1) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelGetdents(fd, first.data(), static_cast<int>(first.size())) == 12);
    Check(std::string(first.data() + 8) == ".");
    std::uint32_t dotNumber = 0;
    std::memcpy(&dotNumber, first.data(), sizeof(dotNumber));
    Check(dotNumber == static_cast<std::uint32_t>(descriptorStatus.st_ino));
    const int duplicate = dup_nid_postfix(fd);
    Check(duplicate >= 0);
    std::array<char, 12> second{};
    Check(sceKernelGetdents(duplicate, second.data(), static_cast<int>(second.size())) == 12);
    Check(std::string(second.data() + 8) == "..");
    FileStat parentStatus{};
    std::uint32_t dotdotNumber = 0;
    std::memcpy(&dotdotNumber, second.data(), sizeof(dotdotNumber));
    Check(sceKernelStat("/app0", &parentStatus) == 0 && dotdotNumber == static_cast<std::uint32_t>(parentStatus.st_ino));
    Check(sceKernelLseek(fd, 0, 1) == 2);
    Check(sceKernelClose(fd) == 0);
    const auto remaining = ReadKernelDirectory(duplicate);
    Check(remaining.size() == 2 && remaining.contains("Big.bin") && remaining.contains("Small.txt"));
    Check(sceKernelClose(duplicate) == 0);

    const int reused = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(reused >= 0 && File::IsDirectoryDescriptor(reused));
    Check(sceKernelClose(reused) == 0);
    const int regular = sceKernelOpen("/app0/eboot.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(regular == reused && !File::IsDirectoryDescriptor(regular));
    Check(sceKernelFstat(regular, &descriptorStatus) == 0 && (descriptorStatus.st_mode & TypeMask) == RegularType);
    Check(sceKernelGetdents(regular, first.data(), static_cast<int>(first.size())) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelGetdents(regular, first.data(), 1) == SCE_KERNEL_ERROR_EINVAL);
    void* invalidStream = fdopendir_nid_postfix(regular);
    Check(invalidStream == nullptr);
    Check(sceKernelClose(regular) == 0);

    const int replaced = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    const int replacement = sceKernelOpen("/app0/eboot.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(replaced >= 0 && replacement >= 0 && dup2_nid_postfix(replacement, replaced) == replaced);
    Check(!File::IsDirectoryDescriptor(replaced) && sceKernelFstat(replaced, &descriptorStatus) == 0 && (descriptorStatus.st_mode & TypeMask) == RegularType);
    Check(sceKernelClose(replaced) == 0 && sceKernelClose(replacement) == 0);

    const int duplicateFrom = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    const int duplicateTo = fcntl_nid_postfix(duplicateFrom, DuplicateFrom, duplicateFrom + 5);
    Check(duplicateFrom >= 0 && duplicateTo >= duplicateFrom + 5 && File::IsDirectoryDescriptor(duplicateTo));
    Check(sceKernelClose(duplicateFrom) == 0 && File::IsDirectoryDescriptor(duplicateTo));
    Check(sceKernelClose(duplicateTo) == 0);

    const int owned = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(owned >= 0 && sceKernelGetdents(owned, first.data(), static_cast<int>(first.size())) == 12);
    void* stream = fdopendir_nid_postfix(owned);
    Check(stream != nullptr);
    auto* next = readdir_nid_postfix(stream);
    Check(next != nullptr && std::strcmp(next->name, "..") == 0);
    const auto cookie = telldir_nid_postfix(stream);
    Check(cookie == 2);
    next = readdir_nid_postfix(stream);
    Check(next != nullptr && std::strcmp(next->name, "Big.bin") == 0);
    seekdir_nid_postfix(stream, cookie);
    next = readdir_nid_postfix(stream);
    Check(next != nullptr && std::strcmp(next->name, "Big.bin") == 0);
    Check(closedir_nid_postfix(stream) == 0);
    Check(!DescriptorIsOpen_nid_no_patch(owned));
}

void Reads() {
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    const int fd = sceKernelOpen("/app0/data/big.BIN", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    std::vector<char> buffer(BigSize + 16);
    Check(sceKernelRead(fd, buffer.data(), 70000) == 70000);
    Check(std::equal(buffer.begin(), buffer.begin() + 70000, expected.begin()));
    Check(sceKernelLseek(fd, 0, 1) == 70000);
    Check(lseek_nid_postfix(fd, 0, 1) == 70000);

    std::vector<char> left(10), right(65536);
    const KernelIovec vector[] = {{left.data(), left.size()}, {right.data(), right.size()}};
    Check(sceKernelReadv(fd, vector, 2) == 65546);
    Check(std::equal(left.begin(), left.end(), expected.begin() + 70000) && std::equal(right.begin(), right.end(), expected.begin() + 70010));
    Check(sceKernelPreadv(fd, vector, 2, 5) == 65546);
    Check(std::equal(left.begin(), left.end(), expected.begin() + 5) && std::equal(right.begin(), right.end(), expected.begin() + 15));
    Check(sceKernelLseek(fd, 0, 1) == 135546);

    Check(sceKernelPread(fd, buffer.data(), 100, 65500) == 100 && std::equal(buffer.begin(), buffer.begin() + 100, expected.begin() + 65500));
    Check(pread_nid_postfix(fd, buffer.data(), 100, BigSize - 40) == 40 && std::equal(buffer.begin(), buffer.begin() + 40, expected.end() - 40));
    Check(sceKernelPread(fd, buffer.data(), 1, -1) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelLseek(fd, 0, 1) == 135546);

    int available = 0;
    Check(ioctl_nid_postfix(fd, ReadableRequest, &available) == 0 && available == static_cast<int>(BigSize - 135546));

    Check(sceKernelLseek(fd, -7, 2) == static_cast<std::int64_t>(BigSize - 7));
    Check(sceKernelRead(fd, buffer.data(), 100) == 7 && std::equal(buffer.begin(), buffer.begin() + 7, expected.end() - 7));
    Check(sceKernelRead(fd, buffer.data(), 100) == 0);
    Check(sceKernelLseek(fd, -1, 0) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelLseek(fd, 0, 1) == static_cast<std::int64_t>(BigSize));
    Check(sceKernelWrite(fd, "x", 1) == SCE_KERNEL_ERROR_EBADF);

    FileStat status{};
    Check(sceKernelFstat(fd, &status) == 0);
    Check((status.st_mode & TypeMask) == RegularType && status.st_size == static_cast<std::int64_t>(BigSize) && status.st_nlink == 1);

    KernelAioResult result{-1, 0};
    KernelAioRwRequest request{65530, 20, buffer.data(), &result, fd};
    std::int32_t id = 0;
    std::int32_t state = 0;
    Check(sceKernelAioSubmitReadCommands(&request, 1, 1, &id) == 0);
    Check(sceKernelAioWaitRequest(id, &state, nullptr) == 0);
    Check(result.return_value == 20 && std::equal(buffer.begin(), buffer.begin() + 20, expected.begin() + 65530));
    std::int32_t removed = 0;
    Check(sceKernelAioDeleteRequest(id, &removed) == 0);

    Check(sceKernelLseek(fd, 1000, 0) == 1000);
    const int duplicate = dup_nid_postfix(fd);
    Check(duplicate >= 0 && sceKernelLseek(duplicate, 0, 1) == 1000);
    Check(sceKernelRead(duplicate, buffer.data(), 24) == 24 && sceKernelLseek(fd, 0, 1) == 1024);
    const int above = fcntl_nid_postfix(fd, DuplicateFrom, 200);
    Check(above >= 200 && sceKernelLseek(above, 0, 1) == 1024);
    Check(sceKernelClose(fd) == 0);
    Check(sceKernelRead(duplicate, buffer.data(), 4) == 4 && std::equal(buffer.begin(), buffer.begin() + 4, expected.begin() + 1024));
    Check(sceKernelLseek(above, 0, 1) == 1028);
    Check(close_nid_postfix(above) == 0);

    const int plain = sceKernelOpen("/app0/eboot.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(plain >= 0);
    Check(dup2_nid_postfix(plain, duplicate) == duplicate);
    Check(sceKernelPread(duplicate, buffer.data(), 8, 0) == 4 && std::memcmp(buffer.data(), "disk", 4) == 0);
    Check(sceKernelClose(plain) == 0);
    Check(sceKernelClose(duplicate) == 0);
}

void Status() {
    FileStat status{};
    Check(sceKernelStat("/app0/Data/Small.txt", &status) == 0);
    Check((status.st_mode & TypeMask) == RegularType && (status.st_mode & 0222) == 0 && status.st_size == 3 && status.st_blocks == 1);
    Check(sceKernelStat("/app0/data", &status) == 0 && (status.st_mode & TypeMask) == DirectoryType);
    Check(sceKernelStat("/app0/Data/", &status) == 0);
    Check(sceKernelStat("/app0/Data/Small.txt/", &status) == SCE_KERNEL_ERROR_ENOTDIR);
    Check(sceKernelStat("/app0/Data/Missing.txt", &status) == SCE_KERNEL_ERROR_ENOENT);
    Check(sceKernelStat("/app0/eboot.bin", &status) == 0 && status.st_size == 4);
    Check(sceKernelCheckReachability("/app0/Data/Small.txt") == 0);
    Check(sceKernelCheckReachability("/app0/Data/Missing.txt") == SCE_KERNEL_ERROR_ENOENT);
}

void Refusals() {
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDWR, 0) == SCE_KERNEL_ERROR_EROFS);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_TRUNC, 0) == SCE_KERNEL_ERROR_EROFS);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0666) == SCE_KERNEL_ERROR_EEXIST);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0) == SCE_KERNEL_ERROR_ENOTDIR);
    Check(sceKernelOpen("/app0/Data/Missing.txt", SCE_KERNEL_O_RDONLY, 0) == SCE_KERNEL_ERROR_ENOENT);
    const int directory = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(directory >= 0 && sceKernelClose(directory) == 0);
    void* archivedDirectory = opendir_nid_postfix("/app0/sce_sys");
    Check(archivedDirectory != nullptr && closedir_nid_postfix(archivedDirectory) == 0);
    Directories();
    const int fd = sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    Check(NotImplemented([fd] { (void)fdopen_nid_postfix(fd, "r"); }));
    Check(NotImplemented([fd] { (void)flock_nid_postfix(fd, 1); }));
    Check(NotImplemented([fd] { alignas(8) char statfs[512]; (void)_fstatfs_nid_postfix(fd, reinterpret_cast<KernelStatfs*>(statfs)); }));
    Check(NotImplemented([fd] { (void)ftruncate_nid_postfix(fd, 0); }));
    Check(NotImplemented([fd] { (void)sceKernelFsync(fd); }));
    Check(NotImplemented([fd] { (void)sceKernelFdatasync(fd); }));
    Check(NotImplemented([fd] { char entries[512]; (void)sceKernelGetdents(fd, entries, sizeof(entries)); }));
    Check(NotImplemented([fd] { (void)sceKernelFchmod(fd, 0444); }));
    Check(NotImplemented([fd] { (void)sceKernelFutimes(fd, nullptr); }));
    FileStat status{};
    Check(sceKernelFstat(fd, &status) == 0 && status.st_size == 3);
    Check(sceKernelClose(fd) == 0);
}

void WithoutArchive() {
    { std::ofstream file(std::filesystem::current_path() / "app0" / "Plain.txt", std::ios::binary); file << "plain"; }
    const int fd = sceKernelOpen("/app0/Plain.txt", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    char buffer[8] = {};
    Check(sceKernelRead(fd, buffer, sizeof(buffer)) == 5 && std::memcmp(buffer, "plain", 5) == 0);
    Check(sceKernelClose(fd) == 0);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY, 0) == SCE_KERNEL_ERROR_ENOENT);
    FileStat status{};
    Check(sceKernelStat("/app0/Data/Small.txt", &status) == SCE_KERNEL_ERROR_ENOENT);
    const int directory = sceKernelOpen("/app0/sce_sys", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(directory >= 0 && sceKernelClose(directory) == 0);
}

void NativeDirectoryIdentity(const std::filesystem::path& root) {
    const auto original = root / "ordinary-directory-original";
    const auto renamed = root / "ordinary-directory-renamed";
    std::filesystem::create_directory(original);
    { std::ofstream file(original / "original-entry"); file << "old"; }
    const int fd = File::OpenDirectoryDescriptor(original);
    Check(fd >= 0 && File::IsDirectoryDescriptor(fd) && !File::IsVirtualDirectoryDescriptor(fd) && File::IsNativeDirectoryDescriptor(fd));
    FileStat before{};
    Check(sceKernelFstat(fd, &before) == 0);
    std::filesystem::rename(original, renamed);
    std::filesystem::create_directory(original);
    { std::ofstream file(original / "replacement-entry"); file << "new"; }
#ifdef _WIN32
    auto expectedPath = renamed;
    auto expectedNative = expectedPath.native();
    if (expectedNative.size() >= 2 && expectedNative[1] == L':') {
        if (expectedNative[0] >= L'A' && expectedNative[0] <= L'Z') expectedNative[0] += L'a' - L'A';
        else if (expectedNative[0] >= L'a' && expectedNative[0] <= L'z') expectedNative[0] -= L'a' - L'A';
        expectedPath = expectedNative;
    }
#else
    const auto expectedPath = renamed;
#endif
    FileStat after{};
    Check(sceKernelFstat(fd, &after) == 0 && after.st_ino == before.st_ino);
    const auto path = File::DirectoryDescriptorPath(fd);
    Check(path.has_value());
    std::error_code pathError;
    Check(std::filesystem::equivalent(*path, expectedPath, pathError) && !pathError);
    std::error_code replacementError;
    Check(!std::filesystem::equivalent(*path, original, replacementError) && !replacementError);
    std::array<char, 512> entries{};
    const int bytes = sceKernelGetdents(fd, entries.data(), static_cast<int>(entries.size()));
    Check(bytes > 0);
    const std::string listing(entries.data(), static_cast<std::size_t>(bytes));
    Check(listing.find("original-entry") != std::string::npos && listing.find("replacement-entry") == std::string::npos);
    Check(sceKernelFchmod(fd, 0700) == 0);
    Check(sceKernelFutimes(fd, nullptr) == 0);
    Check(sceKernelFsync(fd) == 0);
    KernelStatfs fileSystem{};
    Check(_fstatfs_nid_postfix(fd, &fileSystem) == 0);
    Check(sceKernelClose(fd) == 0);
    std::filesystem::remove_all(original);
    std::filesystem::remove_all(renamed);
}

constexpr std::size_t MapPage = 16384;
const auto MapFailed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));

void MappingFixture(const std::filesystem::path& root) {
    std::map<std::string, std::vector<char>> files;
    std::filesystem::create_directory(root / "app0" / "Reference");
    for (const std::size_t size : {std::size_t{0}, std::size_t{3}, MapPage, MapPage + 77, BigSize}) {
        const auto name = std::to_string(size) + ".bin";
        auto bytes = AppArchiveFixture::Pattern(size);
        std::ofstream plain(root / "app0" / "Reference" / name, std::ios::binary);
        plain.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        Check(static_cast<bool>(plain));
        files.emplace("Mapped/" + name, std::move(bytes));
    }
    files.emplace("Override.bin", std::vector<char>{'z'});
    AppArchiveFixture::WriteArchive(root / "app0.zar", files);
    std::ofstream overrideFile(root / "app0" / "Override.bin", std::ios::binary);
    overrideFile << "loose";
}

struct MappingResult {
    void* address;
    int error;
};

MappingResult TryMapping(int fd, std::size_t length, int protection, int flags, std::int64_t offset, void* hint = nullptr) {
    *__error_nid_postfix() = 0;
    void* address = mmap_nid_postfix(hint, length, protection, flags, fd, offset);
    return {address, address == MapFailed ? *__error_nid_postfix() : 0};
}

void MappingBackingWindows() {
    const auto path = "/app0/Mapped/" + std::to_string(BigSize) + ".bin";
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    const int fd = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0 && sceKernelLseek(fd, 31, 0) == 31);
    for (const auto offset : {std::size_t{0}, MapPage, BigSize & ~(MapPage - 1), (BigSize + MapPage - 1) & ~(MapPage - 1)}) {
        for (const auto length : {MapPage, MapPage * 3}) {
            int backing = -1;
            Check(AppArchiveMappingDescriptor_nid_no_patch(fd, offset, length, &backing) && backing >= 0);
            const auto available = offset < BigSize ? std::min(length, BigSize - offset) : 0;
#ifdef _WIN32
            LARGE_INTEGER size{};
            Check(GetFileSizeEx(reinterpret_cast<HANDLE>(_get_osfhandle(backing)), &size) && size.QuadPart == static_cast<std::int64_t>(available));
#else
            struct stat size{};
            Check(fstat(backing, &size) == 0 && size.st_size == static_cast<std::int64_t>(available));
#endif
            std::vector<char> bytes(available);
            Check(sceKernelPread(backing, bytes.data(), bytes.size(), 0) == static_cast<std::int64_t>(available));
            if (available != 0) Check(std::memcmp(bytes.data(), expected.data() + offset, available) == 0);
#ifdef _WIN32
            Check(_close(backing) == 0);
#else
            Check(close(backing) == 0);
#endif
        }
    }
    Check(sceKernelLseek(fd, 0, 1) == 31 && sceKernelClose(fd) == 0);
}

void MappingRanges() {
    for (const std::size_t size : {std::size_t{0}, std::size_t{3}, MapPage, MapPage + 77, BigSize}) {
        const auto name = std::to_string(size) + ".bin";
        const int archive = sceKernelOpen(("/app0/Mapped/" + name).c_str(), SCE_KERNEL_O_RDONLY, 0);
        const int plain = sceKernelOpen(("/app0/Reference/" + name).c_str(), SCE_KERNEL_O_RDONLY, 0);
        Check(archive >= 0 && plain >= 0);
        for (const auto offset : {std::int64_t{-1}, std::int64_t{1}, std::int64_t{4096}, std::int64_t{0},
             static_cast<std::int64_t>(MapPage), static_cast<std::int64_t>((size + MapPage - 1) & ~(MapPage - 1)),
             std::numeric_limits<std::int64_t>::max() & ~static_cast<std::int64_t>(MapPage - 1)}) {
            for (const std::size_t length : {std::size_t{0}, std::size_t{1}, MapPage, MapPage + 1, std::numeric_limits<std::size_t>::max()}) {
                for (const int kind : {1, 2}) {
                    const auto left = TryMapping(archive, length, 1, kind, offset);
                    const auto right = TryMapping(plain, length, 1, kind, offset);
                    Check((left.address == MapFailed) == (right.address == MapFailed) && left.error == right.error);
                    if (left.address == MapFailed) continue;
                    Check((reinterpret_cast<std::uintptr_t>(left.address) & (MapPage - 1)) == 0);
                    const auto rounded = (length + MapPage - 1) & ~(MapPage - 1);
                    if (offset >= 0 && static_cast<std::uint64_t>(offset) < size) {
                        const auto available = size - static_cast<std::size_t>(offset);
                        const auto accessible = std::min(rounded, (available + 4095) & ~std::size_t{4095});
                        Check(std::memcmp(left.address, right.address, accessible) == 0);
                    }
                    Check(munmap_nid_postfix(left.address, length) == 0);
                    Check(munmap_nid_postfix(right.address, length) == 0);
                }
            }
        }
        Check(sceKernelClose(archive) == 0 && sceKernelClose(plain) == 0);
    }
}

void MappingLifecycle(const char* path) {
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    const int fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0 && sceKernelLseek(fd, 19, 0) == 19);
    Check(TryMapping(fd, MapPage, 3, 1, 0).error == 13);
    const auto shared = TryMapping(fd, MapPage * 3, 1, 1, 0);
    Check(shared.address != MapFailed && std::memcmp(shared.address, expected.data(), MapPage * 3) == 0);
    Check(mprotect_nid_postfix(shared.address, MapPage, 3) == -1 && *__error_nid_postfix() == 13);
    Check(mprotect_nid_postfix(shared.address, MapPage, 0) == 0);
    Check(mprotect_nid_postfix(shared.address, MapPage, 1) == 0);
    Check(sceKernelPread(fd, shared.address, 1, 0) == static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT));
    Check(pread_nid_postfix(fd, shared.address, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Check(sceKernelLseek(fd, 0, 1) == 19);
    const int duplicate = dup_nid_postfix(fd);
    const int above = fcntl_nid_postfix(fd, DuplicateFrom, fd + 8);
    Check(duplicate >= 0 && above >= fd + 8);
    const auto privateCopy = TryMapping(duplicate, MapPage * 3, 1, 2, MapPage);
    Check(privateCopy.address != MapFailed);
    Check(mprotect_nid_postfix(privateCopy.address, MapPage * 3, 3) == 0);
    auto* bytes = static_cast<char*>(privateCopy.address);
    bytes[7] ^= 0x5a;
    Check(static_cast<char*>(shared.address)[MapPage + 7] == expected[MapPage + 7]);
    char source = 0;
    Check(sceKernelPread(fd, &source, 1, MapPage + 7) == 1 && source == expected[MapPage + 7]);
    Check(sceKernelClose(fd) == 0 && sceKernelClose(duplicate) == 0);
    const auto survivor = TryMapping(above, MapPage, 3, 2, MapPage);
    Check(survivor.address != MapFailed && static_cast<char*>(survivor.address)[7] == expected[MapPage + 7]);
    Check(sceKernelClose(above) == 0);
    Check(std::memcmp(shared.address, expected.data(), MapPage * 3) == 0);
    Check(bytes[7] == static_cast<char>(expected[MapPage + 7] ^ 0x5a));
    Check(munmap_nid_postfix(bytes + MapPage, MapPage) == 0);
    Check(munmap_nid_postfix(static_cast<char*>(shared.address) + MapPage, MapPage) == 0);
    VirtualQueryInfo info{};
    Check(sceKernelVirtualQuery(bytes + MapPage, 0, &info, sizeof(info)) == SCE_KERNEL_ERROR_EACCES);
    Check(bytes[MapPage * 2] == expected[MapPage * 3]);
    Check(mprotect_nid_postfix(bytes + MapPage * 2, MapPage, 1) == 0);
    Check(munmap_nid_postfix(bytes, MapPage) == 0 && munmap_nid_postfix(bytes + MapPage * 2, MapPage) == 0);
    Check(munmap_nid_postfix(shared.address, MapPage) == 0);
    Check(munmap_nid_postfix(static_cast<char*>(shared.address) + MapPage * 2, MapPage) == 0);
    Check(munmap_nid_postfix(survivor.address, MapPage) == 0);
    const int reopened = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    const int replacement = sceKernelOpen("/app0/Override.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(reopened >= 0 && replacement >= 0);
    void* kernelMapping = nullptr;
    Check(sceKernelMmap(nullptr, MapPage, 1, 1, reopened, MapPage, &kernelMapping) == 0);
    Check(dup2_nid_postfix(replacement, reopened) == reopened);
    Check(std::memcmp(kernelMapping, expected.data() + MapPage, MapPage) == 0);
    Check(sceKernelClose(reopened) == 0 && sceKernelClose(replacement) == 0);
    Check(munmap_nid_postfix(kernelMapping, MapPage) == 0);
}

bool CollectMapping(void* mapping) {
#ifdef _WIN32
    std::array<void*, 16> pages{};
    std::size_t count = pages.size();
    Check(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), MapPage, pages.data(), &count, true));
    return count != 0;
#else
    bool written = false;
    Check(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), MapPage,
        [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &written));
    return written;
#endif
}

std::int64_t MappingAioRead(int fd, void* buffer, std::size_t length, std::int64_t offset) {
    KernelAioResult result{-1, 0};
    KernelAioRwRequest request{offset, length, buffer, &result, fd};
    std::int32_t id = 0;
    Check(sceKernelAioSubmitReadCommands(&request, 1, 1, &id) == 0 && result.state == 3);
    std::int32_t removed = -1;
    Check(sceKernelAioDeleteRequest(id, &removed) == 0 && removed == 0);
    return result.return_value;
}

void MappingReadDestinations() {
    const auto path = "/app0/Mapped/" + std::to_string(BigSize) + ".bin";
    const int fd = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0 && sceKernelLseek(fd, 19, 0) == 19);
    for (void* destination : {reinterpret_cast<void*>(std::uintptr_t{1}), MapFailed}) {
        const KernelIovec vector{destination, 1};
        Check(sceKernelRead(fd, destination, 1) == SCE_KERNEL_ERROR_EFAULT);
        Check(read_nid_postfix(fd, destination, 1) == -1 && *__error_nid_postfix() == 14);
        Check(sceKernelPread(fd, destination, 1, 0) == SCE_KERNEL_ERROR_EFAULT);
        Check(pread_nid_postfix(fd, destination, 1, 0) == -1 && *__error_nid_postfix() == 14);
        Check(sceKernelReadv(fd, &vector, 1) == SCE_KERNEL_ERROR_EFAULT);
        Check(sceKernelPreadv(fd, &vector, 1, 0) == SCE_KERNEL_ERROR_EFAULT);
        Check(readv_nid_postfix(fd, &vector, 1) == -1 && *__error_nid_postfix() == 14);
        Check(preadv_nid_postfix(fd, &vector, 1, 0) == -1 && *__error_nid_postfix() == 14);
        Check(MappingAioRead(fd, destination, 1, 0) == SCE_KERNEL_ERROR_EFAULT);
        Check(sceKernelLseek(fd, 0, 1) == 19);
    }
    std::array<char, 77> bytes{};
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    const KernelIovec vectors[] = {{bytes.data(), bytes.size()}, {reinterpret_cast<void*>(std::uintptr_t{1}), 1}};
    for (const bool positioned : {false, true}) {
        Check(sceKernelLseek(fd, 19, 0) == 19);
        const auto result = positioned ? sceKernelPreadv(fd, vectors, 2, 0) : sceKernelReadv(fd, vectors, 2);
        Check(result == static_cast<std::int64_t>(bytes.size()));
        Check(std::memcmp(bytes.data(), expected.data() + (positioned ? 0 : 19), bytes.size()) == 0);
        Check(sceKernelLseek(fd, 0, 1) == (positioned ? 19 : 19 + static_cast<std::int64_t>(bytes.size())));
    }
    Check(sceKernelClose(fd) == 0);
}

void MappingHostWrites(const char* path) {
    const int fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    const auto mapping = TryMapping(fd, MapPage, 3, 2, 0);
    Check(mapping.address != MapFailed);
#ifdef _WIN32
    bool watch = GuestArena::GuestArenaWriteWatched_nid_postfix();
    Check(watch);
#else
    bool watch = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
#endif
    if (watch) {
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(mapping.address, MapPage);
#ifndef _WIN32
        watch = GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping.address), MapPage);
#endif
        if (watch) CollectMapping(mapping.address);
    }
    Check(sceKernelPread(fd, mapping.address, MapPage, MapPage) == static_cast<std::int64_t>(MapPage));
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    Check(std::memcmp(mapping.address, expected.data() + MapPage, MapPage) == 0);
    if (watch) Check(CollectMapping(mapping.address));
    const KernelIovec vectors[] = {{mapping.address, MapPage / 2}, {static_cast<char*>(mapping.address) + MapPage / 2, MapPage / 2}};
    Check(sceKernelPreadv(fd, vectors, 2, 0) == static_cast<std::int64_t>(MapPage));
    Check(std::memcmp(mapping.address, expected.data(), MapPage) == 0);
    if (watch) Check(CollectMapping(mapping.address));
    Check(MappingAioRead(fd, mapping.address, MapPage, MapPage * 2) == static_cast<std::int64_t>(MapPage));
    Check(std::memcmp(mapping.address, expected.data() + MapPage * 2, MapPage) == 0);
    if (watch) Check(CollectMapping(mapping.address));
    {
        GuestArena::HostWrite write(mapping.address, MapPage);
        Check(write.Open());
        Check(sceKernelPread(fd, mapping.address, MapPage, MapPage * 2) == static_cast<std::int64_t>(MapPage));
    }
    Check(std::memcmp(mapping.address, expected.data() + MapPage * 2, MapPage) == 0);
    if (watch) {
        Check(CollectMapping(mapping.address));
#ifndef _WIN32
        Check(GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapping.address, MapPage));
#endif
    }
    Check(munmap_nid_postfix(mapping.address, MapPage) == 0 && sceKernelClose(fd) == 0);
}

#if defined(__linux__) || defined(_WIN32)
std::int64_t MappingPartialRead(const char* path) {
    const int fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    const auto mapping = TryMapping(fd, MapPage * 2, 3, 2, 0);
    Check(mapping.address != MapFailed);
    auto* bytes = static_cast<char*>(mapping.address);
    Check(mprotect_nid_postfix(bytes + MapPage, MapPage, 1) == 0);
    const auto read = sceKernelRead(fd, bytes + MapPage - 77, 154);
    if (read > 0) {
        Check(read == 77 && std::memcmp(bytes + MapPage - 77, AppArchiveFixture::Pattern(77).data(), 77) == 0);
        Check(sceKernelLseek(fd, 0, 1) == read);
    } else {
#ifdef _WIN32
        Check(read == static_cast<std::int64_t>(SCE_KERNEL_ERROR_EINVAL));
#else
        Check(read == static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT));
#endif
        Check(sceKernelLseek(fd, 0, 1) == 0);
    }
    Check(sceKernelClose(fd) == 0 && munmap_nid_postfix(mapping.address, MapPage * 2) == 0);
    return read;
}
#endif

class MappingTemporaryDirectory {
public:
    explicit MappingTemporaryDirectory(const std::filesystem::path& directory) {
        for (const auto* name : {"TMP", "TEMP", "TMPDIR"}) {
            const auto* value = std::getenv(name);
            saved.emplace_back(name, value == nullptr ? std::nullopt : std::optional<std::string>(value));
        }
        Set(directory);
    }
    ~MappingTemporaryDirectory() {
        for (const auto& [name, value] : saved) SetVariable(name.c_str(), value ? value->c_str() : nullptr);
    }
    void Set(const std::filesystem::path& directory) {
        for (const auto& [name, value] : saved) SetVariable(name.c_str(), directory.string().c_str());
    }
private:
    static void SetVariable(const char* name, const char* value) {
#ifdef _WIN32
        Check(_putenv_s(name, value == nullptr ? "" : value) == 0);
#else
        Check(value == nullptr ? unsetenv(name) == 0 : setenv(name, value, 1) == 0);
#endif
    }
    std::vector<std::pair<std::string, std::optional<std::string>>> saved;
};

std::size_t MappingHandleCount() {
#ifdef _WIN32
    DWORD count = 0;
    Check(GetProcessHandleCount(GetCurrentProcess(), &count));
    return count;
#elif defined(__linux__)
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator("/proc/self/fd"), std::filesystem::directory_iterator{}));
#else
    return 0;
#endif
}

#if defined(__linux__)
class MappingDescriptorLimit {
public:
    MappingDescriptorLimit() {
        Check(getrlimit(RLIMIT_NOFILE, &saved) == 0);
        Check(saved.rlim_cur >= 1 && fcntl(STDIN_FILENO, F_GETFD) >= 0);
        auto limit = saved;
        limit.rlim_cur = 1;
        Check(setrlimit(RLIMIT_NOFILE, &limit) == 0);
    }
    ~MappingDescriptorLimit() {
        Check(setrlimit(RLIMIT_NOFILE, &saved) == 0);
    }
private:
    rlimit saved{};
};

void MappingStorageLimit() {
    const auto path = "/app0/Mapped/" + std::to_string(BigSize) + ".bin";
    const int fd = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0 && sceKernelLseek(fd, 39, 0) == 39);
    rlimit saved{};
    Check(getrlimit(RLIMIT_FSIZE, &saved) == 0);
    auto limited = saved;
    limited.rlim_cur = MapPage;
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    Check(previousSignal != SIG_ERR && setrlimit(RLIMIT_FSIZE, &limited) == 0);
    const auto mapping = TryMapping(fd, 1, 3, 2, MapPage * 4);
    Check(mapping.address != MapFailed);
    const auto initialFailure = TryMapping(fd, MapPage * 2, 1, 2, MapPage);
    Check(initialFailure.address == MapFailed && initialFailure.error == 27);
    const auto before = MappingHandleCount();
    for (int iteration = 0; iteration < 3; ++iteration) {
        const auto failure = TryMapping(fd, MapPage * 2, 1, 2, MapPage);
        Check(failure.address == MapFailed && failure.error == 27);
    }
    Check(MappingHandleCount() == before);
    Check(setrlimit(RLIMIT_FSIZE, &saved) == 0 && std::signal(SIGXFSZ, previousSignal) != SIG_ERR);
    Check(sceKernelLseek(fd, 0, 1) == 39 && sceKernelClose(fd) == 0);
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    Check(std::memcmp(mapping.address, expected.data() + MapPage * 4, MapPage) == 0);
    Check(munmap_nid_postfix(mapping.address, MapPage) == 0);
    const int retry = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(retry >= 0);
    const auto restored = TryMapping(retry, MapPage * 2, 1, 2, MapPage);
    Check(restored.address != MapFailed && sceKernelClose(retry) == 0);
    Check(munmap_nid_postfix(restored.address, MapPage * 2) == 0);
}

void MappingStorageIndependence(const std::filesystem::path& root) {
    const auto verify = [] {
        const auto name = std::to_string(BigSize) + ".bin";
        for (const int kind : {1, 2}) {
            for (const bool upgrade : {false, true}) {
                const int archive = sceKernelOpen(("/app0/Mapped/" + name).c_str(), SCE_KERNEL_O_RDONLY, 0);
                const int plain = sceKernelOpen(("/app0/Reference/" + name).c_str(), SCE_KERNEL_O_RDONLY, 0);
                Check(archive >= 0 && plain >= 0);
                const auto reference = TryMapping(plain, MapPage, upgrade ? 1 : 5, kind, 0);
                Check(reference.address != MapFailed);
                const auto mapping = TryMapping(archive, MapPage, upgrade ? 1 : 5, kind, 0);
                Check(mapping.address != MapFailed);
                if (upgrade) {
                    Check(mprotect_nid_postfix(reference.address, MapPage, 5) == 0);
                    Check(mprotect_nid_postfix(mapping.address, MapPage, 5) == 0);
                }
                Check(std::memcmp(mapping.address, reference.address, MapPage) == 0);
                Check(sceKernelClose(archive) == 0 && sceKernelClose(plain) == 0);
                Check(munmap_nid_postfix(mapping.address, MapPage) == 0 && munmap_nid_postfix(reference.address, MapPage) == 0);
            }
        }
    };
    {
        MappingTemporaryDirectory temporary(root / "missing-backing-directory");
        verify();
    }
    for (const auto* candidate : {"/dev/shm", "/run/lock"}) {
        struct statvfs status{};
        if (statvfs(candidate, &status) != 0 || (status.f_flag & ST_NOEXEC) == 0) continue;
        const auto directory = std::filesystem::path(candidate) /
            ("anyps5-archive-map-storage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::error_code error;
        if (!std::filesystem::create_directory(directory, error)) continue;
        {
            MappingTemporaryDirectory temporary(directory);
            verify();
        }
        Check(std::filesystem::is_empty(directory) && std::filesystem::remove(directory));
        std::puts("archive executable mappings passed with noexec temporary storage");
        return;
    }
    std::puts("no writable noexec temporary filesystem available; missing-directory backing independence passed");
}
#endif

void MappingFailures(const std::filesystem::path& root, bool damaged) {
    MappingFixture(root);
    if (damaged) {
        std::fstream archive(root / "app0.zar", std::ios::binary | std::ios::in | std::ios::out);
        const std::array<char, 8> garbage{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0};
        archive.write(garbage.data(), garbage.size());
    }
    const auto directory = root / "backings";
    std::filesystem::create_directory(directory);
    MappingTemporaryDirectory temporary(damaged ? directory : root / "missing");
    const int fd = sceKernelOpen(("/app0/Mapped/" + std::to_string(BigSize) + ".bin").c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0 && sceKernelLseek(fd, 27, 0) == 27);
#if defined(__linux__)
    const int reference = damaged ? -1 : sceKernelOpen(("/app0/Reference/" + std::to_string(BigSize) + ".bin").c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(damaged || reference >= 0);
#endif
    const auto failedMapping = [&] {
#if defined(__linux__)
        if (!damaged) {
            MappingDescriptorLimit limit;
            const auto actual = TryMapping(fd, MapPage, 1, 2, 0);
            const auto expected = TryMapping(reference, MapPage, 1, 2, 0);
            Check(actual.address == expected.address && actual.error == expected.error);
            return actual;
        }
#endif
        return TryMapping(fd, MapPage, 1, 2, 0);
    };
    int expectedError = 5;
#if defined(__linux__)
    if (!damaged) expectedError = 24;
#endif
    const auto initialFailure = failedMapping();
    Check(initialFailure.address == MapFailed && initialFailure.error == expectedError);
    Check(std::filesystem::is_empty(directory));
    const auto before = MappingHandleCount();
    for (int iteration = 0; iteration < 8; ++iteration) {
        const auto result = failedMapping();
        Check(result.address == MapFailed && result.error == expectedError);
        Check(std::filesystem::is_empty(directory));
    }
    Check(MappingHandleCount() == before);
    Check(sceKernelLseek(fd, 0, 1) == 27);
    if (!damaged) {
        temporary.Set(directory);
        const auto mapping = TryMapping(fd, MapPage, 1, 1, 0);
        Check(mapping.address != MapFailed);
        Check(sceKernelClose(fd) == 0);
        Check(static_cast<char*>(mapping.address)[7] == AppArchiveFixture::Pattern(BigSize)[7]);
        Check(munmap_nid_postfix(mapping.address, MapPage) == 0);
    } else {
        Check(sceKernelClose(fd) == 0);
    }
    Check(std::filesystem::is_empty(directory));
#if defined(__linux__)
    if (reference >= 0) Check(sceKernelClose(reference) == 0);
#endif
}

void MappingDescriptorReplacement() {
    const auto path = "/app0/Mapped/" + std::to_string(BigSize) + ".bin";
    const int archive = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    const int foreign = sceKernelOpen("/app0/Override.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(archive >= 0 && foreign >= 0);
    const int first = dup_nid_postfix(foreign);
    const int second = dup_nid_postfix(foreign);
    Check(first >= 0 && second >= 0);
    Check(sceKernelClose(first) == 0 && sceKernelClose(second) == 0);
    const auto initial = TryMapping(archive, MapPage, 1, 2, 0);
    Check(initial.address != MapFailed && static_cast<char*>(initial.address)[0] == 0);
    Check(dup2_nid_postfix(foreign, first) == first && dup2_nid_postfix(foreign, second) == second);
    const auto later = TryMapping(archive, MapPage, 1, 2, 0);
    Check(later.address != MapFailed && static_cast<char*>(later.address)[0] == 0);
    const auto expected = AppArchiveFixture::Pattern(MapPage);
    Check(std::memcmp(initial.address, expected.data(), MapPage) == 0 && std::memcmp(later.address, expected.data(), MapPage) == 0);
    Check(sceKernelClose(archive) == 0);
    for (const int descriptor : {first, second}) {
        char buffer[5]{};
        Check(sceKernelPread(descriptor, buffer, sizeof(buffer), 0) == 5 && std::memcmp(buffer, "loose", 5) == 0);
        Check(sceKernelClose(descriptor) == 0);
    }
    Check(sceKernelClose(foreign) == 0);
    Check(munmap_nid_postfix(initial.address, MapPage) == 0 && munmap_nid_postfix(later.address, MapPage) == 0);
}

void MappingPlacement(const char* path) {
    const int fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    const auto aligned = TryMapping(fd, MapPage, 1, 2 | 0x1000000, MapPage);
    Check(aligned.address != MapFailed && (reinterpret_cast<std::uintptr_t>(aligned.address) & ((1 << 21) - 1)) == 0);
    Check(munmap_nid_postfix(aligned.address, MapPage) == 0);
    void* placeholder = mmap_nid_postfix(nullptr, MapPage * 3, 3, 0x1002, -1, 0);
    Check(placeholder != MapFailed);
    auto* middle = static_cast<char*>(placeholder) + MapPage;
    const auto fixed = TryMapping(fd, MapPage, 1, 0x11, MapPage, middle);
    Check(fixed.address == middle);
    Check(std::memcmp(middle, AppArchiveFixture::Pattern(BigSize).data() + MapPage, MapPage) == 0);
    Check(munmap_nid_postfix(placeholder, MapPage * 3) == 0);
    Check(sceKernelClose(fd) == 0);
}

void MappingProtections() {
    const auto name = std::to_string(BigSize) + ".bin";
    const int archive = sceKernelOpen(("/app0/Mapped/" + name).c_str(), SCE_KERNEL_O_RDONLY, 0);
    const int plain = sceKernelOpen(("/app0/Reference/" + name).c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(archive >= 0 && plain >= 0);
    for (const int protection : {0, 1, 2, 3, 4, 5, 6, 7, 8}) {
        for (const int kind : {1, 2}) {
            const auto left = TryMapping(archive, MapPage, protection, kind, 0);
            const auto right = TryMapping(plain, MapPage, protection, kind, 0);
            Check((left.address == MapFailed) == (right.address == MapFailed) && left.error == right.error);
            if (left.address == MapFailed) continue;
            if (kind == 2) {
                Check(mprotect_nid_postfix(left.address, MapPage, 3) == 0);
                Check(mprotect_nid_postfix(right.address, MapPage, 3) == 0);
                Check(std::memcmp(left.address, right.address, MapPage) == 0);
            }
            Check(munmap_nid_postfix(left.address, MapPage) == 0 && munmap_nid_postfix(right.address, MapPage) == 0);
        }
    }
    Check(sceKernelClose(archive) == 0 && sceKernelClose(plain) == 0);
}

void MappingTailProtections(const char* directory) {
    const auto path = "/app0/" + std::string(directory) + "/" + std::to_string(MapPage + 77) + ".bin";
    const int fd = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    const auto mapping = TryMapping(fd, MapPage * 2, 1, 1, 0);
    Check(mapping.address != MapFailed);
    auto* tail = static_cast<char*>(mapping.address) + MapPage;
    Check(mprotect_nid_postfix(tail, MapPage, 3) == -1 && *__error_nid_postfix() == 13);
    Check(mprotect_nid_postfix(tail, MapPage, 0) == 0 && mprotect_nid_postfix(tail, MapPage, 1) == 0);
    Check(tail[76] == AppArchiveFixture::Pattern(MapPage + 77).back() && tail[77] == 0);
    Check(sceKernelClose(fd) == 0 && munmap_nid_postfix(mapping.address, MapPage * 2) == 0);
}

void MappingEndOfFile(const char* directory) {
    const auto path = "/app0/" + std::string(directory) + "/3.bin";
    const int fd = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    for (const int kind : {1, 2}) {
        const auto mapping = TryMapping(fd, MapPage, 1, kind, 0);
        Check(mapping.address != MapFailed);
        const auto* bytes = static_cast<const volatile char*>(mapping.address);
        for (std::size_t offset = 3; offset < 4096; ++offset) Check(bytes[offset] == 0);
#ifdef _WIN32
        for (std::size_t offset = 4096; offset < MapPage; ++offset) Check(bytes[offset] == 0);
#elif defined(__linux__)
        const auto child = fork();
        Check(child >= 0);
        if (child == 0) {
            const rlimit coreLimit{0, 0};
            if (setrlimit(RLIMIT_CORE, &coreLimit) != 0) _exit(1);
            std::signal(SIGBUS, SIG_DFL);
            _exit(bytes[4096]);
        }
        int status = 0;
        Check(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGBUS);
#endif
        Check(munmap_nid_postfix(mapping.address, MapPage) == 0);
    }
    Check(sceKernelClose(fd) == 0);
}

void MappingConcurrent() {
    const int fd = sceKernelOpen(("/app0/Mapped/" + std::to_string(BigSize) + ".bin").c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    std::barrier start(4);
    std::vector<std::thread> threads;
    for (int index = 0; index < 4; ++index) threads.emplace_back([&, index] {
        start.arrive_and_wait();
        for (int iteration = 0; iteration < 8; ++iteration) {
            const auto mapping = TryMapping(fd, MapPage, index % 2 == 0 ? 3 : 1, index % 2 == 0 ? 2 : 1, MapPage * index);
            Check(mapping.address != MapFailed);
            const auto expected = AppArchiveFixture::Pattern(BigSize);
            Check(std::memcmp(mapping.address, expected.data() + MapPage * index, MapPage) == 0);
            if (index % 2 == 0) static_cast<char*>(mapping.address)[0] ^= 0x5a;
            Check(munmap_nid_postfix(mapping.address, MapPage) == 0);
        }
    });
    for (auto& thread : threads) thread.join();
    Check(sceKernelLseek(fd, 0, 1) == 0 && sceKernelClose(fd) == 0);
}

void Mappings(const std::filesystem::path& root) {
    MappingFixture(root);
    MappingBackingWindows();
    MappingReadDestinations();
#if defined(__linux__)
    MappingStorageLimit();
    MappingStorageIndependence(root);
#endif
    MappingDescriptorReplacement();
    MappingConcurrent();
    MappingRanges();
    MappingProtections();
#if defined(__linux__) || defined(_WIN32)
    const auto archivePartial = MappingPartialRead(("/app0/Mapped/" + std::to_string(BigSize) + ".bin").c_str());
    const auto plainPartial = MappingPartialRead(("/app0/Reference/" + std::to_string(BigSize) + ".bin").c_str());
#ifdef _WIN32
    Check(archivePartial == 77 && plainPartial == static_cast<std::int64_t>(SCE_KERNEL_ERROR_EINVAL));
#else
    Check(archivePartial == plainPartial);
#endif
#endif
    for (const auto* directory : {"Mapped", "Reference"}) {
        const auto path = "/app0/" + std::string(directory) + "/" + std::to_string(BigSize) + ".bin";
        MappingLifecycle(path.c_str());
        MappingHostWrites(path.c_str());
        MappingPlacement(path.c_str());
        MappingTailProtections(directory);
        MappingEndOfFile(directory);
    }
    const int fd = sceKernelOpen("/app0/Override.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    const auto mapping = TryMapping(fd, 5, 1, 2, 0);
    Check(mapping.address != MapFailed && std::memcmp(mapping.address, "loose", 5) == 0);
    Check(munmap_nid_postfix(mapping.address, 5) == 0 && sceKernelClose(fd) == 0);
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: guest_kernel_app_archive_tests archive|plain|mmap|mmap-failure|mmap-damaged\n");
        return 2;
    }
    const std::string mode = argv[1];
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-kernel-app-archive");
    if (mode == "archive") {
        Archive(root);
        FileNumbers();
        ConcurrentArchiveDirectoryEntries();
        Reads();
        Status();
        Refusals();
    } else if (mode == "mmap") {
        Mappings(root);
    } else if (mode == "mmap-failure" || mode == "mmap-damaged") {
        MappingFailures(root, mode == "mmap-damaged");
    } else if (mode == "plain") {
        WithoutArchive();
        NativeDirectoryIdentity(root);
    } else {
        return 2;
    }
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
