#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

constexpr std::size_t Size = 0x10000;
constexpr int Read = 0x1;
constexpr int Write = 0x2;
constexpr int Execute = 0x4;
bool g_mappingObserved = false;

void ObserveMapping(std::uintptr_t, std::size_t) {
    g_mappingObserved = true;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "requirement failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

int Create(const char* name, int maxProt) {
    int fd = -1;
    Require(sceKernelJitCreateSharedMemory(name, Size, maxProt, &fd) == 0);
    Require(fd >= 0);
    return fd;
}

void* Map(int fd, int prot, void* hint = nullptr) {
    void* address = hint;
    Require(sceKernelJitMapSharedMemory(fd, prot, &address) == 0);
    Require(address != nullptr && (reinterpret_cast<std::uintptr_t>(address) & 0x3fff) == 0);
    return address;
}

int Call(const void* code) {
    using Function = int (*)();
    Function function;
    std::memcpy(&function, &code, sizeof(function));
    return function();
}

void WriteReturn(void* code, std::uint32_t value) {
    unsigned char bytes[] = {0xb8, 0, 0, 0, 0, 0xc3};
    std::memcpy(bytes + 1, &value, sizeof(value));
    std::memcpy(code, bytes, sizeof(bytes));
}

void WriteThroughAliasRunsThroughExecutableView() {
    const int fd = Create(nullptr, Read | Write | Execute);
    int alias = -1;
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read | Write, &alias) == 0);
    Require(alias >= 0 && alias != fd);
    auto* writable = static_cast<unsigned char*>(Map(alias, Read | Write));
    auto* executable = static_cast<unsigned char*>(Map(fd, Read | Execute));
    Require(writable != executable);
    Require(executable[0] == 0 && executable[Size - 1] == 0);
    WriteReturn(writable, 0x12345678);
    Require(std::memcmp(writable, executable, 6) == 0);
    Require(Call(executable) == 0x12345678);
    WriteReturn(writable + 0x4000, 0x0badcafe);
    Require(Call(executable + 0x4000) == 0x0badcafe);
    writable[Size - 1] = 0x5a;
    Require(executable[Size - 1] == 0x5a);
    Require(close_nid_postfix(fd) == 0);
    auto* again = static_cast<unsigned char*>(Map(alias, Read));
    Require(again[Size - 1] == 0x5a);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(again), Size) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(writable), Size) == 0);
    Require(Call(executable) == 0x12345678);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(executable), Size) == 0);
    Require(close_nid_postfix(alias) == 0);
}

void OneViewWritesAndRuns() {
    const int fd = Create("ps5-platform-probe", Read | Write | Execute);
    void* hint = reinterpret_cast<void*>(std::uintptr_t{0x1000000000});
    auto* code = static_cast<unsigned char*>(Map(fd, Read | Write | Execute, hint));
    Require(reinterpret_cast<std::uintptr_t>(code) >= 0x1000000000);
    WriteReturn(code, 42);
    Require(Call(code) == 42);
    const int duplicate = dup_nid_postfix(fd);
    Require(duplicate >= 0);
    Require(close_nid_postfix(fd) == 0);
    auto* view = static_cast<unsigned char*>(Map(duplicate, Read | Execute));
    Require(Call(view) == 42);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(view), Size) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(code), Size) == 0);
    Require(close_nid_postfix(duplicate) == 0);
}

void MmapOfTheDescriptorSharesTheObject() {
    const int fd = Create(nullptr, Read | Write);
    auto* jit = static_cast<volatile unsigned char*>(Map(fd, Read | Write));
    void* mapped = mmap_nid_postfix(nullptr, Size, Read | Write, 0x1, fd, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    auto* view = static_cast<volatile unsigned char*>(mapped);
    jit[0x200] = 0x5a;
    Require(view[0x200] == 0x5a);
    view[0x300] = 0x6b;
    Require(jit[0x300] == 0x6b);
    Require(munmap_nid_postfix(mapped, Size) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(const_cast<unsigned char*>(jit)), Size) == 0);
    Require(close_nid_postfix(fd) == 0);
}

