#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "nid/Sha1.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace {
constexpr int ReadOnly = 0;
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;
constexpr int Exclusive = 0x800;
constexpr int Shared = 0x1;
constexpr int Private = 0x2;
constexpr std::size_t Page = 0x4000;
using Unlink = int (*)(const char*);

int CreateOrdinaryFile() {
    char path[] = "/tmp/anyps5-lifecycle-XXXXXX";
    const int descriptor = ::mkstemp(path);
    if (descriptor < 0) throw std::runtime_error("ordinary file creation failed");
    if (::unlink(path) != 0) {
        ::close(descriptor);
        throw std::runtime_error("ordinary file unlink failed");
    }
    constexpr char contents[] = "ordinary file contents";
    if (::write(descriptor, contents, sizeof(contents)) != static_cast<ssize_t>(sizeof(contents)) ||
        ::lseek(descriptor, 0, SEEK_SET) != 0) {
        ::close(descriptor);
        throw std::runtime_error("ordinary file initialization failed");
    }
    return descriptor;
}

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string(message) + " (guest errno=" + std::to_string(*__error_nid_postfix()) + ")");
}

struct Object {
    std::string name = "/anyps5-lifecycle-" + std::to_string(getpid());
    std::string hostName = "/anyps5-shm-";
    Object() {
        constexpr char hex[] = "0123456789abcdef";
        const auto digest = Nid::Sha1(std::vector<std::uint8_t>(name.begin(), name.end()));
        for (const auto byte : digest) {
            hostName += hex[byte >> 4];
            hostName += hex[byte & 15];
        }
        ::shm_unlink(hostName.c_str());
    }
    ~Object() { ::shm_unlink(hostName.c_str()); }
    int Open(int flags) const { return shm_open_nid_postfix(name.c_str(), flags, 0600); }
};

