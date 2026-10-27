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

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <typeinfo>
#include <vector>

extern "C" {

using TerminateHandler = void (*)();

}

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

void OnMmapRefused() {
    const auto* type = __cxa_current_exception_type_nid_postfix();
    const auto* error = static_cast<const std::runtime_error*>(__cxa_current_primary_exception_nid_postfix());
    const bool refused = type != nullptr && error != nullptr && std::strcmp(type->name(), typeid(std::runtime_error).name()) == 0 &&
        std::strstr(error->what(), "mmap on a file read from app0.zar") != nullptr && std::strstr(error->what(), "not implemented") != nullptr;
    std::_Exit(refused ? 0 : 1);
}

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

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: guest_kernel_app_archive_tests archive|plain\n");
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
        Archive(root);
        const int fd = sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY, 0);
        Check(fd >= 0);
        _ZSt13set_terminatePFvvE_nid_postfix(OnMmapRefused);

        (void)mmap_nid_postfix(nullptr, 16384, 1, 2, fd, 0);
        return 1;
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