void Dup2KeepsTheDescriptorTableInStep() {
    const int fd = Create(nullptr, Read | Write);
    const int target = shm_open_nid_postfix(reinterpret_cast<const char*>(1), 0x2, 0600);
    Require(target >= 0 && target != fd);
    Require(dup2_nid_postfix(fd, target) == target);
    auto* writable = static_cast<unsigned char*>(Map(fd, Read | Write));
    auto* readable = static_cast<unsigned char*>(Map(target, Read));
    writable[0x100] = 0x77;
    Require(readable[0x100] == 0x77);
    void* address = nullptr;
    Require(sceKernelJitMapSharedMemory(target, Read | Execute, &address) == SCE_KERNEL_ERROR_EACCES);
    Require(dup2_nid_postfix(fd, fd) == fd);
    auto* same = static_cast<unsigned char*>(Map(fd, Read));
    Require(same[0x100] == 0x77);
    const int plain = shm_open_nid_postfix(reinterpret_cast<const char*>(1), 0x2, 0600);
    Require(plain >= 0);
    Require(dup2_nid_postfix(plain, target) == target);
    address = nullptr;
    Require(sceKernelJitMapSharedMemory(target, Read, &address) == SCE_KERNEL_ERROR_EBADF);
    Require(address == nullptr);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(same), Size) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(readable), Size) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(writable), Size) == 0);
    Require(close_nid_postfix(plain) == 0);
    Require(close_nid_postfix(target) == 0);
    Require(close_nid_postfix(fd) == 0);
}

void InvalidArgumentsAreRejected() {
    int fd = -1;
    Require(sceKernelJitCreateSharedMemory(nullptr, 0, Read | Write | Execute, &fd) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateSharedMemory(nullptr, 0x1000, Read | Write | Execute, &fd) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateSharedMemory(nullptr, Size + 0x2000, Read | Write | Execute, &fd) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateSharedMemory(nullptr, Size, 0x10, &fd) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateSharedMemory(nullptr, Size, Read | Write | Execute, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateSharedMemory(nullptr, Size, Read | Write | Execute, reinterpret_cast<int*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    Require(fd == -1);

    fd = Create(nullptr, Read | Execute);
    int alias = -1;
    void* address = nullptr;
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read | Write, &alias) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, 0x20, &alias) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read, reinterpret_cast<int*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelJitCreateAliasOfSharedMemory(-1, Read, &alias) == SCE_KERNEL_ERROR_EBADF);
    Require(alias == -1);
    Require(sceKernelJitMapSharedMemory(fd, Read | Write, &address) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelJitMapSharedMemory(fd, 0x8, &address) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitMapSharedMemory(fd, Read, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitMapSharedMemory(fd, Read, reinterpret_cast<void**>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    void* roPage = mmap_nid_postfix(nullptr, 0x4000, Read | Write, 0x1002, -1, 0);
    Require(roPage != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    auto* roOut = static_cast<void**>(roPage);
    *roOut = nullptr;
    Require(mprotect_nid_postfix(roPage, 0x4000, Read) == 0);
    Require(sceKernelJitMapSharedMemory(fd, Read, roOut) == SCE_KERNEL_ERROR_EFAULT);
    Require(munmap_nid_postfix(roPage, 0x4000) == 0);
    Require(sceKernelJitMapSharedMemory(-1, Read, &address) == SCE_KERNEL_ERROR_EBADF);
    Require(address == nullptr);
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read, &alias) == 0);
    Require(sceKernelJitMapSharedMemory(alias, Read | Execute, &address) == SCE_KERNEL_ERROR_EACCES);
    Require(close_nid_postfix(alias) == 0);
    Require(close_nid_postfix(fd) == 0);
    Require(sceKernelJitMapSharedMemory(fd, Read, &address) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelJitCreateAliasOfSharedMemory(alias, Read, &alias) == SCE_KERNEL_ERROR_EBADF);

    const int shared = shm_open_nid_postfix(reinterpret_cast<const char*>(1), 0x2, 0600);
    Require(shared >= 0);
    Require(sceKernelJitMapSharedMemory(shared, Read, &address) == SCE_KERNEL_ERROR_EBADF);
    Require(close_nid_postfix(shared) == 0);
}

void OversizedAndWritablePrechecks() {
    int oversizedFd = -1;
    errno = 789;
    Require(sceKernelJitCreateSharedMemory("oversized", ~static_cast<std::size_t>(0x3fff), Read | Write | Execute, &oversizedFd) == SCE_KERNEL_ERROR_ENOMEM);
    Require(oversizedFd == -1);
    Require(errno == 789);

    if constexpr (sizeof(std::size_t) >= 8) {
        const auto aboveInt64 = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) + 1;
        errno = 790;
        Require(sceKernelJitCreateSharedMemory("above-int64", aboveInt64, Read | Write | Execute, &oversizedFd) == SCE_KERNEL_ERROR_ENOMEM);
        Require(oversizedFd == -1);
        Require(errno == 790);
    }

    const int fd = Create(nullptr, Read | Execute);

    void* roPage = mmap_nid_postfix(nullptr, 0x4000, Read | Write, 0x1002, -1, 0);
    Require(roPage != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    auto* roFdOut = static_cast<int*>(roPage);
    auto* roStartOut = static_cast<void**>(roPage);
    *roStartOut = nullptr;
    Require(mprotect_nid_postfix(roPage, 0x4000, Read) == 0);

    errno = 101;
    Require(sceKernelJitCreateSharedMemory("oversized-ro", ~static_cast<std::size_t>(0x3fff), Read | Write | Execute, roFdOut) == SCE_KERNEL_ERROR_ENOMEM);
    Require(errno == 101);

    errno = 102;
    Require(sceKernelJitCreateAliasOfSharedMemory(-1, Read, roFdOut) == SCE_KERNEL_ERROR_EBADF);
    Require(errno == 102);

    errno = 103;
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read | Write, roFdOut) == SCE_KERNEL_ERROR_EACCES);
    Require(errno == 103);

    errno = 104;
    Require(sceKernelJitMapSharedMemory(-1, Read, roStartOut) == SCE_KERNEL_ERROR_EBADF);
    Require(errno == 104);

    errno = 105;
    Require(sceKernelJitMapSharedMemory(fd, Read | Write, roStartOut) == SCE_KERNEL_ERROR_EACCES);
    Require(errno == 105);

    errno = 106;
    Require(sceKernelJitMapSharedMemory(-1, Read, reinterpret_cast<void**>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 106);

#ifdef _WIN32
    const int seed = shm_open_nid_postfix(reinterpret_cast<const char*>(1), 0x2, 0600);
    Require(seed >= 0);
    std::vector<int> exhausted;
    while (exhausted.size() < 16384) {
        const int dup = dup_nid_postfix(seed);
        if (dup < 0) break;
        exhausted.push_back(dup);
    }
    Require(exhausted.size() < 16384);

    errno = 201;
    Require(sceKernelJitCreateSharedMemory("efault-ro", Size, Read | Write | Execute, roFdOut) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 201);

    errno = 202;
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read, roFdOut) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 202);

    int emfileFd = -1;
    errno = 203;
    Require(sceKernelJitCreateSharedMemory("emfile-create", Size, Read | Write | Execute, &emfileFd) == SCE_KERNEL_ERROR_EMFILE);
    Require(emfileFd == -1);
    Require(errno == 203);

    errno = 204;
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read, &emfileFd) == SCE_KERNEL_ERROR_EMFILE);
    Require(emfileFd == -1);
    Require(errno == 204);

    for (const int extra : exhausted) Require(close_nid_postfix(extra) == 0);
    Require(close_nid_postfix(seed) == 0);