void Run() {
    Object object;
    const char* anonymousName = reinterpret_cast<const char*>(1);
    const int anonymous = shm_open_nid_postfix(anonymousName, 0x100e02, 0600);
    Require(anonymous >= 0 && ftruncate_nid_postfix(anonymous, Page) == 0, "anonymous shared-memory setup failed");
    auto* anonymousMap = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 3, Shared, anonymous, 0));
    Require(anonymousMap != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "anonymous MAP_SHARED failed");
    anonymousMap[0] = 'a';
    char anonymousByte = 0;
    Require(pread_nid_postfix(anonymous, &anonymousByte, 1, 0) == 1 && anonymousByte == 'a',
            "anonymous MAP_SHARED write was not visible through the descriptor");
    Require(munmap_nid_postfix(anonymousMap, Page) == 0 && close_nid_postfix(anonymous) == 0, "anonymous shared-memory cleanup failed");
    const int descriptor = object.Open(ReadWrite | Create | Exclusive);
    Require(descriptor >= 0, "named object creation failed");
    Require(ftruncate_nid_postfix(descriptor, Page) == 0, "guest resize failed");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "old", 4, 0) == 4, "guest positioned write failed");
    char bytes[8]{};
    Require(pread_nid_postfix(descriptor, bytes, sizeof(bytes), Page) == 0, "read at EOF was not empty");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 0, Page) == 0, "zero-length write failed");
    *__error_nid_postfix() = 0;
    Require(lseek_nid_postfix(descriptor, Page + 2, SEEK_SET) == -1 && *__error_nid_postfix() == 22,
            "seek past EOF did not fail with EINVAL");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_END) == static_cast<std::int64_t>(Page), "seek to EOF returned the wrong position");
    Require(write_nid_postfix(descriptor, "z", 1) == 0, "write at EOF grew the shared object");
    Require(lseek_nid_postfix(descriptor, Page - 1, SEEK_SET) == static_cast<std::int64_t>(Page - 1), "seek near EOF failed");
    Require(write_nid_postfix(descriptor, "xy", 2) == 1, "cross-EOF write did not return its short length");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_END) == static_cast<std::int64_t>(Page), "cross-EOF write changed object size");
    char vectorBytes[] = "uv";
    KernelIovec vectorWrite{vectorBytes, 2};
    Require(pwritev_nid_postfix(descriptor, nullptr, 0, Page) == 0, "zero-vector positioned write on a writable descriptor failed");
    Require(pwritev_nid_postfix(descriptor, &vectorWrite, 1, Page) == 0, "positioned vector write at EOF grew the object");
    Require(pwritev_nid_postfix(descriptor, &vectorWrite, 1, Page - 1) == 1, "positioned vector write past EOF was not short");
    Require(lseek_nid_postfix(descriptor, Page - 1, SEEK_SET) == static_cast<std::int64_t>(Page - 1), "seek before vector write failed");
    Require(writev_nid_postfix(descriptor, &vectorWrite, 1) == 1, "vector write past EOF was not short");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_END) == static_cast<std::int64_t>(Page), "vector write changed object size");
    *__error_nid_postfix() = 0;
    Require(lseek_nid_postfix(descriptor, -1, SEEK_SET) == -1 && *__error_nid_postfix() == 22,
            "negative seek did not return EINVAL");
    Require(lseek_nid_postfix(descriptor, std::numeric_limits<std::int64_t>::max(), SEEK_CUR) == -1 &&
            *__error_nid_postfix() == 84, "overflowing seek did not return EOVERFLOW");

    const int second = object.Open(ReadWrite);
    Require(second >= 0 && pread_nid_postfix(second, bytes, 4, 0) == 4 && std::memcmp(bytes, "old", 4) == 0,
            "independent open did not see the shared object");
    auto* firstMap = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 3, Shared, descriptor, 0));
    Require(firstMap != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "guest shared mapping failed");
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(firstMap), Page),
            "file-backed shared mapping was registered with write watch");
    auto* secondMap = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 3, Shared, second, 0));
    Require(secondMap != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "second guest shared mapping failed");
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(secondMap), Page),
            "file-backed shared alias was registered with write watch");
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(nullptr, Page, 1, Shared, second, 1) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)) &&
            *__error_nid_postfix() == 22, "unaligned shared mapping offset did not return EINVAL");
    firstMap[0] = 'n';
    Require(secondMap[0] == 'n', "shared mappings did not observe each other's writes");
    auto* privateMap = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 3, Private, second, 0));
    Require(privateMap != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "guest private mapping failed");
    firstMap[1] = 'v';
    Require(privateMap[1] == 'v', "private mapping did not reflect backing changes before its first write");
    privateMap[0] = 'p';
    Require(firstMap[0] == 'n', "private mapping write escaped copy-on-write isolation");
    Require(pread_nid_postfix(second, bytes, 1, 0) == 1 && bytes[0] == 'n', "private mapping write changed the backing object");
    const int readOnly = object.Open(ReadOnly);
    Require(readOnly >= 0, "read-only open failed");
    *__error_nid_postfix() = 0;
    Require(pwrite_nid_disambig1_nid_postfix(readOnly, "x", 1, 0) == -1 && *__error_nid_postfix() == 9,
            "positioned write on a read-only descriptor did not return EBADF");
    *__error_nid_postfix() = 0;
    Require(pwrite_nid_disambig1_nid_postfix(readOnly, "x", 1, Page) == -1 && *__error_nid_postfix() == 9,
            "positioned write at EOF on a read-only descriptor did not return EBADF");
    *__error_nid_postfix() = 0;
    Require(pwritev_nid_postfix(readOnly, nullptr, 0, 0) == -1 && *__error_nid_postfix() == 9,
            "zero-vector positioned write on a read-only descriptor did not return EBADF");
    FileStat readOnlyInfo{};
    Require(fstat_nid_disambig1_nid_postfix(readOnly, &readOnlyInfo) == 0 && readOnlyInfo.st_size == Page,
            "read-only EOF write changed the shared-object size");
    void* rejected = mmap_nid_postfix(nullptr, Page, 3, Shared, readOnly, 0);
    Require(rejected == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)), "writable shared mapping accepted a read-only descriptor");
    auto* writableMap = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 1, Shared, descriptor, 0));
    Require(writableMap != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "read-protected writable-fd mapping failed");
    Require(mprotect_nid_postfix(writableMap, Page, 3) == 0, "mprotect could not restore write access from a writable descriptor");
    writableMap[2] = 'm';
    Require(firstMap[2] == 'm', "mprotect-upgraded mapping did not remain shared");
    Require(munmap_nid_postfix(writableMap, Page) == 0, "upgraded mapping unmap failed");
    auto* readOnlyMap = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 1, Shared, readOnly, 0));
    Require(readOnlyMap != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "read-only shared mapping failed");
    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(readOnlyMap, Page, 3) == -1 && *__error_nid_postfix() == 13,
            "mprotect on a read-only shared mapping did not return EACCES");
    Require(munmap_nid_postfix(readOnlyMap, Page) == 0, "read-only mapping unmap failed");

    const int ordinary = CreateOrdinaryFile();
    Require(ordinary >= 0, "ordinary file setup failed");
    Require(dup2_nid_postfix(ordinary, second) == second, "dup2 onto shared descriptor failed");
    Require(lseek_nid_postfix(second, 0, SEEK_SET) == 0 && read_nid_postfix(second, bytes, sizeof(bytes)) > 0,
            "dup2 target did not become the ordinary file");
    Require(close_nid_postfix(second) == 0, "close after dup2 failed");
    Require(lseek_nid_postfix(ordinary, 0, SEEK_SET) == 0 && read_nid_postfix(ordinary, bytes, sizeof(bytes)) > 0,
            "closing reused descriptor damaged the ordinary file");
    Require(close_nid_postfix(ordinary) == 0, "ordinary file close failed");
    const int reused = object.Open(ReadWrite);
    Require(reused >= 0, "shared object reopen after descriptor close failed");
    Require(dup2_nid_postfix(reused, second) == second, "guest dup2 into the freed descriptor slot failed");
    Require(pread_nid_postfix(second, bytes, 3, 0) == 3 && std::memcmp(bytes, "nvm", 3) == 0,
            "reused descriptor slot retained ordinary-file state");
    Require(close_nid_postfix(second) == 0, "reused descriptor slot close failed");
    if (reused != second) Require(close_nid_postfix(reused) == 0, "reopened descriptor close failed");

    Require(close_nid_postfix(descriptor) == 0 && close_nid_postfix(readOnly) == 0, "descriptor close failed");
    Require(firstMap[0] == 'n', "mapping did not survive descriptor close");
    const auto unlinkObject = reinterpret_cast<Unlink>(dlsym(RTLD_DEFAULT, "shm_unlink_nid_postfix"));
    Require(unlinkObject != nullptr, "guest shm_unlink export is missing");
    Require(unlinkObject(object.name.c_str()) == 0, "guest unlink failed");
    *__error_nid_postfix() = 0;
    Require(unlinkObject(object.name.c_str()) == -1 && *__error_nid_postfix() == 2, "missing-name unlink did not return ENOENT");
    const int recreated = object.Open(ReadWrite | Create | Exclusive);
    Require(recreated >= 0, "name recreation after unlink failed");
    Require(ftruncate_nid_postfix(recreated, Page) == 0, "recreated object resize failed");
    Require(pread_nid_postfix(recreated, bytes, 4, 0) == 4 && std::memcmp(bytes, "\0\0\0\0", 4) == 0,
            "recreated name still refers to the unlinked object");
    Require(firstMap[0] == 'n', "unlink or recreation changed the old mapping");
    Require(close_nid_postfix(recreated) == 0, "recreated descriptor close failed");
    Require(munmap_nid_postfix(firstMap, Page) == 0 && munmap_nid_postfix(secondMap, Page) == 0 && munmap_nid_postfix(privateMap, Page) == 0,
            "guest unmap failed");
    std::puts("named shared-memory lifecycle checks passed");
}
}

int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
