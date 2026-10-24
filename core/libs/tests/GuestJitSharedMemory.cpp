#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

extern "C" {
int APS5_VABI sceKernelJitCreateSharedMemory(const char*, std::size_t, int, int*);
int APS5_VABI sceKernelJitCreateAliasOfSharedMemory(int, int, int*);
int APS5_VABI sceKernelJitMapSharedMemory(int, int, void**);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI dup_nid_postfix(int);
int APS5_VABI dup2_nid_postfix(int, int);
int APS5_VABI shm_open_nid_postfix(const char*, int, int);
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
}

namespace {

constexpr std::size_t Size = 0x10000;
constexpr int Read = 0x1;
constexpr int Write = 0x2;
constexpr int Execute = 0x4;

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
    Require(sceKernelMunmap(again, Size) == 0);
    Require(sceKernelMunmap(writable, Size) == 0);
    Require(Call(executable) == 0x12345678);
    Require(sceKernelMunmap(executable, Size) == 0);
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
    Require(sceKernelMunmap(view, Size) == 0);
    Require(sceKernelMunmap(code, Size) == 0);
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
    Require(sceKernelMunmap(const_cast<unsigned char*>(jit), Size) == 0);
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
    Require(sceKernelMunmap(same, Size) == 0);
    Require(sceKernelMunmap(readable, Size) == 0);
    Require(sceKernelMunmap(writable, Size) == 0);
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
    Require(fd == -1);

    fd = Create(nullptr, Read | Execute);
    int alias = -1;
    void* address = nullptr;
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read | Write, &alias) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, 0x20, &alias) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateAliasOfSharedMemory(fd, Read, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitCreateAliasOfSharedMemory(-1, Read, &alias) == SCE_KERNEL_ERROR_EBADF);
    Require(alias == -1);
    Require(sceKernelJitMapSharedMemory(fd, Read | Write, &address) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelJitMapSharedMemory(fd, 0x8, &address) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelJitMapSharedMemory(fd, Read, nullptr) == SCE_KERNEL_ERROR_EINVAL);
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

}

int main() {
    WriteThroughAliasRunsThroughExecutableView();
    OneViewWritesAndRuns();
    Dup2KeepsTheDescriptorTableInStep();
#ifndef __APPLE__
    MmapOfTheDescriptorSharesTheObject();
#endif
    InvalidArgumentsAreRejected();
}