#else
    rlimit previous{};
    Require(::getrlimit(RLIMIT_NOFILE, &previous) == 0);
    const int lowest = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    Require(lowest >= 0);
    Require(::close(lowest) == 0);
    rlimit limited = previous;
    limited.rlim_cur = static_cast<rlim_t>(lowest);
    Require(::setrlimit(RLIMIT_NOFILE, &limited) == 0);

    errno = 201;
    const int createResult = sceKernelJitCreateSharedMemory("efault-ro", Size, Read | Write | Execute, roFdOut);
    const int createErrno = errno;
    errno = 202;
    const int aliasResult = sceKernelJitCreateAliasOfSharedMemory(fd, Read, roFdOut);
    const int aliasErrno = errno;

    int emfileFd = -1;
    errno = 203;
    const int createEmfileResult = sceKernelJitCreateSharedMemory("emfile-create", Size, Read | Write | Execute, &emfileFd);
    const int createEmfileErrno = errno;
    errno = 204;
    const int aliasEmfileResult = sceKernelJitCreateAliasOfSharedMemory(fd, Read, &emfileFd);
    const int aliasEmfileErrno = errno;

    Require(::setrlimit(RLIMIT_NOFILE, &previous) == 0);
    Require(createResult == SCE_KERNEL_ERROR_EFAULT && createErrno == 201);
    Require(aliasResult == SCE_KERNEL_ERROR_EFAULT && aliasErrno == 202);
    Require(emfileFd == -1);
    Require(createEmfileResult == SCE_KERNEL_ERROR_EMFILE && createEmfileErrno == 203);
    Require(aliasEmfileResult == SCE_KERNEL_ERROR_EMFILE && aliasEmfileErrno == 204);
#endif

    g_mappingObserved = false;
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(&ObserveMapping);
    errno = 333;
    Require(sceKernelJitMapSharedMemory(fd, Read, roStartOut) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 333);
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(nullptr);
    Require(!g_mappingObserved);

    Require(munmap_nid_postfix(roPage, 0x4000) == 0);
    Require(close_nid_postfix(fd) == 0);
}

}

int main() {
    WriteThroughAliasRunsThroughExecutableView();
    OneViewWritesAndRuns();
    Dup2KeepsTheDescriptorTableInStep();
#ifndef __APPLE__
    MmapOfTheDescriptorSharesTheObject();
#endif
    InvalidArgumentsAreRejected();
    OversizedAndWritablePrechecks();
}
