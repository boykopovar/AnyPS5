#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <sys/mount.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory entry check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int ErrorEinval = static_cast<int>(0x80020016u);
static constexpr int ErrorEfault = static_cast<int>(0x8002000Eu);
static constexpr int ErrorEbadf = static_cast<int>(0x80020009u);

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY));
        Check(memory != nullptr, __LINE__);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Check(pageSize > 0, __LINE__);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Check(memory != MAP_FAILED, __LINE__);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    char* Address() const { return memory; }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

struct Entry {
    std::uint8_t type;
    std::uint16_t recordLength;
};

static int Collect(const char* buffer, int bytes, std::map<std::string, Entry>& entries) {
    int records = 0;
    for (int offset = 0; offset < bytes; ++records) {
        std::uint16_t recordLength = 0;
        std::memcpy(&recordLength, buffer + offset + 4, sizeof(recordLength));
        const auto nameLength = static_cast<std::uint8_t>(buffer[offset + 7]);
        const std::string name(buffer + offset + 8);
        Require(name.size() == nameLength);
        Require(recordLength == ((8 + nameLength + 1 + 3) & ~3));
        Require(offset + recordLength <= bytes);
        Require(entries.emplace(name, Entry{static_cast<std::uint8_t>(buffer[offset + 6]), recordLength}).second);
        offset += recordLength;
    }
    return records;
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
static void CheckLargeDirectoryPaging() {
    const auto root = std::filesystem::path("anyps5-large-directory-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    constexpr int FileCount = 2500;
    for (int i = 0; i < FileCount; ++i) {
        std::ofstream file(root / ("large-directory-entry-" + std::to_string(i)));
        Require(static_cast<bool>(file));
    }
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::vector<char> buffer(128 * 1024);
    std::map<std::string, Entry> entries;
    int calls = 0;
    std::int64_t previousBase = -1;
    for (;;) {
        std::fill(buffer.begin(), buffer.end(), 'x');
        std::int64_t base = -1;
        const int read = sceKernelGetdirentries(directory, buffer.data(), static_cast<int>(buffer.size()), &base);
        Require(read >= 0 && read <= 64 * 1024);
        Require(std::all_of(buffer.begin() + read, buffer.end(), [](char byte) { return byte == 'x'; }));
        Require(base > previousBase);
        previousBase = base;
        if (read == 0) break;
        Require(Collect(buffer.data(), read, entries) > 0);
        ++calls;
    }
    Require(calls >= 2 && entries.size() == FileCount + 2);
    Require(entries.contains(".") && entries.contains(".."));
    for (int i = 0; i < FileCount; ++i) Require(entries.contains("large-directory-entry-" + std::to_string(i)));
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}
#endif

#ifdef __APPLE__
static std::filesystem::path MakeDirectory(const char* prefix, int count) {
    const auto root = std::filesystem::path(std::string(prefix) +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    for (int i = 0; i < count; ++i) {
        std::ofstream file(root / ("paged-entry-" + std::to_string(i)));
        Require(static_cast<bool>(file));
    }
    return root;
}

static std::vector<std::string> Names(const char* buffer, int bytes) {
    std::vector<std::string> names;
    for (int offset = 0; offset < bytes;) {
        std::uint16_t recordLength = 0;
        std::memcpy(&recordLength, buffer + offset + 4, sizeof(recordLength));
        Require(recordLength != 0 && offset + recordLength <= bytes);
        const std::string name(buffer + offset + 8);
        if (name.rfind("._", 0) != 0) names.push_back(name);
        offset += recordLength;
    }
    return names;
}

static void CheckPagingWithBufferSize(int bufferSize) {
    constexpr int FileCount = 1500;
    const auto root = MakeDirectory("anyps5-macos-paging-test-", FileCount);
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::vector<char> buffer(static_cast<std::size_t>(bufferSize));
    std::map<std::string, int> seen;
    for (;;) {
        std::int64_t base = -1;
        const int read = sceKernelGetdirentries(directory, buffer.data(), bufferSize, &base);
        Require(read >= 0 && read <= bufferSize);
        if (read == 0) break;
        for (const auto& name : Names(buffer.data(), read)) ++seen[name];
    }
    Require(seen.size() == FileCount + 2);
    Require(seen["."] == 1 && seen[".."] == 1);
    for (int i = 0; i < FileCount; ++i) Require(seen["paged-entry-" + std::to_string(i)] == 1);
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}

static void CheckRemovingEachBatch() {
    constexpr int FileCount = 1200;
    const auto root = MakeDirectory("anyps5-macos-remove-batch-test-", FileCount);
    struct statfs volume{};
    Require(statfs(root.c_str(), &volume) == 0);
    const std::string type(volume.f_fstypename);
    if (type != "apfs" && type != "msdos" && type != "exfat") {
        std::filesystem::remove_all(root);
        return;
    }
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 1024> buffer{};
    std::map<std::string, int> seen;
    int batches = 0;
    for (;;) {
        const int read = sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size()));
        Require(read >= 0);
        if (read == 0) break;
        ++batches;
        for (const auto& name : Names(buffer.data(), read)) {
            ++seen[name];
            if (name != "." && name != "..") Require(std::filesystem::remove(root / name));
        }
    }
    Require(batches > 2);
    Require(seen.size() == FileCount + 2);
    for (const auto& [name, count] : seen) Require(count == 1);
    Require(std::filesystem::is_empty(root));
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}

static void CheckSeekingToReturnedBase() {
    const auto root = MakeDirectory("anyps5-macos-seek-base-test-", 400);
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 512> first{};
    std::array<char, 512> again{};
    std::int64_t base = -1;
    Require(sceKernelGetdirentries(directory, first.data(), static_cast<int>(first.size()), &base) > 0 && base == 0);
    std::int64_t secondBase = -1;
    const int second = sceKernelGetdirentries(directory, first.data(), static_cast<int>(first.size()), &secondBase);
    Require(second > 0 && secondBase != 0);
    Require(sceKernelGetdirentries(directory, again.data(), static_cast<int>(again.size()), &base) > 0 && base != secondBase);
    Require(sceKernelLseek(directory, secondBase, 0) == secondBase);
    std::int64_t replayBase = -1;
    Require(sceKernelGetdirentries(directory, again.data(), static_cast<int>(again.size()), &replayBase) == second);
    Require(replayBase == secondBase && Names(first.data(), second) == Names(again.data(), second));
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}

static void CheckBufferBelowHostMinimum() {
    const auto root = MakeDirectory("anyps5-macos-small-buffer-test-", 3);
    int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 20> buffer{};
    Require(sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size())) == 12);
    Require(std::strcmp(buffer.data() + 8, ".") == 0);
    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size())) == 12);
    Require(std::strcmp(buffer.data() + 8, ".") == 0);
    const int duplicate = dup_nid_postfix(directory);
    Require(duplicate >= 0);
    Require(sceKernelGetdents(duplicate, buffer.data(), 12) == 12);
    Require(std::strcmp(buffer.data() + 8, "..") == 0);
    Require(sceKernelClose(duplicate) == 0);
    std::map<std::string, int> seen{{".", 1}, {"..", 1}};
    std::array<char, 64> rest{};
    int read = 0;
    while ((read = sceKernelGetdents(directory, rest.data(), static_cast<int>(rest.size()))) > 0) {
        for (const auto& name : Names(rest.data(), read)) ++seen[name];
    }
    Require(read == 0 && seen.size() == 5);
    for (const auto& [name, count] : seen) Require(count == 1);
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}
#endif

