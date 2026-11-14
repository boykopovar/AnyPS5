#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
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
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
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

int main() {
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
