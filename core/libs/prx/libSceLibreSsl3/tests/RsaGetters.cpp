#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"

#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Factors = void (APS5_VABI*)(const LibreSsl3Rsa*, const void**, const void**);
using CrtParams = void (APS5_VABI*)(const LibreSsl3Rsa*, const void**, const void**, const void**);

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial = nullptr, std::size_t size = 0) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

void Run(Factors factors, CrtParams crtParams) {
    std::array<int, 6> values{};
    for (unsigned fields = 0; fields < 32; ++fields) {
        LibreSsl3Rsa rsa{};
        rsa.p = fields & 1 ? &values[0] : nullptr;
        rsa.q = fields & 2 ? &values[1] : nullptr;
        rsa.dmp1 = fields & 4 ? &values[2] : nullptr;
        rsa.dmq1 = fields & 8 ? &values[3] : nullptr;
        rsa.iqmp = fields & 16 ? &values[4] : nullptr;
        rsa.references = 37;
        rsa.flags = 123;
        std::array<unsigned char, sizeof(rsa)> before;
        std::memcpy(before.data(), &rsa, sizeof(rsa));
        const void* const sentinel = &values[5];
        for (unsigned outputs = 0; outputs < 4; ++outputs) {
            const void* p = sentinel;
            const void* q = sentinel;
            factors(&rsa, outputs & 1 ? &p : nullptr, outputs & 2 ? &q : nullptr);
            Require(p == (outputs & 1 ? rsa.p : sentinel));
            Require(q == (outputs & 2 ? rsa.q : sentinel));
            const void* shared = sentinel;
            factors(&rsa, outputs & 1 ? &shared : nullptr, outputs & 2 ? &shared : nullptr);
            Require(shared == (outputs & 2 ? rsa.q : outputs & 1 ? rsa.p : sentinel));
            Require(std::memcmp(before.data(), &rsa, sizeof(rsa)) == 0);
        }
        for (unsigned outputs = 0; outputs < 8; ++outputs) {
            const void* dmp1 = sentinel;
            const void* dmq1 = sentinel;
            const void* iqmp = sentinel;
            crtParams(&rsa, outputs & 1 ? &dmp1 : nullptr, outputs & 2 ? &dmq1 : nullptr,
                outputs & 4 ? &iqmp : nullptr);
            Require(dmp1 == (outputs & 1 ? rsa.dmp1 : sentinel));
            Require(dmq1 == (outputs & 2 ? rsa.dmq1 : sentinel));
            Require(iqmp == (outputs & 4 ? rsa.iqmp : sentinel));
            const void* shared = sentinel;
            crtParams(&rsa, outputs & 1 ? &shared : nullptr, outputs & 2 ? &shared : nullptr,
                outputs & 4 ? &shared : nullptr);
            Require(shared == (outputs & 4 ? rsa.iqmp : outputs & 2 ? rsa.dmq1 : outputs & 1 ? rsa.dmp1 : sentinel));
            Require(std::memcmp(before.data(), &rsa, sizeof(rsa)) == 0);
        }
    }
    factors(nullptr, nullptr, nullptr);
    crtParams(nullptr, nullptr, nullptr, nullptr);
    for (unsigned outputs = 1; outputs < 4; ++outputs) {
        const void* value = &values[5];
        bool threw = false;
        try { factors(nullptr, outputs & 1 ? &value : nullptr, outputs & 2 ? &value : nullptr); }
        catch (const std::exception&) { threw = true; }
        Require(threw && value == &values[5]);
    }
    for (unsigned outputs = 1; outputs < 8; ++outputs) {
        const void* value = &values[5];
        bool threw = false;
        try {
            crtParams(nullptr, outputs & 1 ? &value : nullptr, outputs & 2 ? &value : nullptr,
                outputs & 4 ? &value : nullptr);
        }
        catch (const std::exception&) { threw = true; }
        Require(threw && value == &values[5]);
    }

    LibreSsl3Rsa guestRsa{};
    guestRsa.p = &values[0];
    guestRsa.q = &values[1];
    guestRsa.dmp1 = &values[2];
    guestRsa.dmq1 = &values[3];
    guestRsa.iqmp = &values[4];
    const void* const sentinel = &values[5];
    ReadOnlyPage roOut(&sentinel, sizeof(sentinel));
    const void** const badPtr = roOut.As<const void*>();

    const void* outP = sentinel;
    const void* outQ = sentinel;
    errno = 73;
    ExpectErr<std::runtime_error>([&] { factors(reinterpret_cast<const LibreSsl3Rsa*>(0x10), &outP, &outQ); });
    Require(errno == 73 && outP == sentinel && outQ == sentinel);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { factors(&guestRsa, badPtr, &outQ); });
    Require(errno == 73 && outQ == sentinel);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { factors(&guestRsa, &outP, badPtr); });
    Require(errno == 73 && outP == sentinel);

    const void* outDmp1 = sentinel;
    const void* outDmq1 = sentinel;
    const void* outIqmp = sentinel;
    const void* shared = sentinel;
    errno = 73;
    ExpectErr<std::runtime_error>([&] { crtParams(reinterpret_cast<const LibreSsl3Rsa*>(0x10), &outDmp1, &outDmq1, &outIqmp); });
    Require(errno == 73 && outDmp1 == sentinel && outDmq1 == sentinel && outIqmp == sentinel);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { crtParams(&guestRsa, badPtr, &outDmq1, &outIqmp); });
    Require(errno == 73 && outDmq1 == sentinel && outIqmp == sentinel);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { crtParams(&guestRsa, &outDmp1, badPtr, &outIqmp); });
    Require(errno == 73 && outDmp1 == sentinel && outIqmp == sentinel);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { crtParams(&guestRsa, &outDmp1, &outDmq1, badPtr); });
    Require(errno == 73 && outDmp1 == sentinel && outDmq1 == sentinel);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { crtParams(&guestRsa, &shared, &shared, badPtr); });
    Require(errno == 73 && shared == sentinel);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
#ifdef _WIN32
    const auto library = LoadLibraryExW(std::filesystem::path(argv[1]).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    Require(library != nullptr);
    const auto factors = std::bit_cast<Factors>(GetProcAddress(library, "PcUZS7XHwHk"));
    const auto crtParams = std::bit_cast<CrtParams>(GetProcAddress(library, "eyrIagsax0w"));
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr);
    const auto factors = reinterpret_cast<Factors>(dlsym(library, "PcUZS7XHwHk"));
    const auto crtParams = reinterpret_cast<CrtParams>(dlsym(library, "eyrIagsax0w"));
#endif
    Require(factors != nullptr && crtParams != nullptr);
    bool failed = false;
    try { factors(nullptr, nullptr, nullptr); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "RSA_get0_factors threw: %s\n", error.what());
        failed = true;
    }
    try { crtParams(nullptr, nullptr, nullptr, nullptr); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "RSA_get0_crt_params threw: %s\n", error.what());
        failed = true;
    }
    if (failed) return 1;
    try { Run(factors, crtParams); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "RSA getter threw: %s\n", error.what());
        failed = true;
    }
    if (failed) return 1;
    std::puts("LibreSSL3 RSA getter tests passed");
}
