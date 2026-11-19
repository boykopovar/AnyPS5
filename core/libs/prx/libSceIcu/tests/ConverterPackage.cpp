#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t FileAccess = 4;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<class TException, class TFunction>
void RequireThrow(TFunction function, const char* what) {
    try {
        function();
    } catch (const TException&) {
        return;
    }
    Require(false, what);
}

struct Pages {
    char* begin = nullptr;
    std::size_t size = 0;

    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        begin = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(begin != nullptr, "VirtualAlloc");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto* mapped = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap");
        begin = static_cast<char*>(mapped);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        VirtualFree(begin, 0, MEM_RELEASE);
#else
        munmap(begin, size * 2);
#endif
    }

    void Protect(std::size_t page, bool writable, bool readable = true) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protection = readable ? (writable ? PAGE_READWRITE : PAGE_READONLY) : PAGE_NOACCESS;
        Require(VirtualProtect(begin + page * size, size, protection, &previous) != 0, "VirtualProtect");
#else
        const int protection = readable ? (writable ? PROT_READ | PROT_WRITE : PROT_READ) : PROT_NONE;
        Require(mprotect(begin + page * size, size, protection) == 0, "mprotect");
#endif
    }
};

void StatusAndNulls() {
    const auto* invalid = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    std::int32_t status = IllegalArgument;
    Require(ucnv_openPackage_nid_postfix(invalid, invalid, &status) == nullptr, "prefailed result");
    Require(status == IllegalArgument, "prefailed status");
    RequireThrow<std::invalid_argument>([&] { ucnv_openPackage_nid_postfix(invalid, invalid, nullptr); }, "null status");
    status = Success;
    RequireThrow<std::invalid_argument>([&] { ucnv_openPackage_nid_postfix(invalid, nullptr, &status); }, "null converter name");
    Require(status == Success, "null converter status");
}

void InvalidAndOrdering() {
    std::array<char, 60> converter{};
    converter.fill('x');
    std::int32_t status = Success;
    const auto* invalid = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    Require(ucnv_openPackage_nid_postfix(invalid, converter.data(), &status) == nullptr, "long converter result");
    Require(status == IllegalArgument, "long converter status");

    status = Success;
    RequireThrow<std::invalid_argument>([&] { ucnv_openPackage_nid_postfix("package", invalid, &status); }, "unreadable converter");
    Require(status == Success, "unreadable converter status");
    RequireThrow<std::invalid_argument>([&] { ucnv_openPackage_nid_postfix(invalid, "converter", &status); }, "unreadable package");
    Require(status == Success, "unreadable package status");
}

void BoundariesAndReadOnly() {
    Pages pages;
    auto* end = pages.begin + pages.size;
    std::memcpy(end - 10, "pack", 5);
    std::memcpy(end - 5, "name", 5);
    pages.Protect(0, false);
    pages.Protect(1, false, false);
    std::int32_t status = Success;
    errno = 73;
    Require(ucnv_openPackage_nid_postfix(end - 10, end - 5, &status) == nullptr, "guarded names");
    Require(status == FileAccess, "guarded names status");
    Require(errno == 73, "guarded names errno");

    pages.Protect(0, true);
    auto* readOnlyStatus = reinterpret_cast<std::int32_t*>(pages.begin);
    *readOnlyStatus = Success;
    pages.Protect(0, false);
    std::array<char, 60> converter{};
    converter.fill('y');
    RequireThrow<std::invalid_argument>(
        [&] { ucnv_openPackage_nid_postfix(nullptr, converter.data(), readOnlyStatus); }, "read-only status");
    Require(*readOnlyStatus == Success, "read-only status unchanged");
}

}

int main() {
    StatusAndNulls();
    InvalidAndOrdering();
    BoundariesAndReadOnly();
}
