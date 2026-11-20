#include "prx/libc/include/GuestPtr.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest pointer: %s\n", message);
    std::abort();
}

struct Payload {
    std::uint32_t first;
    std::uint64_t second;
};

std::size_t PageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

void* MapPage(std::size_t bytes) {
#ifdef _WIN32
    return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* page = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return page == MAP_FAILED ? nullptr : page;
#endif
}

bool MakeReadOnly(void* page, std::size_t bytes) {
#ifdef _WIN32
    DWORD previous = 0;
    return VirtualProtect(page, bytes, PAGE_READONLY, &previous) != 0;
#else
    return mprotect(page, bytes, PROT_READ) == 0;
#endif
}

bool MakeReadWrite(void* page, std::size_t bytes) {
#ifdef _WIN32
    DWORD previous = 0;
    return VirtualProtect(page, bytes, PAGE_READWRITE, &previous) != 0;
#else
    return mprotect(page, bytes, PROT_READ | PROT_WRITE) == 0;
#endif
}

void UnmapPage(void* page, std::size_t bytes) {
#ifdef _WIN32
    (void)bytes;
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, bytes);
#endif
}

void CheckMapped() {
    const Payload source{7, 0x1122334455667788ull};
    Payload copied{};
    Require(GuestPtr<const Payload>(&source).CopyIn(copied), "a readable guest pointer did not copy in");
    Require(std::memcmp(&copied, &source, sizeof(Payload)) == 0, "a copied-in value differs from its source");
    const Payload replacement{9, 0xaabbccddeeff0011ull};
    Require(GuestPtr<Payload>(&copied).CopyOut(replacement), "a writable guest pointer did not copy out");
    Require(std::memcmp(&copied, &replacement, sizeof(Payload)) == 0, "a copied-out value differs from its source");
}

void CheckArraysAndBytes() {
    const std::array<Payload, 2> source{{{1, 2}, {3, 4}}};
    std::array<Payload, 2> copied{};
    Require(GuestPtr<const Payload>(source.data()).CopyIn(copied.data(), copied.size()), "a readable guest array did not copy in");
    Require(std::memcmp(copied.data(), source.data(), sizeof(source)) == 0, "a copied-in array differs from its source");
    const std::array<Payload, 2> replacement{{{5, 6}, {7, 8}}};
    Require(GuestPtr<Payload>(copied.data()).CopyOut(replacement.data(), replacement.size()), "a writable guest array did not copy out");
    Require(std::memcmp(copied.data(), replacement.data(), sizeof(replacement)) == 0, "a copied-out array differs from its source");
    Require(GuestPtr<const Payload>(source.data()).CopyIn(copied.data(), 0), "a zero-length guest array did not copy in");
    Require(GuestPtr<Payload>(copied.data()).CopyOut(replacement.data(), 0), "a zero-length guest array did not copy out");
    Require(GuestPtr<const Payload>().CopyIn(copied.data(), 0), "a zero-length copy from an empty guest pointer failed");

    std::array<std::byte, 16> bytes{};
    const char text[] = "guest";
    Require(GuestBytes(bytes.data()).CopyOut(text, sizeof(text)), "a writable guest byte range did not copy out");
    Require(std::memcmp(bytes.data(), text, sizeof(text)) == 0, "a copied-out byte range differs from its source");
    std::array<char, 16> readback{};
    Require(GuestBytes(bytes.data()).CopyIn(readback.data(), sizeof(text)), "a readable guest byte range did not copy in");
    Require(std::memcmp(readback.data(), text, sizeof(text)) == 0, "a copied-in byte range differs from its source");
    Require(GuestBytes(bytes.data()).CopyIn(readback.data(), 0), "a zero-length guest byte range did not copy in");
    Require(GuestBytes(bytes.data()).CopyOut(text, 0), "a zero-length guest byte range did not copy out");
    Require(GuestBytes().CopyOut(text, 0), "a zero-length copy to an empty guest byte range failed");
}

void CheckFaults() {
    Payload value{};
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        const auto* pointer = reinterpret_cast<const void*>(address);
        Require(!GuestPtr<const Payload>(pointer).CopyIn(value), "an unreadable guest pointer was copied in");
        Require(!GuestPtr<Payload>(pointer).CopyOut(value), "an unwritable guest pointer was copied out");
        Require(!GuestPtr<Payload>(pointer).CopyIn(&value, 1), "an unreadable guest array was copied in");
        Require(!GuestPtr<Payload>(pointer).CopyOut(&value, 1), "an unwritable guest array was copied out");
        Require(!GuestBytes(pointer).CopyIn(&value, sizeof(value)), "an unreadable guest byte range was copied in");
        Require(!GuestBytes(pointer).CopyOut(&value, sizeof(value)), "an unwritable guest byte range was copied out");
    }
    const GuestPtr<const Payload> empty;
    Require(!empty.CopyIn(value), "an empty guest pointer was copied in");
    Require(empty.Address() == nullptr, "an empty guest pointer has an address");
    Require(!GuestBytes().CopyOut(&value, sizeof(value)), "an empty guest byte range was copied out");
    Require(!GuestPtr<Payload>(&value).CopyIn(&value, std::numeric_limits<std::size_t>::max()), "an overflowing guest array was copied in");
    Require(!GuestPtr<Payload>(&value).CopyOut(&value, std::numeric_limits<std::size_t>::max()), "an overflowing guest array was copied out");
}

void CheckReadOnly() {
    const std::size_t pageSize = PageSize();
    void* page = MapPage(pageSize);
    Require(page != nullptr, "a read-only test page was not mapped");
    const Payload source{7, 0x1122334455667788ull};
    std::memcpy(page, &source, sizeof(source));
    Require(MakeReadOnly(page, pageSize), "the test page was not made read-only");
    Payload copied{};
    Require(GuestPtr<const Payload>(page).CopyIn(copied), "a read-only guest pointer was not copied in");
    Require(std::memcmp(&copied, &source, sizeof(source)) == 0, "a copied-in read-only value differs from its source");
    Require(!GuestPtr<Payload>(page).CopyOut(source), "a read-only guest pointer was copied out");
    Require(!GuestBytes(page).CopyOut(&source, sizeof(source)), "a read-only guest byte range was copied out");
    Require(GuestBytes(page).CopyIn(&copied, sizeof(copied)), "a read-only guest byte range was not copied in");
    Require(MakeReadWrite(page, pageSize), "the test page was not made writable");
    UnmapPage(page, pageSize);
}

}  // namespace

int main() {
    CheckMapped();
    CheckArraysAndBytes();
    CheckFaults();
    CheckReadOnly();
    std::puts("guest pointer tests passed");
}
