#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "File vector memory check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Transfer {
    const char* name;
    bool posix;
    bool positioned;
    bool write;
    std::function<std::int64_t(int, const KernelIovec*, int)> call;
};

static std::uintptr_t observedTable = 0;
static std::size_t observedBytes = 0;

static void ObserveRead(std::uintptr_t address, std::size_t bytes) {
    if (address == observedTable) observedBytes = bytes;
}

static void Protect(void* address, std::size_t size, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(address, size, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(::mprotect(address, size, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

int main(int argc, char** argv) {
    const std::array<Transfer, 10> transfers{{
        {"readv", true, false, false, readv_nid_postfix},
        {"writev", true, false, true, writev_nid_postfix},
        {"preadv", true, true, false, [](int fd, const KernelIovec* iov, int count) { return preadv_nid_postfix(fd, iov, count, 1); }},
        {"pwritev", true, true, true, [](int fd, const KernelIovec* iov, int count) { return pwritev_nid_postfix(fd, iov, count, 1); }},
        {"_readv", true, false, false, _readv_nid_postfix},
        {"_writev", true, false, true, _writev_nid_postfix},
        {"sceKernelReadv", false, false, false, sceKernelReadv},
        {"sceKernelWritev", false, false, true, sceKernelWritev},
        {"sceKernelPreadv", false, true, false, [](int fd, const KernelIovec* iov, int count) { return sceKernelPreadv(fd, iov, count, 1); }},
        {"sceKernelPwritev", false, true, true, [](int fd, const KernelIovec* iov, int count) { return sceKernelPwritev(fd, iov, count, 1); }},
    }};
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t page = system.dwPageSize;
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr);
#else
    const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
    auto* memory = static_cast<char*>(::mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED);
#endif
    std::array<char, 2> bytes{'A', 'B'};
    const std::array<KernelIovec, 2> vectors{{{bytes.data(), 1}, {bytes.data() + 1, 1}}};
    std::memcpy(memory, vectors.data(), sizeof(vectors));
    std::memcpy(memory + page - sizeof(KernelIovec), vectors.data(), sizeof(KernelIovec));
    Protect(memory, page, true);
    Protect(memory + page, page, false);
    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-iovec-memory-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    AddPathAlias_nid_no_patch("/anyps5-iovec-memory", path.parent_path().string().c_str());
    const auto guestPath = "/anyps5-iovec-memory/" + path.filename().string();
    for (const auto& transfer : transfers) {
        if (argc > 1 && transfer.name != std::string(argv[1])) continue;
        std::fprintf(stderr, "%s\n", transfer.name);
        { std::ofstream file(path, std::ios::binary); file << "ABCD"; Require(static_cast<bool>(file)); }
        const int fd = sceKernelOpen(guestPath.c_str(), SCE_KERNEL_O_RDWR, 0);
        Require(fd >= 0);
        const std::array<const KernelIovec*, 6> invalid{{
            nullptr,
            reinterpret_cast<const KernelIovec*>(std::uintptr_t{1}),
            reinterpret_cast<const KernelIovec*>(memory + page),
            reinterpret_cast<const KernelIovec*>(memory + page - sizeof(KernelIovec)),
            reinterpret_cast<const KernelIovec*>(memory + page - sizeof(KernelIovec) / 2),
            reinterpret_cast<const KernelIovec*>(UINTPTR_MAX - sizeof(KernelIovec) + 1),
        }};
        for (const auto* table : invalid) {
            *__error_nid_postfix() = 0;
            const auto expected = transfer.posix ? -1 : static_cast<int>(0x8002000eu);
            Require(transfer.call(fd, table, 2) == expected);
            if (transfer.posix) Require(*__error_nid_postfix() == 14);
            Require(sceKernelLseek(fd, 0, SEEK_CUR) == 0);
            Require((bytes == std::array<char, 2>{'A', 'B'}));
            std::ifstream file(path, std::ios::binary);
            std::string contents;
            file >> contents;
            Require(contents == "ABCD");
            Require(transfer.call(fd, table, 0) == 0);
            const auto invalidCount = transfer.posix ? -1 : static_cast<int>(0x80020016u);
            Require(transfer.call(fd, table, -1) == invalidCount);
            if (transfer.posix) Require(*__error_nid_postfix() == 22);
            Require(transfer.call(fd, table, 1025) == invalidCount);
            if (transfer.posix) Require(*__error_nid_postfix() == 22);
        }
        observedTable = reinterpret_cast<std::uintptr_t>(memory);
        observedBytes = 0;
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ObserveRead);
        Require(transfer.call(fd, reinterpret_cast<const KernelIovec*>(memory), 2) == 2);
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
        Require(observedBytes == sizeof(vectors));
        Require(sceKernelLseek(fd, 0, SEEK_CUR) == (transfer.positioned ? 0 : 2));
        Require(std::memcmp(memory, vectors.data(), sizeof(vectors)) == 0);
        if (transfer.write) {
            std::ifstream file(path, std::ios::binary);
            std::string contents;
            file >> contents;
            Require(contents == (transfer.positioned ? "AABD" : "ABCD"));
        } else {
            Require((bytes == (transfer.positioned ? std::array<char, 2>{'B', 'C'} : std::array<char, 2>{'A', 'B'})));
        }
        Require(sceKernelLseek(fd, 0, SEEK_SET) == 0);
        Require(transfer.call(fd, reinterpret_cast<const KernelIovec*>(memory + page - sizeof(KernelIovec)), 1) == 1);
        Require(sceKernelLseek(fd, 0, SEEK_CUR) == (transfer.positioned ? 0 : 1));
        Require(sceKernelLseek(fd, 0, SEEK_SET) == 0);
        std::array<KernelIovec, 1024> maximum{};
        maximum.back() = {bytes.data(), 1};
        Require(transfer.call(fd, maximum.data(), static_cast<int>(maximum.size())) == 1);
        Require(sceKernelLseek(fd, 0, SEEK_CUR) == (transfer.positioned ? 0 : 1));
        Require(sceKernelClose(fd) == 0);
        bytes = {'A', 'B'};
    }
    Require(std::filesystem::remove(path));
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
    Require(::munmap(memory, page * 2) == 0);
#endif
}