int main() {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    CheckLargeDirectoryPaging();
#endif
#ifdef __APPLE__
    for (const int size : {28, 48, 100, 512, 4096, 128 * 1024}) CheckPagingWithBufferSize(size);
    CheckRemovingEachBatch();
    CheckSeekingToReturnedBase();
    CheckBufferBelowHostMinimum();
#endif
    const auto root = std::filesystem::path("anyps5-directory-entries-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const std::string longName = "a-much-longer-entry-name-0123456789";
    { std::ofstream stream(root / "data.bin", std::ios::binary); stream << "0123456789"; }
    { std::ofstream stream(root / "b"); }
    { std::ofstream stream(root / longName); }
    Require(std::filesystem::create_directory(root / "sub"));

    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 48> small{};
    std::map<std::string, Entry> entries;
    int calls = 0;
    std::int64_t previousBase = -1;
    for (;;) {
        std::int64_t base = -1;
        const int read = sceKernelGetdirentries(directory, small.data(), static_cast<int>(small.size()), &base);
        Require(read >= 0 && read <= static_cast<int>(small.size()));
        Require(base >= 0 && base != previousBase);
        if (calls == 0) Require(base == 0);
        previousBase = base;
        if (read == 0) break;
        Require(Collect(small.data(), read, entries) > 0);
        ++calls;
    }
    Require(calls >= 3);
    Require(entries.size() == 6);
    for (const char* name : {".", "..", "data.bin", "b", "sub"}) Require(entries.contains(name));
    Require(entries.contains(longName) && entries[longName].recordLength == 44);
    Require(entries["b"].recordLength == 12);
    Require(entries["sub"].type == 4 || entries["sub"].type == 0);
    Require(entries["data.bin"].type == 8 || entries["data.bin"].type == 0);

    Require(sceKernelLseek(directory, 0, 0) == 0);
    std::array<char, 8> tooSmall{};
    Require(sceKernelGetdents(directory, tooSmall.data(), static_cast<int>(tooSmall.size())) == ErrorEinval);
    Require(sceKernelGetdents(directory, nullptr, 64) == ErrorEfault);
    Require(sceKernelGetdents(directory, small.data(), 0) == ErrorEinval);
    {
        ReadOnlyPage page;
        char* const unusable[] = {reinterpret_cast<char*>(0x10), page.Address()};
        std::int64_t base = -1;
        for (char* pointer : unusable) {
            Require(sceKernelGetdents(directory, pointer, 64) == ErrorEfault);
            Require(sceKernelGetdirentries(directory, pointer, 64, &base) == ErrorEfault && base == -1);
            Require(getdents_nid_postfix(directory, pointer, 64) == -1 && *__error_nid_postfix() == 14);
        }
        std::int64_t* const unusableBase[] = {reinterpret_cast<std::int64_t*>(0x10), reinterpret_cast<std::int64_t*>(page.Address())};
        for (std::int64_t* pointer : unusableBase) {
            Require(sceKernelGetdirentries(directory, small.data(), static_cast<int>(small.size()), pointer) == ErrorEfault);
            *__error_nid_postfix() = 0;
            Require(getdirentries_nid_postfix(directory, small.data(), static_cast<int>(small.size()), pointer) == -1 && *__error_nid_postfix() == 14);
        }
    }
    std::array<char, 4096> large{};
    std::map<std::string, Entry> all;
    const int readAll = sceKernelGetdents(directory, large.data(), static_cast<int>(large.size()));
    Require(readAll > 0 && Collect(large.data(), readAll, all) == 6);
    Require(sceKernelGetdents(directory, large.data(), static_cast<int>(large.size())) == 0);

    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(getdents_nid_postfix(directory, tooSmall.data(), static_cast<int>(tooSmall.size())) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(getdents_nid_postfix(directory, nullptr, 64) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(getdirentries_nid_postfix(directory, small.data(), 0, nullptr) == -1 && *__error_nid_postfix() == 22);
    std::int64_t posixBase = -1;
    *__error_nid_postfix() = 1234;
    const int firstPart = getdirentries_nid_postfix(directory, small.data(), static_cast<int>(small.size()), &posixBase);
    Require(firstPart > 0 && posixBase == 0 && *__error_nid_postfix() == 1234);
    const int secondPart = getdirentries_nid_postfix(directory, small.data(), static_cast<int>(small.size()), &posixBase);
    Require(secondPart > 0 && posixBase > 0 && *__error_nid_postfix() == 1234);
    Require(sceKernelLseek(directory, 0, 0) == 0);
    std::map<std::string, Entry> posix;
    const int posixRead = getdirentries_nid_postfix(directory, large.data(), static_cast<int>(large.size()), &posixBase);
    Require(posixRead == readAll && posixBase == 0 && Collect(large.data(), posixRead, posix) == 6);
    Require(getdents_nid_postfix(directory, large.data(), static_cast<int>(large.size())) == 0);
    Require(sceKernelLseek(directory, 0, 0) == 0);
    std::map<std::string, Entry> dents;
    const int dentsRead = getdents_nid_postfix(directory, large.data(), static_cast<int>(large.size()));
    Require(dentsRead == readAll && Collect(large.data(), dentsRead, dents) == 6);
    Require(sceKernelClose(directory) == 0);
    Require(getdents_nid_postfix(directory, large.data(), static_cast<int>(large.size())) == -1 && *__error_nid_postfix() == 9);
    *__error_nid_postfix() = 0;
    Require(getdirentries_nid_postfix(directory, large.data(), static_cast<int>(large.size()), nullptr) == -1 && *__error_nid_postfix() == 9);

    const int file = sceKernelOpen((root / "data.bin").string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(file >= 0);
    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    for (const int descriptor : {file, pipe[0], pipe[1], -1}) {
        const int expected = descriptor < 0 ? 9 : 22;
        *__error_nid_postfix() = 0;
        Require(getdents_nid_postfix(descriptor, nullptr, 64) == -1 && *__error_nid_postfix() == expected);
        *__error_nid_postfix() = 0;
        Require(getdirentries_nid_postfix(descriptor, nullptr, 64, nullptr) == -1 && *__error_nid_postfix() == expected);
        Require(sceKernelGetdents(descriptor, nullptr, 64) == static_cast<int>(0x80020000u | expected));
    }
    for (const int length : {0, -1}) {
        *__error_nid_postfix() = 0;
        Require(getdirentries_nid_postfix(-1, small.data(), length, nullptr) == -1 && *__error_nid_postfix() == 9);
        *__error_nid_postfix() = 0;
        Require(getdirentries_nid_postfix(file, small.data(), length, nullptr) == -1 && *__error_nid_postfix() == 22);
    }
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
    Require(sceKernelLseek(file, 2, 0) == 2);
    char bytes[16] = {};
    Require(sceKernelPread(file, bytes, 4, 5) == 4 && std::memcmp(bytes, "5678", 4) == 0);
    Require(sceKernelLseek(file, 0, 1) == 2);
    Require(sceKernelPwrite(file, "XY", 2, 8) == 2);
    Require(sceKernelLseek(file, 0, 1) == 2);
    Require(sceKernelRead(file, bytes, 3) == 3 && std::memcmp(bytes, "234", 3) == 0);
    Require(sceKernelPread(file, bytes, sizeof(bytes), 0) == 10 && std::memcmp(bytes, "01234567XY", 10) == 0);
    Require(sceKernelPread(file, bytes, sizeof(bytes), 10) == 0);
    Require(sceKernelPread(file, bytes, 1, -1) == ErrorEinval);
    Require(sceKernelPwrite(file, nullptr, 1, 0) == ErrorEfault);
    Require(sceKernelClose(file) == 0);
    Require(sceKernelPread(file, bytes, 1, 0) == ErrorEbadf);

    std::filesystem::remove_all(root);
}
